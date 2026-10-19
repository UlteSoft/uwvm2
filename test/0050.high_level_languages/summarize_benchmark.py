#!/usr/bin/env python3
"""Derive portable tables from a completed, admitted equal-input benchmark.

This reads evidence only. Unsupported/failed pairs receive no metrics or ratio.
Run after the benchmark, retaining its original report and all raw run logs.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics


ENGINES = ['uwvm2', 'uwvm2-stable', 'uwvm2-ros', 'wasmtime', 'wavm', 'wasmedge-jit', 'wasmedge-aot']
FIELDS = ['kernel_seconds', 'process_wall_seconds', 'process_user_seconds', 'process_system_seconds']
SHA256 = re.compile(r'[0-9a-f]{64}')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def cell(value):
    return str(value).replace('|', '\\|').replace('\n', ' ').replace('\r', ' ')


def validate_retirement(sample, membership, cpu):
    assert sample['exit'] == 0 and not sample['error'] and sample['retired'] is True and not sample['cleanup_error']
    ownership, target = sample['process_ownership'], sample['target']
    assert ownership['closed'] is True and ownership['members']
    assert all(member['pidfd_retained'] is False for member in ownership['members'])
    assert ownership['leader_pid'] == target['pid']
    leaders = [member['identity'] for member in ownership['members'] if member['origin'] == 'leader']
    assert len(leaders) == 1 and leaders[0]['pid'] == target['pid']
    assert leaders[0]['start'] == ownership['leader_start']
    assert leaders[0]['session'] == leaders[0]['pgrp'] == target['pid']
    assert target['cgroup'] == membership and target['tasks']
    observations = sample['placement_observations']
    assert observations and sample['memory_observations']
    for tasks in [target['tasks'], *[item['tasks'] for item in observations]]:
        assert tasks and all(item['affinity'] == [cpu] and item['cgroup'] == membership for item in tasks.values())
    assert math.isfinite(sample['cleanup_seconds']) and sample['cleanup_seconds'] >= 0


def validate_sample(sample, case, membership, cpu):
    validate_retirement(sample, membership, cpu)
    assert SHA256.fullmatch(sample['binary_sha256'])
    lines = sample['stdout'].replace('\r', '').splitlines()
    if case['kernel_time'] == 'ticks':
        timer = re.compile(r'csharp kernel iterations=([0-9]+) ticks=([0-9]+) frequency=([0-9]+)')
        prefix = 'csharp kernel '
    else:
        prefix = case.get('clock_line_prefix', 'Kernel ns: ')
        timer = re.compile(re.escape(prefix) + r'([0-9]+(?:\.[0-9]+)?)')
    matches = [timer.fullmatch(line) for line in lines if line.startswith(prefix)]
    assert len(matches) == 1 and matches[0], 'one complete guest timer line required'
    match = matches[0]
    if case['kernel_time'] == 'ticks':
        iterations, ticks, frequency = map(int, match.groups())
        expected = case.get('iterations', case.get('rounds'))
        if expected is None and case.get('args'):
            expected = int(case['args'][0])
        assert expected is not None and iterations == int(expected) and iterations > 0 and frequency > 0
        elapsed = ticks / frequency
    else:
        elapsed = float(match[1]) / {'ns': 1e9, 'ms': 1e3}[case['kernel_time']]
    assert math.isfinite(elapsed) and elapsed > 0 and elapsed == sample['kernel_seconds']
    stable = [line for line in lines if not timer.fullmatch(line)]
    expected = case.get('checks', case.get('expected_stdout', '').splitlines())
    assert isinstance(expected, list) and expected and all(isinstance(line, str) and line for line in expected)
    assert stable == expected, 'complete stable stdout/checksum mismatch'
    checksum_line = case['checksum_line']
    checksum = re.search(r'(?:^|\b)(?:checksum=|Checksum: )(-?[0-9]+)$', checksum_line)
    assert checksum_line in expected and checksum and int(checksum[1]) == case['expected_checksum']
    for key in FIELDS:
        assert math.isfinite(sample[key]) and sample[key] >= 0, ('invalid metric', key)
    wall = sample['process_wall_seconds']
    assert wall > 0 and elapsed <= wall + max(.05, .05 * wall), 'guest timer exceeds process window'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--expected-routes', type=int, default=7,
                        help='final scope is seven routes; use 6 only for an explicitly labelled historical report')
    args = parser.parse_args()
    report = json.loads(args.report.read_text())
    assert report.get('artifacts_synced_before_receipt') and not report.get('error')
    assert report['cases_sha256'] == report['cases_sha256_after'] == sha(args.manifest)
    assert report['recipes_sha256'] == report['recipes_sha256_after']
    assert report['helpers_sha256'] == report['helpers_sha256_after']
    assert report['warmup_rounds'] == 1 and report['samples_per_pair'] == 5 and report['cpu'] == 21
    before, after = report['admission_before'], report['admission_after']
    assert before['membership'] == after['membership']
    events = lambda text: dict((key, int(value)) for key, value in (row.split() for row in text.splitlines()))
    old, new = events(before['memory_events']), events(after['memory_events'])
    assert all(old[key] == new[key] for key in ['high', 'max', 'oom', 'oom_kill', 'oom_group_kill'])
    cases = [row for row in json.loads(args.manifest.read_text()) if row.get('purpose') == 'ordinary-long']
    names = [row['name'] for row in cases]
    assert len(cases) == args.expected_routes > 0 and len(names) == len(set(names)), 'unexpected/duplicate route count'
    assert len({(row['language'], row['target']) for row in cases}) == len(cases), 'duplicate language/target route'
    assert args.expected_routes != 7 or any(row['language'] == 'python' for row in cases), 'final scope requires Python'
    pairs = {(row['case'], row['engine']): row for row in report['pairs']}
    assert len(pairs) == len(report['pairs']) == len(cases) * len(ENGINES)
    assert set(pairs) == {(name, engine) for name in names for engine in ENGINES}
    qualified, failures, derived = 0, [], []
    binary_hashes = {}
    for case in cases:
        result = {'case': case['name'], 'language': case['language'], 'target': case['target'],
                  'wasm_sha256': case['wasm_sha256'], 'kernel_clock': case['kernel_clock'], 'engines': {}}
        assert SHA256.fullmatch(case['wasm_sha256'])
        for engine in ENGINES:
            row = pairs[case['name'], engine]
            assert row['wasm_sha256'] == case['wasm_sha256']
            assert row['kernel_clock'] == case['kernel_clock'] and row['kernel_time'] == case['kernel_time']
            if not row['qualified'] or 'metrics' not in row:
                assert 'metrics' not in row
                failures.append({'case': case['name'], 'engine': engine,
                                 'class': row.get('failure_class'), 'failure': row.get('failure'),
                                 'raw_failures': row['failures']})
                result['engines'][engine] = {'qualified': False}
                continue
            assert not row['failures'] and len(row['samples']) == 5
            for sample in [row['warmup'], *row['samples']]:
                validate_sample(sample, case, before['membership'], report['cpu'])
                assert binary_hashes.setdefault(engine, sample['binary_sha256']) == sample['binary_sha256']
            metrics = {key: {'median': statistics.median(sample[key] for sample in row['samples']),
                             'minimum': min(sample[key] for sample in row['samples']),
                             'maximum': max(sample[key] for sample in row['samples'])} for key in FIELDS}
            assert metrics == row['metrics']
            item = {'qualified': True, 'samples': 5, 'metrics': metrics,
                    'binary_sha256': binary_hashes[engine]}
            if engine == 'wasmedge-aot':
                validate_retirement(row['aot'], before['membership'], report['cpu'])
                assert row['aot']['compiler_sha256'] == binary_hashes[engine]
                assert SHA256.fullmatch(row['aot']['native_sha256'])
                assert math.isfinite(row['aot']['process_wall_seconds']) and row['aot']['process_wall_seconds'] > 0
                for sample in [row['warmup'], *row['samples']]:
                    assert sample['aot_native_sha256_before'] == sample['aot_native_sha256_after'] == row['aot']['native_sha256']
                item['offline_compilation_seconds'] = row['aot']['process_wall_seconds']
                item['native_sha256'] = row['aot']['native_sha256']
                assert row['aot']['original_wasm_sha256'] == case['wasm_sha256']
            result['engines'][engine] = item
            qualified += 1
        reference = result['engines']['wasmtime']
        for engine in ENGINES:
            item = result['engines'][engine]
            if item['qualified'] and reference['qualified']:
                item['kernel_time_over_wasmtime'] = item['metrics']['kernel_seconds']['median'] / reference['metrics']['kernel_seconds']['median']
        derived.append(result)
    assert report['passed'] == (qualified == len(pairs)), 'report qualification flag disagrees with raw pairs'
    summary = {'evidence_admitted': True, 'all_requested_pairs_qualified': qualified == len(pairs),
               'mandatory_uwvm_and_wasmtime_pairs_qualified': all(row['engines'][engine]['qualified']
                    for row in derived for engine in ENGINES[:4]),
               'qualified_pairs': qualified, 'requested_pairs': len(pairs), 'routes': len(cases),
               'engines': ENGINES, 'samples_per_pair': 5,
               'report_sha256': sha(args.report), 'manifest_sha256': sha(args.manifest),
               'helper_sha256': sha(__file__), 'cases': derived, 'failures': failures}
    lines = ['# 普通执行：相同 Wasm 的引擎对比', '',
             'CPU21；每对一个预热、五个随机交错的独立进程样本；磁盘引擎缓存禁用，OS 页缓存保留。',
             f'{len(cases)} 条路线 × {len(ENGINES)} 种引擎/模式 = {len(pairs)} 对；三仓 UWVM 与 Wasmtime 为必测资格，状态见派生 JSON。',
             '不同路线的工作量不同；仅比较同一行。guest 内核与冷进程耗时分别记录；时钟种类保留在每条路线的派生 JSON。', '',
             '内核中位数（毫秒）；N/A 表示该对未通过运行/计时资格，不产生比值。', '',
             '| case | ' + ' | '.join(ENGINES) + ' |', '| --- | ' + ' | '.join(['---:'] * len(ENGINES)) + ' |']
    for row in derived:
        lines.append('| ' + cell(row['case']) + ' | ' + ' | '.join(
            f"{row['engines'][engine]['metrics']['kernel_seconds']['median'] * 1000:.3f}"
            if row['engines'][engine]['qualified'] else 'N/A' for engine in ENGINES) + ' |')
    lines += ['', 'UWVM / Wasmtime 内核耗时比（越小越快，仅同一行）：', '',
              '| case | uwvm2 | uwvm2-stable | uwvm2-ros |', '| --- | ---: | ---: | ---: |']
    for row in derived:
        lines.append('| ' + cell(row['case']) + ' | ' + ' | '.join(
            f"{row['engines'][engine]['kernel_time_over_wasmtime']:.3f}"
            if 'kernel_time_over_wasmtime' in row['engines'][engine] else 'N/A' for engine in ENGINES[:3]) + ' |')
    lines += ['', '冷进程 wall 中位数（秒），包含加载/编译、初始化、内核与退出；不等于单独编译时间。WasmEdge AOT 的离线构建另列。', '',
              '| case | ' + ' | '.join(ENGINES) + ' |', '| --- | ' + ' | '.join(['---:'] * len(ENGINES)) + ' |']
    for row in derived:
        lines.append('| ' + cell(row['case']) + ' | ' + ' | '.join(
            f"{row['engines'][engine]['metrics']['process_wall_seconds']['median']:.3f}"
            if row['engines'][engine]['qualified'] else 'N/A' for engine in ENGINES) + ' |')
    lines += ['', 'WasmEdge AOT 离线构建（秒，不计入运行样本）：', '']
    lines += ['- ' + cell(row['case']) + ': ' + f"{row['engines']['wasmedge-aot']['offline_compilation_seconds']:.3f}"
              for row in derived if row['engines']['wasmedge-aot']['qualified']]
    lines += ['', '失败对的完整 stderr/stdout、退出码与身份保留在原始报告及对应 runs 目录。', '',
              '| case | engine | failure |', '| --- | --- | --- |']
    lines += ['| ' + cell(row['case']) + ' | ' + cell(row['engine']) + ' | ' + cell(row['failure'])[:240] + ' |'
              for row in failures]
    lines += ['', '[原始 JSON](' + os.path.relpath(args.report, args.out) + ')；[派生 JSON](summary.json)。', '']
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / 'summary.json').write_text(json.dumps(summary, indent=2, allow_nan=False) + '\n')
    (args.out / 'tables.md').write_text('\n'.join(lines))
    print(json.dumps({key: summary[key] for key in ['evidence_admitted', 'mandatory_uwvm_and_wasmtime_pairs_qualified',
                                                   'qualified_pairs', 'requested_pairs']}))


if __name__ == '__main__':
    main()
