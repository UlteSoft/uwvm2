#!/usr/bin/env python3
"""Narrow current R5 native-TLS long-GC cold recipe; no new supervisor.

This source prepares 34 commands. Only the sole Linux keeper executes them.
Old S6e product results are deliberately excluded from R5 qualification.
"""
from decimal import Decimal
from pathlib import Path
import hashlib
import json
import os
import re
import sys
import tempfile

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
D = B / 'builds/general-gc-R5-nativeTLS-long-cold-20261002-r1'
E = B / 'evidence/general-gc-R5-nativeTLS-long-cold-20261002-r1'
BASE_COLD = B / 'builds/general-gc-R5-nativeTLS-cold-20261002-r2'
OLD = B / 'builds/general-gc-current-S6e-long-cold-20261002-r1'
OLD_E = B / 'evidence/general-gc-current-S6e-long-cold-20261002-r1'
PRODUCT = B / 'builds/debug-joint-r5-nativeTLS-current-cli-cold-20261002-r2/main'
SOURCE = B / 'candidates/debug-joint-r5-full-current-cold-20261002-r1/uwvm2'
SID = 'sha256:a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a'
PRODUCT_SHA = '4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632'
PRODUCT_BYTES = 94107376
HELPER_SHA = '30c404354f823ad11fe80d409b10d993227edc0b4b0ea55566869ab9d453ba7f'
SUPERVISOR_SHA = '3a05cc7fc0d019b929b10e4d7518d3f8373ddbffaa2124a947a8ec9999fe07a9'
OLD_COMMANDS_SHA = 'b2d912971bcc3999ed3d4ed73b85b5f34ec0c694eed3a494fe89a698371acc31'
OLD_RECEIPTS_SHA = 'f0825c9be1a24f5423088018e8655474728b282572ddf6a356ced71c532de7cc'
GENERATOR_SHA = 'eb6048d1d1fe7ad0f3264af41cc742b5a89ee17c2ca720c891ac5571fef9471e'
FAMILIES = ('mutable-struct', 'reference-cycle', 'numeric-array', 'reference-array')
PHASES = ('allocate', 'mutate')
ITERATIONS = (1000000, 2000000)
POLICIES = ('instruction', 'unwind')
LD_PATH = '/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu:/home/macromodel/Documents/uwvm3-implementation/deps/usr/lib/x86_64-linux-gnu'
ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pin(path):
    path = Path(path).resolve(strict=True)
    return {'path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}


def write_atomic(path, data):
    path = Path(path)
    data = data.encode() if isinstance(data, str) else data
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name + '-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def write_json(path, value):
    write_atomic(path, json.dumps(value, indent=2) + '\n')


def helpers():
    # Reuse only reviewed product()/tools() definitions, not the old cold main.
    # The complete pinned helper is immutable and includes real RT1/host3
    # origin/dependency checks. Its old eight-fixture stage/results are unused.
    require(__debug__, 'Python -O is unsupported for reviewed helper assertions')
    path = BASE_COLD / 'closure.py'
    require(sha(path) == HELPER_SHA, 'Reviewed R5 closure helper changed')
    text = path.read_text()
    prefix, marker, _ = text.partition('\nmode=sys.argv[1]\n')
    require(bool(marker) and text.count('\nmode=sys.argv[1]\n') == 1,
            'Reviewed helper main boundary changed')
    namespace = {'__name__': '_reviewed_r5_closure_definitions'}
    exec(compile(prefix, str(path), 'exec'), namespace)
    return namespace


def fingerprint():
    # Same canonical source fingerprint algorithm as tools/ci:
    # resolved src+third-parties roots, sorted repository paths + raw SHA,
    # Finder metadata excluded, compact canonical JSON. No child process.
    entries = []
    for directory in ('src', 'third-parties'):
        root = (SOURCE / directory).resolve(strict=True)
        require(root.is_dir(), 'Source/dependency directory missing')
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.name != '.DS_Store' and not path.name.startswith('._'):
                entries.append({'path': (Path(directory) / path.relative_to(root)).as_posix(),
                                'sha256': sha(path)})
    require(bool(entries), 'Empty source fingerprint')
    payload = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    result = {'source_id': 'sha256:' + hashlib.sha256(payload).hexdigest(), 'files': entries}
    require(result['source_id'] == SID, 'Current R5 source changed')
    return result


def argv_for(family, phase, count, policy):
    name = f'{family}-{phase}-{count}'
    return ['env', '-u', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
            'LD_LIBRARY_PATH=' + LD_PATH, 'RAYON_NUM_THREADS=1', str(PRODUCT),
            '-Rcc', 'jit', '-Rcm', 'full', '-Rllvm-full-policy', 'pb-o3',
            '-Rllvm-call-stack', policy, '-Rllvm-exception-dispatch', 'auto',
            '-Rllvm-cache-path', 'disable', '-Rct', '0', '-WFE-gc',
            '--wasm-feature-enable-reference-types',
            '--wasm-feature-enable-function-references',
            '--log-verbose', '-Rclog', 'err', '--run', str(OLD / 'fixtures' / (name + '.wasm'))]


def commands():
    result = [['input-before', ['python3', str(D / 'closure.py'), 'before']]]
    for family in FAMILIES:
        for phase in PHASES:
            for count in ITERATIONS:
                for policy in POLICIES:
                    result.append([f'{family}-{phase}-{count}-ordinary-{policy}',
                                   argv_for(family, phase, count, policy)])
    result.append(['input-after', ['python3', str(D / 'closure.py'), 'after']])
    require(len(result) == 34 and len({v[0] for v in result}) == 34, 'Cold command cardinality')
    return result


def retired_receipt(row, label, argv, command_file, log_path, *, current):
    require(row['label'] == label and row['passed'] is True and row['returncode'] == 0,
            'Stage did not actually pass: ' + label)
    require(row['argv'] == argv and
            row['argv_sha256'] == hashlib.sha256(json.dumps(argv).encode()).hexdigest() and
            row['command_file_sha256'] == sha(command_file), 'Actual command changed: ' + label)
    require(row['memory_max_bytes'] == 64 << 30 and row['swap_max_bytes'] == 0,
            'Stage resource bounds: ' + label)
    require(row['remaining_roster'] == [row['init']['pid']] and row['retirement'] and
            all(v['pidfd_readable'] is True for v in row['retirement']),
            'Stage retirement incomplete: ' + label)
    require(all(row['memory_events_before'][k] == row['memory_events_after'][k]
                for k in ('oom', 'oom_kill', 'oom_group_kill')), 'Stage OOM: ' + label)
    require(sha(log_path) == row['log_sha256'], 'Stage log changed: ' + label)
    if current:
        require(row['supervisor_sha256'] == SUPERVISOR_SHA, 'Current supervisor changed')
        require(row['source_root'] == str(SOURCE), 'Current source owner mismatch')
        require(row['Popen_root_pidfd_retirement']['pidfd_readable'] is True and
                row['Popen_root_pidfd_retirement']['actual_reaped_returncode'] == 0,
                'Current root retirement missing')
        witness = row['actual_stopped_child_environment']
        require(witness['capture_presence'] == {'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT': False},
                'Actual stopped guest still has capture environment')
    return row


def oracle_fixtures():
    # Only old official validate, WAT roundtrip, copying run/start are admitted.
    # No lookup of old "-ordinary-unwind" row or old product qualified flag.
    require(sha(OLD / 'commands.json') == OLD_COMMANDS_SHA and
            sha(OLD_E / 'receipts.json') == OLD_RECEIPTS_SHA, 'Old oracle corpus changed')
    require(sha(OLD / 'generate_general_gc.py') == GENERATOR_SHA, 'Long generator changed')
    rows = json.loads((OLD_E / 'receipts.json').read_text())
    by_label = {r['label']: r for r in rows}
    require(len(by_label) == len(rows), 'Duplicate old oracle receipt')
    actual_commands = dict(json.loads((OLD / 'commands.json').read_text()))
    result = {}
    for family in FAMILIES:
        for phase in PHASES:
            for count in ITERATIONS:
                name = f'{family}-{phase}-{count}'
                root = OLD / 'fixtures'
                meta, wasm, wat, roundtrip = [root / (name + suffix)
                                              for suffix in ('.json', '.wasm', '.wat', '.roundtrip.wasm')]
                manifest = json.loads(meta.read_text())
                require(manifest['schema'] == 'uwvm-general-gc-fixture-v1' and
                        (manifest['family'], manifest['phase'], manifest['iterations']) ==
                        (family, phase, count), 'Long fixture manifest mismatch')
                require(manifest['root_ring'] == 1024 and
                        manifest['expected']['final_root_group_count'] == 1024 and
                        manifest['compact_numeric_eligible'] is False, 'Long root/layout contract')
                require(sha(wasm) == manifest['wasm_sha256'] and
                        wasm.stat().st_size == manifest['wasm_bytes'] and
                        sha(wat) == manifest['wat_sha256'] and
                        wasm.read_bytes() == roundtrip.read_bytes(), 'Long fixture bytes changed')
                logs = {}
                for suffix in ('validate', 'roundtrip', 'wasmtime-run', 'wasmtime-start'):
                    label = name + '-' + suffix
                    row = retired_receipt(by_label[label], label, actual_commands[label],
                                          OLD / 'commands.json', OLD_E / (label + '.log'), current=False)
                    require(str(wasm) in row['argv'] or str(wat) in row['argv'],
                            'Old oracle did not use this fixture')
                    logs[suffix] = {'log': pin(OLD_E / (label + '.log')),
                                    'actual_argv': row['argv'], 'returncode': row['returncode']}
                wt_argv = logs['wasmtime-run']['actual_argv']
                require(wt_argv[wt_argv.index('-C') + 1] == 'cache=n,collector=copying' and
                        wt_argv[wt_argv.index('--invoke') + 1] == 'run',
                        'Wasmtime exact copying/run contract changed')
                words = (OLD_E / (name + '-wasmtime-run.log')).read_text().split()
                numeric = [int(word) for word in words if re.fullmatch(r'-?[0-9]+', word)]
                require(len(numeric) == 1 and numeric[0] & 0xffffffff ==
                        manifest['expected']['return_checksum_u32'], 'Wasmtime observable checksum')
                result[name] = {'manifest': pin(meta), 'wasm': pin(wasm), 'wat': pin(wat),
                                'roundtrip': pin(roundtrip), 'expected': manifest['expected'],
                                'required_features': manifest['required_features'],
                                'table_root_slots': manifest['table_root_slots'],
                                'old_official_and_copying_only': logs,
                                'old_S6e_product_used_for_R5_qualification': False}
    return result


def bindings():
    namespace = helpers()
    product = namespace['product']()
    require(product['external_source_id'] == SID and
            product['product']['sha256'] == PRODUCT_SHA and
            product['product']['bytes'] == PRODUCT_BYTES, 'Wrong current R5 product')
    require(sha(BASE_COLD / 'supervisor.py') == SUPERVISOR_SHA, 'Reviewed supervisor changed')
    require(sha(D / 'supervisor.py') == SUPERVISOR_SHA, 'Derived supervisor is not unchanged')
    require(json.loads((D / 'commands.json').read_text()) == commands(), 'Exact new argv changed')
    return {'product': product, 'fixtures': oracle_fixtures(), 'tools': namespace['tools'](),
            'reviewed_R5_closure_helper': pin(BASE_COLD / 'closure.py'),
            'supervisor': pin(D / 'supervisor.py'), 'helper': pin(D / 'closure.py'),
            'commands': pin(D / 'commands.json'), 'old_oracle_receipts': pin(OLD_E / 'receipts.json'),
            'old_oracle_commands': pin(OLD / 'commands.json')}


def parse_log(text, expected, phase):
    text = ANSI.sub('', text)
    lines = [line for line in text.splitlines() if '[gc-managed]' in line]
    require(len(lines) == 1, 'Exactly one actual managed-GC metrics record required')
    pairs = dict(re.findall(r'([a-z_]+)=(true|false|[0-9]+)', lines[0]))
    metrics = {key: value == 'true' if value in ('true', 'false') else int(value)
               for key, value in pairs.items()}
    required = ('allocations', 'attempts', 'collections', 'reclaimed', 'disabled',
                'reason', 'roots_requested')
    require(all(key in metrics for key in required), 'Incomplete actual GC metrics')
    require(metrics['allocations'] == expected['guest_planned_allocations'] and
            metrics['roots_requested'] == 1 and metrics['disabled'] == 0,
            'Allocation/root/disabled contract failed')
    collector = (phase == 'allocate' and metrics['attempts'] > 0 and
                 metrics['collections'] > 0 and metrics['reclaimed'] > 0 and metrics['reason'] == 0)
    lookup = (phase == 'mutate' and metrics['attempts'] == 0 and
              metrics['collections'] == 0 and metrics['reclaimed'] == 0 and metrics['reason'] == 2)
    require(collector or lookup, 'Allocate collector / mutation phase-pending contract failed')
    require(len(re.findall(r'\[llvm-jit-full\] owning-source=yes pending-plan=native '
                           r'object-cache=disabled body-fallback=no(?: |$)', text)) == 1,
            'Actual owning native/no-fallback/cache-disabled witness required')
    timer = {}
    for label, key in (('Total WASM execution time', 'wasm_execution_ns'),
                       ('Total process time', 'reported_process_ns')):
        values = re.findall(re.escape(label) + r':\s*([0-9]+(?:\.[0-9]+)?)s\.', text)
        require(len(values) == 1, 'Actual ' + label + ' missing/duplicate')
        value = Decimal(values[0]) * 1000000000
        require(value > 0 and value == value.to_integral_value(), 'Timer precision/positivity')
        timer[key] = int(value)
    require(timer['reported_process_ns'] >= timer['wasm_execution_ns'], 'Timer scope ordering')
    return {'actual_metrics': metrics, 'actual_metrics_line': lines[0],
            'collector_qualified': bool(collector), 'field_lookup_qualified': bool(lookup),
            'internal_timers': timer,
            'timer_scope': 'Wasm execution excludes startup/JIT; includes setup, all GC/fields and checksum. Process timer is separate. Neither is pure collector ROI.'}


def prepare():
    require(not D.exists() and not E.exists(), 'New attempt paths must not already exist')
    namespace = helpers()
    product = namespace['product']()
    require(product['product']['sha256'] == PRODUCT_SHA and product['product']['bytes'] == PRODUCT_BYTES,
            'Current product pin mismatch')
    require(sha(BASE_COLD / 'supervisor.py') == SUPERVISOR_SHA, 'Supervisor source changed')
    # Check the reused corpus before creating an executable new recipe.
    oracle_fixtures()
    D.mkdir(mode=0o700)
    write_atomic(D / 'closure.py', Path(__file__).read_bytes())
    write_atomic(D / 'supervisor.py', (BASE_COLD / 'supervisor.py').read_bytes())
    write_json(D / 'commands.json', commands())
    write_json(D / 'plan.json', {
        'scope': 'planned R5 nativeTLS production long cold only; not yet executed or passed',
        'cold_result': 'pending actual 32 owned product executions',
        'product': product, 'iterations': list(ITERATIONS), 'policies': list(POLICIES),
        'families': list(FAMILIES), 'phases': list(PHASES), 'command_count': 34,
        'official_and_Wasmtime_oracle_source': str(OLD),
        'old_S6e_product_cold_is_R5_qualification': False,
        'supervisor_sha256': SUPERVISOR_SHA, 'helper_sha256': sha(D / 'closure.py'),
        'capture_compiled_support': 1,
        'capture_runtime_policy': 'parent strips inherited key; actual stopped child witness required; argv env -u',
        'cache': 'disable', 'collection_mutation_contracts': 'allocation positive real collections/reclaimed reason0; mutation no-attempt reason2 is field-lookup only',
        'cold_resource_contract': 'unchanged reviewed supervisor: 64GiB/swap0, 56GiB owned RSS, E16-31, PIDFD ownership/retirement, no concurrent build/VM',
        'temperature': 'observation only; no rejection or pair gate',
        'formal_acceptance': False, 'measured_performance': False,
        'later_measurement_families': ['P0 unprofiled wait4 + internal Wasm timer',
                                      'P0 single-TID hardware wholeguest cycles/instructions exact enabled/running',
                                      'independent VTune HW Hotspots/uarch with actual target filters/MUX']})
    print('prepared pending 34-command R5 long cold recipe: ' + str(D))


def before():
    require(not (D / 'closure-before.json').exists(), 'Do not overwrite original before receipt')
    source = fingerprint()
    approved_source = json.loads((BASE_COLD / 'source-before.json').read_text())
    require(source == approved_source, 'Current R5 source differs from actual approved baseline')
    write_json(D / 'source-before.json', source)
    write_json(D / 'closure-before.json', {'scope': 'actual before binding; long guest executions still pending',
                                         'bindings': bindings(), 'source_before': pin(D / 'source-before.json')})
    print('current R5 product/RT1/host3 and reused exact-byte official oracles bound; long product pending')


def after():
    original = json.loads((D / 'closure-before.json').read_text())
    require(bindings() == original['bindings'], 'Actual input bindings changed during long cold')
    source = fingerprint()
    require(source == json.loads((D / 'source-before.json').read_text()), 'Source changed during cold')
    write_json(D / 'source-after.json', source)
    rows = json.loads((E / 'receipts.json').read_text())
    by_label = {r['label']: r for r in rows}
    require(len(by_label) == len(rows), 'Duplicate current actual receipt')
    actual_commands = dict(commands())
    results = []
    for family in FAMILIES:
        for phase in PHASES:
            for count in ITERATIONS:
                fixture = original['bindings']['fixtures'][f'{family}-{phase}-{count}']
                for policy in POLICIES:
                    label = f'{family}-{phase}-{count}-ordinary-{policy}'
                    log = E / (label + '.log')
                    receipt = retired_receipt(by_label[label], label, actual_commands[label],
                                               D / 'commands.json', log, current=True)
                    semantic = parse_log(log.read_text(), fixture['expected'], phase)
                    results.append({'family': family, 'phase': phase, 'iterations': count,
                                    'policy': policy, 'actual_argv': receipt['argv'],
                                    'receipt_label': label, 'log': pin(log),
                                    'fixture': fixture['wasm'], 'passed_semantics': True,
                                    'actual_stopped_capture_environment': receipt['actual_stopped_child_environment'],
                                    **semantic})
    require(len(results) == 32, 'Actual long cold cardinality')
    write_json(D / 'summary.json', {
        'scope': 'current ordinary R5 nativeTLS production long cold; actual 32 product runs. No ROS, perf, industry ranking or whole-collector release claim.',
        'passed_semantics': True, 'rows': results,
        'all_allocation_collector_qualified': all(r['collector_qualified'] for r in results if r['phase'] == 'allocate'),
        'all_mutation_field_lookup_qualified': all(r['field_lookup_qualified'] for r in results if r['phase'] == 'mutate'),
        'source_product_transitive_dependencies_tools_fixtures_before_after_equal': True,
        'source_id_external': SID, 'product_sha256': PRODUCT_SHA,
        'before_sha256': sha(D / 'closure-before.json'),
        'old_S6e_product_used_for_R5_qualification': False,
        'formal_acceptance': False, 'performance_qualified': False})
    print('actual R5 nativeTLS long cold semantics/metrics complete: 32 product rows; no performance claim')


def main():
    require(len(sys.argv) == 2 and sys.argv[1] in ('prepare', 'before', 'after'), 'Usage: SCRIPT prepare|before|after')
    {'prepare': prepare, 'before': before, 'after': after}[sys.argv[1]]()


if __name__ == '__main__':
    main()
