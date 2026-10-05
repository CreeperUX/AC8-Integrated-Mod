"""Build a fixed English edition of 2.3.6; retain gameplay bytes and the Chinese release."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import shutil
import subprocess
import uuid

REPO = Path(__file__).resolve().parents[1]

def build(baseline, output):
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    info = json.loads((baseline / 'package-info.json').read_text(encoding='utf-8'))
    if info['version'] != '2.3.6':
        raise ValueError('The English supplement must use the released 2.3.6 baseline')
    # Check the complete baseline, not just a user-supplied DLL.
    for line in (baseline / 'FILES.sha256').read_text(encoding='utf-8').splitlines():
        digest, name = line.split('  ', 1)
        path = (baseline / name).resolve()
        if not path.is_relative_to(baseline) or sha(path) != digest:
            raise ValueError(f'Changed baseline file: {name}')
    stage = REPO / 'native/test-runtime' / ('english-build-' + uuid.uuid4().hex)
    stage.mkdir(parents=True)
    subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(REPO / 'scripts/translate-english-template.ps1'), '-Source', str(REPO / 'package-template'), '-Destination', str(stage / 'package-template'), '-CatalogRoot', str(REPO / 'localization')], check=True)
    for name in ('tools', 'models', 'licenses', 'docs', 'native'):
        shutil.copytree(REPO / name, stage / name, ignore=shutil.ignore_patterns('build', 'test-runtime', '__pycache__', '*.pyc'))
    # This source-only checker is not a runtime analysis command.
    (stage / 'tools/check-localization.py').unlink(missing_ok=True)
    for name in ('runtime-dependencies.json', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md'):
        shutil.copy2(REPO / name, stage / name)
    shutil.copy2(REPO / 'README.en.md', stage / 'README.md')
    version = json.loads((REPO / 'version.json').read_text(encoding='utf-8'))
    version.update(version='2.3.6', status='release', language='en-US', edition='English', language_switching=False)
    (stage / 'version.json').write_text(json.dumps(version, indent=2) + '\n', encoding='utf-8')
    (stage / 'package-template/START-HERE.md').write_text('''# AC8 Integrated 2.3.6 — English edition

**Offline single-player only. Do not use online or in multiplayer.**

1. Extract this complete package outside the game directory.
2. Exit the game and let the previous launcher finish cleanup.
3. Run **Start-GUI.cmd**. Wait for automatic Steam/game detection or browse manually.
4. Choose guidance only, full missile enhancements, or flight control only. All include cameras and HMD. The default is full enhancements.
5. Save settings, copy the launch-options line, and paste it into Steam > AC8 > Properties > General > Launch Options.
6. Launch through Steam; use Expert controls and a third-person view. Keep the console open until cleanup finishes after exit.

F2 toggles HMD (off at startup); F3 changes camera position; F4 switches flight policy; hold C for free look. Release Shift/Ctrl/Alt before F2/F3/F4. See docs/en/KEYBINDINGS.md for the author's updated bindings.

This package is English-only, with no in-app language switch. The separate Chinese package remains available. A shared language switch is planned for 2.3.7. Windows system dialogs and OS-originated error details may follow your Windows language.

Use Back up and clean up only after confirming loader ownership. To stop using the mod, also remove its Steam launch option. Share the original ZIP, not sessions or save backups. Normal play needs neither Python nor NumPy.

See README.md and docs/en/INSTALL.md for full instructions and limitations.
''', encoding='utf-8')
    (stage / 'package-template/FEEDBACK-TEMPLATE.txt').write_text('''AC8 Integrated 2.3.6 English edition feedback
Windows version / display scaling:
Game build / aircraft / mission:
Installation mode:
F2 / F3 / F4 state:
Detection / manual path selection:
Save / Steam launch / normal exit / cleanup:
English text or layout issue:
Steps to reproduce:
Expected / actual result:
Error text or screenshot:
Do not attach complete sessions, save backups or account data.
''', encoding='utf-8')
    # Pin all 28 gameplay files to the existing release (also avoids newline drift).
    payload = stage / 'package-template/payload'
    baseline_payload = baseline / 'payload'
    expected = {p.relative_to(baseline_payload) for p in baseline_payload.rglob('*') if p.is_file()}
    unexpected = {p.relative_to(payload) for p in payload.rglob('*') if p.is_file()} - expected
    if unexpected:
        raise ValueError(f'Unexpected new gameplay files: {unexpected}')
    for relative in expected:
        target = payload / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(baseline_payload / relative, target)
    loader = importlib.util.spec_from_file_location('package_builder', REPO / 'scripts/build-package.py')
    module = importlib.util.module_from_spec(loader)
    loader.loader.exec_module(module)
    module.REPO = stage
    native = baseline_payload / 'Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/ac8_mouse_aim_010.dll'
    result = module.build(baseline_payload, native, output)
    for relative in expected:
        assert sha(output / 'payload' / relative) == sha(baseline_payload / relative), relative
    result['language'] = 'en-US'
    result['gameplay_files_identical'] = len(expected)
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.baseline.resolve(), args.output.resolve()), indent=2))
