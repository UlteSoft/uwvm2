#!/usr/bin/env python3
"""Offline join of real VTune sample IPs to one READY-gated MCJIT snapshot.

No target access, collection, managed-method or source-line claim. Scaled event
values are never used as sample counts. All addresses belong to the same saved
PID/start/boot/binary/Wasm identity; an old snapshot cannot label another run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from capture_jit_symbols import ELF, MEMBERSHIP
from inspect_wasm import inspect
from vtune_reports import digest, durable_json, require, sample_table


def join(profile_path, metadata_path, qualification_path, out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    data = {'passed': False, 'purpose': 'offline-same-process-jit-sample-ip-join',
            'scope': 'Wasm function indices/names in registered native ELF ranges only',
            'source_line_or_managed_method_attribution': False, 'target_access': False,
            'event_values_are_instruction_counts': False,
            'helper_sha256': digest(__file__), 'inputs': {}}
    paths = [Path(profile_path), Path(metadata_path), Path(qualification_path)]
    try:
        for path in paths:
            data['inputs'][str(path)] = digest(path)
        profile, metadata, qualification = [json.loads(path.read_text()) for path in paths]
        require(profile['passed'] and metadata['passed'] and qualification['passed'],
                'profile, quiescent metadata and hardware qualification must all pass')
        require(profile['jit_metadata'] == metadata, 'capture receipt differs from embedded profile metadata')
        pid, cpu = profile['target_pid'], profile['hybrid_pmu']['target']
        atom = cpu in profile['hybrid_pmu']['cpu_atom']
        before, after = metadata['target_before'], metadata['target_after']
        require(before == after and metadata['quiescent_snapshot_verified'], 'snapshot target identity not quiescent')
        owner = profile['process_ownership']['target']
        require(owner['leader_pid'] == pid == metadata['pid'] == before['pid'] == qualification['target_pid'],
                'same exact target PID required')
        require(owner['leader_start'] == before['start_time_ticks'], 'target PID start-time mismatch')
        require(metadata['boot'] == profile['boot'], 'boot identity mismatch')
        require(metadata['maps_before_sha256'] == metadata['maps_after_sha256'], 'maps changed at capture')
        require(metadata['descriptors_before'] == metadata['descriptors_after'], 'registered ELF list changed at capture')
        require(before['cgroup'] == MEMBERSHIP, 'snapshot is not from original existing cgroup')
        for admission in ('admission_before', 'admission_after'):
            require(metadata[admission]['membership'] == profile[admission]['membership'] == MEMBERSHIP,
                    'saved admission identity mismatch')
        for task in before['tasks'].values():
            require(task['affinity'] == [cpu] and task['tracer_pid'] == 0
                    and all(value == 0 for value in task['capabilities'].values()),
                    'snapshot target task affinity/capability/tracer qualification failed')
        require(qualification['target_cpu'] == cpu and qualification['target_pmu'] == ('cpu_atom' if atom else 'cpu_core'),
                'hardware qualification CPU/PMU mismatch')
        result = Path(qualification['result'])
        require(result.resolve() == (paths[0].parent / 'result').resolve(), 'sample report belongs to another result')
        config = result / 'config/runsa.options'
        paths.append(config)
        data['inputs'][str(config)] = digest(config)
        require(digest(config) == qualification['runsa_options_sha256'], 'collector configuration changed')
        sample_commands = [row for row in qualification['commands']
                           if '-show-as' in row['argv'] and row['argv'][row['argv'].index('-show-as') + 1] == 'samples']
        require(len(sample_commands) == 1 and sample_commands[0]['exit'] == 0, 'one actual samples export required')
        command = sample_commands[0]
        require(Path(command['argv'][command['argv'].index('-r') + 1]).resolve() == result.resolve(),
                'sample command result binding mismatch')
        samples = Path(command['stdout'])
        paths.append(samples)
        data['inputs'][str(samples)] = digest(samples)
        require(digest(samples) == command['stdout_sha256'], 'sample report changed after qualification')
        for name in ('binary_sha256', 'wasm_sha256'):
            require(metadata[name] == metadata['expected_' + name] == profile[name], 'binary/Wasm identity mismatch')
        helper_hashes = [value for key, value in profile['helper_sha256'].items()
                         if Path(key).name == 'capture_jit_symbols.py']
        require(helper_hashes == [metadata['helper_sha256']], 'capture helper provenance mismatch')
        wasm, binary = Path(metadata['wasm']), Path(metadata['binary'])
        for path, checksum in ((wasm, metadata['wasm_sha256']), (binary, metadata['binary_sha256'])):
            require(digest(path) == checksum, f'original input changed: {path}')
            paths.append(path)
            data['inputs'][str(path)] = checksum
        argv = before['argv']
        require(argv[0] == str(binary) and argv[argv.index('--run') + 1] == str(wasm), 'actual module argv mismatch')
        require(profile['target_argv'][-len(argv):] == argv, 'profile and metadata target argv differ')
        names = inspect(wasm)['functions']
        maps_text = metadata['maps_before']
        require(hashlib.sha256(maps_text.encode()).hexdigest() == metadata['maps_before_sha256'],
                'saved executable maps changed')
        executable = []
        for line in maps_text.splitlines():
            fields = line.split(None, 5)
            if 'x' in fields[1]:
                executable.append(tuple(int(value, 16) for value in fields[0].split('-')))
        object_symbols = []
        for item in metadata['objects']:
            path = Path(item['file'])
            if not path.exists():
                path = paths[1].parent / path.name
            require(path.stat().st_size == item['bytes'] and digest(path) == item['sha256'], 'registered ELF changed')
            paths.append(path)
            data['inputs'][str(path)] = digest(path)
            elf = ELF(data=path.read_bytes())
            require(elf.kind == 1, 'only registered ET_REL address rule is supported')
            symbols = set()
            for symbol in elf.symbols():
                if symbol['type'] == 2 and 0 < symbol['section'] < len(elf.sections) and symbol['size'] > 0:
                    symbols.add((symbol['name'], elf.sections[symbol['section']]['addr'] + symbol['value'], symbol['size']))
            object_symbols.append(symbols)
        ranges = metadata['ranges']
        for row in ranges:
            require(row['end'] == row['start'] + row['size'] and row['size'] > 0, 'invalid saved function extent')
            require((row['native_symbol'], row['start'], row['size']) in object_symbols[row['registered_object']],
                    'range not present in saved registered ELF')
            require(any(low <= row['start'] < row['end'] <= high for low, high in executable),
                    'native function range outside saved executable maps')
            if row['wasm_module_bound']:
                index = row['wasm_function_index']
                require(row['wasm_sha256'] == profile['wasm_sha256']
                        and row['native_module_prefix'] == metadata['wasm_module_binding']['native_prefix'],
                        'Wasm module prefix/SHA mismatch')
                require(row['wasm_name'] == names.get(index), 'Wasm name differs from same-SHA artifact')
        parsed = sample_table(samples, pid, cpu, atom, qualification['target_tids'])
        events = [event for event, count in parsed['target_samples'].items() if count > 0]
        totals = {event: parsed['target_samples'][event] for event in events}
        counts = {kind: dict.fromkeys(events, 0) for kind in ('wasm', 'native_only', 'unmatched', 'ambiguous')}
        functions, native, unmatched, ambiguous, joined_ips = {}, {}, [], [], []
        for row in parsed['target_rows']:
            hits = [item for item in ranges if item['start'] <= row['address'] < item['end']]
            bound = [item for item in hits if item['wasm_module_bound']]
            indices = {item['wasm_function_index'] for item in bound}
            kind = ('unmatched' if not hits else 'native_only' if not bound else
                    'wasm' if len(indices) == 1 and len(bound) == len(hits) else 'ambiguous')
            nonzero = {event: row['samples'][event] for event in events if row['samples'][event]}
            for event, value in nonzero.items():
                counts[kind][event] += value
            detail = {**{key: value for key, value in row.items() if key != 'samples'}, 'samples': nonzero,
                      'classification': kind, 'native_symbols': [item['native_symbol'] for item in hits]}
            joined_ips.append(detail)
            if kind == 'wasm':
                index = next(iter(indices))
                entry = functions.setdefault(index, {'wasm_function_index': index, 'wasm_name': bound[0]['wasm_name'],
                    'samples': dict.fromkeys(events, 0), 'native_ranges': [], 'sample_addresses': []})
                for item in bound:
                    if item not in entry['native_ranges']:
                        entry['native_ranges'].append(item)
                entry['sample_addresses'].append(detail)
                for event, value in nonzero.items():
                    entry['samples'][event] += value
            elif kind == 'unmatched':
                unmatched.append(detail)
            elif kind == 'ambiguous':
                ambiguous.append(detail)
            else:
                for item in hits:
                    native[item['native_symbol']] = item
        for entry in functions.values():
            entry['share_of_target_samples'] = {event: entry['samples'][event] / totals[event] for event in events}
        ordered = sorted(functions.values(), key=lambda item: item['samples'].get('INST_RETIRED.ANY', 0), reverse=True)
        data.update(target_pid=pid, target_start_time_ticks=owner['leader_start'], boot=profile['boot'],
                    binary_sha256=profile['binary_sha256'], wasm_sha256=profile['wasm_sha256'],
                    target_cpu=cpu, target_pmu=qualification['target_pmu'],
                    performance_eligible=profile['performance_eligible'], measurement_context=profile['measurement_context'],
                    actual_sample_totals=totals, sample_classification=counts,
                    wasm_sample_coverage={event: counts['wasm'][event] / totals[event] for event in events},
                    top_wasm_functions=ordered, unmatched_ips=unmatched, ambiguous_ips=ambiguous,
                    native_only_ranges=list(native.values()), joined_ips=joined_ips)
        for path in paths:
            require(digest(path) == data['inputs'][str(path)], f'input changed during offline join: {path}')
        data['passed'] = True
    except BaseException as error:
        data['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        durable_json(out / 'receipt.json', data)
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--metadata', type=Path)
    parser.add_argument('--qualification', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    data = join(args.profile, args.metadata or args.profile.parent / 'jit-metadata/receipt.json', args.qualification, args.out)
    print(json.dumps({key: data[key] for key in ('passed', 'target_pid', 'wasm_sample_coverage', 'actual_sample_totals')}, allow_nan=False))


if __name__ == '__main__':
    main()
