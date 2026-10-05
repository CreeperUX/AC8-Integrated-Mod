from pathlib import Path
import subprocess,shutil,json,os,hashlib,uuid
r=Path(__file__).resolve().parents[1];root=r/'native/test-runtime'/('mode switch '+uuid.uuid4().hex);root.mkdir(parents=True,exist_ok=True)
pkg=root/'package';pkg.mkdir(exist_ok=True)
for p in (r/'package-template').glob('*.ps1'):shutil.copy2(p,pkg/p.name)
shutil.copy2(r/'package-template/Start-AC8-From-Steam.cmd',pkg/'Start-AC8-From-Steam.cmd')
game=root/'game';w=game/'Game/Binaries/Win64';w.mkdir(parents=True,exist_ok=True);(w/'AceCombat8.exe').write_bytes(b'fixture only')
(pkg/'game-path.txt').write_text(str(game))
ps=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
def mode(action,ok=True):
 p=subprocess.run([str(ps),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(pkg/'Mod-Mode.ps1'),'-Action',action],capture_output=True)
 assert (p.returncode==0)==ok,(action,p.stdout,p.stderr)
mode('CheckOriginal',False);mode('Disable');assert (pkg/'mod-disabled.flag').exists();mode('CheckOriginal')
(w/'dwmapi.dll').write_bytes(b'unknown fixture');mode('CheckOriginal',False);mode('Disable',False);assert (w/'dwmapi.dll').read_bytes()==b'unknown fixture';(w/'dwmapi.dll').unlink()
# Verify the bridge passes the original command/arguments and clears only the offline anti-cheat-client override.
stub=root/'original.ps1';mark=root/'original.json'
def quote_ps(s):return "'"+str(s).replace("'","''")+"'"
stub.write_text("@{args=@($args);override=$env:EOS_USE_ANTICHEATCLIENTNULL}|ConvertTo-Json|Set-Content -Encoding UTF8 -LiteralPath "+quote_ps(mark),encoding='utf-8-sig')
command='"'+str(pkg/'Start-AC8-From-Steam.cmd')+'" "'+str(ps)+'" -NoProfile -ExecutionPolicy Bypass -File "'+str(stub)+'" "argument with spaces"'
env=os.environ.copy();env['EOS_USE_ANTICHEATCLIENTNULL']='1'
p=subprocess.run('cmd.exe /d /s /c "'+command+'"',env=env,stdin=subprocess.DEVNULL,capture_output=True,timeout=20)
assert p.returncode==0,(p.stdout,p.stderr)
result=json.loads(mark.read_text(encoding='utf-8-sig'));assert result['args']==['argument with spaces'] and not result['override']
mode('Enable');assert not (pkg/'mod-disabled.flag').exists()
# Running-game guard is tested in a local PowerShell scope; no real game launched.
cmd="function Get-Process { param($Name,$ErrorAction) if($Name -eq 'AceCombat8'){[pscustomobject]@{Id=1}} }; & "+quote_ps(pkg/'Mod-Mode.ps1')+" -Action Disable"
p=subprocess.run([str(ps),'-NoProfile','-ExecutionPolicy','Bypass','-Command',cmd],capture_output=True);assert p.returncode!=0 and not (pkg/'mod-disabled.flag').exists()
print('PASS disabled original-command passthrough, arguments, offline environment removal, residue refusal, no unknown deletion, enable and running-game guard')

managed=root/'managed';managed.mkdir();managed_name='AC8-Missiles-MouseAim-v9.0-test-candidate';child=managed/managed_name;shutil.copytree(pkg,child)
(managed/'AC8-Managed-Packages.json').write_text(json.dumps({'activePackage':managed_name,'packages':[{'name':managed_name,'launcherSHA256':hashlib.sha256((child/'Launch-Offline.ps1').read_bytes()).hexdigest().upper()}]}))
p=subprocess.run([str(ps),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(child/'Mod-Mode.ps1'),'-Action','Disable'],capture_output=True)
assert p.returncode==0 and (managed/'mod-disabled.flag').exists() and not (child/'mod-disabled.flag').exists(),(p.stdout,p.stderr)
mark.unlink()
command='"'+str(child/'Start-AC8-From-Steam.cmd')+'" "'+str(ps)+'" -NoProfile -ExecutionPolicy Bypass -File "'+str(stub)+'" "argument with spaces"'
p=subprocess.run('cmd.exe /d /s /c "'+command+'"',env=env,stdin=subprocess.DEVNULL,capture_output=True,timeout=20)
assert p.returncode==0 and mark.exists(),(p.stdout,p.stderr)
print('PASS managed scope resolves common disable marker and original-command bridge')
