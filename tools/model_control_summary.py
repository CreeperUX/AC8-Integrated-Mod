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
   result[str(plane)]['selected_mode_seconds']={str(mode):float(sum(weights[arr('selected_control_mode')==mode]))for mode in (0,1,2,3)}
   transition=(arr('control_mode_blend')>0)&(arr('control_mode_blend')<1)
   if all('capture_weight' in r for r in rows):transition|=(arr('capture_weight')>0)&(arr('capture_weight')<1)
   result[str(plane)]['mode_transition_seconds']=float(sum(weights[transition]))
  if all('level_phase' in r and 'capture_target_bank_deg' in r for r in rows):
   selected=arr('selected_control_mode');phase=arr('level_phase')
   valid=(arr('manual_mask')==0)&(arr('model_source_roll')>=0)&(arr('primary_weight_roll')>=.99)&(arr('input_age_ms')>=0)&(arr('input_age_ms')<=100)
   diagnostics={}
   for mode in (0,1,2,3):
    mask=valid&(selected==mode)
    if mode==2:mask&=(arr('capture_weight')>=.999)
    level=mask&(phase==3)
    diagnostics[str(mode)]={
     'valid_seconds':float(sum(weights[mask])),
     'phase_seconds':{str(p):float(sum(weights[mask&(phase==p)]))for p in (0,1,2,3,4)},
     'leveling_seconds':float(sum(weights[level])),
     'leveling_horizontal_target_seconds':float(sum(weights[level&(abs(arr('capture_target_bank_deg'))<.01)])),
     'leveling_high_bank_seconds':float(sum(weights[level&(abs(arr('actual_bank_deg'))>20)])),
     'leveling_guidance_ratecap_range_deg_s':[float(arr('capture_roll_ratecap_deg_s')[level].min()),float(arr('capture_roll_ratecap_deg_s')[level].max())]if level.any()else None,
     'leveling_model_ratecap_range_deg_s':[float(arr('primary_ratecap_roll')[level].min()),float(arr('primary_ratecap_roll')[level].max())]if level.any()else None,
     'limit_cause_seconds':{str(c):float(sum(weights[mask&(arr('roll_limit_cause')==c)]))for c in (0,1,2,3)}
    }
   result[str(plane)]['roll_control_diagnostics']=diagnostics
  if all('capture_gain_pitch' in r for r in rows):
   # These are the independent CAPTURE-v4 models, not the shared CLASSIC/AGILE model fields.
   applied=(arr('selected_control_mode')==2)&(arr('capture_weight')>=.999)&(arr('manual_mask')==0)&(arr('input_age_ms')>=0)&(arr('input_age_ms')<=100)&(arr('primary_weight_pitch')>=.999)
   adaptive={}
   for axis in AXES:
    valid=applied&(arr('capture_gain_'+axis)>0)
    subset=lambda key:[float(arr(key)[valid].min()),float(arr(key)[valid].max())]if valid.any()else None
    adaptive[axis]={
     'applied_seconds':float(sum(weights[valid])),
     'validated_mix_range':subset('capture_mix_'+axis),
     'gain_deg_s_per_input':subset('capture_gain_'+axis),
     'tau_s':subset('capture_tau_'+axis),
     'delay_s':subset('capture_delay_'+axis),
     'total_bias_deg_s':subset('capture_total_bias_'+axis),
     'rate_rmse_deg_s':subset('capture_rate_rmse_'+axis),
     'accepted_models_max':int(arr('capture_deployments_'+axis).max()),
     'finite_source_checked_seconds':float(sum(weights[valid&(arr('capture_finite_source_checked_'+axis)>0)])),
     'predicted_crossing_range_deg':subset('capture_finite_crossing_'+axis),
     'inherited_crossing_range_deg':subset('capture_inherited_crossing_'+axis)
    }
   result[str(plane)]['capture_v4_adaptive_models']=adaptive
  if all('vector_plan_valid' in r for r in rows):
   active=(arr('selected_control_mode')==3)&(arr('vector_weight')>=.999)&(arr('vector_plan_valid')==1)&(arr('manual_mask')==0)&(arr('input_age_ms')>=0)&(arr('input_age_ms')<=100)
   model={}
   for axis in AXES:
    mask=active&(arr('vector_gain_'+axis)>0)
    span=lambda key:[float(arr(key)[mask].min()),float(arr(key)[mask].max())]if mask.any()else None
    model[axis]={
     'controlled_seconds':float(sum(weights[mask])),
     'gain':span('vector_gain_'+axis),'tau_s':span('vector_tau_'+axis),'delay_s':span('vector_delay_'+axis),'total_bias_deg_s':span('vector_total_bias_'+axis),
     'model_mix':span('vector_model_mix_'+axis),'prediction_rmse_deg_s':span('vector_prediction_rmse_'+axis),
     'fit_samples_max':int(arr('vector_fit_samples_'+axis).max()),'short_validation_max':int(arr('vector_short_validation_'+axis).max()),'long_validation_max':int(arr('vector_long_validation_'+axis).max()),
     'proposals_max':int(arr('vector_proposals_'+axis).max()),'accepted_max':int(arr('vector_accepted_'+axis).max()),'rejected_max':int(arr('vector_rejected_'+axis).max()),
     'learning_stage_seconds':{str(stage):float(sum(weights[mask&(arr('vector_learning_stage_'+axis)==stage)]))for stage in (0,1,2,3)},
     'stop_priority_seconds':float(sum(weights[mask&(arr('vector_safety_priority_'+axis)>0)]))
    }
   result[str(plane)]['vector_adaptive_models']=model
   values=arr('vector_path_error')[active]
   result[str(plane)]['vector_motion']={'active_seconds':float(sum(weights[active])),'path_error_abs_deg_p50_p90_max':np.percentile(abs(values),[50,90,100]).tolist()if len(values)else None,'target_stop_guard_seconds':float(sum(weights[active&(arr('vector_target_stop_guard')>0)])),'terminal_feedback_seconds':float(sum(weights[active&(arr('vector_terminal_mix')>.01)]))if 'vector_terminal_mix'in rows[0]else None}
  if all('vector_maneuver_mode' in r for r in rows):
   active=(arr('selected_control_mode')==3)&(arr('vector_weight')>=.999)&(arr('vector_plan_valid')==1)&(arr('manual_mask')==0)
   result[str(plane)]['vector_v2_maneuver']={
    'strategy_seconds':{str(mode):float(sum(weights[active&(arr('vector_maneuver_mode')==mode)]))for mode in (1,2)},
    'pull_preparing_seconds':float(sum(weights[active&(arr('vector_pull_preparing')>0)])),
    'large_negative_pursuit_seconds':float(sum(weights[active&(arr('vector_angle')>10)&(arr('vector_desired_rate_pitch')< -2)])),
    'positive_pitch_gain_range':ranges('vector_positive_pitch_gain'),'negative_pitch_gain_range':ranges('vector_negative_pitch_gain'),
    'pull_cost_range':ranges('vector_pull_cost'),'push_cost_range':ranges('vector_push_cost')}
  if any('heritage_revision' in r for r in rows):
   h=lambda key:np.array([r.get(key,0.)for r in rows])
   active=(arr('selected_control_mode')==3)&(h('heritage_revision')>=9)&(h('heritage_weight')>=.999)&(h('heritage_plan_valid')==1)&(arr('manual_mask')==0)&(arr('input_age_ms')>=0)&(arr('input_age_ms')<=100)
   span=lambda key:np.percentile(abs(h(key)[active]),[50,90,100]).tolist()if active.any()else None
   result[str(plane)]['peace_agile_war']={
    'revision_max':float(h('heritage_revision').max()),'active_seconds':float(sum(weights[active])),
    'boost_pitch_seconds':float(sum(weights[active&(h('heritage_boost_pitch')>.01)])),
    'boost_yaw_seconds':float(sum(weights[active&(h('heritage_boost_yaw')>.01)])),
    'roll_uses_peace_seconds':float(sum(weights[active&(h('heritage_roll_peace')==1)])),
    'roll_guarded_seconds':float(sum(weights[active&(h('heritage_roll_guard')<.99)])),
    'cross_guarded_seconds':float(sum(weights[active&(h('heritage_cross_guard')<.99)])),
    'pointing_error_abs_deg_p50_p90_max':span('heritage_angle'),
    'transverse_rate_abs_deg_s_p50_p90_max':span('heritage_transverse_rate'),
    'joint_transport_seconds':float(sum(weights[active&(h('heritage_joint_choice')==2)])),
    'joint_checked_seconds':float(sum(weights[active&(h('heritage_joint_checked')==1)])),
    'fallback_seconds_by_axis':{axis:float(sum(weights[(arr('selected_control_mode')==3)&(h('heritage_revision')>=9)&(arr('manual_mask')==0)&(h('heritage_source_'+axis)==0)&np.array(['heritage_source_'+axis in r for r in rows])])) for axis in ('pitch','roll','yaw')} if any('heritage_source_pitch' in r for r in rows) else {},
    'fallback_reason_counts':{axis:{str(int(reason)):int(sum((h('heritage_reason_'+axis)==reason)&(arr('selected_control_mode')==3)&(h('heritage_revision')>=9))) for reason in np.unique(h('heritage_reason_'+axis)) if reason>0} for axis in ('pitch','roll','yaw')},
    'architecture':'PEACE roll/leveling plus common-frame verified AGILE/transport pitch-yaw; legacy VECTOR inactive'
   }
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
  if 'selected_mode_seconds' in v:lines.append(f"  机型{plane}选择档位时间(0=PEACE,3=WAR；历史1=AGILE,2=CAPTURE)：{v['selected_mode_seconds']}；过渡{v['mode_transition_seconds']:.2f}s")
 for plane,v in result.items():
  if 'roll_control_diagnostics' in v:
   d=v['roll_control_diagnostics']['2'];lines.append(f"  机型{plane} 第三档有效时间{d['valid_seconds']:.2f}s，改平{d['leveling_seconds']:.2f}s，其中水平目标{d['leveling_horizontal_target_seconds']:.2f}s；调度限速{d['leveling_guidance_ratecap_range_deg_s']}，模型限速{d['leveling_model_ratecap_range_deg_s']}")
 for plane,v in result.items():
  for axis,a in v.get('capture_v4_adaptive_models',{}).items():
   lines.append(f"  机型{plane} CAPTURE v4 {labels[axis]}：应用{a['applied_seconds']:.2f}s，独立验收模型最多{a['accepted_models_max']}次；实际增益{a['gain_deg_s_per_input']}，响应{a['tau_s']}s，延迟{a['delay_s']}s，总偏置{a['total_bias_deg_s']}deg/s；已评估有限窗口{a['finite_source_checked_seconds']:.2f}s")
 lines.append('CAPTURE v4 的有限窗口检查是模型预测，不能当作实飞零超调保证；实测穿越仍需从姿态与世界目标计算。')
 for plane,v in result.items():
  if 'peace_agile_war' in v:
   a=v['peace_agile_war'];lines.append(f"  机型{plane} WAR修订{a['revision_max']:.0f}：PEACE/敏捷架构控制{a['active_seconds']:.2f}s；俯仰加速{a['boost_pitch_seconds']:.2f}s、偏航加速{a['boost_yaw_seconds']:.2f}s；滚转使用PEACE {a['roll_uses_peace_seconds']:.2f}s；滚转/横向门控{a['roll_guarded_seconds']:.2f}/{a['cross_guarded_seconds']:.2f}s。")
   lines.append(f"    三轴预测检查{a['joint_checked_seconds']:.2f}s；滚转预补偿{a['joint_transport_seconds']:.2f}s；各轴回退秒数{a['fallback_seconds_by_axis']}；回退原因计数{a['fallback_reason_counts']}。")
   lines.append('    此版本VECTOR分支已退出控制，vector_plan_valid=0不是记录失效；控制模型查看上方主控制字段，额外响应查看heritage_*。')
 for plane,v in result.items():
  for axis,a in v.get('vector_adaptive_models',{}).items():
   if a['controlled_seconds']<=0:continue
   lines.append(f"  机型{plane} VECTOR {labels[axis]}：控制{a['controlled_seconds']:.2f}s，拟合窗口最高{a['fit_samples_max']}，独立短/长验收{a['short_validation_max']}/{a['long_validation_max']}；候选{a['proposals_max']}、通过{a['accepted_max']}、拒绝{a['rejected_max']}；实际增益{a['gain']}，tau{a['tau_s']}s，延迟{a['delay_s']}s，总偏置{a['total_bias_deg_s']}deg/s，混合{a['model_mix']}")
 lines.append('VECTOR 的参数验收、指向轨迹与停标超调需分别检查；预测窗口通过不能当作实测零超调保证。')
 lines.append('非零修正时间不能代表模型覆盖率：即使模型与旧算法指令相同，模型仍可能全权控制。实飞改善需另看超调、指向误差和同条件对照。')
 (session/'三轴主控制报告.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
