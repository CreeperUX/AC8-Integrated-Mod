"""Validate preparation catalogs and bilingual navigation; does not modify files."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]

def unique_object(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError(f'Duplicate JSON key: {key}')
        obj[key] = value
    return obj

catalogs = {}
for locale in ('zh-CN', 'en-US'):
    data = json.loads((root / 'localization' / f'{locale}.json').read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    assert data['schema_version'] == 1 and data['locale'] == locale
    assert data['integration_status'] == 'preparation-only'
    messages = data['messages']
    assert all(isinstance(value, str) and value.strip() for value in messages.values())
    catalogs[locale] = messages
assert catalogs['zh-CN'].keys() == catalogs['en-US'].keys(), 'Locale key sets differ'
for key, chinese in catalogs['zh-CN'].items():
    english = catalogs['en-US'][key]
    assert sorted(re.findall(r'\{[a-z_]+\}', chinese)) == sorted(re.findall(r'\{[a-z_]+\}', english)), f'Placeholder mismatch: {key}'

pages = ['README.md', 'README.en.md', 'docs/LOCALIZATION.md']
for name in ('INSTALL', 'KEYBINDINGS', 'CLEANUP', 'LIMITATIONS'):
    pages.extend([f'docs/{name}.md', f'docs/en/{name}.md'])
for name in pages:
    page = root / name
    text = page.read_text(encoding='utf-8')
    for target in re.findall(r'\]\(([^)]+)\)', text):
        if '://' in target or target.startswith('#'):
            continue
        path = target.split('#', 1)[0]
        assert (page.parent / path).is_file(), f'Broken local link: {name} -> {target}'
assert 'README.en.md' in (root / 'README.md').read_text(encoding='utf-8')
assert 'README.md' in (root / 'README.en.md').read_text(encoding='utf-8')
print(f'PASS {len(catalogs["en-US"])} paired preparation messages, placeholders, duplicate-key checks and {len(pages)} document link sets.')
