from pathlib import Path
import argparse
import analyze_shadow
import shadow_model_v2
import shadow_generalization
import assist_summary
import adaptive_summary
import online_learning_summary
import model_control_summary
p=argparse.ArgumentParser();p.add_argument('--session',type=Path,required=True);a=p.parse_args()
analyze_shadow.analyze_session(a.session)
shadow_model_v2.validate_frozen(a.session,Path(__file__).resolve().parents[1]/'models/f15e-shadow-v2.json')
shadow_generalization.analyze(a.session)
assist_summary.summarize(a.session)
adaptive_summary.summarize(a.session)
online_learning_summary.summarize(a.session)
model_control_summary.summarize(a.session)
print('Experiment analysis complete. Read fixed-model validation report in: '+str(a.session))
