"""Audit tracked/unignored source without reading ignored runtime artifacts."""
from pathlib import Path
import subprocess,re,json
root=Path(__file__).resolve().parents[1]
names=set(subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=root).decode().split('\0'))-{''}
blocked={'.dll','.exe','.obj','.lib','.pdb','.zip','.sav','.csv','.log','.vdf','.dmp','.usmap','.uasset','.uexp','.ucas','.utoc','.pak'}
for name in sorted(names):
 p=root/name
 assert p.resolve().is_relative_to(root.resolve()),f'Path escapes source: {name}'
 assert p.suffix.lower()not in blocked,f'Generated/private binary in source: {name}'
 raw=p.read_bytes()
 for text in (raw.decode('utf-8',errors='ignore'),raw.decode('utf-16le',errors='ignore')):
  assert not re.search(r'(?i)[a-z]:[/\\]+users[/\\]+[^/\\\r\n]+',text),f'User directory in {name}'
  assert not re.search(r'gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |OPENSSH )?PRIVATE KEY-----',text),f'Credential-like content in {name}'
 if p.suffix=='.py':compile(p.read_text(encoding='utf-8-sig'),name,'exec')
 if p.suffix=='.json':json.loads(p.read_text(encoding='utf-8-sig'))
print(f'PASS {len(names)} tracked/unignored source files; no prohibited runtime artifacts or user-directory/credential patterns.')
