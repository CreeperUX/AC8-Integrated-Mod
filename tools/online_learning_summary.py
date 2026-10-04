from pathlib import Path
import csv,json,math
import numpy as np
FIELDS=('learn_tau_pitch','learn_tau_roll','learn_weight_pitch','learn_weight_roll','learn_promotions_pitch','learn_promotions_roll')
def summarize(session):
 planes={}
 for p in session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv'):
  with p.open(encoding='utf-8-sig',newline='')as f:
   for row in csv.DictReader(f):
    if row.get('kind')!='S' or row.get('assist_profile')not in ('2','3','4','5','6'):continue
    try:
     values=[float(row[k])for k in FIELDS];plane=int(float(row['plane_id']))
     if all(map(math.isfinite,values)):planes.setdefault(plane,[]).append(values)
    except (ValueError,TypeError,KeyError):continue
 result={}
 for plane,rows in planes.items():
  a=np.array(rows);result[str(plane)]={'samples':len(rows),'effective_tau_pitch_range_s':[float(a[:,0].min()),float(a[:,0].max())],'effective_tau_roll_range_s':[float(a[:,1].min()),float(a[:,1].max())],'learned_weight_pitch_max':float(a[:,2].max()),'learned_weight_roll_max':float(a[:,3].max()),'validated_promotions_pitch':int(a[:,4].max()),'validated_promotions_roll':int(a[:,5].max())}
 report={'mode':'all_aircraft_gain_and_timeconstant_online_calibration','session_only_learning':True,'aircraft':result,'limits':'Promotion means lower prediction error in a later validation block, not proof of better closed-loop flight or a full MPC controller.'}
 (session/'online-learning-summary.json').write_text(json.dumps(report,indent=2))
 lines=['全机型在线学习报告（俯仰/滚转摘要）','分支4/5/6的三轴控制从标准模型起步；学习验证通过后只替换模型参数。learn_weight记录已部署参数混合，候选混合另见candidate_weight。','学习状态只保留在本次游戏进程；三轴主控制、偏航和延迟的完整统计见三轴主控制报告。']
 for plane,v in result.items():lines.append(f"机型{plane}：验证通过次数P/R={v['validated_promotions_pitch']}/{v['validated_promotions_roll']}，最大学习权重P/R={v['learned_weight_pitch_max']:.3f}/{v['learned_weight_roll_max']:.3f}；有效时间常数P={v['effective_tau_pitch_range_s']}，R={v['effective_tau_roll_range_s']}")
 if not result:lines.append('没有在线学习分支的有效状态数据；不能据此判断学习效果。')
 lines.append('以上是模型与权重记录，不代表实际超调已经改善。')
 (session/'在线学习报告.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig')
 return report
