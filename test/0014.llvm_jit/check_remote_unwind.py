#!/usr/bin/env python3
"""Run isolated foreign ELF code/CFI with actual target unwinder and C ABI.

Build fixtures/mcjit_remote_unwind_{emit,dump}.cpp with the chosen LLVM static
library closure first (no llvm-config fallback). Supply execution profiles in
run_simd_cross_codegen.py's JSON format. Run inside an external memory/CPU cap.
This is NOT a full foreign ROS CLI test or a production signal-handler contract.
Every selected profile is attempted; failed/unsupported cases stay failures.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('emitter', 'loader', 'llvm-nm', 'profiles', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--profile', action='append', help='Exact profile name; repeat to select several')
    parser.add_argument('--bridge', choices=('indirect', 'direct'), default='indirect')
    parser.add_argument('--unique-temp-labels', action='store_true')
    parser.add_argument('--both-nan-kinds', action='store_true',
                        help='Repeat conversion-trap unwinding with a quiet NaN as well as a signaling NaN')
    parser.add_argument('--pic-host', action='store_true',
                        help='Compile the fixed-address host executable with PIC function bodies to check callee GP setup')
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    emitter, loader, nm = (path.resolve() for path in (args.emitter, args.loader, args.llvm_nm))
    fixture = Path(__file__).resolve().parent/'fixtures'
    ir = fixture/('mcjit_remote_unwind_indirect.ll' if args.bridge == 'indirect' else 'mcjit_remote_unwind.ll')
    guest_source = fixture/'mcjit_remote_unwind_guest.c'
    profiles = json.loads(args.profiles.read_text())
    selected = set(args.profile or [profile['name'] for profile in profiles])
    assert selected <= {profile['name'] for profile in profiles}, 'Unknown profile'
    assert all('/' not in name and name not in ('.', '..') for name in selected)
    # Faulting probes are intentional and disposable; do not fill a disk with
    # core files. This only changes this driver's descendants, not ROS settings.
    if os.name == 'posix':
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    rows = []
    record = dict(scope=__doc__, bridge=args.bridge, unique_temp_labels=args.unique_temp_labels,
                  both_nan_kinds=args.both_nan_kinds, pic_host=args.pic_host, rows=rows)

    def save():
        (args.out/'results.json').write_text(json.dumps(record, indent=2)+'\n')

    for profile in profiles:
        name = profile['name']
        if name not in selected:
            continue
        directory = args.out/name
        directory.mkdir()
        row = dict(profile=profile, steps=[], passed=False)
        rows.append(row)

        def run(phase, command, host=False):
            with (directory/(phase+'.log')).open('wb') as log:
                try:
                    status = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                            env={**os.environ, **({} if host else profile.get('env', {}))},
                                            timeout=180).returncode
                except (OSError, subprocess.TimeoutExpired) as error:
                    status = type(error).__name__
                    log.write(str(error).encode())
            row['steps'].append(dict(phase=phase, command=command, status=status))
            save()
            print(name, phase, status, flush=True)
            return status == 0

        guest = directory/'guest'
        command = [*profile['cxx'], *profile.get('flags', []), '-x', 'c', '-std=gnu11', '-O2',
                   '-fno-pie', '-no-pie', '-fno-omit-frame-pointer', '-fasynchronous-unwind-tables',
                   # Keep the final executable at fixed addresses for symbol
                   # relocation, but exercise the host callback's PIC ABI too.
                   # In particular, static non-PIC O32/N32 bodies can hide a
                   # loader stub that jumps through $at without setting $t9.
                   *(['-fPIC'] if args.pic_host else []),
                   *(['-DUWVM_REMOTE_INDIRECT_BRIDGE'] if args.bridge == 'indirect' else []),
                   str(guest_source), '-o', str(guest)]
        if not run('guest', command):
            continue
        if not run('symbols', [str(nm), '--defined-only', '--format=posix', str(guest)], host=True):
            continue
        symbols = {fields[0]: int(fields[2], 16) for line in (directory/'symbols.log').read_text().splitlines()
                   if len(fields := line.split()) >= 3}
        abi = next((flag.split('=', 1)[1] for flag in profile.get('llc_flags', []) if flag.startswith('-target-abi=')), '')
        obj = directory/'recursive.o'
        command = [str(emitter), str(ir), profile['triple'], profile['cpu'], profile.get('features', ''), abi, str(obj)]
        if args.unique_temp_labels:
            command.append('unique-temporaries')
        if not run('object', command, host=True):
            continue
        bundle = directory/'bundle'
        bundle.mkdir()
        if not run('relocate', [str(loader), str(obj), hex(symbols['uwvm_host_capture']),
                               hex(symbols['uwvm_remote_state']), str(bundle)], host=True):
            continue
        passed = True
        for mode in range(4):
            passed = run('mode'+str(mode), [*profile.get('run', []), str(guest), str(bundle), str(mode)]) and passed
        if args.both_nan_kinds:
            passed = run('mode3-quiet', [*profile.get('run', []), str(guest), str(bundle), '3', 'quiet-nan']) and passed
        row['passed'] = passed
        save()
    def digest(path):
        with path.open('rb') as stream:
            return hashlib.file_digest(stream, 'sha256').hexdigest()
    record['inputs'] = {str(path): digest(path) for path in
                       (emitter, loader, nm, args.profiles.resolve(), ir, guest_source, Path(__file__).resolve())}
    save()
    raise SystemExit(any(not row['passed'] for row in rows))


if __name__ == '__main__':
    main()
