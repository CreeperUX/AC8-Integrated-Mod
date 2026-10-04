from pathlib import Path
import csv,json,math
def summarize(session):
    n=enabled=corrected=model_active=0;pitch_max=roll_max=0.;psum=rsum=0.
    for p in session.glob('runtime-copy/Mods/AC8MouseAim/Logs/Shadow-*.csv'):
        with p.open(encoding='utf-8-sig',newline='')as f:
            for r in csv.DictReader(f):
                if r.get('kind')!='S':continue
                try:on=float(r['assist_on']);dp=float(r['assist_delta_pitch']);dr=float(r['assist_delta_roll'])
                except (KeyError,ValueError,TypeError):continue
                if not all(map(math.isfinite,[on,dp,dr])):continue
                n+=1;enabled+=on>0;corrected+=abs(dp)>1e-5 or abs(dr)>1e-5
                try:model_active+=any(float(r.get('primary_weight_'+axis,0))>0 and float(r.get('model_source_'+axis,-1))>=0 for axis in ('pitch','roll','yaw'))
                except (ValueError,TypeError):pass
                pitch_max=max(pitch_max,abs(dp));roll_max=max(roll_max,abs(dr));psum+=dp*dp;rsum+=dr*dr
    report=dict(mode='bounded_shared_model_braking',configured_to_actuate=True,full_MPC=False,new_hotkey=False,state_samples=n,enabled_samples=enabled,nonzero_correction_samples=corrected,max_pitch_correction=pitch_max,max_roll_correction=roll_max,rms_pitch_correction=math.sqrt(psum/n)if n else 0,rms_roll_correction=math.sqrt(rsum/n)if n else 0,note='Corrections are commanded increments; actual native processor inputs are recorded separately. Does not prove reduced overshoot.')
    (session/'shared-assist-summary.json').write_text(json.dumps(report,indent=2))
    for name in ['shadow-analysis.json','f15e-frozen-validation.json','cross-aircraft-analysis.json']:
        p=session/name
        if p.exists():
            d=json.loads(p.read_text(encoding='utf-8'));d.update(actuation=True if model_active or corrected else None,active_predictive_control=True if model_active else None,observed_model_samples=model_active,actuation_mode='model_control' if model_active else 'not_confirmed_by_recording');p.write_text(json.dumps(d,indent=2,ensure_ascii=False),encoding='utf-8')
    for name in ['实验结果.txt','固定模型验证.txt','跨机型分析.txt']:
        p=session/name
        if p.exists() and (model_active or corrected):
            s=p.read_text(encoding='utf-8-sig').replace('本次没有让预测模型接管飞机。','本次启用了有限模型制动辅助，完整MPC未接管。').replace('仍为影子模式，不控制飞机。','本次启用了有限模型制动辅助；本报告中的重新拟合模型未用于操纵。').replace('跨机型通用能力分析（影子模式，不控制飞机）','跨机型通用能力分析（本次实际启用了有限制动辅助）')
            p.write_text(s,encoding='utf-8-sig')
    lines=['通用模型制动辅助测试','配置允许模型控制；实际介入以状态记录为准。',f'观察到模型介入的样本：{model_active}',f'状态样本：{n}，其中非零辅助指令：{corrected}',f'最大额外指令：俯仰{pitch_max:.4f}、滚转{roll_max:.4f}（满舵量=1）','这些记录只能确认是否介入，是否改善超调仍需结合飞行与对照。']
    (session/'辅助介入报告.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig');return report
