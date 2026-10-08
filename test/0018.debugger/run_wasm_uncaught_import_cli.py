#!/usr/bin/env python3
"""Actual imported-tag/cross-Wasm exception inspection; verified Linux cgroup only."""
import argparse
import hashlib
import json
import os
import re
import resource
import signal
import subprocess
import time
from pathlib import Path
import run_wasm_operand_preview_cli as preview
from run_wasm_uncaught_cli import expected_rejection, resume_to_fatal, stopped
from wasm_uncaught_import_cases import imported_uncaught_examples

FEATURES = ('exceptions', 'reference-types', 'function-references', 'tail-call')
CASE_FILE = Path(__file__).with_name('wasm_uncaught_import_cases.py')


def sha(path):
    with Path(path).open('rb') as image:
        return hashlib.file_digest(image, 'sha256').hexdigest()


def request(console, command):
    console.transcript.extend(b'\n>>> ' + command.encode() + b'\n')
    console.child.stdin.write(command.encode() + b'\n'); console.child.stdin.flush()
    return console.prompt()


def status(console):
    reply = console.send('status')
    stop = re.search(rb'^stop-id ([1-9][0-9]*)$', reply, re.M)
    participants = re.findall(rb'^thread ([0-9]+) module=([0-9]+) function=([0-9]+) byte-offset=([0-9]+) generation=([0-9]+)$', reply, re.M)
    assert b'stopped:' in reply and stop and len(participants) == 1, reply
    thread, module, function, offset, epoch = map(int, participants[0])
    return dict(stop_id=int(stop[1]), thread=thread, module=module, function=function,
                offset=offset, runtime_epoch=epoch), reply


def inspect(console, case, bindings, view):
    stop, raw = status(console)
    thread = stop['thread']; count = len(case['frames'])
    reply = console.send(f'frames wasm {thread} {stop["stop_id"]} 0 16')
    head = re.search(rb'^wasm-frames stop=([0-9]+) thread=([0-9]+) selected=0 count=([0-9]+) total=([0-9]+) first=0 physical=0$', reply, re.M)
    assert head and tuple(map(int, head.groups())) == (stop['stop_id'], thread, count, count), reply
    frames = re.findall(rb'^  frame ([0-9]+) kind=(physical|caller) module=([0-9]+) function=([0-9]+) generation=([0-9]+) runtime-epoch=([0-9]+) ', reply, re.M)
    assert len(frames) == count, reply
    copied = []
    for ordinal, (frame, expected) in enumerate(zip(frames, case['frames'])):
        actual_ordinal, kind, module, function, generation, epoch = frame
        module, function, generation, epoch = map(int, (module, function, generation, epoch))
        assert int(actual_ordinal) == ordinal and kind == (b'physical' if ordinal == 0 else b'caller'), reply
        assert function == expected['function'] and generation > 0 and epoch == stop['runtime_epoch'], reply
        role = expected['role']
        if role not in bindings:
            assert module not in bindings.values(), ('distinct role reused a module', bindings, module)
            bindings[role] = module
        assert bindings[role] == module and 0 <= module < len(case['providers']) + 1, reply
        page = dict(ordinal=ordinal, role=role, module=module, function=function,
                    generation=generation, runtime_epoch=epoch)
        for selection, label in (('operands', 'operand'), ('locals', 'local')):
            command = f'operands {thread} {ordinal} 0 64' if selection == 'operands' else f'locals wasm {thread} {ordinal} 0 64'
            packet = console.send(command)
            header = re.search(rb'^Wasm state thread=([0-9]+) module=([0-9]+) epoch=([0-9]+) first=0 total=([0-9]+)$', packet, re.M)
            wanted = expected['values'] if selection == 'operands' else expected['locals']
            assert header and tuple(map(int, header.groups())) == (thread, module, epoch, len(wanted)), packet
            values = re.findall(rb'^' + label.encode() + rb' ([0-9]+) (.*)$', packet, re.M)
            assert [(int(n), v.decode()) for n, v in values] == list(enumerate(wanted)), packet
            page[selection] = [v.decode() for _, v in values]
            if selection == 'operands' and view == 'after-actual-throw':
                assert b'Note: Uncaught Wasm snapshot; stack has not unwound.' in packet, packet
        copied.append(page)
    assert status(console)[0] == stop, 'readonly inspection changed the stop'
    return dict(kind=view, **stop, frames=copied)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'binary', 'wasm-tools', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--jit-policy', choices=('default', 'max'), required=True)
    parser.add_argument('--call-stack-policy', action='append', choices=('instruction', 'unwind'))
    parser.add_argument('--runner-prefix-json', type=Path)
    parser.add_argument('--case', action='append')
    args = parser.parse_args(); args.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    runner = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(v, str) and v for v in runner)
    with args.binary.open('rb') as image:
        header = image.read(20)
    assert header[:7] == b'\x7fELF\x02\x01\x01'
    machine = int.from_bytes(header[18:20], 'little')
    fatal = {62: signal.SIGILL, 243: signal.SIGILL, 183: signal.SIGTRAP}[machine]
    cases = [c for c in imported_uncaught_examples() if args.case is None or c['name'] in args.case]
    assert cases and (args.case is None or {c['name'] for c in cases} == set(args.case))
    policies = args.call_stack_policy or ['instruction', 'unwind']
    inputs = dict(binary_sha256=sha(args.binary), harness_sha256=sha(__file__), cases_sha256=sha(CASE_FILE),
                  runner_prefix=runner, runner_sha256=sha(runner[0]) if runner else None,
                  target_elf_machine=machine, jit_policy=args.jit_policy, call_stack_policies=policies,
                  cgroup=Path('/proc/self/cgroup').read_text())
    (args.out / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    results = []
    for case in cases:
        binaries = {}
        for role, wat in [*case['providers'], ('C', case['consumer'])]:
            path = args.out / (case['name'] + '-' + role + '.wat'); wasm = path.with_suffix('.wasm')
            path.write_text(wat + '\n')
            subprocess.run([str(args.wasm_tools), 'parse', str(path), '-o', str(wasm)], check=True)
            subprocess.run([str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True)
            binaries[role] = wasm
        sites, dump = preview.markers(args.wasm_tools, binaries[case['leaf_role']])
        (args.out / (case['name'] + '-leaf.dump')).write_text(dump)
        function = case['leaf_function']; assert len(sites[function]) == 1
        offset = sites[function][0]
        for policy in policies:
            console = None
            row = dict(case=case['name'], policy=policy, handled=case['handled'], actual_VM=True,
                       actual_uncaught_stop=False, observations=[], role_modules={}, passed=False)
            try:
                preload = ['--wasm-set-main-module-name', 'C']
                for role, _ in case['providers']:
                    preload += ['--wasm-preload-library', str(binaries[role]), role]
                argv = [*runner, str(args.binary), '-Rdbg', '-Rct', '0', '-Rllvm-cache-path', 'disable',
                        '-Rllvm-call-stack', policy, '-Rllvm-policy', args.jit_policy,
                        *['-WFE-' + feature for feature in FEATURES],
                        *preload, '--run', str(binaries['C'])]
                row['argv'] = argv
                console = preview.OperandConsole(argv, args.out / (case['name'] + '-' + policy + '.log'))
                console.prompt(); assert b'prepared; no Wasm instruction executed' in console.send('status')
                breaks = []
                for module in range(len(binaries)):
                    reply = request(console, f'break {module} {function} {offset}')
                    if reply.startswith(b'error:'):
                        assert b'breakpoint target is not a defined LLVM-full Wasm function' in reply, reply
                    else:
                        assert b'registered' in reply, reply
                        breaks.append(module)
                assert len(breaks) == 1, ('fixture leaf must be unique', breaks)
                row['role_modules'][case['leaf_role']] = breaks[0]
                reply = console.send(f'catch wasm uncaught {breaks[0]} all')
                match = re.search(rb'wasm-catchpoint ([0-9]+) after-throw; before-unwind', reply); assert match, reply
                catchpoints = [int(match[1])]
                console.send('trace wasm on uncaught'); console.send('continue'); stopped(console, 60)
                before = inspect(console, case, row['role_modules'], 'before-nop'); row['observations'].append(before)
                assert (before['module'], before['function'], before['offset']) == (breaks[0], function, offset)
                assert b'wasm-event sequence=' not in console.send('trace wasm read')
                console.send('continue')
                if case['handled']:
                    deadline = time.monotonic() + 60
                    while True:
                        reply = console.send('status'); assert b'stopped:' not in reply, reply
                        if b'guest exited: 0' in reply:
                            break
                        assert b'guest exited:' not in reply and time.monotonic() < deadline, reply
                        time.sleep(.01)
                    assert b'wasm-event sequence=' not in console.send('trace wasm read')
                    console.close(); assert console.child.returncode == 0
                    row.update(passed=True, actual_process_exit=0, no_false_uncaught_stop=True)
                    continue
                reply = stopped(console, 60)
                assert b'stopped: Uncaught Wasm exception before unwind; continue propagates' in reply, reply
                after = inspect(console, case, row['role_modules'], 'after-actual-throw'); row['observations'].append(after)
                assert (after['module'], after['function'], after['offset']) == (breaks[0], function, offset + 1)
                assert after['stop_id'] > before['stop_id'] and after['thread'] == before['thread']
                assert re.findall(rb'^uncaught-thread ([0-9]+)$', reply, re.M) == [str(after['thread']).encode()]
                row['actual_uncaught_stop'] = True
                trace = console.send('trace wasm read')
                event = re.findall(rb'^wasm-event sequence=[0-9]+ thread=([0-9]+) module=([0-9]+) function=([0-9]+) byte-offset=([0-9]+) .* unhandled-before-unwind opcode=', trace, re.M)
                assert len(event) == 1 and tuple(map(int, event[0])) == (after['thread'], after['module'], function, offset + 1), trace
                for command in (f'step asm {after["thread"]}', f'step wasm {after["thread"]}',
                                f'info registers {after["thread"]} {after["stop_id"]}', f'disassemble {after["thread"]} {after["stop_id"]} 1'):
                    expected_rejection(console, command)
                    assert status(console)[0] == {k: after[k] for k in before if k not in ('kind', 'frames')}
                for catchpoint in catchpoints:
                    console.send(f'disable wasm-event {catchpoint}')
                exit_code = resume_to_fatal(console, 60)
                transcript = re.sub(rb'\x1b\[[0-9;]*m', b'', bytes(console.transcript))
                tag = re.findall(rb'tag module_id=([0-9]+) module="P" tag_idx=0 type_idx=[0-9]+', transcript)
                assert len(tag) == 1 and b'Uncaught WebAssembly exception' in transcript, transcript[-4000:]
                provider_module = int(tag[0]); assert provider_module < len(binaries)
                if 'P' in row['role_modules']:
                    assert row['role_modules']['P'] == provider_module
                else:
                    assert provider_module not in row['role_modules'].values(); row['role_modules']['P'] = provider_module
                assert exit_code == -fatal and b'guest exited: 0' not in transcript and b'Runtime invariant' not in transcript
                row.update(passed=True, actual_process_exit=exit_code, actual_tag_owner='P',
                           terminal_commands_rejected=True, exact_before_unwind_inputs=True)
            except Exception as error:
                row.update(passed=False, error=repr(error))
            finally:
                if console is not None:
                    if console.child.poll() is None:
                        try:
                            console.close()
                        except Exception as error:
                            row.update(passed=False, cleanup_error=repr(error))
                    else:
                        console.transcript.extend(console.child.stdout.read()); console.record_retirement()
                        console.selector.close(); console.log.write_bytes(console.transcript)
                results.append(row); (args.out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print(json.dumps({k: v for k, v in row.items() if k not in ('argv', 'observations')}), flush=True)
    raise SystemExit(0 if all(row['passed'] for row in results) else 1)


if __name__ == '__main__':
    main()
