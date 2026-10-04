from pathlib import Path
import csv,json,math
import numpy as np
AXES=('pitch','roll','yaw')

def summarize(session):
 planes={};bad=0
 for path in sorted(session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv')):
  with path.open(encoding='utf-8-sig',newline='')as f:
   for row in csv.DictReader(f):
    if row.get('kind')!='S' or row.get('assist_profile')not in ('3','4','5','6'):continue
    try:
     r={k:(v if k in ('kind','pawn')else float(v))for k,v in row.items()}
     if not all(math.isfinite(v)for v in r.values()if isinstance(v,float)):raise ValueError('nonfinite')
     for axis in AXES:assert 'primary_weight_'+axis in r
     planes.setdefault(int(r['plane_id']),[]).append(r)
    except (ValueError,TypeError,KeyError,AssertionError):bad+=1
 result={}
 for plane,rows in planes.items():
  rows.sort(key=lambda r:r['t_us'])
  weights=np.array([max(0,min(.12,(rows[i+1]['t_us']-r['t_us'])/1e6))if i+1<len(rows)and r['epoch']==rows[i+1]['epoch']and r['pawn']==rows[i+1]['pawn']else 0 for i,r in enumerate(rows)])
  arr=lambda k:np.array([r[k]for r in rows]);ranges=lambda k:[float(arr(k).min()),float(arr(k).max())]
  axes={};total=float(sum(weights))
  for axis in AXES:
   weight=arr('primary_weight_'+axis);active=(weight>.01)&(abs(arr('primary_delta_'+axis))>1e-4)
   s={'promotions':int(arr('learn_promotions_'+axis).max()),'gain_range':ranges('gain_'+axis),'tau_range_s':ranges('learn_tau_'+axis),'delay_range_s':ranges('learn_delay_'+axis),'learned_weight_max':float(arr('learn_weight_'+axis).max()),'primary_weight_max':float(weight.max()),'primary_delta_range':ranges('primary_delta_'+axis),'primary_active_seconds':float(sum(weights[active])),'long_validation_checks_max':int(arr('primary_checks_'+axis).max()),'reference_rate_range_deg_s':ranges('reference_rate_'+axis)}
   if all('model_source_'+axis in r for r in rows):
    source=arr('model_source_'+axis);applied=source>=0
    s.update(model_control_seconds=float(sum(weights[applied&(weight>.01)])),full_model_seconds=float(sum(weights[applied&(weight>=.999)])),deployed_models_max=int(arr('model_deployments_'+axis).max()),candidate_weight_max=float(arr('candidate_weight_'+axis).max()),source_seconds={name:float(sum(weights[source==code]))for code,name in ((-1,'fallback_or_manual'),(0,'standard'),(1,'learned'),(2,'transition'))})
    s['source_fractions']={k:v/total if total else 0 for k,v in s['source_seconds'].items()}
   if all('pursuit_'+axis in r for r in rows):
    pursuit=arr('pursuit_'+axis);on=arr('model_source_'+axis)>=0
    s.update(pursuit_seconds=float(sum(weights[(pursuit>.05)&on])),pursuit_range=ranges('pursuit_'+axis),damping_range=ranges('effective_damping_'+axis),planned_input_limit_range=ranges('plan_input_limit_'+axis),ratecap_range=ranges('primary_ratecap_'+axis))
   axes[axis]=s
  result[str(plane)]={'samples':len(rows),'profiles':sorted(set(int(r['assist_profile'])for r in rows)),'recorded_seconds':total,'axes':axes}
  if all('selected_control_mode' in r for r in rows):
   result[str(plane)]['selected_mode_seconds']={str(mode):float(sum(weights[arr('selected_control_mode')==mode]))for mode in (0,1)}
   result[str(plane)]['mode_transition_seconds']=float(sum(weights[(arr('control_mode_blend')>0)&(arr('control_mode_blend')<1)]))
 report={'mode':'three_axis_model_control_and_separate_parameter_learning','aircraft':result,'malformed_state_rows':bad,'limits':'Command participation is not flight accuracy. For profile4 learn_weight is serving blend, candidate_weight is learner blend. Quality fields monitor deployed model, not candidate validation. Source fractions include transition; -1 combines manual and fallback. Multiple CSV clocks should be inspected separately.'}
 (session/'model-control-summary.json').write_text(json.dumps(report,indent=2))
 labels={'pitch':'俯仰','roll':'滚转','yaw':'偏航'}
 lines=['三轴模型控制报告','分支4/5/6：标准模型直接控制，学习参数通过验证后逐渐替换；分支3为旧版验证后才混合主控制。']
 for plane,v in result.items():
  lines.append(f"机型{plane}：{v['samples']}状态样本，约{v['recorded_seconds']:.1f}秒有效片段")
  for axis,s in v['axes'].items():
   lines.append(f"  {labels[axis]}：候选更新{s['promotions']}次；增益{s['gain_range']}，响应时间{s['tau_range_s']}s，延迟{s['delay_range_s']}s；主权重最高{s['primary_weight_max']:.3f}，与旧算法不同的非零指令约{s['primary_active_seconds']:.1f}s")
   if 'source_seconds'in s:lines.append(f"    模型控制{s['model_control_seconds']:.1f}s，其中全权模型{s['full_model_seconds']:.1f}s；学习参数部署累计最高{s['deployed_models_max']}次；模型来源占比{s['source_fractions']}")
 for plane,v in result.items():
  for axis,a in v['axes'].items():
   if 'pursuit_seconds' in a:lines.append(f"  机型{plane} {labels[axis]}积极追赶约{a['pursuit_seconds']:.1f}s，规划输入限幅{a['planned_input_limit_range']}，阻尼{a['damping_range']}")
 if not result:lines.append('未找到分支3/4/5/6的有效三轴主控制数据。')
 for plane,v in result.items():
  if 'selected_mode_seconds' in v:lines.append(f"  机型{plane}选择档位时间(0=2.0,1=积极)：{v['selected_mode_seconds']}；过渡{v['mode_transition_seconds']:.2f}s")
 lines.append('非零修正时间不能代表模型覆盖率：即使模型与旧算法指令相同，模型仍可能全权控制。实飞改善需另看超调、指向误差和同条件对照。')
 (session/'三轴主控制报告.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
