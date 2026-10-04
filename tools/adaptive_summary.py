from pathlib import Path
import csv,json
import numpy as np
def summarize(session):
    planes={}
    for p in session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv'):
        with p.open(encoding='utf-8-sig',newline='')as f:
            for r in csv.DictReader(f):
                if r.get('kind')!='S':continue
                try:
                    plane=int(float(r['plane_id']));v=[float(r[k])for k in ['assist_profile','confidence_pitch','confidence_roll','gain_pitch','gain_roll','assist_delta_pitch','assist_delta_roll']]
                    if all(np.isfinite(v)):planes.setdefault(plane,[]).append(v)
                except (KeyError,ValueError,TypeError):continue
    result={}
    for plane,rows in planes.items():
        v=np.asarray(rows);result[str(plane)]={'samples':len(v),'profile_counts':{str(int(k)):int(sum(v[:,0]==k))for k in np.unique(v[:,0])},'confidence_pitch_p10_p50_p90':np.percentile(v[:,1],[10,50,90]).tolist(),'confidence_roll_p10_p50_p90':np.percentile(v[:,2],[10,50,90]).tolist(),'gain_pitch_range':[float(v[:,3].min()),float(v[:,3].max())],'gain_roll_range':[float(v[:,4].min()),float(v[:,4].max())],'max_abs_correction':np.max(abs(v[:,5:]),axis=0).tolist()}
    report={'mode':'per-aircraft online learning; profile2 includes Typhoon','full_MPC':False,'profiles':{'0':'Typhoon original1.5.0 branch','1':'legacy adaptive branch','2':'global online learning','3':'three-axis learned primary model control','4':'full three-axis standard/learned model control','5':'agility scheduled three-axis model control','6':'switchable classic/agile model control'},'aircraft':result}
    (session/'adaptive-assist-summary.json').write_text(json.dumps(report,indent=2))
    lines=['自适应辅助报告','分支4/5/6：三轴全程模型控制；模型来源、主控制覆盖与偏航统计见三轴主控制报告。','记录的权重与增益不代表实飞品质已经达标。']
    for plane,v in result.items():lines.append(f"机型{plane}：{v['samples']}样本，分支{v['profile_counts']}；俯仰置信度中位数{v['confidence_pitch_p10_p50_p90'][1]:.3f}，滚转{v['confidence_roll_p10_p50_p90'][1]:.3f}；增益范围P{v['gain_pitch_range']} R{v['gain_roll_range']}")
    (session/'自适应辅助报告.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
