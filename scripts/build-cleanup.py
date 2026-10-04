"""Package only the standalone cleaner; never reads game files or user data."""
from pathlib import Path
import argparse
import hashlib
import shutil
import zipfile

REPO = Path(__file__).resolve().parents[1]
VERSION = '1.0.1'

def build(output):
    archive = Path(str(output) + '.zip')
    if output.exists() or archive.exists():
        raise ValueError('Use a fresh output directory')
    output.mkdir(parents=True)
    for name in ('PowerShell-Compat.ps1', 'Cleanup-Core.ps1', 'Recover-Cleanup.ps1', 'Recover-Cleanup.cmd'):
        shutil.copy2(REPO / 'package-template' / name, output / name)
    (output / 'Recover-Cleanup.cmd').write_bytes(b'@echo off\r\n"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0Recover-Cleanup.ps1" -Interactive\r\nset "AC8_RESULT=%errorlevel%"\r\npause\r\nexit /b %AC8_RESULT%\r\n')
    shutil.copy2(REPO / 'docs/CLEANUP.md', output / 'README.md')
    shutil.copy2(REPO / 'LICENSE', output / 'LICENSE')
    files = sorted(output.iterdir())
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    for p in files:
        if p.suffix == '.ps1' and p.read_bytes().startswith(b'\xef\xbb\xbf\xef\xbb\xbf'):
            raise ValueError('Duplicate BOM')
    (output / 'FILES.sha256').write_text(''.join(f'{digest(p)}  {p.name}\n' for p in files))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(output.iterdir()):
            z.write(p, output.name + '/' + p.name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for p in files:
            assert hashlib.sha256(z.read(output.name + '/' + p.name)).hexdigest() == digest(p)
    Path(str(archive) + '.sha256').write_text(f'{digest(archive)}  {archive.name}\n')
    print(f'{archive.name}: {archive.stat().st_size} bytes; SHA256 {digest(archive)}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=REPO / f'dist/AC8-Cleanup-v{VERSION}')
    build(parser.parse_args().output.resolve())
