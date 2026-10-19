#!/usr/bin/env python3
"""Attribute actual Wasm body bytes to static builtin archive definitions.

Name collisions are retained as ambiguous. This is static size evidence,
not a prediction that linker removal or a smaller debugger build will pass.
"""
import argparse
from collections import defaultdict
import json
import os
from pathlib import Path
import re
import sys

from linux_control import digest, guard, invoke
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from inspect_wasm import Reader, inspect


def functions(path):
    module = inspect(path)
    imported = sum(row['kind'] == 0 for row in module['imports'])
    reader = Reader(Path(path).read_bytes())
    reader.pos = 8
    rows = []
    while reader.pos < len(reader.data):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        if tag == 10:
            for number in range(reader.leb()):
                body_bytes = reader.leb()
                body_end = reader.pos + body_bytes
                locals_count = 0
                for _ in range(reader.leb()):
                    locals_count += reader.leb()
                    reader.value_type()
                index = imported + number
                rows.append({'index': index, 'name': module['functions'].get(index, ''),
                             'body_bytes': body_bytes, 'locals': locals_count,
                             'instruction_bytes': body_end - reader.pos})
                reader.pos = body_end
        reader.pos = end
    return rows


def category(module):
    if module in {'_md5', '_sha1', '_sha2', '_sha3', '_blake2', '_hmac', 'HACL'}:
        return 'optional-hashes'
    if module in {'_decimal', 'mpdecimal'}:
        return 'optional-decimal'
    if module in {'pyexpat', '_elementtree', 'expat'}:
        return 'optional-xml'
    if module.startswith('_codecs_') or module == '_multibytecodec':
        return 'optional-cjk-codecs'
    if module in {'math', 'cmath', '_statistics'}:
        return 'optional-math'
    if module in {'select', '_socket'}:
        return 'optional-network'
    if module == 'core':
        return 'core-or-bootstrap'
    return 'optional-other'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    guard(args.out)
    os.sched_setaffinity(0, {4})
    build_path = args.build / 'build-receipt.json'
    build = json.loads(build_path.read_text())
    assert build['passed']
    wasi = Path(build['wasi_build'])
    artifact = build['artifacts']['semantics-wasi']
    assert digest(artifact['path']) == artifact['sha256']
    config = wasi / 'Modules/config.c'
    builtins = re.findall(r'\{"([^"]+)",\s*([^}]+)\}', config.read_text())
    setup_paths = [wasi / 'Modules/Setup.stdlib', wasi / 'Modules/Setup.bootstrap',
                   Path(build['source']) / 'Modules/Setup.local']
    object_modules = defaultdict(set)
    module_sources = defaultdict(list)
    for path in setup_paths:
        for line in path.read_text().splitlines():
            line = line.split('#', 1)[0].strip()
            if not line or line.startswith('*'):
                continue
            tokens = line.split()
            module = tokens[0]
            # Only generated Setup.stdlib entries are classified optional.
            group = module if path.name == 'Setup.stdlib' else 'core'
            for token in tokens[1:]:
                if token.endswith('.c'):
                    object_modules[Path(token).stem + '.o'].add(group)
                    module_sources[module].append(token)
    nm = Path(build['sdk_clang']['path']).parent / 'llvm-nm'
    link = next(row['argv'] for row in build['commands']
                if row['argv'][-1] == artifact['path'])
    archives = [('cpython', wasi / 'libpython3.14.a')]
    for token in link:
        if token.endswith('.a') and token != str(wasi / 'libpython3.14.a'):
            path = Path(token)
            if not path.is_absolute():
                path = wasi / path
            label = ('mpdecimal' if 'libmpdec' in token else 'HACL' if 'Hacl' in token
                     else 'expat' if 'libexpat' in token else 'other-library')
            archives.append((label, path))
    definitions = defaultdict(set)
    archive_receipts = []
    for number, (label, path) in enumerate(archives):
        folder = args.out / ('nm-' + str(number))
        result = invoke([nm, '-A', '-P', '--defined-only', path], folder)
        assert result['retired'] and result['ownership']['closed']
        archive_receipts.append({'label': label, 'path': str(path), 'sha256': digest(path),
                                 'log': str(folder / 'stdout.log')})
        for line in (folder / 'stdout.log').read_text().splitlines():
            tokens = line.split()
            if len(tokens) < 3 or tokens[2] not in {'T', 't'}:
                continue
            owner = tokens[0].rsplit('.a', 1)[-1].strip('[]:()')
            groups = object_modules.get(Path(owner).name, {'core'}) if label == 'cpython' else {label}
            for group in groups:
                definitions[tokens[1]].add((group, owner))
    rows = functions(artifact['path'])
    totals = defaultdict(lambda: {'functions': 0, 'body_bytes': 0, 'instruction_bytes': 0,
                                  'locals': 0})
    for row in rows:
        matches = definitions.get(row['name'], set())
        modules = sorted({module for module, _ in matches})
        categories = {category(module) for module in modules}
        group = next(iter(categories)) if len(categories) == 1 else ('ambiguous' if matches else 'unattributed')
        row.update(category=group, candidate_modules=modules,
                   candidate_objects=sorted({owner for _, owner in matches}))
        for key in ['body_bytes', 'instruction_bytes', 'locals']:
            totals[group][key] += row[key]
        totals[group]['functions'] += 1
    total_bytes = sum(row['body_bytes'] for row in rows)
    for value in totals.values():
        value['percent_body_bytes'] = 100 * value['body_bytes'] / total_bytes
    report = {'passed': True, 'kind': 'static size attribution, not debugger qualification',
              'build_receipt_sha256': digest(build_path), 'wasm': artifact,
              'total_functions': len(rows), 'total_body_bytes': total_bytes,
              'total_instruction_bytes': sum(row['instruction_bytes'] for row in rows),
              'total_declared_locals': sum(row['locals'] for row in rows),
              'builtin_table': [{'name': name, 'initializer': init.strip()} for name, init in builtins],
              'test_builtins': [name for name, _ in builtins if name.startswith(('_test', 'xx'))],
              'config_sha256': digest(config),
              'setup_files': [{'path': str(path), 'sha256': digest(path)} for path in setup_paths],
              'module_sources': module_sources, 'archive_receipts': archive_receipts,
              'categories': dict(totals),
              'largest_optional': sorted([row for row in rows if row['category'].startswith('optional')],
                                         key=lambda row: row['body_bytes'], reverse=True)[:30],
              'largest_core': sorted([row for row in rows if row['category'] == 'core-or-bootstrap'],
                                     key=lambda row: row['body_bytes'], reverse=True)[:20],
              'limitation': 'Archive/Wasm name matching; ambiguous names and unmapped runtime helpers '
                            'are separate. Categories are static candidate attribution, not measured '
                            'link savings or per-opcode/safepoint counts.'}
    (args.out / 'functions.json').write_text(json.dumps(rows, indent=2) + '\n')
    (args.out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
