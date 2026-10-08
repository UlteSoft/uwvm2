#!/usr/bin/env python3
"""Check actual POWER10 ELFv2 CLI register-ring handler assembly from llc.

The cross CLI builder emits this assembly and the executed object with matching
codegen options and mandatory machine verification. This checks the real runtime
policy instantiations, separately from the forced-ring fixture configuration.
"""
import argparse
import gzip
import json
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('assembly', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
text = gzip.open(a.assembly, 'rt').read() if a.assembly.suffix == '.gz' else a.assembly.read_text()
a.output.mkdir(parents=True, exist_ok=False)
rows, bodies = [], []
for match in re.finditer(r'^(_ZN\S+):', text, re.M):
    symbol = match[1]
    handler = re.search(r'\d+(i32_load16|i32_storeN|uwvmint_memory_copy)I', symbol)
    if not handler or 'uwvm_interpreter_translate_option_tELb1' not in symbol:
        continue
    if handler[1] != 'uwvmint_memory_copy' and 'bounds_check_mmap_full' not in symbol:
        continue
    end = text.find('\n\t.size', match.end())
    if end < 0:
        raise RuntimeError('missing function extent')
    body = text[match.start():end]
    if not re.search(r'^\s*bctr\s*$', body, re.M):
        raise RuntimeError('missing indirect musttail dispatch: ' + symbol)
    calls = len(re.findall(r'^\s*(?:bl|bctrl|bla)\s', body, re.M))
    if handler[1] != 'uwvmint_memory_copy' and calls:
        raise RuntimeError('helper call in direct scalar ring handler: ' + symbol)
    rows.append(dict(handler=handler[1], function=symbol, native_calls=calls, indirect_tail_dispatch=True))
    bodies.append(body)
if {r['handler'] for r in rows} != {'i32_load16', 'i32_storeN', 'uwvmint_memory_copy'}:
    raise RuntimeError('missing required instantiated memory handlers')
(a.output / 'assembly.txt').write_text('\n'.join(bodies))
(a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
print('PASS', len(rows), 'actual POWER10 ring handlers: bctr tail dispatch; scalar memory handlers have no helper calls')
