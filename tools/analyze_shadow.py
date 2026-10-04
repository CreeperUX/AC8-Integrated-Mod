"""Passive AC8 identification. No game access, no control/config writes.
Fits independent first-order rate responses and evaluates held-out input replay.
This is not an AC8 flight-model reconstruction or an MPC controller.
"""
from pathlib import Path
import argparse,csv,json,math,re
import numpy as np

AXES=('pitch','yaw','roll')
class InputSeries(list):
    def __init__(self,records):
        super().__init__(records)
        self.times=np.array([r['t']for r in self])
        self.values=np.array([r['u']for r in self])
def basis(p,y,r):
    p,y,r=np.deg2rad([p,y,r]);f=np.array([math.cos(p)*math.cos(y),math.cos(p)*math.sin(y),math.sin(p)])
    right=np.array([-math.sin(y),math.cos(y),0.]);up=np.cross(f,right)
    return np.column_stack([f,right*math.cos(r)-up*math.sin(r),up*math.cos(r)+right*math.sin(r)])
def load(path):
    states=[];inputs=[];bad=0
    with open(path,encoding='utf-8-sig',newline='')as f:
      for raw in csv.DictReader(f):
        try:
          row={k:(v if k in ('kind','pawn')else float(v))for k,v in raw.items()}
          if not all(math.isfinite(v)for v in row.values()if isinstance(v,float)):raise ValueError('nonfinite')
          row['t']=row['t_us']/1e6
          row['u']=np.array([row['u_'+a]for a in AXES])
          if max(abs(row['u']))>1.2:raise ValueError('input range')
          if row['kind']=='I':inputs.append(row)
          elif row['kind']=='S':
            if not 0<=row['input_age_ms']<=100 or not 0<row['frame_dt']<=.1:continue
            row['R']=basis(row['pitch_deg'],row['yaw_deg'],row['roll_deg'])
            row['speed']=np.linalg.norm([row['vx_cm_s'],row['vy_cm_s'],row['vz_cm_s']])/100
            states.append(row)
        except (ValueError,TypeError,KeyError):bad+=1
    return states,inputs,bad
def input_at(events,t,axis):
    if isinstance(events,InputSeries):ts=events.times;us=events.values[:,axis]
    else:ts=np.array([r['t']for r in events]);us=np.array([r['u'][axis]for r in events])
    ix=np.searchsorted(ts,t,side='right')-1
    return us[np.clip(ix,0,len(us)-1)],ix>=0
def rate_records(states):
    rates=[]
    for a,b in zip(states,states[1:]):
        dt=b['t']-a['t']
        if not .008<=dt<=.12 or a['epoch']!=b['epoch']or a['pawn']!=b['pawn']:continue
        omega=sum((np.cross(a['R'][:,i],b['R'][:,i])for i in range(3)))/(2*dt)*180/math.pi
        w=np.array([-omega@b['R'][:,1],omega@b['R'][:,2],-omega@b['R'][:,0]])
        if max(abs(w))>600:continue
        rates.append(dict(t=(a['t']+b['t'])/2,w=w,epoch=b['epoch'],pawn=b['pawn'],plane=b['plane_id'],speed=b['speed'],brake=b['brake'],R=b['R']))
    return rates
def fit_axis(rates,events,axis,split):
    pairs=[(a,b)for a,b in zip(rates,rates[1:])if a['epoch']==b['epoch']and a['pawn']==b['pawn']and .008<=b['t']-a['t']<=.12]
    if len(pairs)<180:return {'status':'insufficient_data','pairs':len(pairs)}
    t=np.array([(a['t']+b['t'])/2 for a,b in pairs]);dt=np.array([b['t']-a['t']for a,b in pairs])
    w=np.array([a['w'][axis]for a,b in pairs]);nxt=np.array([b['w'][axis]for a,b in pairs])
    train=np.array([b['t']<split-.15 for a,b in pairs]);test=np.array([a['t']>split+.15 for a,b in pairs])
    if sum(train)<100 or sum(test)<50:return {'status':'insufficient_train_test'}
    raw,valid=input_at(events,t,axis)
    if np.std(raw[train&valid])<.08:return {'status':'insufficient_input_variation','input_std':float(np.std(raw[train&valid]))}
    best=None
    for delay in (0,.02,.04,.06,.08,.10,.12):
      u,valid=input_at(events,t-delay,axis);tr=train&valid;te=test&valid
      for tau in np.geomspace(.04,1.2,24):
        a=np.exp(-dt/tau);X=np.column_stack([(1-a)*u,1-a]);y=nxt-a*w
        params=np.linalg.lstsq(X[tr],y[tr],rcond=None)[0];gain,bias=params
        if not 1<=abs(gain)<=600 or abs(bias)>30:continue
        prediction=a*w+X@params;loss=np.mean((prediction[tr]-nxt[tr])**2)
        if best is None or loss<best['loss']:
          best=dict(loss=float(loss),tau_s=float(tau),delay_s=delay,gain_deg_s=float(gain),bias_deg_s=float(bias),train_pairs=int(sum(tr)),test_pairs=int(sum(te)),test_rmse_deg_s=float(np.sqrt(np.mean((prediction[te]-nxt[te])**2))),persistence_rmse_deg_s=float(np.sqrt(np.mean((w[te]-nxt[te])**2))))
    if best is None:return {'status':'no_plausible_model'}
    best['status']='fitted_shadow_only';best['gain_sign_expected']=best['gain_deg_s']>0
    best['better_than_rate_holdout_baseline']=best['test_rmse_deg_s']<best['persistence_rmse_deg_s']
    return best
def evaluate_horizons(rates,events,models,split):
    """Known-future-input replay AND causal held-input predictions, separately."""
    out={};tt=np.array([r['t']for r in rates]);step=max(1,len(rates)//350)
    for horizon in (.1,.2,.3):
      errors=[];held_errors=[];persistence=[];count=0
      for i in range(0,len(rates),step):
        origin=rates[i]
        if origin['t']<=split+.15:continue
        j=int(np.searchsorted(tt,origin['t']+horizon))
        if j>=len(rates)or tt[j]-tt[i]>horizon+.05:continue
        segment=rates[i:j+1]
        if any(r['epoch']!=origin['epoch']or r['pawn']!=origin['pawn']for r in segment):continue
        if any(b['t']-a['t']>.12 for a,b in zip(segment,segment[1:])):continue
        wp=origin['w'].copy();wh=wp.copy();good=True
        for axis,name in enumerate(AXES):
          model=models[name]
          if model.get('status')!='fitted_shadow_only':good=False;break
          mid=np.array([(a['t']+b['t'])/2-model['delay_s']for a,b in zip(segment,segment[1:])])
          u,valid=input_at(events,mid,axis)
          initial,_=input_at(events,np.array([origin['t']]),axis)
          if not all(valid):good=False;break
          for k,(a,b)in enumerate(zip(segment,segment[1:])):
            decay=math.exp(-(b['t']-a['t'])/model['tau_s'])
            wp[axis]=decay*wp[axis]+(1-decay)*(model['gain_deg_s']*u[k]+model['bias_deg_s'])
            # Before the actuator delay expires, use only already-known past inputs.
            held_u=u[k]if mid[k]<=origin['t']else initial[0]
            wh[axis]=decay*wh[axis]+(1-decay)*(model['gain_deg_s']*held_u+model['bias_deg_s'])
        if not good:continue
        errors.append((wp-rates[j]['w'])**2);held_errors.append((wh-rates[j]['w'])**2);persistence.append((origin['w']-rates[j]['w'])**2);count+=1
      if count:out[str(horizon)]=dict(samples=count,known_future_input_rate_rmse_deg_s=np.sqrt(np.mean(errors,axis=0)).tolist(),held_command_rate_rmse_deg_s=np.sqrt(np.mean(held_errors,axis=0)).tolist(),constant_rate_baseline_rmse_deg_s=np.sqrt(np.mean(persistence,axis=0)).tolist())
    return out
def analyze_file(path):
    states,inputs,bad=load(path);rates=rate_records(states);groups={}
    for r in rates:
        key=str(int(r['plane']))if r['plane']>=0 else 'unknown-'+r['pawn']
        groups.setdefault(key,[]).append(r)
    report=dict(file=path.name,valid_states=len(states),input_events=len(inputs),malformed_rows=bad,mode='observer_only',axes=list(AXES),models={})
    for key,rr in groups.items():
        actors={r['pawn']for r in rr}
        # Fit each actor separately: never join identical plane IDs across sessions.
        for actor in actors:
            group=[r for r in rr if r['pawn']==actor];events=InputSeries(sorted([r for r in inputs if r['pawn']==actor],key=lambda x:x['t']))
            label=key+'@'+actor
            if len(group)<180 or len(events)<30:report['models'][label]={'status':'insufficient_data','rate_samples':len(group)};continue
            split=group[int(len(group)*.6)]['t']
            models={name:fit_axis(group,events,axis,split)for axis,name in enumerate(AXES)}
            report['models'][label]=dict(status='shadow_analysis',time_split_s=split,speed_range_m_s=[min(r['speed']for r in group),max(r['speed']for r in group)],axes=models,horizons=evaluate_horizons(group,events,models,split))
    return report
def analyze_session(session):
    files=sorted(session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv'))
    results=[analyze_file(p)for p in files]
    diagnostics='\n'.join(p.read_text(encoding='utf-8',errors='replace')for p in session.glob('runtime-copy/Mods/AC8MouseAim/Logs/MouseAim-*.log'))
    dropped=max([0]+[int(x)for x in re.findall(r'SHADOW_CAPTURE rows=\d+ dropped=(\d+)',diagnostics)])
    report=dict(schema='ac8-shadow-identification/v1',files=results,active_predictive_control=False,limits=['Independent first-order axis model only; no full AC8 aerodynamics','World speed is not measured airspeed','No separate high-G, stall, wind or aircraft-upgrade model yet','Collected axes are inputs to the native processor, not measured control-surface angles','Known-future-input replay is not a causal forecast','Model validation on later time segment; correlated closed-loop data can remain biased','A missing input event due to queue loss can bias reconstruction; check SHADOW_CAPTURE dropped count','No model is applied automatically'])
    report['data_quality']={'logged_dropped_records':dropped,'native_recording_errors':'SHADOW_ERROR' in diagnostics,'control_ready':False}
    (session/'shadow-analysis.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    lines=['AC8 影子模型实验报告','本次没有让预测模型接管飞机。','JSON: shadow-analysis.json','角速度单位：度/秒；三轴顺序：俯仰、偏航、滚转。','训练为时间序列前60%，验证为后40%，边界留出空档。','']
    lines+=[f'原生日志记录的丢弃行数：{dropped}。如大于0，预测评分只能作为探索结果。']
    if not files:lines+=['没有找到实验CSV。可能尚未进入任务、观察器未运行或正常退出前数据尚未归档。']
    for f in results:
        lines += [f"{f['file']}: 状态{f['valid_states']}条，输入{f['input_events']}条，无法解析{f['malformed_rows']}条"]
        for key,m in f['models'].items():
            lines += [f'机型/实例 {key}：{m["status"]}']
            for axis,a in m.get('axes',{}).items():
                if a.get('status')=='fitted_shadow_only':lines += [f"  {axis}: tau={a['tau_s']:.3f}s，延迟={a['delay_s']:.3f}s，增益={a['gain_deg_s']:.2f}deg/s，验证RMSE={a['test_rmse_deg_s']:.2f}，保持角速度基线={a['persistence_rmse_deg_s']:.2f}"]
                else:lines += [f"  {axis}: {a.get('status')}"]
            for h,v in m.get('horizons',{}).items():lines += [f"  {h}s预测，{v['samples']}样本：已知后续输入回放RMSE={v['known_future_input_rate_rmse_deg_s']}；保持当前指令预测RMSE={v['held_command_rate_rmse_deg_s']}"]
    lines+=['','结果仅供研究；模型误差、激励不足、速度变化和输入时序都可能影响拟合。不能把拟合成功等同于可安全接管。']
    (session/'实验结果.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig')
    return report
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--session',type=Path,required=True);args=parser.parse_args()
    if not args.session.is_dir():raise SystemExit('Session folder missing')
    r=analyze_session(args.session);print(f"Shadow analysis: {len(r['files'])} CSV files; report saved in {args.session}")
