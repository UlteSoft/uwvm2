#!/usr/bin/env python3
"""Real PTY: skipped numeric memory sites, fresh hit state and demand transitions."""
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path
from run_debug_run_cli import Console

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '0018.debugger'))
from run_wasm_operand_preview_cli import markers

p = argparse.ArgumentParser(description=__doc__)
for name in ('uwvm', 'wasm-tools', 'out'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=False)
wat = a.out.resolve() / 'memory-loop.wat'; wasm = wat.with_suffix('.wasm')
wat.write_text('''(module (memory 1) (func (export "_start") (local i32)
  loop i32.const 0 i32.load nop i32.const 1 i32.add local.set 0
  i32.const 0 local.get 0 i32.store local.get 0 i32.const 100 i32.lt_u
  br_if 0 end local.get 0 nop drop
  i32.const 0 i32.load i32.const 100 i32.ne if unreachable end))''')
subprocess.run([str(a.wasm_tools.resolve()), 'parse', str(wat), '-o', str(wasm)], check=True)
subprocess.run([str(a.wasm_tools.resolve()), 'validate', str(wasm)], check=True)
sites, _ = markers(a.wasm_tools.resolve(), wasm)
assert len(sites[0]) == 2
load_offset = sites[0][0] - 3  # the independently decoded nop follows 28 02 00
os.environ.update(TERM='dumb', NO_COLOR='1')
c = Console([str(a.uwvm.resolve()), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
             '-Rclog', 'err', '--run', str(wasm)])
actions = []
def cmd(text):
    at = len(c.log); os.write(c.fd, text.encode() + b'\n')
    reply = c.wait(lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), at, 60)
    assert b'error:' not in reply, reply[-3000:]
    actions.append({'command': text, 'reply': reply.decode(errors='replace')}); return reply
def values(local, operand, offset):
    answer = cmd('status'); location = re.search(rb'thread (\d+) module=0 function=0 byte-offset=(\d+)', answer)
    assert location and int(location[2]) == offset, answer
    thread = location[1].decode()
    assert f'i32 = {local}'.encode() in cmd(f'locals wasm {thread} 0 0 1')
    assert f'i32 = {operand}'.encode() in cmd(f'operands {thread} 0 0 1')
try:
    c.wait(lambda b: b'(uwvm-debug) ' in b, 0, 120); cmd('startw')
    first = re.search(rb'breakpoint (\d+)', cmd(f'bw 0 0 {load_offset}'))[1].decode()
    last = re.search(rb'breakpoint (\d+)', cmd(f'bw 0 0 {sites[0][1]}'))[1].decode()
    cmd(f'ignore {first} 39'); cmd('c'); values(39, 0, load_offset)
    cmd('nw'); values(39, 39, sites[0][0])
    cmd(f'disable {first}'); cmd('c'); values(100, 100, sites[0][1])
    cmd('c'); assert b'Program exited normally.' in cmd('status')
    cmd('startw'); cmd(f'enable {first}'); cmd(f'ignore {first} 59')
    cmd('c'); values(59, 0, load_offset)
    cmd('nw'); values(59, 59, sites[0][0])
    cmd(f'delete {first}'); cmd(f'disable {last}'); cmd('c')
    assert b'Program exited normally.' in cmd('status')
    # Start the same activation again with complete collection. Break at the
    # final marker, then run its memory check after leaving trace mode. The local and memory
    # values must refresh after both FULL/selective transitions and a loop edge.
    restarted = cmd('startw')
    # Restart explicitly reports remapping after an earlier breakpoint was deleted.
    remap = re.search(f'restart breakpoint {last} -> (\\d+)'.encode(), restarted)
    assert remap, restarted
    last = remap[1].decode()
    cmd('trace wasm on'); cmd(f'enable {last}'); cmd('c')
    values(100, 100, sites[0][1])
    trace = cmd('trace wasm read 0 128')
    page = re.search(rb'wasm-trace page oldest=(\d+) newest=(\d+)', trace)
    assert page and int(page[2])-int(page[1])==511, trace
    cmd('trace wasm off'); cmd(f'disable {last}'); cmd('c')
    assert b'Program exited normally.' in cmd('status')
    c.finish()
    (a.out / 'result.json').write_text(json.dumps({'passed': True, 'actions': actions,
        'sites': sites, 'load_offset': load_offset,
        'binary_sha256': hashlib.sha256(a.uwvm.read_bytes()).hexdigest()}, indent=2))
    print('PASS cold memory loop, before-load inputs, Wasm stepping, fresh state, policies and restart')
finally:
    (a.out / 'console.bin').write_bytes(c.log)
    (a.out / 'actions.json').write_text(json.dumps(actions, indent=2)); c.close()
