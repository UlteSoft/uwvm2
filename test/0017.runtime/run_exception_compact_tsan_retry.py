#!/usr/bin/env python3
"""Re-enter a previously built TSan ELF only after its exact startup failure.

Every attempt uses the original stopped-worker, pidfd, E16 and 2 GiB guards.
This does not rebuild the binary, change ASLR/seccomp, or replace a failed
original result. A race, other diagnostic or guard failure stops immediately.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import resource
import sys


STARTUP_LOG = (
    "FATAL: ThreadSanitizer: encountered an incompatible memory layout but was unable to disable ASLR (perhaps sandboxing is enabled?).\n"
    "FATAL: Please rerun with lower ASLR entropy, ASLR disabled, and/or sandboxing disabled.\n"
)
PASS_PREFIX = "PASS compact exception"


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def startup_failure(row, log):
    return row['exit'] == 66 and row['failure'] is None and log == STARTUP_LOG


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError('required source-bound control module is absent')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def verify_inputs(inputs):
    for path, expected in inputs.items():
        if sha(path) != expected:
            raise RuntimeError('original native qualification input changed: ' + path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--source-id', required=True)
    parser.add_argument('--original-summary', type=Path, required=True)
    parser.add_argument('--original-sha256', required=True)
    parser.add_argument('--binary-sha256', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    original = args.original_summary.resolve(strict=True)
    own = Path(__file__).resolve().parents[2]
    if sha(original) != args.original_sha256:
        raise RuntimeError('original failed qualification summary drifted')
    baseline = json.loads(original.read_text())
    if baseline['passed'] or baseline['source_id'] != args.source_id:
        raise RuntimeError('retry requires the selected original failed native qualification')
    rows = [row for row in baseline['commands'] if row['name'] == 'tsan-compact-run']
    if len(rows) != 1:
        raise RuntimeError('original qualification has no unique actual TSan run')
    row = rows[0]
    if sha(row['log']) != row['log_sha256'] or not startup_failure(row, Path(row['log']).read_text()):
        raise RuntimeError('original failure is not the exact pre-main TSan initialization failure')
    if len(row['command']) != 1:
        raise RuntimeError('original TSan command is not one pinned executable')
    executable = Path(row['command'][0]).resolve(strict=True)
    if sha(executable) != args.binary_sha256:
        raise RuntimeError('actual TSan ELF differs from the reviewed failed run')
    original_inputs = dict(baseline['inputs_before'])
    original_inputs.update(baseline['copied_exception_folder'])
    verify_inputs(original_inputs)
    controls_path = own / 'tools/ci/run_core3_component_bounded_slot.py'
    reader_path = own / 'test/0014.llvm_jit/run_native_unwind_noexcept_abi.py'
    bounded_path = own / 'test/0017.runtime/run_exception_compact_trace.py'
    controls = load(controls_path, 'uwvm_eh_retry_owned_tree')
    reader = load(reader_path, 'uwvm_eh_retry_cgroup')
    bounded = load(bounded_path, 'uwvm_eh_retry_native_invoke')
    before = reader.cgroup_state(root)
    if any(before['events'].get(key, 0) for key in ('oom', 'oom_kill')):
        raise RuntimeError('preceding shared OOM invalidates the component qualification')
    if 16 not in os.sched_getaffinity(0):
        raise RuntimeError('established E16 CPU is unavailable')
    os.sched_setaffinity(0, {16})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    if any(out.is_relative_to(root / part) for part in ('src', 'third-parties')):
        raise RuntimeError('qualification cannot write frozen source inputs')
    out.mkdir(parents=True, exist_ok=False)
    pinned = [Path(__file__).resolve(), controls_path, reader_path, bounded_path, original, executable,
        root / 'tools/ci/wasm3_source_fingerprint.py']
    retry_inputs = {str(path): sha(path) for path in pinned}
    summary = {'passed': False, 'scope': 'same actual compact EH TSan ELF; initialization retries only',
        'whole_vm_qualified': False, 'performance_qualified': False,
        'original_failed_result_sha256': args.original_sha256, 'source_id': args.source_id,
        'inputs_before': retry_inputs, 'cgroup_before': before, 'commands': [], 'attempts': [],
        'period_end_input_fingerprint_qualified': False}

    def invoke(name, command, env=None):
        result = bounded.bounded_invoke(command, out, name, root, controls, reader.cgroup_state, env)
        summary['commands'].append(result)
        (out / 'commands.json').write_text(json.dumps(summary['commands'], indent=2) + '\n')
        return result

    def fingerprint(stage):
        destination = out / ('source-' + stage + '.json')
        result = invoke('source-' + stage,
            [sys.executable, root / 'tools/ci/wasm3_source_fingerprint.py', root, destination])
        if result['exit'] != 0 or result['failure'] is not None:
            raise RuntimeError('source fingerprint command failed')
        if json.loads(destination.read_text())['source_id'] != args.source_id:
            raise RuntimeError('frozen product source ID changed')

    try:
        fingerprint('before')
        environment = dict(os.environ, TSAN_OPTIONS='halt_on_error=1')
        for attempt in range(1, 9):
            result = invoke('tsan-attempt-' + str(attempt), [executable], environment)
            log = Path(result['log']).read_text()
            startup = startup_failure(result, log)
            record = dict(result, attempt=attempt, startup_layout_failure=startup)
            summary['attempts'].append(record)
            (out / 'attempts.json').write_text(json.dumps(summary['attempts'], indent=2) + '\n')
            if startup:
                continue
            if result['exit'] != 0 or result['failure'] is not None:
                raise RuntimeError('actual TSan fixture/guard failed; no retry is allowed')
            if not log.startswith(PASS_PREFIX) or 'WARNING: ThreadSanitizer' in log or 'FATAL:' in log:
                raise RuntimeError('actual TSan run did not produce the compact fixture success witness')
            summary['passed'] = True
            break
        fingerprint('after')
        verify_inputs(original_inputs)
        verify_inputs(retry_inputs)
        after = reader.cgroup_state(root)
        if any(after['events'].get(key, 0) != before['events'].get(key, 0) for key in ('oom', 'oom_kill')):
            raise RuntimeError('shared cgroup OOM events changed')
        summary['cgroup_after'] = after
        summary['period_end_input_fingerprint_qualified'] = True
        summary['status'] = ('actual TSan compact fixture passed' if summary['passed'] else
            'TSan remained unqualified: eight exact pre-main initialization failures')
    except BaseException as error:
        summary['passed'] = False
        summary['failure'] = str(error)
        raise
    finally:
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(summary['status'])
    if not summary['passed']:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
