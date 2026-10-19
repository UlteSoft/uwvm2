#!/usr/bin/env python3
"""Build an isolated ordinary Java profile guest and verify both real stdin gates.

Run only inside the authorized Linux cgroup. This creates no placeholder Wasm
hash or manifest: publication requires JVM/Wasmtime protocol and checksum proofs.
"""
import argparse
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time

from build_dwarf_fixed import CORE, JAR, sha
from build_perf_timed import parse


READY = 'UWVM_PROFILE_READY'
DONE = 'UWVM_PROFILE_DONE'


def kernel_loop(source):
    start = source.index('for (int i = 0; i < iterations; ++i) {')
    at = source.index('{', start)
    depth, end = 1, at + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return re.sub(r'\s+', '', source[start:end])


def protocol(argv, folder, ready_timeout, kernel_timeout):
    """Release each byte only after observing its marker and a live blocked child."""
    folder.mkdir(parents=True, exist_ok=False)
    row = {'argv': [str(value) for value in argv], 'gate_events': []}
    started = time.monotonic()
    process = subprocess.Popen(row['argv'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, start_new_session=True, bufsize=0)
    selector = selectors.DefaultSelector()
    output, error, pending = bytearray(), bytearray(), bytearray()
    selector.register(process.stdout, selectors.EVENT_READ, 'stdout')
    selector.register(process.stderr, selectors.EVENT_READ, 'stderr')
    stage, release_at, deadline = 'startup', None, started + ready_timeout
    try:
        while selector.get_map() or process.poll() is None:
            now = time.monotonic()
            assert now < deadline, f'{stage} timeout'
            for key, _ in selector.select(.02):
                data = os.read(key.fileobj.fileno(), 65536)
                if not data:
                    selector.unregister(key.fileobj)
                    continue
                if key.data == 'stdout':
                    assert stage == 'finished', 'stable stdout appeared before the D release'
                    output.extend(data)
                    continue
                error.extend(data)
                pending.extend(data)
                while b'\n' in pending:
                    line, _, tail = pending.partition(b'\n')
                    pending[:] = tail
                    line = line.decode('utf-8')
                    if line == READY:
                        assert stage == 'startup', f'unexpected READY in {stage}'
                        stage = 'ready'
                    elif line == DONE:
                        assert stage == 'running', f'unexpected DONE in {stage}'
                        stage = 'done'
                    else:
                        raise AssertionError('unexpected reference stderr: ' + line)
                    now = time.monotonic()
                    row['gate_events'].append({'marker': line, 'seconds': now - started})
                    # This is a correctness observation, never a kernel timing sample.
                    release_at = now + .05
                    deadline = now + 5
            now = time.monotonic()
            if stage in ('ready', 'done') and now >= release_at:
                assert process.poll() is None and not output, 'child did not remain at the gate'
                byte = b'G' if stage == 'ready' else b'D'
                row['gate_events'].append({'release': byte.decode(), 'seconds': now - started,
                                           'observed_alive_without_release': True})
                stage = 'running' if byte == b'G' else 'finished'
                process.stdin.write(byte)
                process.stdin.flush()
                deadline = now + (kernel_timeout if byte == b'G' else 15)
        row['exit'] = process.wait(timeout=5)
        row['wall_seconds'] = time.monotonic() - started
        assert row['exit'] == 0 and stage == 'finished', row
        assert bytes(error) == (READY + '\n' + DONE + '\n').encode(), bytes(error)
        assert not pending
        return row, output.decode('utf-8')
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)
        selector.close()
        process.stdin.close()
        process.stdout.close()
        process.stderr.close()
        (folder / 'stdout.log').write_bytes(output)
        (folder / 'stderr.log').write_bytes(error)
        (folder / 'protocol.json').write_text(json.dumps(row, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--maven', type=Path, required=True)
    parser.add_argument('--m2', type=Path, required=True)
    parser.add_argument('--toolchain-receipt', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--output', default='perf-profile-gated')
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--iterations', type=int, nargs='+', default=[17, 200000000, 2000000000])
    parser.add_argument('--case-iterations', type=int, default=2000000000)
    parser.add_argument('--ready-timeout', type=float, default=900)
    parser.add_argument('--reference-kernel-timeout', type=float, default=120)
    parser.add_argument('--helpers', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    assert sys.platform == 'linux', 'execute only inside the authorized SSH Linux cgroup'
    sys.path.insert(0, str(args.helpers.resolve(strict=True)))
    from build_products import admission
    guard = admission()
    project, repository = args.project.resolve(strict=True), args.m2.resolve(strict=True)
    assert args.output and Path(args.output).name == args.output
    assert all(1 <= value <= 2000000000 for value in args.iterations)
    assert args.case_iterations in args.iterations
    assert args.ready_timeout > 0 and args.reference_kernel_timeout > 0
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    output = project / 'target' / args.output
    manifest = (args.manifest or project / 'perf-profile-cases.json').resolve()
    assert not output.exists() and not manifest.exists(), 'refuse to replace an existing profile artifact/manifest'
    source = project / 'src/main/java/uwvm/hll/PerfProfile.java'
    original_source = project / 'src/main/java/uwvm/hll/PerfTimed.java'
    assert kernel_loop(source.read_text()) == kernel_loop(original_source.read_text()), 'kernel body changed'
    compiler = json.loads(args.toolchain_receipt.read_text())
    assert compiler['passed'] and compiler['wasi_monotonic_mapping'] and compiler['wasi_stdin_mapping']
    assert sha(repository / CORE / JAR) == compiler['patched_jar_sha256'], 'private compiler identity differs'

    def preserved():
        paths = [project / 'src/main/java/uwvm/hll/Perf.java', original_source,
                 project / 'perf-timed-cases.json']
        for name in ('perf', 'perf-timed'):
            root = project / 'target' / name
            assert root.is_dir(), 'original artifact directory is missing: ' + str(root)
            paths.extend(path for path in root.rglob('*') if path.is_file())
        return {str(path): sha(path) for path in sorted(paths)}

    before = preserved()
    record = {'passed': False, 'purpose': 'hardware-gated', 'admission_before': guard,
              'preserved_before': before, 'source_sha256': sha(source),
              'original_source_sha256': sha(original_source), 'kernel_loop_identical': True,
              'pom_sha256': sha(project / 'pom.xml'), 'builder_sha256': sha(__file__),
              'toolchain_receipt': str(args.toolchain_receipt.resolve(strict=True)),
              'toolchain_receipt_sha256': sha(args.toolchain_receipt),
              'compiler_jar_sha256': compiler['patched_jar_sha256'],
              'classlib_jar_sha256': sha(repository / 'org/teavm/teavm-classlib/0.13.1/teavm-classlib-0.13.1.jar'),
              'commands': [], 'reference_runs': [], 'negative_gate_runs': []}

    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')

    try:
        shutil.copy2(project / 'pom.xml', work / 'pom.xml')
        destination = work / 'src/main/java/uwvm/hll/PerfProfile.java'
        destination.parent.mkdir(parents=True)
        shutil.copy2(source, destination)
        os.environ['MAVEN_OPTS'] = '-Xmx2g -XX:ActiveProcessorCount=2'
        command = [str(args.maven.resolve(strict=True)), '-o', '--batch-mode', '--no-transfer-progress',
                   'process-classes', '-Dmaven.repo.local=' + str(repository),
                   '-Dteavm.target=WEBASSEMBLY_WASI', '-Dteavm.output=profile-gated',
                   '-Dteavm.optimization=FULL', '-Dfixture.main=uwvm.hll.PerfProfile',
                   '-Dfixture.file=java-perf-profile.wasm']
        record['commands'].append({'argv': command, 'cwd': str(work), 'log': 'maven.log'})
        save()
        with (work / 'maven.log').open('wb') as stream:
            result = subprocess.run(command, cwd=work, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        record['commands'][-1]['exit'] = result.returncode
        assert result.returncode == 0, (work / 'maven.log').read_text(errors='replace')[-6000:]
        artifact = work / 'target/profile-gated/java-perf-profile.wasm'
        original_wasm = project / 'target/perf-timed/java-perf-timed.wasm'
        assert sha(artifact) != sha(original_wasm), 'profile module unexpectedly reused the original timed artifact'
        java = Path(os.environ.get('JAVA_HOME', '/usr/lib/jvm/java-21-openjdk-amd64')) / 'bin/java'
        engines = {
            'jvm': [java, '-Xmx512m', '-XX:ActiveProcessorCount=1', '-cp', work / 'target/classes', 'uwvm.hll.PerfProfile'],
            'wasmtime': [args.wasmtime.resolve(strict=True), 'run', '-C', 'cache=n,parallel-compilation=n', artifact],
        }
        for iterations in dict.fromkeys(args.iterations):
            answers = {}
            for engine, base in engines.items():
                print(f'verify {engine}: iterations={iterations}', flush=True)
                row, text = protocol([*base, iterations], work / f'{engine}-{iterations}',
                                     args.ready_timeout, args.reference_kernel_timeout)
                row.update(engine=engine, **parse(text, iterations))
                assert row['kernel_ns'] > 0, row
                answers[engine] = row
            assert answers['jvm']['checksum'] == answers['wasmtime']['checksum'], answers
            record['reference_runs'].append(answers)
            save()
        # All cases initialize and warm the same kernel; use tiny measured work for
        # invalid-protocol tests. Never pre-feed bytes during a positive proof.
        for engine, base in engines.items():
            for name, data in [('eof-before-G', b''), ('wrong-G', b'X'),
                               ('eof-before-D', b'G'), ('wrong-D', b'GX')]:
                command = [str(value) for value in [*base, 17]]
                result = subprocess.run(command, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        timeout=args.ready_timeout + 15)
                prefix = engine + '-' + name
                (work / (prefix + '.stdout')).write_bytes(result.stdout)
                (work / (prefix + '.stderr')).write_bytes(result.stderr)
                assert result.returncode != 0 and not result.stdout, (prefix, result.returncode, result.stdout)
                assert (READY + '\n').encode() in result.stderr, (prefix, result.stderr)
                assert ((DONE + '\n').encode() in result.stderr) == ('before-D' in name or name == 'wrong-D')
                record['negative_gate_runs'].append({'engine': engine, 'case': name, 'argv': command,
                                                      'input_hex': data.hex(), 'exit': result.returncode})
                save()
        shutil.copytree(artifact.parent, output)
        shutil.copytree(work / 'target/classes', output / 'classes')
        selected = next(row for row in record['reference_runs'] if row['jvm']['iterations'] == args.case_iterations)
        checksum = selected['jvm']['checksum']
        wasm = output / artifact.name
        case = {'name': f'java-teavm-perf-profile-{args.case_iterations}', 'language': 'java',
                'target': 'teavm-wasi', 'mode': 'release', 'purpose': 'hardware-gated',
                'rounds': args.case_iterations, 'iterations': args.case_iterations,
                'expected_checksum': checksum, 'clock_kind': 'monotonic-ns',
                'wasm': str(wasm), 'wasm_sha256': sha(wasm), 'args': [str(args.case_iterations)],
                'checks': [f'java perf timed: iterations={args.case_iterations}', f'checksum={checksum}'],
                'kernel_time': 'ns', 'kernel_clock': 'monotonic', 'clock_line_prefix': 'Kernel ns: ',
                'checksum_line': f'checksum={checksum}', 'generated_source': str(source),
                'generated_source_sha256': sha(source), 'oracle': str(work / 'receipt.json'),
                'compiler_jar_sha256': compiler['patched_jar_sha256'],
                'classlib_jar_sha256': record['classlib_jar_sha256'],
                'toolchain_receipt': str(args.toolchain_receipt.resolve(strict=True)),
                'toolchain_receipt_sha256': sha(args.toolchain_receipt),
                'gate_stream': 'stderr', 'gate_ready': READY, 'gate_done': DONE,
                'gate_release': 'G', 'gate_finish': 'D', 'gate_ready_timeout': args.ready_timeout,
                'kernel_timeout': 28, 'warmup_iterations': 65536, 'warmup_runs': 2}
        record.update(artifact={'path': str(wasm), 'sha256': sha(wasm)}, manifest_case=case,
                      original_and_profile_module_layout_claimed_equal=False,
                      reference_timings_are_performance_results=False)
    except Exception as error:
        record['error'] = repr(error)
    finally:
        record['preserved_after'] = preserved()
        record['admission_after'] = admission()
        record['passed'] = ('error' not in record and before == record['preserved_after']
                            and guard == record['admission_after'])
        if record['passed']:
            manifest.write_text(json.dumps([record['manifest_case']], indent=2) + '\n')
            record['manifest'] = {'path': str(manifest), 'sha256': sha(manifest)}
        save()
    print(json.dumps({'passed': record['passed'], 'artifact': record.get('artifact'),
                      'manifest': record.get('manifest'), 'error': record.get('error'),
                      'receipt': str(work / 'receipt.json')}), flush=True)
    raise SystemExit(0 if record['passed'] else 1)


if __name__ == '__main__':
    main()
