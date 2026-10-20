#!/usr/bin/env python3
"""Independent inert readback. No archived Python, Wasm or native code executes."""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import re
import resource
import time

DEFAULT = '/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-tinygo-sdk-resources-20261019-a6'
GROUP = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def read_json(path):
    return json.loads(Path(path).read_bytes(), object_pairs_hook=unique)


class Reader:
    def __init__(self, raw):
        self.raw, self.at = raw, 0

    def take(self, count):
        require(0 <= count <= len(self.raw) - self.at, 'bounded binary field')
        result = self.raw[self.at:self.at + count]
        self.at += count
        return result

    def byte(self):
        return self.take(1)[0]

    def uint(self):
        value = 0
        for index in range(5):
            part = self.byte()
            if index == 4:
                require(part < 16, 'u32 width')
            value |= (part & 127) << (7 * index)
            if part < 128:
                return value
        raise ValueError('unterminated u32')

    def name(self):
        size = self.uint()
        require(size <= 65536, 'bounded name')
        return self.take(size).decode('utf-8')

    def end(self):
        require(self.at == len(self.raw), 'trailing binary bytes')


def binary_imports(raw):
    source = Reader(raw)
    require(source.take(8) == b'\0asm\1\0\0\0', 'core module header')
    types, imports, custom, seen = [], [], [], set()
    scalar = {127: 'i32', 126: 'i64', 125: 'f32', 124: 'f64', 123: 'v128',
              112: 'funcref', 111: 'externref'}
    while source.at < len(raw):
        section = source.byte()
        body = Reader(source.take(source.uint()))
        if section in (1, 2):
            require(section not in seen, 'duplicate parsed section')
            seen.add(section)
        if section == 1:
            for _ in range(body.uint()):
                require(body.byte() == 96, 'actual TinyGo scalar function type')
                signature = []
                for _ in range(2):
                    count = body.uint()
                    require(count < 4096, 'bounded function signature')
                    signature.append([scalar[body.byte()] for _ in range(count)])
                types.append(signature)
            body.end()
        elif section == 2:
            for _ in range(body.uint()):
                module, name, kind = body.name(), body.name(), body.byte()
                require(kind == 0, 'actual closed TinyGo function import inventory')
                index = body.uint()
                require(index < len(types), 'actual import type index')
                imports.append({'module': module, 'name': name, 'kind': kind,
                                'params': types[index][0], 'results': types[index][1]})
            body.end()
        elif section == 0:
            custom.append(body.name())
    return imports, custom


def verify(root):
    checked, cached, result = {}, {}, {'all15_complete': False, 'full_upstream_parity': False}

    def file(path, sha=None, size=None):
        path = Path(path)
        stat = path.stat()
        identity = (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)
        if identity not in cached:
            raw = path.read_bytes()
            require(path.stat().st_mtime_ns == stat.st_mtime_ns and len(raw) == stat.st_size, 'input changed')
            cached[identity] = digest(raw)
        actual = cached[identity]
        require(sha is None or sha == actual, 'SHA mismatch: ' + str(path))
        require(size is None or size == stat.st_size, 'size mismatch: ' + str(path))
        checked[str(path)] = {'bytes': stat.st_size, 'sha256': actual}
        return actual

    def record(name, status, folder=None, owner=False):
        path = root / name
        file(path)
        data = read_json(path)
        require(data['status'] == status, 'incomplete or failed: ' + name)
        if 'after' in data and 'before' in data:
            require(data['after']['memory_events'] == data['before']['memory_events'], 'memory event bracket')
        if folder:
            for row in data['commands']:
                require(row['passed'] and row['complete_output'], 'failed/incomplete command')
                require(row['exit'] == row['expected_exit'], 'exact command exit')
                for stream in ('stdout', 'stderr'):
                    file(root / folder / row['label'] / (stream + '.log'),
                         row[stream + '_sha256'], row[stream + '_bytes'])
                if owner:
                    own = row['ownership']
                    require(own['closed'] and not own['signals'] and not own['retirement_errors'], 'owned actor retirement')
                    require(all(not m['pidfd_retained'] for m in own['members']), 'retained actor pidfd')
        return data

    producer = record('producer-result-r5.json', 'ACTUAL_TINYGO_30_PRODUCERS_AND_NATIVE_MODELS_PASSED', 'build-runs-r5')
    require(len(producer['artifacts']) == 30 and len(producer['commands']) == 82, 'complete producer matrix')
    required = set(itertools.product(('ros1', 'ros2'), ('0', '1', '2', 's', 'z'), ('resources', 'coverage', 'sdk-tests')))
    require({(a['backend'], a['profile'], a['kind']) for a in producer['artifacts']} == required, 'producer product Cartesian matrix')
    schema = read_json(root / 'source-r3/sdk/abi/abi-v1.json')
    expected = {module: {r['name']: ([p['type'] for p in r['params']], r['results'])
                         for r in data['functions']} for module, data in schema['modules'].items()}
    parsed = []
    for artifact in producer['artifacts']:
        path = Path(artifact['path'])
        file(path, artifact['sha256'], artifact['bytes'])
        imports, custom = binary_imports(path.read_bytes())
        observed = set()
        for row in imports:
            if row['module'].startswith('uwvm_ros'):
                require(row['module'] == 'uwvm_' + artifact['backend'] + '_v1', 'actual backend import')
                require((row['params'], row['results']) == expected[row['module']][row['name']], 'actual ROS import signature')
                require(row['name'] not in observed, 'duplicate ROS import')
                observed.add(row['name'])
            else:
                require(row['module'] == 'wasi_snapshot_preview1', 'unexpected non-ROS import module')
        if artifact['kind'] == 'coverage':
            require(observed == set(expected['uwvm_' + artifact['backend'] + '_v1']), 'complete emitted ABI inventory')
        if artifact['import_report']:
            file(artifact['import_report'])
            inspection = read_json(artifact['import_report'])
            require(inspection['imports'] == imports and inspection['sha256'] == artifact['sha256'], 'independent binary/report agreement')
        parsed.append({'backend': artifact['backend'], 'profile': artifact['profile'], 'kind': artifact['kind'],
                       'imports': imports, 'custom_sections': custom, 'sha256': artifact['sha256']})
    result['binary_inspections'] = parsed
    for row in read_json(root / 'source-formatted-r5.json')['members']:
        file(root / 'source-r3' / row['path'], row['sha256'], row['bytes'])
    ordinary = record('ordinary-result-r2.json', 'ACTUAL_TINYGO40_ORDINARY_WORKFLOWS_AND60_ARGUMENT_REFUSALS_PASSED', 'ordinary-runs-r2')
    require(len(ordinary['commands']) == 102 and len(ordinary['target_workflows']) == 40, 'complete ordinary matrix')
    top, nested = 0, 0
    for row in ordinary['target_workflows']:
        out = (root / 'ordinary-runs-r2' / row['label'] / 'stdout.log').read_text()
        if row['kind'] == 'resources':
            require(out == 'PASS_TINYGO_ROS_RESOURCES_DEFAULT_DENIED\n', 'exact default denial output')
        else:
            require(out.endswith('\nPASS\n') and 'FAIL' not in out and 'TestExecutorResetsGuardAfterCallbackPanic' not in out, 'supported Wasm SDK tests')
            count = sum(line.startswith('--- PASS:') for line in out.splitlines())
            require(count == 25, 'all supported top-level SDK tests')
            top += count
            nested += sum(line.lstrip().startswith('--- PASS:') and not line.startswith('--- PASS:') for line in out.splitlines())
    for control in ordinary['panic_trap_controls']:
        label = control['label']
        err = (root / 'ordinary-runs-r2' / label / 'stderr.log').read_bytes()
        require(b'catch unreachable' in err and b'TestExecutorResetsGuardAfterCallbackPanic' in err, 'actual expected trap source')
    result.update(ordinary_workflows=40, argument_refusals=60, expected_panic_traps=2, wasm_SDK_top_level_tests=top, wasm_SDK_subtests=nested)
    startup = record('startup-result-r2.json', 'ACTUAL_TINYGO_16_STARTUP_PERMISSION_OBSERVATIONS_RECORDED')
    require(len(startup['observations']) == 16 and all(r['exit'] == 0 and r['application_default_denial_reached'] for r in startup['observations']), 'actual startup subtraction outcomes')
    for row in startup['commands']:
        for stream in ('stdout', 'stderr'):
            file(root / 'startup-runs-r2' / row['label'] / (stream + '.log'), row[stream + '_sha256'], row[stream + '_bytes'])
    native = record('native-full-r1-result.json', 'ACTUAL_TINYGO_NATIVE_RESOURCES_AND_SELECTED_WRONG_VALUES_PASSED', 'native-full-r1', True)
    require(len(native['cases']) == 54 and len(native['commands']) == 216, 'complete native actor matrix')
    positives = [c for c in native['cases'] if c['control'] == 'positive']
    require({(c['runtime_mode'], c['producer_profile'], c['lesson']) for c in positives} == set(itertools.product(('release', 'debug'), ('0', '1', '2', 's', 'z'), ('subscribe', 'trigger', 'set-bool', 'server', 'parameter'))), 'native positive Cartesian matrix')
    controls = [c for c in native['cases'] if c['control'] == 'wrong-value']
    require(len(controls) == 4 and {c['lesson'] for c in controls} == {'subscribe', 'trigger', 'set-bool', 'parameter'}, 'actual native wrong-value controls')
    values = {'subscribe': 'bounded native TinyGo telemetry', 'trigger': 'native-tinygo-ready', 'set-bool': 'native-tinygo-enabled', 'server': 'bounded tinygo wasm service', 'parameter': 'bounded native TinyGo parameter'}
    wrong = {'subscribe': 'unexpected native TinyGo telemetry', 'trigger': 'unexpected-native-tinygo-ready', 'parameter': 'unexpected native TinyGo parameter'}
    for index, case in enumerate(native['cases']):
        protocol = []
        for line in (root / 'native-full-r1' / ('c' + str(index) + '-peer') / 'stdout.log').read_text().splitlines():
            phase, rest = line.split('_NATIVE_TINYGO_RESOURCE ', 1)
            protocol.append({'phase': phase, **dict(field.split('=', 1) for field in rest.split(' '))})
        require(protocol == case['native_protocol'], 'raw native phases match receipt')
        last, mode = protocol[-1], case['lesson']
        value = wrong.get(mode, values[mode]) if case['control'] == 'wrong-value' else values[mode]
        require(last['phase'] == 'PASS' and last['count'] == '1' and last['namespace'] == case['namespace'] and last['gateway'] == 'tinygo_resource_gateway', 'complete final native identity/count')
        require(bytes.fromhex(last['value-hex']).decode() == value, 'complete actual native value')
        if mode == 'server':
            require(last['identity-kind'] == 'client-request-sequence' and last['identity'] == 'not-applicable'
                    and int(last['send-call-ns']) > 0, 'actual client request sequence; writer GUID not exposed')
        else:
            require(last['identity-kind'] == ('subscription-gid' if mode == 'subscribe' else 'request-writer-guid'), 'native identity kind')
            require(last['identity'] != 'not-applicable' and any(bytes.fromhex(last['identity'])), 'nonzero actual native operation identity')
        if mode != 'subscribe':
            require(int(last['sequence']) > 0, 'actual native writer sequence')
        if mode in ('trigger', 'set-bool', 'server'):
            require(last['value-success'] == ('false' if mode == 'set-bool' and case['control'] == 'wrong-value' else 'true'), 'actual service semantic status')
        require(case['quiet_min_seconds'] == 2 and case['native_gateway_node_count_at_exit'] == 0, 'finite quiet and retirement contract')
        file(root / 'native-full-r1' / ('case' + str(index)) / 'policy/policy.json', case['policy_json_sha256'])
    result.update(native_positive_scenes=50, native_wrong_value_refusals=4, native_scene_commands=216)
    rollback = record('rollback-r2-result.json', 'ACTUAL_TINYGO10_PARTIAL_FACTORY_ROLLBACKS_PASSED', 'rollback-r2', True)
    require(len(rollback['cases']) == 10 and len(rollback['commands']) == 30, 'complete partial factory rollback matrix')
    require(all(c['missing_respond_refused13'] and c['one_create_one_close'] and c['session_remained_open_before_explicit_shutdown'] and not c['native_peer_delivery'] for c in rollback['cases']), 'rollback scope and outcomes')
    debug = record('debug-r2-result.json', 'ACTUAL_TINYGO_MINIMAL_CODEC_SOURCE_DEBUG_SESSIONS_PASSED', 'debug-r2', True)
    require({s['runtime'] for s in debug['sessions']} == {'uwvm2', 'uwvm2-stable', 'uwvm2-ros'}, 'three actual source debugger products')
    for session in debug['sessions']:
        actions = session['actions']
        require(session['source_breakpoint'] and session['source_step'] and session['stale_read_refused'], 'finite actual source debugger workflow')
        reads = [r for r in actions if r['command'] and r['command'].startswith('print ') and 'state.Code' in r['command']]
        require(len(reads) == 3 and reads[0]['reply'] == reads[1]['reply'] and re.search(r'(?:value=|i32=)13\b', reads[0]['reply']) and 'error:' in reads[-1]['reply'], 'actual repeated SDK field13 and stale-stop refusal')
    pure = record('codec-ordinary-result.json', 'ACTUAL_TINYGO8_PURE_CODEC_ALL_AND_NONE_EXECUTIONS_PASSED', 'codec-ordinary-runs')
    require(len(pure['commands']) == 8 and sum(c['selection'] == 'none' for c in pure['workflows']) == 2, 'legal Wasm without selected language adapters')
    gate = record('source-gate-result-r2.json', 'ACTUAL_CURRENT_ALL15_RAW_SOURCE_CONTRACTS_PASSED', 'source-gate-runs-r2')
    require(gate['source_gate']['backend_qualified_declarations_checked'] == 891 and len(gate['source_gate']['languages']) == 15, 'all15 raw source scope')
    result.update(partial_factory_rollbacks=10, source_debugger_sessions=3, pure_all_and_none_executions=8, all15_raw_declarations=891,
                  limitations=['ROS2 Humble x86_64 finite native scenes only', 'no full upstream client compatibility', 'TinyGo scalar-local failure and panic traps retained', 'no new QEMU/cache-update qualification'])
    for report in ('probe-producer-result-r1.json', 'probe-producer-result-r2.json'):
        data = read_json(root / report)
        for artifact in data['artifacts']:
            file(artifact['path'], artifact['sha256'], artifact['bytes'])
            if artifact['path'].endswith('.wasm'):
                imports, _ = binary_imports(Path(artifact['path']).read_bytes())
                require(all(r['module'] == 'wasi_snapshot_preview1' for r in imports), 'pure probe/benchmark has no ROS import')
    result['performance_credit'] = False
    result['performance_status'] = 'Separate existing benchmark lock/actual measured cohort must qualify; functional checks give no timing or PMU credit'
    result['checked_files'] = checked
    result['status'] = 'INDEPENDENT_INERT_TINYGO_RESOURCE_MATRIX_READBACK_PASSED'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(DEFAULT))
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--wait', action='store_true')
    args = parser.parse_args()
    require(not args.out.exists(), 'fresh readback result required')
    require(Path('/proc/self/cgroup').read_text().strip() == '0::' + GROUP, 'original SSH Linux cgroup')
    os.sched_setaffinity(0, {15})
    resource.setrlimit(resource.RLIMIT_AS, (512 << 20, 512 << 20))
    if args.wait:
        deadline = time.monotonic() + 1800
        while True:
            d = read_json(args.root / 'native-full-r1-result.json')
            if d['status'] != 'RUNNING':
                break
            require(time.monotonic() < deadline, 'inert native-evidence wait deadline')
            time.sleep(15)
    result = verify(args.root)
    result['reader_sha256'] = digest(Path(__file__).read_bytes())
    args.out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('checked_files', 'binary_inspections')}))


if __name__ == '__main__':
    main()
