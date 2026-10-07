from pathlib import Path
import subprocess,sys,time,json
sys.stdout.reconfigure(encoding='utf-8',errors='replace')
r=Path(__file__).resolve().parent
names=sys.argv[1:]
report=[]
for name in names:
 t=time.time()
 cmd=['cl','/nologo','/std:c++20','/EHsc','/MD','/O2','/Iinclude',name+'.cpp','/Fobuild\\'+name+'.obj','/Febuild\\'+name+'.exe','/link','/DELAYLOAD:UE4SS.dll','delayimp.lib','build/UE4SS.lib','build/buffer.obj','build/hook.obj','build/trampoline.obj','build/hde64.obj']
 compile=subprocess.run(cmd,cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
 log=compile.stdout
 result=compile.returncode
 if not result:
  test=subprocess.run([str(r/'build'/(name+'.exe'))],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace',timeout=180)
  result=test.returncode;log+=test.stdout
 (r/'build'/(name+'.checked.log')).write_text(log,encoding='utf-8')
 report.append({'test':name,'exit':result,'seconds':round(time.time()-t,2)})
 print(name,'PASS' if result==0 else 'FAIL',result,flush=True)
 if result:print(log[-3500:],flush=True)
(r/('build/checked-results-'+names[0]+'.json')).write_text(json.dumps(report,indent=2))
(r/'build/checked-results.json').write_text(json.dumps(report,indent=2))
sys.exit(any(x['exit']!=0 for x in report))
