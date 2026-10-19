#!/usr/bin/env python3
"""Real native stepping counts and complete opcode trace under the default debug JIT."""
import argparse, hashlib, json, os, platform, re, subprocess
from pathlib import Path
from run_debug_run_cli import Console

p = argparse.ArgumentParser(description=__doc__)
for name in ('uwvm', 'wasm-tools', 'out'):
    p.add_argument('--' + name, type=Path, required=True)
p.add_argument('--baseline', type=Path)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=False)
wat = a.out.resolve() / 'integer.wat'
wasm = wat.with_suffix('.wasm')
wat.write_text('(module (global $x (mut i32) (i32.const 17)) '
               '(func (export "_start") (local $a i32) global.get $x local.set $a '
               'local.get $a i32.const 3 i32.add global.set $x nop))')
subprocess.run([str(a.wasm_tools.resolve()), 'parse', str(wat), '-o', str(wasm)], check=True)
os.environ['NO_COLOR'] = '1'
os.environ['TERM'] = 'dumb'
versions = [('after', a.uwvm.resolve())]
if a.baseline:
    versions.insert(0, ('before', a.baseline.resolve()))
results = []
for label, binary in versions:
    c = Console([str(binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
                 '-Rclog', 'err', '--run', str(wasm)])
    actions = []

    def command(text):
        at = len(c.log)
        os.write(c.fd, (text + '\n').encode())
        reply = c.wait(lambda b: re.search(rb'(?:^|\n)\(uwvm-debug\) $', b), at, 30)
        reply = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', reply).replace(b'\r', b'')
        actions.append({'command': text, 'reply': reply.decode(errors='replace')})
        assert b'error:' not in reply, reply
        return reply

    try:
        c.wait(lambda b: b'(uwvm-debug) ' in b, 0, 120)
        reply = command('startw')
        offset = int(re.search(rb'byte-offset=(\d+)', reply)[1])
        transitions, count = [], 0
        for _ in range(512):
            reply = command('ni')
            assert b'native-pc=' in reply, reply
            count += 1
            location = re.search(rb'native-wasm provenance=exact .*byte-offset=(\d+)', reply)
            if not location:
                continue
            current = int(location[1])
            if current != offset:
                transitions.append({'from_wasm_offset': offset, 'to_wasm_offset': current,
                                    'native_next_commands': count})
                offset, count = current, 0
                if len(transitions) == 4:
                    break
        assert len(transitions) == 4
        command('startw')
        command('trace wasm on')
        command('c')
        trace = command('trace wasm read 0 128')
        events = [(int(offset), int(opcode, 16)) for offset, opcode in re.findall(
            rb'^wasm-event .*byte-offset=(\d+) .*before-instruction opcode=0x([0-9a-f]+)', trace, re.M)]
        # Entry was observed before trace was enabled; every remaining opcode,
        # including the constant with no native instruction and end, must appear.
        assert events == [(2, 0x21), (4, 0x20), (6, 0x41), (8, 0x6a),
                          (9, 0x24), (11, 0x01), (12, 0x0b)], events
        policies = re.findall(rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)', c.log)
        bounded_backend = (platform.system(), platform.machine()) in [('Darwin', 'arm64'), ('Linux', 'x86_64')]
        expected = b'less' if label == 'after' and bounded_backend else b'none'
        expected_pipeline = b'debug-local-ssa' if label == 'after' and (platform.system(), platform.machine()) == ('Linux', 'x86_64') else b'none'
        assert policies and all(row == (b'session:debug', expected_pipeline, expected) for row in policies), policies
        c.finish()
        results.append({'version': label, 'passed': True, 'transitions': transitions,
                        'trace': events, 'policy': [v.decode() for v in policies[-1]],
                        'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
        print(label, transitions, flush=True)
    finally:
        (a.out / (label + '.bin')).write_bytes(c.log)
        (a.out / (label + '.json')).write_text(json.dumps(actions, indent=2))
        (a.out / 'result.json').write_text(json.dumps(results, indent=2))
        c.close()
