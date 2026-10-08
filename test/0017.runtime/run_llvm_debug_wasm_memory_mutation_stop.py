#!/usr/bin/env python3
"""Actual CLI Wasm memory commits retire stop/path labels; Linux cgroup only.

This launches a fresh real LLVM-full product and original Wasm loader. It is
not a fake broker or a runtime owner issuer. Native testing belongs to the
authorized Linux keeper after ROOT integrates the exact source proposal.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import selectors
import subprocess
import sys
import time


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pinned_inputs(args):
    """Check an actual keeper ALL3 receipt; JSON labels are not native authority.

    The immutable record itself must be externally reviewed by ROOT/keeper.
    This routine only binds that record to current bytes and refuses failure,
    reused runtime, mixed source and omitted whole-product component evidence.
    """
    fixed = ((args.uwvm, args.uwvm_sha256), (args.wasm_tools, args.wasm_tools_sha256),
             (args.qualified_build_record, args.qualified_build_record_sha256),
             (args.source_fingerprint, args.source_fingerprint_sha256),
             (args.source_cut_manifest, args.source_cut_manifest_sha256))
    pins = {}
    for path, wanted in fixed:
        if re.fullmatch(r'[0-9a-f]{64}', wanted) is None or digest(path) != wanted:
            raise RuntimeError('input SHA mismatch: '+str(path))
        pins[str(path)] = {'sha256': wanted, 'bytes': path.stat().st_size}
    receipt = json.loads(args.qualified_build_record.read_text())
    if (receipt.get('passed') is not True or receipt.get('repository') != args.repository or
        receipt.get('all_product_TUs_fresh') is not True or receipt.get('runtime_reused') is not False or
        receipt.get('source_tools_headers_SDK_MD_before_after_equal') is not True or
        type(receipt.get('main_link_returncode')) is not int or receipt['main_link_returncode'] != 0 or
        receipt.get('product') != pins[str(args.uwvm)] or
        Path(receipt.get('product_path', '')).resolve(strict=True) != args.uwvm):
        raise RuntimeError('keeper successful fresh ALL3/product binding required')
    if receipt.get('developer_provider_only') is True and not args.allow_developer_provider:
        raise RuntimeError('developer provider needs explicit limited-scope admission')
    components = receipt.get('components')
    if not isinstance(components, dict) or set(components) != {'runtime', 'main', 'host-api'}:
        raise RuntimeError('all three actual product TU records are required')
    for component in components.values():
        proof = component.get('actual_compile_proof', {})
        if (component.get('same_task_or_historical_object_reused') is not False or
            type(proof.get('returncode')) is not int or proof['returncode'] != 0 or
            not isinstance(proof.get('actual_argv'), list) or not proof['actual_argv'] or
            not isinstance(component.get('actual_dependencies'), dict) or not component['actual_dependencies']):
            raise RuntimeError('actual successful compiler/MD component proof missing')
        for name, pin in component['actual_dependencies'].items():
            path = Path(name).resolve(strict=True)
            if pin != {'sha256': digest(path), 'bytes': path.stat().st_size}:
                raise RuntimeError('actual compiled source/header/SDK bytes changed: '+name)
            if str(path) in pins and pins[str(path)] != pin:
                raise RuntimeError('conflicting actual input dependency pin: '+name)
            pins[str(path)] = pin
    fingerprint = json.loads(args.source_fingerprint.read_text())
    files = fingerprint.get('files')
    if not isinstance(files, list) or not files:
        raise RuntimeError('actual build source fingerprint file is required')
    source_id = 'sha256:'+hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    if source_id != fingerprint.get('source_id') or source_id != receipt.get('source_id_external'):
        raise RuntimeError('source fingerprint and actual ALL3 source identity differ')
    seen = set()
    for row in files:
        relative = Path(row.get('path', ''))
        if relative.is_absolute() or '..' in relative.parts or not relative.parts or relative.parts[0] not in ('src', 'third-parties') or relative in seen:
            raise RuntimeError('source fingerprint has invalid/duplicate paths')
        seen.add(relative)
        path = (args.source_root/relative).resolve(strict=True)
        if digest(path) != row.get('sha256'):
            raise RuntimeError('current source differs from compiled fingerprint: '+str(relative))
        pins[str(path)] = {'sha256': row['sha256'], 'bytes': path.stat().st_size}
    cut = json.loads(args.source_cut_manifest.read_text())
    if cut.get('kind') != 'complete-current-source-development-cut' or not isinstance(cut.get('files'), list):
        raise RuntimeError('ROOT complete immutable source cut is required')
    # The build fingerprint does not include tests. Bind the actual runner and
    # modern fixture separately to the genuine complete source cut inventory.
    required = {'test/0017.runtime/run_llvm_debug_wasm_memory_mutation_stop.py',
                'test/0017.runtime/fixtures/debug_wasm_memory_mutation_stop.wat'}
    matches = {}
    for row in cut['files']:
        name = row.get('path', '')
        prefix = args.repository+'/'
        if name.startswith(prefix) and name[len(prefix):] in required:
            relative = name[len(prefix):]
            if relative in matches:
                raise RuntimeError('duplicate fixture/runner source cut row')
            path = (args.source_root/relative).resolve(strict=True)
            pin = {'sha256': digest(path), 'bytes': path.stat().st_size}
            if pin != {'sha256': row.get('sha256'), 'bytes': row.get('bytes')}:
                raise RuntimeError('fixture/runner differs from ROOT immutable cut')
            matches[relative] = pin; pins[str(path)] = pin
    if set(matches) != required or Path(__file__).resolve(strict=True) != args.source_root/'test/0017.runtime/run_llvm_debug_wasm_memory_mutation_stop.py':
        raise RuntimeError('runner and fixture must come from the admitted exact source cut')
    return pins, {'source_id': source_id, 'developer_provider_only': receipt.get('developer_provider_only') is True,
                  'paired_ROS_runtime_production_qualified': receipt.get('paired_ROS_runtime_production_qualified') is True}


class Console:
    def __init__(self, command, out):
        self.command, self.out = command, out
        self.pending, self.transcript, self.rows = bytearray(), bytearray(), []
        self.process = subprocess.Popen(command, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)

    def prompt(self):
        token, deadline = b'(uwvm-debug) ', time.monotonic()+30
        while token not in self.pending:
            remaining = deadline-time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise AssertionError(('prompt timeout', self.transcript[-6000:]))
            chunk = os.read(self.process.stdout.fileno(), 65536)
            if not chunk:
                raise AssertionError(('early exit', self.process.poll(), self.transcript[-6000:]))
            self.pending.extend(chunk); self.transcript.extend(chunk)
        end = self.pending.index(token)+len(token)
        result = bytes(self.pending[:end]); del self.pending[:end]
        return result

    def send(self, text):
        self.process.stdin.write(text.encode()+b'\n'); self.process.stdin.flush()
        answer = self.prompt()
        self.rows.append({'command': text, 'reply': answer.decode(errors='replace')})
        (self.out/'commands.json').write_text(json.dumps(self.rows, indent=2)+'\n')
        return answer

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=10)
        self.transcript.extend(self.process.stdout.read())
        (self.out/'console.log').write_bytes(self.transcript)
        self.selector.close()


def status(console):
    answer = console.send('status')
    stop = re.search(rb'(?m)^stop-id ([0-9]+)\r?$', answer)
    thread = re.search(rb'(?m)^thread ([0-9]+) module=0 function=0 ', answer)
    assert b'stopped:' in answer and stop and thread, answer
    assert int(stop[1]) > 0 and int(thread[1]) > 0
    return int(stop[1]), int(thread[1])


def path(console, participant, stop):
    answer = console.send(f'path create globals {participant} 0 0 0 0')
    labels = re.search(rb'Wasm path session=([0-9]+) handle=([0-9]+) depth=0 ', answer)
    assert labels and f'wasm-stop {stop}\n'.encode() in answer, answer
    assert int(labels[1]) > 0 and int(labels[2]) > 0
    return int(labels[1]), int(labels[2])


def check_path(console, labels, stop, expected):
    answer = console.send(f'path members {labels[0]} {labels[1]} 0 1')
    if expected:
        assert b'Wasm path session=' in answer and f'wasm-stop {stop}\n'.encode() in answer, answer
    else:
        assert b'Wasm path unavailable: stop or code generation is stale' in answer, answer
        assert b'Wasm path session=' not in answer, answer


def mutation(console, command, success):
    before, participant = status(console)
    labels = path(console, participant, before)
    answer = console.send(command)
    marker = re.search(rb'(?m)^wasm-stop ([0-9]+)\r?$', answer)
    assert marker, answer
    assert b'applied=1' in answer if success else b'applied=0' in answer, answer
    after, current_participant = status(console)
    assert current_participant == participant
    assert after > before if success else after == before, (command, before, after)
    assert int(marker[1]) == after, answer
    check_path(console, labels, after, not success)
    return before, after


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--strategy', choices=('instruction', 'unwind'), required=True)
    parser.add_argument('--repository', choices=('uwvm2', 'uwvm2-ros'), required=True)
    parser.add_argument('--uwvm-sha256', required=True)
    parser.add_argument('--wasm-tools-sha256', required=True)
    parser.add_argument('--qualified-build-record', type=Path, required=True)
    parser.add_argument('--qualified-build-record-sha256', required=True)
    parser.add_argument('--source-fingerprint', type=Path, required=True)
    parser.add_argument('--source-fingerprint-sha256', required=True)
    parser.add_argument('--source-cut-manifest', type=Path, required=True)
    parser.add_argument('--source-cut-manifest-sha256', required=True)
    parser.add_argument('--allow-developer-provider', action='store_true')
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('sole remote Linux/cgroup keeper runner only')
    args.source_root = args.source_root.resolve(strict=True)
    args.uwvm = args.uwvm.resolve(strict=True)
    for name in ('wasm_tools', 'qualified_build_record', 'source_fingerprint', 'source_cut_manifest'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.out = args.out.resolve()
    subprocess.run(['bash', str(args.source_root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before_pins, build_scope = pinned_inputs(args)
    args.out.mkdir(parents=True, mode=0o700, exist_ok=False)
    (args.out/'input-pins-before.json').write_text(json.dumps(before_pins, indent=2)+'\n')
    (args.out/'qualified-build-record.copy.json').write_bytes(args.qualified_build_record.read_bytes())
    wat = args.source_root/'test/0017.runtime/fixtures/debug_wasm_memory_mutation_stop.wat'
    wasm = args.out/'debug-wasm-memory-mutation-stop.wasm'
    subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
    subprocess.run([str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True)
    command = [str(args.uwvm), '-Rdbg', '-Rct', '0', '-Rllvm-call-stack', args.strategy,
               '-Rllvm-cache-path', 'disable', '-WFE-gc', '-WFE-multi-memory', '-WFE-memory64',
               '-WFE-threads', '--run', str(wasm)]
    console, checks = Console(command, args.out), []
    try:
        console.prompt()
        assert b'breakpoint' in console.send('break 0 0 0')
        console.send('continue')
        assert b'stopped:' in console.send('wait')
        _, participant = status(console)
        # Real source67 initialization opcodes +one NOP. No guest memory read
        # permission is inferred from this count: the current path query below
        # independently reborrows the authentic GC global before every commit.
        for _ in range(68):
            assert b'stopped:' in console.send(f'step wasm {participant}')
        stop, participant = status(console)
        check_path(console, path(console, participant, stop), stop, True)
        for name, text in (
            ('offset exactly at end', f'set wasm memory 0 0 {participant} 65536 bytes 00'),
            ('last byte plus overflow', f'set wasm memory 0 1 {participant} 65535 bytes 0011'),
            ('wide addition overflow', f'set wasm memory 0 3 {participant} 18446744073709551615 bytes 0011'),
            ('unknown module', f'set wasm memory 18446744073709551615 0 {participant} 64 bytes 01020304'),
            ('unknown memory', f'set wasm memory 0 18446744073709551615 {participant} 64 bytes 01020304'),
        ):
            before, after = mutation(console, text, False)
            checks.append({'name': name, 'applied': False, 'before': before, 'after': after})
        for index in range(4):
            text = f'set wasm memory 0 {index} {participant} 64 bytes 01020304'
            before, after = mutation(console, text, True)
            checks.append({'name': 'real selected memory'+str(index), 'applied': True, 'before': before, 'after': after})
        text = f'set wasm memory 0 0 {participant} 256 bytes '+bytes(range(256)).hex()
        assert len(text.encode()) > 512
        before, after = mutation(console, text, True)
        checks.append({'name': 'full256 payload survives real CLI input', 'applied': True, 'before': before, 'after': after})
        assert b'running' in console.send('continue')
        assert b'guest exited: 0' in console.send('wait')
        console.process.stdin.write(b'quit\n'); console.process.stdin.flush()
        assert console.process.wait(timeout=15) == 0
    finally:
        console.close()
        (args.out/'results.json').write_text(json.dumps(checks, indent=2)+'\n')
    after_pins, after_scope = pinned_inputs(args)
    (args.out/'input-pins-after.json').write_text(json.dumps(after_pins, indent=2)+'\n')
    if before_pins != after_pins or build_scope != after_scope:
        raise RuntimeError('source/binary/tool/actual build evidence changed during qualification')
    (args.out/'summary.json').write_text(json.dumps({'passed': True, 'checks': len(checks),
        'strategy': args.strategy, 'binary_sha256': digest(args.uwvm), 'wat_sha256': digest(wat),
        'wasm_sha256': digest(wasm), 'build_scope': build_scope,
        'qualified_build_record_sha256': args.qualified_build_record_sha256,
        'source_cut_manifest_sha256': args.source_cut_manifest_sha256,
        'source_fingerprint_sha256': args.source_fingerprint_sha256,
        'scope': 'real CLI stop/path invalidation only; no collector/restore/hosteffects or bundled-provider qualification'}, indent=2)+'\n')
    print('PASS real Wasm memory mutation stop/path labels', len(checks), args.strategy)


if __name__ == '__main__':
    main()
