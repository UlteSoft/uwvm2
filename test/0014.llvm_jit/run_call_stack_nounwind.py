#!/usr/bin/env python3
"""Verify production logical-trace helper contracts and LLVM O3 cleanup removal.

The probe only emits IR. Its host address stubs never execute guest code.
Compilation and this runner must be performed inside the prescribed cgroup.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--llvm', type=Path, default=Path('/toolchain/bin'))
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    subprocess.run(['bash', str(Path(__file__).resolve().parents[2] / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    a.out.mkdir(parents=True, exist_ok=False)
    commands = []

    def run(command, output):
        result = subprocess.run(list(map(str, command)), capture_output=True, timeout=120)
        output.write_bytes(result.stdout)
        output.with_suffix(output.suffix + '.stderr').write_bytes(result.stderr)
        commands.append({'command': list(map(str, command)), 'exit': result.returncode})
        (a.out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        result.check_returncode()

    run([a.probe], a.out / 'before.ll')
    run([a.llvm / 'opt', '-passes=default<O3>', '-S', a.out / 'before.ll', '-o', a.out / 'after.ll'], a.out / 'opt.log')
    ir = (a.out / 'after.ll').read_text()

    def body(name):
        matches = re.findall(r'(?ms)^define [^\n]*@' + re.escape(name) + r'\([^\n]*\{\n.*?^}', ir)
        if len(matches) != 1:
            raise RuntimeError(f'expected exactly one definition of {name}')
        return matches[0]

    normal, guest = body('normal_caller'), body('guest_caller')
    if 'invoke ' in normal or 'landingpad ' in normal:
        raise RuntimeError('LLVM did not eliminate the nonthrowing normal-call cleanup')
    if not all(word in guest for word in ['invoke ', '@actual_guest_call(', 'landingpad ', 'resume ']):
        raise RuntimeError('actual guest-call exceptional edge/cleanup disappeared')
    result = {'passed': True, 'probe_sha256': hashlib.sha256(a.probe.read_bytes()).hexdigest(),
              'normal_invoke_removed': True, 'normal_cleanup_removed': True,
              'guest_invoke_preserved': True, 'guest_cleanup_preserved': True,
              'scope': 'synthetic IR with production trace emitters; no generated guest execution'}
    (a.out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS production helper nounwind; O3 normal cleanup removed; guest unwind preserved')


if __name__ == '__main__':
    main()
