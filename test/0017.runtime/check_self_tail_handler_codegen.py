#!/usr/bin/env python3
"""Inspect real x86 interpreter self-tail handlers before/after ring reset."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('baseline', type=Path)
p.add_argument('current', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
subprocess.run(['bash', str(Path(__file__).resolve().parents[2] / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=False)


def handlers(path):
    listing = subprocess.check_output(['objdump', '-dr', '--no-show-raw-insn', str(path)], text=True)
    chunks = re.split(r'(?m)^([0-9a-f]+) <([^>]+)>:\n', listing)
    result = {}
    for i in range(1, len(chunks), 3):
        name, body = chunks[i + 1:i + 3]
        if 'uwvmint_return_call_selfI' not in name or 'Lb1E' not in name or 'Dv16_' not in name:
            continue
        body = body.split('\nDisassembly of section')[0]
        result[name] = body
    return result


old, new = handlers(a.baseline), handlers(a.current)
assert new, 'no vector-ring self-tail handlers found'
rows = []
for i, (name, body) in enumerate(new.items()):
    assert name in old, 'baseline signature mismatch'
    before = old[name]
    save = r'\b(?:v?mov[a-z]*)\s+%xmm\d+,\s*[^\n]*\(%rsp\)'
    before_saves, after_saves = len(re.findall(save, before)), len(re.findall(save, body))
    assert before_saves > 0 and after_saves == 0, (name, before_saves, after_saves)
    assert re.search(r'\bjmp\w*\s+\*', body) and not re.search(r'\bret[qwl]?\b', body), name
    (a.output / f'{i}-before.s').write_text(before)
    (a.output / f'{i}-after.s').write_text(body)
    rows.append(dict(symbol=name, before_vector_stack_saves=before_saves, after_vector_stack_saves=after_saves,
                     musttail_jump=True))
(a.output / 'summary.json').write_text(json.dumps(dict(passed=True,
    inputs={str(path): hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() for path in [a.baseline, a.current]},
    handlers=rows), indent=2) + '\n')
print(f'PASS {len(rows)} actual self-tail handlers: obsolete vector-ring spills removed, musttail retained')
