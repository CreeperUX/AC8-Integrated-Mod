"""Cross-aircraft model audit; never mixes physical time across aircraft or epochs."""
from pathlib import Path
import argparse,json,math,re
import numpy as np
import shadow_model_v2 as m

def dataset(session):
    streams=[];all_events=[];offset=10.;quality=[];aliases={}
    for file in sorted(session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv')):
        states,inputs,bad=m.base.load(file)
        raw_count=len(states);states=m.common_cadence(states)
        quality.append(dict(file=file.name,raw_valid_states=raw_count,states=len(states),inputs=len(inputs),malformed=bad,analysis_cadence_hz=30))
        # A plane change splits a stream even if Unreal reuses the same pawn address.
        chunks=[];current=[];last=None
        for r in states:
            key=(r['pawn'],int(r['plane_id']),r['epoch'])
            if last is not None and (key!=last[0]or r['t']-last[1]>.12):
                if current:chunks.append(current)
                current=[]
            current.append(r);last=(key,r['t'])
        if current:chunks.append(current)
        for chunk in chunks:
            if len(chunk)<4:continue
            first,last=chunk[0]['t'],chunk[-1]['t'];pawn=chunk[0]['pawn'];plane=int(chunk[0]['plane_id'])
            if plane<0:continue
            ident=str(len(streams));shift=offset-first
            transformed=[]
            for r in chunk:
                q=dict(r);q.update(t=r['t']+shift,pawn=ident,epoch=ident);transformed.append(q)
            rr=m.records(transformed)
            if not rr:continue
            # Seed from a validated state snapshot, not an old actor's input history.
            events=[dict(t=offset,u=chunk[0]['u'].copy())]
            events.extend(dict(t=r['t']+shift,u=r['u'].copy())for r in inputs if r['pawn']==pawn and first<r['t']<=last)
            events.sort(key=lambda x:x['t']);all_events.extend(events)
            streams.append(dict(id=ident,plane=plane,original_pawn=pawn,rates=rr,events=m.base.InputSeries(events),start=rr[0]['t'],end=rr[-1]['t'],speed_range=[min(r['speed']for r in rr),max(r['speed']for r in rr)]))
            offset=rr[-1]['t']+2
    # Optional name labels: require acquisition/activation counts to agree and an unambiguous ID.
    native='\n'.join(p.read_text(encoding='utf-8',errors='replace')for p in session.glob('runtime-copy/Mods/AC8MouseAim/Logs/MouseAim-*.log'))
    ue=session/'UE4SS.log';ue=ue if ue.exists()else session/'runtime-copy/UE4SS.log'
    text=ue.read_text(encoding='utf-8',errors='replace')if ue.exists()else''
    addresses=re.findall(r'aircraft acquired 0x([0-9a-fA-F]+)',native)
    names=re.findall(r'\[AC8MouseAim\] Active for (BP_PlayerPlane_\S+)',text)
    if len(addresses)==len(names):
        for address,name in zip(addresses,names):
            ids={s['plane']for s in streams if s['original_pawn'].upper()==address.upper()}
            if len(ids)==1:aliases[str(next(iter(ids)))]=name
    return streams,m.base.InputSeries(sorted(all_events,key=lambda x:x['t'])),quality,aliases,native

def merge_metrics(scores):
    good=[r for r in scores if r['samples']and r.get('rmse_deg_s')is not None]
    n=sum(r['samples']for r in good)
    if not n:return dict(samples=0,rmse_deg_s=None)
    return dict(samples=n,rmse_deg_s=math.sqrt(sum(r['samples']*r['rmse_deg_s']**2 for r in good)/n),rate_hold_rmse_deg_s=math.sqrt(sum(r['samples']*r['rate_hold_rmse_deg_s']**2 for r in good)/n),out_of_training_speed_samples=sum(r['out_of_training_speed_samples']for r in good))

def evaluate(streams,split,model,axis,horizon,common=False):
    return merge_metrics([m.score(s['rates'],s['events'],axis,model,max(split,s['start']),s['end']+.05,horizon,100,model['speed_bounds']if common else None)for s in streams if s['end']>split+.5])

def analyze(session):
    streams,events,quality,aliases,native=dataset(session);by={}
    for s in streams:by.setdefault(s['plane'],[]).append(s)
    report=dict(schema='ac8-cross-aircraft/v1',actuation=False,files=quality,aircraft={},models={},comparisons=[],limits=['Only the tested aircraft/conditions; two aircraft do not establish universal capability','First60% of each aircraft is training; last40% reserved for evaluation','Same linear-speed model family for per-aircraft and pooled fits; pooled training balanced by pair count','A-to-B transfer never uses B training records; pooled fit sees both aircraft training partitions','Overlapping-speed scores are reported separately; controls, parts and regimes may still differ','No parameter is applied to the live controller'])
    dropped=max([0]+[int(x)for x in re.findall(r'SHADOW_CAPTURE rows=\d+ dropped=(\d+)',native)])
    report['data_quality']={'logged_dropped_records':dropped,'native_error':'SHADOW_ERROR' in native}
    work={}
    for plane,ss in by.items():
        rr=[r for s in ss for r in s['rates']];rr.sort(key=lambda r:r['t'])
        item=dict(name=aliases.get(str(plane),'F-15E'if plane==25010 else'机型ID '+str(plane)),samples=len(rr),segments=len(ss),speed_range_m_s=[min(r['speed']for r in rr),max(r['speed']for r in rr)])
        report['aircraft'][str(plane)]=item
        if len(rr)<450:item['status']='insufficient_data';continue
        split=rr[int(len(rr)*.6)]['t'];P=m.make_pairs(rr)
        starts={s['id']:s['start']for s in ss}
        # Exclude the first350ms of every stream; a delayed input must not come from another stream.
        valid=np.zeros(len(P['t']),dtype=bool)
        for s in ss:valid|=(P['start']>s['start']+.35)&(P['end']<=s['end'])
        P={k:v[valid]for k,v in P.items()};train=P['end']<split-.35
        if sum(train)<150 or sum(P['start']>split+.35)<50:
            item.update(status='insufficient_contiguous_train_test',train_pairs=int(sum(train)));continue
        item.update(status='usable',train_pairs=int(sum(train)),split_virtual_s=split)
        work[plane]=dict(streams=ss,rates=rr,P=P,train=train,split=split)
        models={}
        for axis,name in enumerate(m.AXES):
            try:models[name]=m.fit(P,events,axis,split,'linear_speed')
            except (ValueError,np.linalg.LinAlgError)as e:models[name]={'status':'insufficient_model','reason':str(e)}
        report['models'][str(plane)]=models
    if len(work)>=2:
        # Balance training contributions without incorporating either held-out partition.
        n=min(int(sum(x['train']))for x in work.values());pieces=[]
        for data in work.values():
            ix=np.flatnonzero(data['train']);ix=ix[np.linspace(0,len(ix)-1,n,dtype=int)]
            pieces.append({k:v[ix]for k,v in data['P'].items()})
        pooled={k:np.concatenate([x[k]for x in pieces])for k in pieces[0]}
        report['pooled_train_pairs_per_aircraft']=n
        report['models']['shared']={}
        for axis,name in enumerate(m.AXES):
            try:report['models']['shared'][name]=m.fit(pooled,events,axis,max(pooled['end'])+1,'linear_speed')
            except (ValueError,np.linalg.LinAlgError)as e:report['models']['shared'][name]={'status':'insufficient_model','reason':str(e)}
    for source,models in report['models'].items():
        for target,data in work.items():
            result=dict(training=source,evaluation=str(target),kind='pooled_seen_aircraft_holdout'if source=='shared'else('within_aircraft_holdout'if source==str(target)else'unseen_aircraft_transfer'),horizons={})
            for h in(.1,.2,.3):
                result['horizons'][str(h)]={}
                for axis,name in enumerate(m.AXES):
                    model=models[name]
                    if 'gain_coefficients'not in model:result['horizons'][str(h)][name]=dict(status='model_unavailable');continue
                    result['horizons'][str(h)][name]=dict(all_speeds=evaluate(data['streams'],data['split'],model,axis,h),inside_source_training_speed=evaluate(data['streams'],data['split'],model,axis,h,True))
            report['comparisons'].append(result)
    report['status']='cross_aircraft_analysis_available'if len(work)>=2 else'need_two_usable_aircraft'
    (session/'cross-aircraft-analysis.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    lines=['跨机型通用能力分析（影子模式，不控制飞机）',report['status'],'三轴误差单位：度/秒；以下为0.2秒保持当前指令预测。','同机后段验证、未见机型迁移、两机共同标定是不同检验，不混为一谈。','']
    for plane,x in report['aircraft'].items():lines.append(f"{plane} {x['name']}: {x['samples']}个角速度样本，{x['segments']}段，速度范围{x['speed_range_m_s']}m/s，{x['status']}")
    for row in report['comparisons']:
        lines.append(f"\n训练 {row['training']} → 验证 {row['evaluation']} ({row['kind']})")
        for axis,x in row['horizons']['0.2'].items():
            if 'all_speeds'not in x:lines.append(f'  {axis}: 模型数据不足');continue
            a=x['all_speeds'];b=x['inside_source_training_speed'];lines.append(f"  {axis}: 全部速度RMSE={a.get('rmse_deg_s')}，样本={a['samples']}；训练速度范围内RMSE={b.get('rmse_deg_s')}，样本={b['samples']}")
    lines+=['','两机结果只能评价这两架及所飞条件，不能证明所有机型通用。若迁移误差明显大于各自适配误差，应保留机型校准。']
    (session/'跨机型分析.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--session',type=Path,required=True);a=p.parse_args();r=analyze(a.session);print(r['status'])
