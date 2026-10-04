"""Create a clean portable package. Never reads a game installation or user saves."""
from pathlib import Path
import argparse,hashlib,json,shutil,zipfile,re

REPO=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def build(runtime_root,native_dll,output):
 version=json.loads((REPO/'version.json').read_text())
 runtime=json.loads((REPO/'runtime-dependencies.json').read_text())
 for entry in runtime['files']:
  p=runtime_root/entry['path']
  if not p.is_file()or sha(p)!=entry['sha256']:raise ValueError(f"Pinned runtime mismatch: {entry['path']}")
 if not native_dll.is_file():raise ValueError('Build native/build.cmd first, or supply --native-dll')
 if output.exists()or Path(str(output)+'.zip').exists():raise ValueError('Output already exists; use a fresh directory')
 shutil.copytree(REPO/'package-template',output)
 for entry in runtime['files']:
  p=output/'payload'/entry['path'];p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(runtime_root/entry['path'],p)
 dll=output/'payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/ac8_mouse_aim_010.dll';shutil.copy2(native_dll,dll)
 for name in ('tools','models','licenses','docs'):
  shutil.copytree(REPO/name,output/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
 for name in ('README.md','LICENSE','THIRD_PARTY_NOTICES.md','CHANGELOG.md'):shutil.copy2(REPO/name,output/name)
 shutil.copytree(REPO/'native',output/'source/MouseAim',ignore=shutil.ignore_patterns('build','test-runtime','__pycache__','*.pyc'))
 payload=output/'payload'
 manifest=[{'Path':str(p.relative_to(payload)),'SHA256':sha(p).upper()}for p in sorted(payload.rglob('*'))if p.is_file()]
 (output/'payload-manifest.json').write_text(json.dumps(manifest,indent=2))
 (output/'package-info.json').write_text(json.dumps(dict(version,native_sha256=sha(dll)),indent=2))
 files=[p for p in sorted(output.rglob('*'))if p.is_file()]
 pinned_runtime={str((output/'payload'/entry['path']).resolve()):entry['sha256']for entry in runtime['files']}
 for p in files:
  if p.suffix.lower()in ('.sav','.csv','.log','.vdf','.dmp','.pdb','.obj','.uasset','.uexp','.usmap','.utoc','.ucas','.pak'):raise ValueError(f'Unexpected artifact: {p.name}')
  raw=p.read_bytes()
  # The hash-pinned upstream runtime contains its publisher's compiler paths.
  # Preserve that binary intact; this exception cannot cover newly built files.
  upstream=pinned_runtime.get(str(p.resolve()))==sha(p)
  if not upstream and re.search(rb'(?i)[a-z]:[/\\]+users[/\\]+[^/\\\r\n]+',raw):raise ValueError(f'Private user-directory path: {p.name}')
 (output/'FILES.sha256').write_text(''.join(f'{sha(p)}  {p.relative_to(output).as_posix()}\n'for p in files))
 archive=Path(str(output)+'.zip')
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,6)as z:
  for p in sorted(output.rglob('*')):
   if p.is_file():z.write(p,(Path(output.name)/p.relative_to(output)).as_posix())
 with zipfile.ZipFile(archive)as z:
  assert z.testzip()is None
  for line in (output/'FILES.sha256').read_text().splitlines():
   digest,name=line.split('  ',1);assert hashlib.sha256(z.read(output.name+'/'+name)).hexdigest()==digest
 Path(str(archive)+'.sha256').write_text(f'{sha(archive)}  {archive.name}\n')
 return {'archive':str(archive),'bytes':archive.stat().st_size,'sha256':sha(archive),'payload_files':len(manifest)}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--runtime-root',type=Path,required=True,help='payload directory from the pinned portable runtime distribution');p.add_argument('--native-dll',type=Path,default=REPO/'native/build/ac8_mouse_aim_010.dll');p.add_argument('--output',type=Path,default=REPO/'dist/AC8-Integrated-v2.3.0-share');a=p.parse_args()
 print(json.dumps(build(a.runtime_root.resolve(),a.native_dll.resolve(),a.output.resolve()),indent=2))
