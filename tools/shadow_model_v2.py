"""Frozen F15E shadow models. No actuation, file/config reads only during analysis."""
from pathlib import Path
import json,math,argparse,hashlib
import numpy as np
import analyze_shadow as base
AXES=base.AXES
def common_cadence(states,hz=30):
    """Causal sample selection; never interpolate using a future pose.
    High-rate recordings are decimated onto a common nominal cadence.
    Original measurement timestamps and raw files remain unchanged.
    """
    out=[];key=None;next_time=0.;last=-1e30
    for r in states:
        current=(r['pawn'],r['plane_id'],r['epoch'])
        if current!=key or r['t']-last>.12 or r['t']<last:
            key=current;next_time=r['t']
        last=r['t']
        if r['t']+1e-9>=next_time:
            out.append(r);next_time+=1/hz
            if next_time<r['t']-.1:next_time=r['t']+1/hz
    return out
def records(states):
    out=[]
    for a,b in zip(states,states[1:]):
        dt=b['t']-a['t']
        if not .008<=dt<=.12 or a['epoch']!=b['epoch']or a['pawn']!=b['pawn']:continue
        omega=sum((np.cross(a['R'][:,i],b['R'][:,i])for i in range(3)))/(2*dt)*180/math.pi
        w=np.array([-omega@b['R'][:,1],omega@b['R'][:,2],-omega@b['R'][:,0]])
        if max(abs(w))>600:continue
        # Backward-looking derivative is available only at b, not at interval midpoint.
        out.append(dict(t=b['t'],w=w,epoch=b['epoch'],pawn=b['pawn'],plane=b['plane_id'],speed=b['speed'],brake=b['brake']))
    return out
def features(speed,brake,model):
    z=(np.clip(np.asarray(speed),*model['speed_bounds'])-model['speed_center'])/model['speed_scale']
    cols=[np.ones_like(z)]
    if model['family']!='constant':cols.append(z)
    if model['family']=='quadratic_speed':cols.append(z*z)
    if model['family']=='speed_brake':cols.append(np.clip(brake,0,1))
    return np.array(cols).T
def gain(speed,brake,model):return np.clip(features(speed,brake,model)@model['gain_coefficients'],1,600)
def make_pairs(rr):
    pairs=[(a,b)for a,b in zip(rr,rr[1:])if a['epoch']==b['epoch']and a['pawn']==b['pawn']and .008<=b['t']-a['t']<=.12]
    return dict(t=np.array([(a['t']+b['t'])/2 for a,b in pairs]),start=np.array([a['t']for a,b in pairs]),end=np.array([b['t']for a,b in pairs]),dt=np.array([b['t']-a['t']for a,b in pairs]),w=np.array([a['w']for a,b in pairs]),next=np.array([b['w']for a,b in pairs]),speed=np.array([a['speed']for a,b in pairs]),brake=np.array([a['brake']for a,b in pairs]))
def fit(P,events,axis,end,family,legacy=False):
    mask=P['end']<end-.35
    if sum(mask)<150:raise ValueError('insufficient training')
    bounds=np.percentile(P['speed'][mask],[1,99]).tolist()
    proto=dict(family=family,speed_bounds=bounds,speed_center=float(np.median(P['speed'][mask])),speed_scale=max(50,float(np.std(P['speed'][mask]))))
    phi=features(P['speed'],P['brake'],proto);best=None
    delays=np.arange(0,.121 if legacy else .301,.02)
    taus=np.geomspace(.04,1.2 if legacy else 2.0,24)
    for delay in delays:
      u,valid=base.input_at(events,P['t']-delay,axis);tr=mask&valid
      if np.std(u[tr])<.08:continue
      for tau in taus:
        decay=np.exp(-P['dt']/tau);scale=1-decay
        X=np.column_stack([phi*u[:,None],np.ones(len(u))])*scale[:,None]
        y=P['next'][:,axis]-decay*P['w'][:,axis]
        # Small dimensionless ridge reduces unstable speed coefficients.
        xx=X[tr];yy=y[tr];ridge=np.diag(np.maximum(np.sum(xx*xx,axis=0),1e-8))*.002
        coef=np.linalg.solve(xx.T@xx+ridge,xx.T@yy)
        if abs(coef[-1])>30:continue
        check=np.column_stack([np.linspace(*bounds,12),np.zeros(12)])
        gg=features(check[:,0],check[:,1],proto)@coef[:-1]
        if min(gg)<1 or max(gg)>600:continue
        prediction=decay*P['w'][:,axis]+X@coef;loss=float(np.mean((prediction[tr]-P['next'][tr,axis])**2))
        if best is None or loss<best['training_mse']:
            best=dict(**proto,tau_s=float(tau),delay_s=float(delay),gain_coefficients=coef[:-1].tolist(),bias_deg_s=float(coef[-1]),training_mse=loss,training_pairs=int(sum(tr)),delay_at_upper_bound=bool(abs(delay-delays[-1])<1e-6),tau_at_upper_bound=bool(abs(tau-taus[-1])<1e-6))
    if best is None:raise ValueError('no plausible model')
    return best
def score(rr,events,axis,model,start,end,horizon=.2,max_origins=250,origin_speed_bounds=None):
    tt=np.array([r['t']for r in rr]);origins=[i for i,r in enumerate(rr)if start+.35<r['t']<end-horizon-.05 and (origin_speed_bounds is None or origin_speed_bounds[0]<=r['speed']<=origin_speed_bounds[1])]
    if len(origins)>max_origins:origins=[origins[i]for i in np.linspace(0,len(origins)-1,max_origins,dtype=int)]
    pred=[];truth=[];hold=[];outside=0
    for i in origins:
        a=rr[i];j=int(np.searchsorted(tt,a['t']+horizon))
        if j>=len(rr)or tt[j]-a['t']>horizon+.05:continue
        seg=rr[i:j+1]
        if any(r['epoch']!=a['epoch']or r['pawn']!=a['pawn']for r in seg)or any(b['t']-c['t']>.12 for c,b in zip(seg,seg[1:])):continue
        mids=np.array([(c['t']+b['t'])/2-model['delay_s']for c,b in zip(seg,seg[1:])])
        past,valid=base.input_at(events,mids,axis);u0,valid0=base.input_at(events,np.array([a['t']]),axis)
        if not all(valid)or not valid0[0]:continue
        # No future commands, speed or brake information is used in this forecast.
        u=np.where(mids<=a['t'],past,u0[0]);w=a['w'][axis];g=float(gain(a['speed'],a['brake'],model))
        for k,(c,b)in enumerate(zip(seg,seg[1:])):
            decay=math.exp(-(b['t']-c['t'])/model['tau_s']);w=decay*w+(1-decay)*(g*u[k]+model['bias_deg_s'])
        pred.append(w);truth.append(rr[j]['w'][axis]);hold.append(a['w'][axis]);outside+=not(model['speed_bounds'][0]<=a['speed']<=model['speed_bounds'][1])
    if not pred:return dict(samples=0,rmse_deg_s=None)
    err=np.array(pred)-truth
    return dict(samples=len(pred),rmse_deg_s=float(np.sqrt(np.mean(err*err))),p95_abs_deg_s=float(np.percentile(abs(err),95)),rate_hold_rmse_deg_s=float(np.sqrt(np.mean((np.array(hold)-truth)**2))),out_of_training_speed_samples=outside)
def calibrate(path,out):
    states,inputs,bad=base.load(path);rr=records(states)
    planes={int(r['plane'])for r in rr};assert planes=={25010},planes
    events=base.InputSeries(sorted(inputs,key=lambda r:r['t']));P=make_pairs(rr)
    select_start=rr[int(len(rr)*.45)]['t'];test_start=rr[int(len(rr)*.6)]['t'];end=rr[-1]['t']+.05
    bundle=dict(schema='ac8-f15e-shadow/v2',plane_type_id=25010,aircraft='F-15E',actuation=False,source_csv_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),rate_timestamp='measurement available at interval end',selection='fit first45%, choose family on next15%, refit first60%, audit final40%; next flight is fresh validation',axes={},baseline_axes={},comparison={})
    for axis,name in enumerate(AXES):
        candidates=[]
        for family,legacy in [('constant',True),('constant',False),('linear_speed',False),('quadratic_speed',False),('speed_brake',False)]:
            model=fit(P,events,axis,select_start,family,legacy);validation=score(rr,events,axis,model,select_start,test_start)
            if validation['samples']>=30:candidates.append((validation['rmse_deg_s'],family,legacy,validation))
        candidates.sort(key=lambda x:x[0]);chosen=candidates[0]
        # A more complex family needs at least3% inner-validation improvement.
        old=next(c for c in candidates if c[1]=='constant'and c[2])
        if chosen[0]>old[0]*.97:chosen=old
        selected=fit(P,events,axis,test_start,chosen[1],chosen[2]);baseline=fit(P,events,axis,test_start,'constant',True)
        bundle['axes'][name]=selected
        bundle['baseline_axes'][name]=baseline
        bundle['comparison'][name]={'inner_candidates':[{'family':c[1],'legacy_delay_grid':c[2],'rmse':c[0]}for c in candidates],'chosen_family':chosen[1],'chosen_legacy_grid':chosen[2],'horizons':{str(h):{'selected':score(rr,events,axis,selected,test_start,end,h),'baseline':score(rr,events,axis,baseline,test_start,end,h)}for h in(.1,.2,.3)}}
    out.write_text(json.dumps(bundle,indent=2),encoding='utf-8')
    return bundle
def validate_frozen(session,model_path):
    model=json.loads(model_path.read_text(encoding='utf-8'));results=[]
    for p in sorted(session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv')):
        states,inputs,bad=base.load(p);rr=records(states)
        for actor in {r['pawn']for r in rr}:
            group=[r for r in rr if r['pawn']==actor];plane={int(r['plane'])for r in group}
            if plane!={model['plane_type_id']}:
                results.append(dict(file=p.name,actor=actor,status='unsupported_aircraft_model_not_applied',plane_ids=list(plane)));continue
            events=base.InputSeries(sorted([r for r in inputs if r['pawn']==actor],key=lambda x:x['t']))
            if len(group)<180 or len(events)<30:results.append(dict(file=p.name,status='insufficient_data'));continue
            same=hashlib.sha256(p.read_bytes()).hexdigest()==model['source_csv_sha256']
            horizons={str(h):{name:{'candidate':score(group,events,axis,model['axes'][name],group[0]['t'],group[-1]['t']+.05,h,500),'baseline':score(group,events,axis,model['baseline_axes'][name],group[0]['t'],group[-1]['t']+.05,h,500)}for axis,name in enumerate(AXES)}for h in(.1,.2,.3)}
            clock=[]
            actor_states=[r for r in states if r['pawn']==actor]
            for a,b in zip(actor_states,actor_states[1:]):
                dt=b['t']-a['t'];sim=b.get('sim_seconds',-1)-a.get('sim_seconds',-1)
                if a.get('sim_seconds',-1)>=0 and a['epoch']==b['epoch']and .008<=dt<=.12 and sim>=0:clock.append(sim/dt)
            results.append(dict(file=p.name,plane_id=model['plane_type_id'],status='training_replay_not_fresh_validation'if same else'fresh_frozen_model_validation',training_source_same_as_test=same,horizons=horizons,simulation_to_wall_time_ratio_percentiles=np.percentile(clock,[10,50,90]).tolist()if clock else None))
    report=dict(actuation=False,frozen_parameters=True,results=results,limits=['Not MPC actuation; independent axis surrogate only','F15E25010 only; other aircraft are recorded but excluded','World speed, not measured airspeed; original data came from one flight','No future controls or future speed used; derivative timestamp moved to measurement availability','Timing/regime/cross-axis effects remain incomplete'])
    (session/'f15e-frozen-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    lines=['F-15E 固定模型验证：参数来自上一次实验，本次不重新拟合这些参数。','仍为影子模式，不控制飞机。']
    for r in results:
        lines.append(f"{r.get('file')}: {r['status']}")
        for h,axes in r.get('horizons',{}).items():
            for name,pair in axes.items():
                x=pair['candidate'];b=pair['baseline']
                lines.append(f"  {h}s {name}: 候选RMSE={x.get('rmse_deg_s')}，固定模型RMSE={b.get('rmse_deg_s')} deg/s；样本={x['samples']}；超出训练速度范围={x.get('out_of_training_speed_samples',0)}")
    (session/'固定模型验证.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--calibrate',type=Path);p.add_argument('--model',type=Path,required=True);p.add_argument('--session',type=Path);a=p.parse_args()
    r=calibrate(a.calibrate,a.model)if a.calibrate else validate_frozen(a.session,a.model)
    if a.calibrate:print(json.dumps({k:{'family':v['chosen_family'],'legacy':v['chosen_legacy_grid'],'0.2s':v['horizons']['0.2']}for k,v in r['comparison'].items()}))
    else:print(f"Frozen model validation: {len(r['results'])} groups")
