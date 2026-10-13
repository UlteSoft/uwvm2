#!/usr/bin/env python3
"""Verify cold nonallocating GC accesses at their actual before-opcode stops."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from run_debug_run_cli import Console

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '0018.debugger'))
from run_wasm_operand_preview_cli import markers

p = argparse.ArgumentParser(description=__doc__)
for name in ('uwvm', 'wasm-tools', 'out'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.out.mkdir(parents=True, exist_ok=False)
wat = a.out.resolve() / 'gc-access-loop.wat'
wasm = wat.with_suffix('.wasm')
wat.write_text('''(module
  (type $s (struct (field (mut i32)))) (type $a (array (mut i32)))
  (func (export "_start") (local i32) (local (ref $s)) (local (ref $a))
    nop i32.const 7 struct.new $s local.set 1
    i32.const 0 i32.const 8 array.new $a local.set 2
    loop
      local.get 1 local.get 0 nop struct.set $s 0
      local.get 2 local.get 0 i32.const 7 i32.and local.get 0 nop array.set $a
      local.get 1 nop struct.get $s 0 drop
      local.get 2 local.get 0 i32.const 7 i32.and nop array.get $a drop
      local.get 2 nop array.len drop
      local.get 0 i32.const 1 i32.add local.tee 0
      i32.const 100 i32.lt_u br_if 0
    end local.get 0 nop drop))''')
subprocess.run([str(a.wasm_tools.resolve()), 'parse', str(wat), '-o', str(wasm)], check=True)
subprocess.run([str(a.wasm_tools.resolve()), 'validate', str(wasm)], check=True)
sites, _ = markers(a.wasm_tools.resolve(), wasm)
assert len(sites[0]) == 7, sites
offsets = [value + 1 for value in sites[0][1:6]]
os.environ.update(TERM='dumb', NO_COLOR='1')
c = Console([str(a.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
             '-Rclog', 'err', '-WFE-reference-types', '-WFE-function-references', '-WFE-gc',
             '--run', str(wasm)])
actions = []


def command(text):
    offset = len(c.log)
    os.write(c.fd, text.encode() + b'\n')
    reply = c.wait(lambda data: re.search(rb'(?:^|\n)\(uwvm-debug\) $', data), offset, 60)
    assert b'error:' not in reply, reply[-3000:]
    actions.append({'command': text, 'reply': reply.decode(errors='replace')})
    return reply


def values(iteration, offset, field, initialized=True, access=0):
    reply = command('status')
    position = re.search(rb'thread (\d+) module=0 function=0 byte-offset=(\d+)', reply)
    assert position and int(position[2]) == offset, reply
    thread = position[1].decode()
    reply = command(f'locals wasm {thread} 0 0 3')
    assert f'local 0 i32 = {iteration}'.encode() in reply, reply
    if initialized:
        assert b'local 1 (ref type-index=0 module=0) = struct #' in reply, reply
        assert b'local 2 (ref type-index=1 module=0) = array #' in reply, reply
        reply = command(f'members locals {thread} 0 0 0 1 0 1')
        assert f'  0 i32 = {field} mutable'.encode() in reply, reply
        reply = command(f'members locals {thread} 0 0 0 2 0 1')
        expected = iteration if iteration % 8 == 0 and access >= 2 else (0 if iteration == 0 else ((iteration - 1) // 8) * 8)
        assert f'  0 i32 = {expected} mutable'.encode() in reply, reply
        reply = command(f'operands {thread} 0 0 3')
        assert b'= struct #' in reply or b'= array #' in reply, reply
    else:
        assert reply.count(b'unavailable (local is not initialized)') == 2, reply


try:
    c.wait(lambda data: b'(uwvm-debug) ' in data, 0, 120)
    final = re.search(rb'breakpoint (\d+)', command(f'bw 0 0 {sites[0][-1]}'))[1].decode()
    for ordinal, offset in enumerate(offsets):
        command('startw')
        values(0, sites[0][0], None, False)
        bp = re.search(rb'breakpoint (\d+)', command(f'bw 0 0 {offset}'))[1].decode()
        command(f'ignore {bp} 39')
        command('c')
        values(39, offset, 38 if ordinal == 0 else 39, access=ordinal)
        command(f'condition {bp} 0')
        reply = command('c')
        assert b'breakpoint-condition' in reply and b'unavailable' in reply, reply
        values(40, offset, 39 if ordinal == 0 else 40, access=ordinal)
        command(f'condition {bp}')
        command(f'disable {bp}')
        command('c')
        # At the final numeric nop, no reference operand remains. Locals and
        # heap members still show the last actual writes after all cold accesses.
        reply = command('status')
        thread = re.search(rb'thread (\d+) module=0 function=0 byte-offset=(\d+)', reply)
        assert thread and int(thread[2]) == sites[0][-1], reply
        for local, expected in [(1, 99), (2, 96)]:
            reply = command(f'members locals {thread[1].decode()} 0 0 0 {local} 0 1')
            assert f'  0 i32 = {expected} mutable'.encode() in reply, reply
        command(f'delete {bp}')
        command('c')
    command(f'delete {final}')
    c.finish()
    (a.out / 'result.json').write_text(json.dumps({
        'passed': True, 'actual_gc_opcode_offsets': offsets, 'actions': actions,
        'binary_sha256': hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),
        'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest()}, indent=2))
    print('PASS exact struct/array access stops, roots, unset locals, current values, ignore/condition/disable/delete and restart')
finally:
    (a.out / 'console.bin').write_bytes(c.log)
    (a.out / 'actions.json').write_text(json.dumps(actions, indent=2))
    c.close()
