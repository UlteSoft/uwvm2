#!/usr/bin/env python3
"""Require exact recursive trap stacks after authenticated full-JIT cache replay.

This complements the in-memory RuntimeDyld regression: a real CLI must verify
the persistent object's signature, register its CFI again in a fresh process,
and recover repeated activations without generated logical frames in unwind
mode. Instruction tracking is checked independently, with its own cache key.
Requires a provenanced, signed-cache-enabled build and a working native unwind
provider. Unsupported configurations fail this required-capability test rather
than silently counting an instruction fallback or a cache miss as coverage.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import signal
import subprocess

TRAPS = {
    'unreachable': ('unreachable', 'catch unreachable'),
    'oob': ('i32.const -1 i64.load drop', 'memory access out of bounds'),
    'float-to-int': ('f32.const nan i32.trunc_f32_s drop', 'invalid conversion to integer'),
}
POLICIES = ('auto', 'debug', 'legacy-light', 'pb-o1', 'pb-o2', 'pb-o3')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uwvm', type=Path)
    parser.add_argument('--out', type=Path, required=True, help='must be a new directory')
    parser.add_argument('--wat2wasm', default='wat2wasm')
    parser.add_argument('--policies', nargs='+', choices=POLICIES, default=list(POLICIES))
    parser.add_argument('--traps', nargs='+', choices=tuple(TRAPS), default=list(TRAPS))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary = args.uwvm.resolve(strict=True)
    args.out.mkdir(parents=True, exist_ok=False)
    out = args.out.resolve()
    identity = hashlib.sha256(binary.read_bytes()).hexdigest()
    rows = []

    def invoke(command, log):
        with log.open('wb') as stream:
            process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                code = process.wait(timeout=60)
            except subprocess.TimeoutExpired:
                # Kill compiler/runtime children too; do not leak a timed-out
                # VM outside the caller's aggregate CPU/memory test budget.
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = 124
        plain = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', log.read_text(errors='replace'))
        return code, plain

    for trap in args.traps:
        body, diagnostic = TRAPS[trap]
        wat = out/(trap+'.wat')
        wasm = out/(trap+'.wasm')
        # Warm the SAME entry and module with argument 0, then trap with 1.
        # A trapping cold run can terminate before the async cache writer drains;
        # using such a run to seed the cache makes replay tests nondeterministic.
        # CLI start arguments are runtime data, not constants folded into this IR.
        wat.write_text(f'''(module (memory 1)
          (func $recurse (param $n i32)
            local.get $n if
              local.get $n i32.const 1 i32.sub call $recurse return
            end {body})
          (func (export "_start") (param $trap i32)
            local.get $trap if i32.const 7 call $recurse end))\n''')
        subprocess.run([args.wat2wasm, str(wat), '-o', str(wasm)], check=True, timeout=60)
        for policy in args.policies:
            for tracking in ('instruction', 'unwind'):
                label = f'{trap}-{policy}-{tracking}'
                cache = out/(label+'-cache')
                common = [str(binary), '-Raot', '-Rct', '0', '-Rclog', 'out', '-Rllvm-full-policy', policy,
                          '-Rllvm-call-stack', tracking, '-Rllvm-cache-path', 'path', str(cache)]
                for phase, argument in (('warm', '0'), ('replay1', '1'), ('replay2', '1')):
                    command = common + ['-Wstart', '1', argument, '--run', str(wasm)]
                    code, text = invoke(command, out/(label+'-'+phase+'.log'))
                    fields = dict(re.findall(r'\b(call_stack|call_stack_frames|unwind_check|unwind_replace_frames)=([^\s,;]+)', text))
                    frames = [int(n) for n in re.findall(r'\bfunc_idx=(\d+)', text)]
                    hit = any('object-cache-hit ' in line and 'signature_verified=1' in line for line in text.splitlines())
                    policy_ok = fields.get('call_stack') == tracking and fields.get('call_stack_frames') == ('emit' if tracking == 'instruction' else 'omit')
                    if tracking == 'unwind':
                        policy_ok &= fields.get('unwind_check') == 'live' and fields.get('unwind_replace_frames') == 'yes'
                    passed = policy_ok and (code == 0 if phase == 'warm' else
                        code not in (0, 124) and diagnostic in text and frames == [0]*8+[1] and hit)
                    row = {'trap': trap, 'policy': policy, 'tracking': tracking, 'phase': phase,
                           'command': command, 'returncode': code, 'frames': frames, 'signed_cache_hit': hit,
                           'fields': fields, 'passed': bool(passed), 'binary_sha256': identity}
                    rows.append(row)
                    (out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
                    if not passed:
                        raise RuntimeError(f'{label}/{phase} failed; see {out}')
    print(f'PASS {len(rows)} CLI runs; {sum(r["phase"] != "warm" for r in rows)} signed-cache recursive trap replays')


if __name__ == '__main__':
    main()
