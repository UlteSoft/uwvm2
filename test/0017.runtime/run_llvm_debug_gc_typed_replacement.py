#!/usr/bin/env python3
"""Qualify same-ABI LLVM-full replacement of a Core 3 typed-ref/GC function.

Valid fixtures are checked by the official validator before execution. The
wrong-heap body is valid in its own module but incompatible with the original
exact heap signature. Non-null locals test Core 3 initialization tracking:
https://webassembly.github.io/spec/core/valid/instructions.html#variable-instructions
The uninitialized-local and late-illegal-opcode modules must instead be rejected
by the official validator, then rejected privately by the replacement compiler.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import selectors
import subprocess
import sys
import time


PREFIX = 'debug_replace_gc_typed_'
EXPECTED = {'base': b'value=0\n', 'good': b'value=1\n', 'good_local': b'value=2\n'}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def u32(data, cursor, end):
    value = 0
    for shift in range(0, 35, 7):
        if cursor >= end:
            raise ValueError('truncated unsigned LEB')
        byte = data[cursor]
        cursor += 1
        if shift == 28 and byte & 0xf0:
            raise ValueError('u32 LEB overflow')
        value |= (byte & 0x7f) << shift
        if byte < 0x80:
            return value, cursor
    raise ValueError('overlong unsigned LEB')


def decode_module(path):
    data = path.read_bytes()
    if data[:8] != b'\x00asm\x01\x00\x00\x00':
        raise ValueError(f'bad Wasm header: {path}')
    cursor = 8
    sections = []
    bodies = None
    while cursor < len(data):
        section_id = data[cursor]
        cursor += 1
        size, cursor = u32(data, cursor, len(data))
        section_end = cursor + size
        if section_end > len(data):
            raise ValueError('section exceeds Wasm file')
        payload = data[cursor:section_end]
        sections.append((section_id, payload))
        cursor = section_end
        if section_id != 10:
            continue
        if bodies is not None:
            raise ValueError('duplicate Code section')
        count, offset = u32(payload, 0, len(payload))
        bodies = []
        for _ in range(count):
            body_size, offset = u32(payload, offset, len(payload))
            body_end = offset + body_size
            if body_end > len(payload):
                raise ValueError('function body exceeds Code section')
            bodies.append(payload[offset:body_end])
            offset = body_end
        if offset != len(payload):
            raise ValueError('trailing Code section bytes')
    if bodies is None or not bodies:
        raise ValueError('missing local target body')
    return sections, bodies


def u32_leb(value):
    if not 0 <= value <= 0xffffffff:
        raise ValueError('fixture u32 exceeds Wasm encoding range')
    encoded = bytearray()
    while value >= 0x80:
        encoded.append((value & 0x7f) | 0x80)
        value >>= 7
    encoded.append(value)
    return bytes(encoded)


def write_bodies(path, sections, bodies):
    code = u32_leb(len(bodies)) + b''.join(u32_leb(len(body)) + body for body in bodies)
    module = bytearray(b'\x00asm\x01\x00\x00\x00')
    for kind, original in sections:
        payload = code if kind == 10 else original
        module.append(kind)
        module.extend(u32_leb(len(payload)))
        module.extend(payload)
    path.write_bytes(module)


def official_validation(tool, wasm, expected_valid, log):
    result = subprocess.run([str(tool), 'validate', '--features', 'all', str(wasm)],
                            capture_output=True, timeout=30)
    log.write_bytes(result.stdout + result.stderr)
    if expected_valid:
        assert result.returncode == 0, (wasm, result.returncode, log.read_bytes())
    else:
        # Signal/launcher failures are not evidence that the fixture is invalid.
        assert result.returncode > 0, (wasm, result.returncode, log.read_bytes())
        assert result.stderr, ('invalid fixture needs an official diagnostic', wasm)
    return {'expected': 'valid' if expected_valid else 'invalid',
            'exit_code': result.returncode, 'diagnostic': str(log)}


def read_until(stream, marker, pending, transcript, timeout=60):
    deadline = time.monotonic() + timeout
    with selectors.DefaultSelector() as selector:
        selector.register(stream, selectors.EVENT_READ)
        while marker not in pending:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not selector.select(remaining):
                raise AssertionError(('console timeout', marker, bytes(transcript)[-4000:]))
            chunk = os.read(stream.fileno(), 65536)
            if not chunk:
                raise AssertionError(('console exited', marker, bytes(transcript)[-4000:]))
            pending.extend(chunk)
            transcript.extend(chunk)
    end = pending.index(marker) + len(marker)
    result = bytes(pending[:end])
    del pending[:end]
    return result


def console_run(command, expected, out, rejected_bodies=(), good_body=None):
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    pending = bytearray()
    transcript = bytearray()
    responses = {}

    def reply(text):
        process.stdin.write(text.encode() + b'\n')
        process.stdin.flush()
        return read_until(process.stdout, b'(uwvm-debug) ', pending, transcript)

    try:
        read_until(process.stdout, b'(uwvm-debug) ', pending, transcript)
        initial = reply('status')
        assert b'prepared; no Wasm instruction executed' in initial, initial
        for name, rejected_body in rejected_bodies:
            response = reply(f'replace 0 1 1 {rejected_body}')
            responses[name] = response.decode(errors='replace')
            assert b'replacement body failed WebAssembly validation' in response, response
        if good_body is not None:
            response = reply(f'replace 0 1 1 {good_body}')
            responses['same_abi'] = response.decode(errors='replace')
            # Every preceding rejection must leave the old generation at one.
            assert b'function replaced; generation 2' in response, response
            response = reply(f'replace 0 1 1 {good_body}')
            responses['stale'] = response.decode(errors='replace')
            assert b'function generation changed' in response, response
        response = reply('continue')
        assert b'running' in response, response
        deadline = time.monotonic() + 30
        while True:
            response = reply('status')
            if b'guest exited: 0' in response:
                break
            assert time.monotonic() < deadline, response
            time.sleep(.01)
        process.stdin.write(b'quit\n')
        process.stdin.flush()
        assert process.wait(timeout=15) == 0, process.returncode
        transcript.extend(process.stdout.read())
        assert expected in transcript, bytes(transcript)[-4000:]
        out.write_bytes(transcript)
        return responses
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=15)
        out.write_bytes(transcript)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--macos', action='store_true',
                        help='run the debugger product on macOS under macos_rss_limit.py')
    parser.add_argument('--prepare-only', action='store_true',
                        help='parse/extract/validate fixtures without running a VM')
    parser.add_argument('--policies', nargs='+', choices=('instruction', 'unwind'),
                        default=('instruction', 'unwind'))
    args = parser.parse_args()
    if not args.prepare_only and (args.uwvm is None or args.wasmtime is None):
        parser.error('--uwvm and --wasmtime are required for product qualification')
    root = args.source_root.resolve(strict=True)
    if not args.prepare_only:
        if args.macos:
            if sys.platform != 'darwin':
                parser.error('--macos requires Darwin and the 4 GiB RSS watchdog')
        else:
            subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.out.mkdir(parents=True, exist_ok=False)
    fixtures = {}
    modules = {}
    for name in ('base', 'good', 'wrong', 'good_local', 'bad_uninitialized'):
        wat = root / 'test/0017.runtime/fixtures' / (PREFIX + name + '.wat')
        wasm = args.out / (name + '.wasm')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        validation = official_validation(args.wasm_tools, wasm, name != 'bad_uninitialized',
                                         args.out / (name + '-official-validate.log'))
        fixtures[name] = {'wat_sha256': sha256(wat), 'wasm_sha256': sha256(wasm),
                          'official_validation': validation}
        modules[name] = decode_module(wasm)
    base_sections, base_bodies = modules['base']
    _, wrong_bodies = modules['wrong']
    assert len(base_bodies) == 2
    assert len(wrong_bodies) == 1
    for name in ('good', 'good_local', 'bad_uninitialized'):
        sections, function_bodies = modules[name]
        assert len(function_bodies) == len(base_bodies)
        assert function_bodies[1:] == base_bodies[1:], ('caller changed', name)
        assert [(kind, payload) for kind, payload in base_sections if kind != 10] == [
            (kind, payload) for kind, payload in sections if kind != 10], ('ABI/declarations changed', name)
    good_local_body = modules['good_local'][1][0]
    assert good_local_body[-1:] == b'\x0b'
    # Keep the complete valid GC/non-null-local prefix and final end, so the
    # JIT must discard IR already emitted before this illegal primary opcode.
    late_body = good_local_body[:-1] + b'\xff' + good_local_body[-1:]
    late_wasm = args.out / 'late_invalid.wasm'
    write_bodies(late_wasm, modules['good_local'][0], [late_body, *base_bodies[1:]])
    late_sections, late_bodies = decode_module(late_wasm)
    assert late_bodies[0] == late_body and late_bodies[1:] == base_bodies[1:]
    assert [(kind, payload) for kind, payload in late_sections if kind != 10] == [
        (kind, payload) for kind, payload in base_sections if kind != 10]
    fixtures['late_invalid'] = {
        'wasm_sha256': sha256(late_wasm), 'derived_from': 'good_local',
        'official_validation': official_validation(args.wasm_tools, late_wasm, False,
                                                   args.out / 'late_invalid-official-validate.log')}
    bodies = {'good': modules['good'][1][0], 'wrong': wrong_bodies[0],
              'good_local': good_local_body,
              'bad_uninitialized': modules['bad_uninitialized'][1][0], 'late_invalid': late_body}
    for name, body in bodies.items():
        assert body != base_bodies[0], name
        assert 2 <= len(body) <= 65536
        (args.out / (name + '.body')).write_bytes(body)
        fixtures[name]['body_hex'] = body.hex()
        fixtures[name]['body_sha256'] = hashlib.sha256(body).hexdigest()
    summary = {'prepared': True, 'fixtures': fixtures, 'product_qualified': False,
               'source_root': str(root), 'cases': []}
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    if args.prepare_only:
        print('PASS Core 3 typed-ref/GC replacement fixture preparation')
        return

    for name, expected in EXPECTED.items():
        process = subprocess.run([str(args.wasmtime), '-C', 'cache=n', '-W', 'gc=y',
                                  str(args.out / (name + '.wasm'))], capture_output=True, timeout=30)
        output = process.stdout + process.stderr
        (args.out / (name + '-wasmtime.log')).write_bytes(output)
        assert process.returncode == 0 and expected in output, (name, process.returncode, output)
        summary['cases'].append({'case': name + '-wasmtime', 'passed': True})
    binary = args.uwvm.resolve(strict=True)
    summary['product_sha256'] = sha256(binary)
    mode = [] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    for policy in args.policies:
        prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0',
                  '-Rllvm-call-stack', policy, '-Rllvm-cache-path', 'disable',
                  '-WFE-gc', '-WFE-function-references']
        base_command = prefix + ['--run', str(args.out / 'base.wasm')]
        console_run(base_command, EXPECTED['base'], args.out / (policy + '-baseline.log'))
        summary['cases'].append({'case': policy + '-baseline', 'passed': True})
        responses = console_run(base_command, EXPECTED['good'],
                                args.out / (policy + '-replacement.log'),
                                rejected_bodies=(('wrong_heap', args.out / 'wrong.body'),),
                                good_body=args.out / 'good.body')
        summary['cases'].append({'case': policy + '-replacement', 'passed': True,
                                 'responses': responses})
        rejected = (('wrong_heap', args.out / 'wrong.body'),
                    ('uninitialized_nonnull_local', args.out / 'bad_uninitialized.body'),
                    ('late_illegal_opcode', args.out / 'late_invalid.body'))
        responses = console_run(base_command, EXPECTED['base'],
                                args.out / (policy + '-rejected-old-result.log'),
                                rejected_bodies=rejected)
        summary['cases'].append({'case': policy + '-rejected-old-result', 'passed': True,
                                 'responses': responses})
        responses = console_run(base_command, EXPECTED['good_local'],
                                args.out / (policy + '-nonnull-local-replacement.log'),
                                rejected_bodies=rejected, good_body=args.out / 'good_local.body')
        summary['cases'].append({'case': policy + '-nonnull-local-replacement', 'passed': True,
                                 'responses': responses})
        (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    summary['product_qualified'] = True
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS Core 3 typed-ref/GC LLVM-full replacement', len(summary['cases']), 'checks')


if __name__ == '__main__':
    main()
