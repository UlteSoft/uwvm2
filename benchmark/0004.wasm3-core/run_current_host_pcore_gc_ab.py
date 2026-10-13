#!/usr/bin/env python3
"""Separate four-build ABBA binding for the frozen host hardware protocol.

Only the Linux keeper executes after reviewed actual build/cold receipts exist.
No compilation, administration, software event, sampling or cgroup mutation.
The frozen host/container runners and their raw evidence are never changed.
"""
import argparse, decimal, hashlib, json, os, pathlib, re, resource, signal
import shlex, statistics, types

P = pathlib.Path
HOST_SHA = '673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5'
HW_SHA = '266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8'
BASE_SHA = '0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89'
AUX_SHA = '59fcb19664f87fa3447a6c9cb8a117885f42303e129de6d2d3756cf8a953b4f2'
KEYS = ('ordinary_A', 'ordinary_B', 'ros_A', 'ros_B')
ORDER = ('ordinary_A', 'ordinary_B', 'ordinary_B', 'ordinary_A',
         'ros_A', 'ros_B', 'ros_B', 'ros_A')
FIXTURE = 'gc-allocation-ring-512000000'
FIXTURE_SHA = '4344aec55a95e51d86433ab6d57305528866f18583947a1def97e81b69d26d79'
RSP_SHA = '08974aa924283cd7912e7ee904d29da91669d3f87df3455a54f7ce73d640930a'
GC_DIFF = {
    'src/uwvm2/uwvm/runtime/storage/gc_object.h': (
        '209de02c372a7c082be0aada2760699b6a543631fa9e280b4263ebae528ea456',
        '55cb0d40fb264ba9b825f551d2de16c1f79b0cc9db2eb08de092651ef29ee036'),
    'src/uwvm2/uwvm/runtime/storage/compact_numeric/descriptor.h': (
        'aa290fd3ba2ed501996594d73f64967f0224ec902d0ba86cba77c171ba3c9894',
        '9a770293c14aa2fd7406af8dff5b4b3c0d6d34a4f9511b3f0b2b546872dffb62'),
}
A_SOURCE_IDS = {
    'ordinary': 'sha256:2acdc68e84a815f2bb8e046902f3170d90e0dc60fd999fa669e99cce74f47c2c',
    'ros': 'sha256:a841ae794f515d64f545026699000fc2a74ef97eaf507732aed5d3518a5fbdb8',
}
EXPERIMENTS = ('COMPACT_NUMERIC', 'MANAGED_NUMERIC_PAGE', 'SEALED_COMPACT_CURSOR',
               'SEALED_LOCAL_TABLE', 'PENDING_NUMERIC_FUSED_CATCH', 'PACKED_NUMERIC_ARRAYS')
COMBINES = ('COMBINE_OPS', 'HEAVY_COMBINE_OPS', 'EXTRA_HEAVY_COMBINE_OPS',
            'DELAY_LOCAL_SOFT', 'DELAY_LOCAL_HEAVY')
POLICY_ARGV = ['-Rllvm-full-policy', 'pb-o3',
               '-Rllvm-call-stack', 'unwind', '-Rllvm-exception-dispatch', 'auto',
               '-Rllvm-cache-path', 'disable', '-Rct', '0', '-WFE-gc', '-Rclog', 'err', '--run']

def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)

def sha(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None and value != '0' * 64

def absolute(value):
    return isinstance(value, str) and P(value).is_absolute() and '..' not in P(value).parts

def file_pin(pin):
    require(isinstance(pin, dict) and absolute(pin.get('path')) and sha(pin.get('sha256')), 'Incomplete actual file pin')

def validate_plan(plan):
    """Pure input validation; every generation is bound, never inferred."""
    require(isinstance(plan, dict) and plan.get('schema') == 'uwvm-host-pcore-gc-ab-plan-v1', 'Wrong four-build plan schema')
    require(plan.get('execute_ready') is True, 'Actual four-build/cold bindings are not ready')
    require(plan.get('execution_order') == list(ORDER), 'Only the reviewed eight-row ABBA order is accepted')
    require(isinstance(plan.get('products'), dict) and set(plan['products']) == set(KEYS), 'Expected exactly four actual builds')
    require(isinstance(plan.get('fixtures'), dict) and set(plan['fixtures']) == {FIXTURE}, 'Expected one immutable 512M fixture')
    fixture = plan['fixtures'][FIXTURE]
    file_pin(fixture)
    require(fixture['sha256'] == FIXTURE_SHA, 'Different workload bytes')
    file_pin(plan.get('compiler'))
    file_pin(plan.get('linker'))
    require(plan['linker']['path'] == str(P(plan['compiler']['path']).with_name('ld.lld')), 'Expected actual clang sibling ld.lld binding')
    file_pin(plan.get('wasmtime'))  # Preserved oracle pin; never executed here.
    dependencies = plan.get('build_dependency_files')
    require(isinstance(dependencies, list) and dependencies, 'Actual compiler/link-input closure is missing')
    for pin in dependencies:
        file_pin(pin)
    require(len({pin['path'] for pin in dependencies}) == len(dependencies), 'Duplicate build-dependency path')
    depmap = {pin['path']: pin['sha256'] for pin in dependencies}
    require(all(depmap.get(plan[key]['path']) == plan[key]['sha256'] for key in ('compiler', 'linker')), 'Compiler/linker absent from actual closure')
    identities = {}
    for key, pin in plan['products'].items():
        require(isinstance(pin, dict), 'Missing actual build object')
        require(all(absolute(pin.get(k)) for k in ('source', 'runtime', 'binary', 'build_json')), 'Missing actual build paths')
        require(isinstance(pin.get('source_id'), str) and pin['source_id'].startswith('sha256:') and sha(pin['source_id'][7:]), 'Missing actual canonical source ID')
        require(all(sha(pin.get(k)) for k in ('runtime_sha256', 'binary_sha256', 'build_json_sha256')), 'Missing actual build hashes')
        require(isinstance(pin.get('commands'), dict) and set(pin['commands']) == {'runtime', 'cli'}, 'Actual runtime/CLI sidecars required')
        for step, command in pin['commands'].items():
            file_pin(command)
            argv = command.get('argv')
            require(isinstance(argv, list) and all(isinstance(v, str) for v in argv) and argv and argv[0] == plan['compiler']['path'], 'Invalid actual compiler argv')
            require(argv.count('-o') == 1 and argv[argv.index('-o') + 1:] == [pin['runtime' if step == 'runtime' else 'binary']], 'Unexpected compiler output binding')
            source_define = '-DUWVM2_BUILD_SOURCE_ID=u8"' + pin['source_id'] + '"'
            require(argv.count(source_define) == 1 and sum(v.startswith('-DUWVM2_BUILD_SOURCE_ID') for v in argv) == 1, 'Embedded source ID differs from built source')
            require(not any(v in ('-D', '-U', '-Xclang', '-Xpreprocessor', '-Xlinker') or
                            v.startswith(('-U', '-Wp,', '-B', '--ld-path=')) or
                            v.startswith('-fuse-ld=') and v != '-fuse-ld=lld' for v in argv), 'Compiler overrides or forwards the declared macro/linker profile')
            for macro in EXPERIMENTS:
                prefix = '-DUWVM_EXPERIMENTAL_' + macro
                require([v for v in argv if v == prefix or v.startswith(prefix + '=')] == [prefix + '=1'], 'Experimental macro differs/missing/duplicated')
            require(all(v in ['-DUWVM_EXPERIMENTAL_' + macro + '=1' for macro in EXPERIMENTS]
                        for v in argv if v.startswith('-DUWVM_EXPERIMENTAL_')), 'Unreviewed extra experiment changes the six-macro profile')
            for macro in COMBINES:
                prefix = '-DUWVM_ENABLE_UWVM_INT_' + macro
                require([v for v in argv if v == prefix or v.startswith(prefix + '=')] in ([prefix], [prefix + '=1']), 'Combine/delay macro differs/missing/duplicated')
            require([v for v in argv if re.fullmatch(r'-O(?:[0-3szg]|fast)', v)] == ['-O3'] and '-DUWVM_USE_LLVM_JIT' in argv, 'Expected actual full-JIT O3 compiler profile')
            if step == 'runtime':
                require('-c' in argv, 'Runtime is not independently compiled')
            else:
                require(argv.count(pin['runtime']) == 1 and '-fuse-ld=lld' in argv, 'CLI did not link its own pinned runtime')
            for arg in argv:
                if arg.startswith('@'):
                    require(absolute(arg[1:]) and depmap.get(arg[1:]) == RSP_SHA, 'Response file is not the exact reviewed actual r10 recipe')
        cold = pin.get('cold')
        require(isinstance(cold, dict), 'Missing actual cold receipt binding')
        file_pin(cold)
        require(type(cold.get('row_index')) is int and cold['row_index'] >= 0, 'Invalid actual cold row selection')
        identities[key] = (pin['source'], pin['runtime'], pin['binary'])
    for repo in ('ordinary', 'ros'):
        a, b = (plan['products'][repo + '_' + generation] for generation in ('A', 'B'))
        require(a['source_id'] == A_SOURCE_IDS[repo], 'A is not the actual preserved Linux r10 source')
        require(all(a[field] != b[field] for field in ('source', 'runtime', 'binary')), 'A/B reuse a source/runtime/ELF path')
        require(a['source_id'] != b['source_id'], 'A/B have the same built source ID')
        for step in ('runtime', 'cli'):
            require(normalized_compiler_argv(a, step) == normalized_compiler_argv(b, step), 'A/B compiler/ABI/dependency/profile argv differ')
    require(len({row[1] for row in identities.values()}) == 4 and len({row[2] for row in identities.values()}) == 4, 'Four runtime/ELF paths are not independent')
    require(isinstance(plan.get('commands'), list) and len(plan['commands']) == 4, 'Expected exactly four bound guest commands')
    desired = {key + '/unwind/auto' for key in KEYS}
    require({item.get('profile') for item in plan['commands']} == desired, 'Incomplete/duplicate four-build command set')
    for item in plan['commands']:
        key = item['profile'].split('/')[0]
        expected = ['taskset', '-c', '0', plan['products'][key]['binary'],
                    *(['-Rcc', 'jit', '-Rcm', 'full'] if key.startswith('ordinary_') else ['-Raot']),
                    *POLICY_ARGV, fixture['path']]
        require(item.get('fixture') == FIXTURE and item.get('argv') == expected, 'Actual guest ABI/policy/fixture argv changed')
        require('diagnostic_argv' not in item or item['diagnostic_argv'] == expected, 'Hidden alternate diagnostic command')

def normalized_compiler_argv(pin, step):
    """Derived comparison only; the original sidecar argv stays unchanged."""
    def normalize(arg):
        if arg.startswith('-DUWVM2_BUILD_SOURCE_ID='):
            return '<EMBEDDED_SOURCE_ID>'
        if arg == pin['runtime']:
            return '<OWN_RUNTIME>'
        if arg == pin['binary']:
            return '<OWN_CLI>'
        if arg == pin['source'] or arg.startswith(pin['source'] + '/'):
            return '<OWN_SOURCE>' + arg[len(pin['source']):]
        return arg
    return [normalize(arg) for arg in pin['commands'][step]['argv']]

def exact_gc(receipt):
    managed, sealed = receipt.get('gc-managed', {}), receipt.get('gc-sealed-experimental', {})
    return (receipt.get('passed') is True and receipt.get('iterations') == 512000000 and
            receipt.get('expected_checksum') == 1687964949 and
            managed.get('allocations') == 512000000 and managed.get('collections') == 125000 and
            managed.get('reclaimed') == 511998975 and sealed.get('committed_slots') == 512000000 and
            sealed.get('remaining') == 0 and all(managed.get(k) == 0 for k in
                ('peer_skips', 'policy_skips', 'heap_rejections', 'population_rejections', 'disabled', 'reason')))

def validate_build_receipt(pin, receipt, sidecars, compiler_sha):
    require(receipt.get('source') == pin['source'] and receipt.get('source_id') == receipt.get('source_id_after') == pin['source_id'], 'Build source provenance did not close')
    require(receipt.get('compiler_sha256') == compiler_sha and receipt.get('binary_sha256') == pin['binary_sha256'], 'Build compiler/ELF provenance differs')
    require(receipt.get('fresh_runtime') is True and receipt.get('old_object_reused') is False and receipt.get('passed') is True, 'Actual fresh runtime build is not green')
    require(receipt.get('memory_max') == str(64 << 30) and receipt.get('swap_max') == '0' and receipt.get('cpuset') == '0,2,4,6,16-31', 'Build resource receipt differs')
    steps = receipt.get('steps', [])
    require(isinstance(steps, list) and len(steps) == 2 and {step.get('step') for step in steps} == {'runtime', 'cli'} and all(type(step.get('returncode')) is int and step['returncode'] == 0 for step in steps), 'Actual runtime/CLI steps were not successful')
    before, after = receipt.get('events_before'), receipt.get('events_after')
    require(isinstance(before, str) and isinstance(after, str), 'Missing build memory-event receipt')
    before, after = (dict(re.findall(r'^([a-z_]+) ([0-9]+)$', text, re.M)) for text in (before, after))
    require(all(key in before and key in after and before[key] == after[key] for key in ('oom', 'oom_kill')), 'Build OOM-event receipt changed')
    for step in ('runtime', 'cli'):
        require(sidecars[step] == pin['commands'][step]['argv'], 'Actual command sidecar disagrees with plan argv')

def validate_cold_row(pin, cold, key, expected_argv):
    repo = key.rsplit('_', 1)[0]
    require(isinstance(cold, dict) and cold.get('passed') is True and cold.get('source_ids', {}).get(repo) == pin['source_id'], 'Actual cold source/self-check did not pass')
    require(isinstance(cold.get('rows'), list) and pin['cold']['row_index'] < len(cold['rows']), 'Cold row out of bounds')
    row = cold['rows'][pin['cold']['row_index']]
    require(row.get('exit') == 0 and type(row.get('exit')) is int and row.get('passed') is True and row.get('timed') is False, 'Selected cold row was not an untimed successful execution')
    require(row.get('fixture') == FIXTURE and row.get('profile') == repo + '/unwind/auto', 'Cold policy/fixture differs')
    require(row.get('argv') == expected_argv, 'Cold command differs from the bound executable/policy')
    file_pin({'path': row.get('log'), 'sha256': row.get('log_sha256')})
    return row

def response_receipt(blob, dependencies):
    require(hashlib.sha256(blob).hexdigest() == RSP_SHA, 'Actual compiler response bytes differ from reviewed r10 recipe')
    tokens = shlex.split(blob.decode('utf-8'), posix=True)
    deps = {pin['path'] for pin in dependencies}
    require(tokens, 'Empty actual response recipe')
    for index, token in enumerate(tokens):
        require(not token.startswith(('@', '-U', '-B', '-Wp,', '--ld-path=')) and
                token not in ('-D', '-Xpreprocessor', '-Xlinker'), 'Response forwards or overrides compiler profile')
        require(not token.startswith('-D') or token == '-DNDEBUG', 'Response rewrites a macro other than the reviewed NDEBUG')
        if token == '-Xclang':
            require(index + 1 < len(tokens) and tokens[index + 1] == '-fno-pch-timestamp', 'Unreviewed clang-forward response flag')
        if absolute(token):
            require(token in deps, 'Actual LLVM/link archive omitted from compile-input closure')
    return {'sha256': RSP_SHA, 'actual_tokens': tokens,
            'reviewed_non_GC_macro': '-DNDEBUG', 'reviewed_clang_forward_pair': ['-Xclang', '-fno-pch-timestamp'],
            'GC_macro_override': False, 'actual_absolute_link_inputs': [token for token in tokens if absolute(token)]}

def reviewed_source_difference(a, b):
    def index(receipt):
        files = receipt.get('files')
        require(isinstance(files, list) and files, 'Missing actual canonical source entries')
        require(all(isinstance(file, dict) and isinstance(file.get('path'), str) and
                    (file['path'].startswith('src/') or file['path'].startswith('third-parties/')) and
                    '..' not in P(file['path']).parts and sha(file.get('sha256')) for file in files), 'Malformed actual canonical source entry')
        mapping = {file['path']: file['sha256'] for file in files}
        require(len(mapping) == len(files), 'Duplicate canonical source path')
        expected = 'sha256:' + hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        require(receipt.get('source_id') == expected, 'Canonical source receipt digest did not close')
        return mapping
    af, bf = index(a), index(b)
    changed = {path for path in af.keys() | bf.keys() if af.get(path) != bf.get(path)}
    require(changed == set(GC_DIFF), 'A/B mixes source or vendor changes beyond the two reviewed GC headers')
    require(all((af.get(path), bf.get(path)) == hashes for path, hashes in GC_DIFF.items()), 'A/B GC before/after images differ from reviewed r10/r11 patch')
    return {'source_A': a['source_id'], 'source_B': b['source_id'],
            'actual_changed_files': [{'path': path, 'A_sha256': af[path], 'B_sha256': bf[path]} for path in sorted(changed)],
            'all_other_source_and_vendor_files_identical': True}

def extra_closure(host, base, origin, mapped, mapping, out, stage):
    """Re-read actual receipts/logs/compile inputs, without executing a child."""
    proofs = {}
    for file in origin['build_dependency_files']:
        path = host.host_path(file['path'], mapping)
        require(base.digest(path) == file['sha256'], 'Actual compiler/link-input closure changed')
    responses = {}
    for pin in origin['products'].values():
        for command in pin['commands'].values():
            for arg in command['argv']:
                if arg.startswith('@') and arg[1:] not in responses:
                    path = P(host.host_path(arg[1:], mapping))
                    responses[arg[1:]] = response_receipt(path.read_bytes(), origin['build_dependency_files'])
    for key in KEYS:
        raw_pin, pin = origin['products'][key], mapped['products'][key]
        build = json.loads(P(pin['build_json']).read_text())
        sidecars = {step: json.loads(P(value['path']).read_text()) for step, value in pin['commands'].items()}
        validate_build_receipt(raw_pin, build, sidecars, origin['compiler']['sha256'])
        cold_path = P(host.host_path(raw_pin['cold']['path'], mapping))
        require(base.digest(cold_path) == raw_pin['cold']['sha256'], 'Actual cold receipt changed')
        cold = json.loads(cold_path.read_text())
        raw_item = next(item for item in origin['commands'] if item['profile'].split('/')[0] == key)
        row = validate_cold_row(raw_pin, cold, key, ['taskset', '-c', '16', *base.effective_argv(raw_item)[3:]])
        log = P(host.host_path(row['log'], mapping))
        require(base.digest(log) == row['log_sha256'], 'Actual cold log changed')
        proof = base.semantic_receipt(raw_item, log, row['exit'])
        require(exact_gc(proof), 'Cold exact 512M allocation/root/collection schedule did not pass')
        proofs[key] = {'origin_receipt': str(cold_path), 'origin_receipt_sha256': raw_pin['cold']['sha256'],
                       'origin_row_index': raw_pin['cold']['row_index'], 'actual_log': str(log),
                       'actual_log_sha256': row['log_sha256'], 'reparsed_semantics': proof,
                       'normalized_compiler_argv': {step: normalized_compiler_argv(raw_pin, step) for step in ('runtime', 'cli')}}
    base.save(out / ('four-build-cold-' + stage + '.json'), proofs)
    differences = {repo: reviewed_source_difference(
        json.loads((out / (repo + '_A-source-' + stage + '.json')).read_text()),
        json.loads((out / (repo + '_B-source-' + stage + '.json')).read_text())) for repo in ('ordinary', 'ros')}
    base.save(out / ('reviewed-GC-source-diff-' + stage + '.json'), differences)
    base.save(out / ('actual-compiler-response-' + stage + '.json'), responses)

def frequency_receipt(row):
    # Frozen telemetry has wall timestamps only. Retain its entire monitor
    # envelope; do not invent a monotonic/Wasm ROI alignment.
    values = [p['scaling_cur_freq'] for p in row['during'] if type(p.get('scaling_cur_freq')) is int and p['scaling_cur_freq'] > 0]
    def percentile(q):
        position = (len(values) - 1) * q
        low = int(position)
        return values[low] + (values[min(low + 1, len(values) - 1)] - values[low]) * (position - low)
    values.sort()
    return {'samples': len(values), 'p05_khz': percentile(.05) if values else None,
            'median_khz': statistics.median(values) if values else None,
            'p95_khz': percentile(.95) if values else None,
            'minimum_required_samples': 5, 'sufficient': len(values) >= 5,
            'interval': 'Frozen 20ms monitor envelope, including perf enable/start/finalization; not exact Wasm or counter ROI.'}

def exact_original_counts(row):
    events = row['hardware_counting'].get('events', {}).values()
    counts = {}
    for event in events:
        name, count = event.get('actual_raw_event_name'), event.get('raw_counter_value')
        if name in ('cpu_core/event=0x3c/', 'cpu_core/event=0xc0/'):
            require(name not in counts, 'Duplicate actual raw event in counter receipt')
            counts[name] = count if type(count) is int and count >= 0 else None
    return counts

def count_ratio(a, b):
    return str(decimal.Decimal(a) / decimal.Decimal(b)) if type(a) is int and type(b) is int and a >= 0 and b > 0 else None

def whole_workload_metrics(row):
    counts = exact_original_counts(row)
    cycles, instructions = (counts.get(name) for name in ('cpu_core/event=0x3c/', 'cpu_core/event=0xc0/'))
    return {'cycles_per_step_including_startup_and_JIT': count_ratio(cycles, 512000000),
            'instructions_per_step_including_startup_and_JIT': count_ratio(instructions, 512000000),
            'CPI_whole_guest': count_ratio(cycles, instructions), 'IPC_whole_guest': count_ratio(instructions, cycles),
            'internal_guest_ns_per_workload_step': count_ratio(row['guest_execution_ns'], 512000000),
            'whole_wall_ns_per_workload_step': count_ratio(row['guest_wall_ns'], 512000000),
            'not_a_collector_latency': True}

def pair_receipts(rows, bindings_closed):
    pairs = []
    for repo, indices in (('ordinary', ((0, 1), (3, 2))), ('ros', ((4, 5), (7, 6)))):
        for pair, (ai, bi) in enumerate(indices, 1):
            if max(ai, bi) >= len(rows):
                continue
            a, b = rows[ai], rows[bi]
            require(a['ab_build'] == repo + '_A' and b['ab_build'] == repo + '_B', 'Aggregate order changed')
            af, bf = a['frequency'], b['frequency']
            failures = []
            if not bindings_closed:
                failures.append('Full eight-row actual bindings/order/before-after closure did not close')
            if not af['sufficient'] or not bf['sufficient']:
                failures.append('Too few actual frequency observations')
            ratio = max(af['median_khz'], bf['median_khz']) / min(af['median_khz'], bf['median_khz']) if af['median_khz'] and bf['median_khz'] else None
            if ratio is None or ratio > 1.10:
                failures.append('Whole-envelope median frequency differs by more than10percent')
            if any(r['quality_failures'] or r['hard_failures'] or not r['exact_gc_passed'] or not r['hardware_counting']['counter_quality_passed'] for r in (a, b)):
                failures.append('One row did not qualify')
            ac, bc = exact_original_counts(a), exact_original_counts(b)
            if any(set(counts) != {'cpu_core/event=0x3c/', 'cpu_core/event=0xc0/'} or
                   not all(type(value) is int and value > 0 for value in counts.values()) for counts in (ac, bc)):
                failures.append('Both original raw-event counts are not positive and complete')
            pairs.append({'repository': repo, 'pair': pair, 'a_order_index': ai, 'b_order_index': bi,
                          'evidence_family': 'whole_guest_pure_HW',
                          'original_A_counts_by_actual_raw_event': ac, 'original_B_counts_by_actual_raw_event': bc,
                          'cycles_B_over_A': count_ratio(bc.get('cpu_core/event=0x3c/'), ac.get('cpu_core/event=0x3c/')),
                          'instructions_B_over_A': count_ratio(bc.get('cpu_core/event=0xc0/'), ac.get('cpu_core/event=0xc0/')),
                          'median_frequency_ratio': ratio, 'frequency_matched_counter_pair': not failures,
                          'failures': failures, 'formal_acceptance': False,
                          'pair_effect_estimate_qualified': False,
                          'host_noise_qualification': 'Requires the separate actual P0/SMT observer; unknown activity is not quiet.',
                          'guest_time_B_over_A': count_ratio(b['guest_execution_ns'], a['guest_execution_ns']),
                          'whole_wall_B_over_A': count_ratio(b['guest_wall_ns'], a['guest_wall_ns'])})
    return pairs

def descriptive_pair_summary(pairs):
    result = {}
    for repo in ('ordinary', 'ros'):
        accepted = [pair for pair in pairs if pair['repository'] == repo and pair['frequency_matched_counter_pair']]
        cell = {'evidence_family': 'whole_guest_pure_HW', 'frequency_matched_pairs': len(accepted),
                'required_pairs': 2, 'formal_acceptance': False, 'pair_effect_estimate_qualified': False,
                'host_noise': 'Unknown until actual independent P0/SMT observer analysis; never assumed quiet.',
                'two_pairs_are_descriptive_not_a_confidence_interval': True, 'metrics': {}}
        if len(accepted) == 2:
            for metric in ('cycles_B_over_A', 'instructions_B_over_A', 'guest_time_B_over_A', 'whole_wall_B_over_A'):
                values = [decimal.Decimal(pair[metric]) for pair in accepted if pair[metric] is not None]
                if len(values) == 2 and all(value > 0 for value in values):
                    cell['metrics'][metric] = {'original_pair_ratios': [str(value) for value in values],
                        'range': [str(min(values)), str(max(values))],
                        'geometric_mean': str((values[0] * values[1]).sqrt()),
                        'rounding': 'Python Decimal default28 significant digits; no frequency normalization.'}
        result[repo] = cell
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('plan', 'admission', 'perf_libraries', 'out'):
        parser.add_argument('--' + key.replace('_', '-'), type=P, required=True)
    parser.add_argument('--execute', action='store_true')
    args = parser.parse_args()
    host_path = P(__file__).with_name('run_current_host_pcore_hw_counting.py')
    blob = host_path.read_bytes()
    require(hashlib.sha256(blob).hexdigest() == HOST_SHA, 'Frozen host runner changed')
    host = types.ModuleType('uwvm_frozen_host_hw')
    host.__file__ = str(host_path)
    exec(compile(blob, str(host_path), 'exec'), host.__dict__)
    require(host_path.read_bytes() == blob, 'Frozen host runner changed while loading')
    hw, hw_path = host.fixed_module('ab_frozen_hw', 'run_current_pcore_hw_counting.py', HW_SHA)
    base, base_path = host.fixed_module('ab_frozen_base', 'run_current_pcore_diagnostic.py', BASE_SHA)
    aux, aux_path = host.fixed_module('ab_frozen_aux', 'run_current_pcore_aux_diagnostic.py', AUX_SHA)
    inputs = {str(path): base.digest(path) for path in (args.plan, args.admission, args.perf_libraries, P(__file__), host_path, hw_path, base_path, aux_path)}
    origin = json.loads(args.plan.read_text())
    validate_plan(origin)
    admission, libraries = (json.loads(path.read_text()) for path in (args.admission, args.perf_libraries))
    host.validate_raw_inventory(admission, libraries)
    wrapper = P(admission['trusted_wrapper']['path'])
    require(base.digest(wrapper) == admission['trusted_wrapper']['sha256'], 'Actual trusted wrapper changed')
    inputs[str(wrapper)] = admission['trusted_wrapper']['sha256']
    plan = host.mapped_plan(origin, admission['host_mapping'])
    perf = P(admission['host_perf']['path']).resolve(strict=True)
    require(perf.read_bytes()[:4] == b'\x7fELF' and base.digest(perf) == admission['host_perf']['sha256'], 'Actual direct perf ELF changed')
    require(any(f['real_path'] == str(perf) and f['sha256'] == admission['host_perf']['sha256'] for f in libraries['files']), 'Perf missing from actual loader closure')
    require(any('ld-linux' in f['real_path'] for f in libraries['files']) and any('/libc.so' in f['path'] for f in libraries['files']), 'Actual loader/libc closure missing')
    require(all(base.digest(path) == digest for path, digest in inputs.items()), 'Input changed while loading')
    require(args.execute, 'Only reviewed actual bindings with keeper admission and explicit --execute can run')
    require(not args.out.exists(), 'New evidence directory required')
    args.out.mkdir(parents=True)
    for name, value in (('original-plan.json', origin), ('host-plan.json', plan), ('actual-host-admission.json', admission), ('perf-libraries.json', libraries)):
        base.save(args.out / name, value)
    for name, path in (('ab-runner.py', P(__file__)), ('frozen-host.py', host_path), ('protocol.py', hw_path), ('guard-parser.py', base_path), ('aux.py', aux_path)):
        (args.out / name).write_bytes(path.read_bytes())
    def cancel(signum, frame):
        raise KeyboardInterrupt('Controlled cancellation ' + str(signum))
    signal.signal(signal.SIGTERM, cancel)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    rows, error = [], None
    before_ok = after_ok = complete = False
    try:
        guard = host.HostGuard(base, admission)
        adapted = host.HostBase(base, guard.root)
        base.save(args.out / 'actual-host-initial-identity.json', {'controller': base.json_ident(guard.self), 'init': base.json_ident(guard.init),
                  'init_loaded_executable': guard.init_executable, 'actual_cgroup_root': str(guard.root), 'memory_events': guard.events,
                  'roster': (guard.root / 'cgroup.procs').read_text()})
        host.host_closure(base, plan, libraries, args.out, 'before')
        extra_closure(host, base, origin, plan, admission['host_mapping'], args.out, 'before')
        guard.check()
        pmu_before = host.pmu_receipt()
        base.save(args.out / 'pmu-before.json', pmu_before)
        before_ok = True
        receipts = []
        globals_copy = dict(hw.measure.__globals__, EVENT_GROUP=host.RAW_GROUP,
                            parse_stat=lambda stat, debug='': host.normalized_math(hw, stat, debug),
                            signal_owned=lambda entry, sig: host.submit_owned_signal(hw, base, guard, entry, sig, receipts))
        measure = types.FunctionType(hw.measure.__code__, globals_copy, 'four_build_host_measure')
        commands = {item['profile'].split('/')[0]: item for item in plan['commands']}
        for index, key in enumerate(ORDER):
            receipts.clear()
            row_out = args.out / ('%02d-' % index + key)
            row_out.mkdir()
            row = measure(adapted, aux, guard, commands[key], perf, row_out)
            row['owned_perf_sigint_receipts'] = list(receipts)
            debug = row_out / (row['label'] + '.perf.log')
            attributes = host.final_attributes(debug.read_text(errors='replace'), row['guest_admission']['pid']) if row['guest_admission'] else {'passed': False, 'failures': ['No admitted guest']}
            counts = row['hardware_counting']
            old_failures = list(counts['failures'])
            counts['final_guest_attributes'] = attributes
            counts['counter_quality_passed'] = counts.get('math_quality_passed', False) and attributes['passed']
            counts['failures'] = [f for f in counts['failures'] if not f.startswith('Awaiting actual final-guest')] + attributes['failures']
            completion = host.perf_completion(row)
            row['actual_perf_completion'] = completion
            counts['counter_quality_passed'] = counts['counter_quality_passed'] and completion['passed']
            counts['failures'].extend(completion['failures'])
            row['quality_failures'] = [f for f in row['quality_failures'] if f not in old_failures] + counts['failures']
            row.update(ab_order_index=index, ab_build=key, actual_build_pin=origin['products'][key],
                       exact_gc_passed=exact_gc(row['semantic_receipt']), frequency=frequency_receipt(row),
                       evidence_directory=row_out.name, input_sha256=inputs,
                       measurement_namespace='Host controller/perf/guest admitted to exact original64GiB scope')
            row['whole_workload_derived_metrics'] = whole_workload_metrics(row)
            rows.append(row)
            with (args.out / 'qualified.jsonl').open('a') as stream:
                stream.write(json.dumps(row, allow_nan=False) + '\n')
            require(not row['hard_failures'], 'Ownership/resource/deadline failure')
            require(row['exact_gc_passed'], 'Actual exact workload/root/collection schedule failed')
            require(completion['passed'], 'Actual perf submission/group/count interval did not close')
        host.host_closure(base, plan, libraries, args.out, 'after')
        extra_closure(host, base, origin, plan, admission['host_mapping'], args.out, 'after')
        guard.check()
        pmu_after = host.pmu_receipt()
        base.save(args.out / 'pmu-after.json', pmu_after)
        require(pmu_before == pmu_after, 'Actual PMU changed')
        require(all(base.digest(path) == digest for path, digest in inputs.items()), 'Frozen input/dependency changed')
        require(host.loaded_executable_identity(guard.init_proc_exe, base.digest) == guard.init_executable, 'Original init loaded ELF changed')
        after_ok = complete = True
    except BaseException as failure:
        error = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        pairs = pair_receipts(rows, before_ok and after_ok and complete and len(rows) == len(ORDER))
        base.save(args.out / 'summary.json', {'schema': 'uwvm-host-pcore-gc-four-build-abba-v1', 'input_sha256': inputs,
                  'expected_execution_order': list(ORDER), 'actual_execution_order': [row['ab_build'] for row in rows],
                  'rows': len(rows), 'complete': complete, 'closure_before_ok': before_ok, 'closure_after_ok': after_ok,
                  'execution_error': error, 'exact_gc_pass_count': sum(row['exact_gc_passed'] for row in rows),
                  'hardware_quality_pass_count': sum(row['hardware_counting']['counter_quality_passed'] for row in rows),
                  'pairs': pairs, 'descriptive_two_pair_summary': descriptive_pair_summary(pairs),
                  'formal_acceptance': False, 'temperature_policy': 'observation_only',
                  'raw_event_group': host.RAW_GROUP, 'actual_cgroup_root': admission['cgroup_sysfs_absolute_path'],
                  'limitations': ['Four actual builds and compile-input pins; no candidate source/ELF/timing inferred.',
                                  'Pure HW counts include startup/initialization/JIT; not Wasm-only or collector-only ROI.',
                                  'Frequency-matched counter pairs are conditional on separate P0/SMT noise observations.',
                                  'Two pairs are development comparisons, not a confidence interval or industry ranking.',
                                  'Each original frozen-protocol raw row/log is retained in a separate order-index directory.']})

if __name__ == '__main__':
    main()
