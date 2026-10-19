#!/usr/bin/env python3
"""Offline VTune reports: target-owned actual samples and VTune formula metrics.

The values report contains scaled event counts; only the separate ``samples``
report qualifies sample sufficiency. Counting/emon summary mode cannot qualify
a guest-exclusive profile. No formula or instruction estimate is synthesized.
"""
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

EVENTS = ('INST_RETIRED.ANY', 'CPU_CLK_UNHALTED.REF_TSC')
SAMPLE_PREFIX = 'Hardware Event Sample Count:'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def durable_json(path, data):
    path = Path(path)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as stream:
        temporary = Path(stream.name)
        stream.write((json.dumps(data, indent=2, allow_nan=False) + '\n').encode())
        stream.flush()
        os.fsync(stream.fileno())
    try:
        os.replace(temporary, path)
        fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        temporary.unlink(missing_ok=True)


def table(path, first_column):
    lines = Path(path).read_text().splitlines()
    starts = [i for i, line in enumerate(lines) if line.startswith(first_column + '\t')]
    require(len(starts) == 1, f'unique TSV header {first_column!r} required: {path}')
    cells = list(csv.reader(io.StringIO('\n'.join(lines[starts[0]:])), delimiter='\t'))
    header, result = cells[0], []
    for row in cells[1:]:
        if not row or not any(row):
            continue
        require(len(row) == len(header), f'malformed TSV row: {path}: {row[:5]}')
        parsed = {}
        for key, value in zip(header, row):
            require(key not in parsed or parsed[key] == value,
                    f'duplicate report columns disagree: {path}: {key}')
            parsed[key] = value
        result.append(parsed)
    require(result, f'empty report: {path}')
    return list(dict.fromkeys(header)), result


def integer(text):
    require(bool(re.fullmatch(r'[0-9]+(?:,[0-9]{3})*', text)), f'noninteger sample/identity: {text!r}')
    return int(text.replace(',', ''))


def number(text):
    value = float(text.replace(',', ''))
    require(math.isfinite(value), f'nonfinite VTune metric: {text!r}')
    return value


def core_matches(label, atom):
    is_atom = bool(re.search(r'atom|efficien|e-core', label, re.I))
    is_core = bool(re.search(r'core|performance', label, re.I)) and not is_atom
    return is_atom if atom else is_core


def sample_table(path, target_pid, target_cpu, target_is_atom, target_tids=None):
    header, rows = table(path, 'PID')
    require(all(key in header for key in ('PID', 'TID', 'Logical Core', 'Core Type', 'Code Location')),
            'PID/TID/CPU/core/address sample dimensions required')
    events = [key[len(SAMPLE_PREFIX):] for key in header if key.startswith(SAMPLE_PREFIX)]
    require(all(event in events for event in EVENTS), 'retired and clock actual sample columns required')
    tids = {int(tid) for tid in target_tids} if target_tids is not None else {target_pid}
    selected, excluded, seen = [], [], set()
    for row in rows:
        pid, tid = [integer(row[key]) for key in ('PID', 'TID')]
        cpu_label = row['Logical Core']
        require(bool(re.fullmatch(r'(?:cpu_)?[0-9]+', cpu_label)), f'invalid logical CPU: {cpu_label!r}')
        cpu = int(cpu_label.removeprefix('cpu_'))
        address = row['Code Location']
        require(bool(re.fullmatch(r'0x[0-9a-fA-F]+', address)), f'absolute sample IP required: {address}')
        counts = {event: integer(row[SAMPLE_PREFIX + event]) for event in events}
        key = (pid, tid, cpu, row['Core Type'], int(address, 16))
        require(key not in seen, f'duplicate sample dimension: {key}')
        seen.add(key)
        parsed = {'pid': pid, 'tid': tid, 'cpu': cpu, 'core_type': row['Core Type'],
                  'address': int(address, 16), 'address_hex': address, 'samples': counts}
        if pid != target_pid:
            excluded.append(parsed)
            continue
        require(tid in tids, f'unknown target TID in hardware samples: {tid}')
        require(cpu == target_cpu and core_matches(row['Core Type'], target_is_atom),
                f'target samples escaped selected CPU/PMU: {key}')
        selected.append(parsed)
    require(selected, 'no target PID/TID/CPU/core samples')
    counts = {event: sum(row['samples'][event] for row in selected) for event in events}
    return {'target_rows': selected, 'excluded_rows': excluded, 'target_samples': counts,
            'target_tids': sorted(tids), 'actual_sample_columns': events}


def xml_hardware_events(path):
    root = ET.parse(path).getroot()
    result = []
    for metric in root.iter('metric'):
        if metric.get('name') != 'Hardware Events':
            continue
        for node in metric.findall('table'):
            columns = [item.text for item in node.findall('columns/column')]
            for row in node.findall('row'):
                cells = [item.findtext('value') for item in row.findall('value')]
                require(len(columns) == len(cells), 'malformed XML hardware event table')
                result.append(dict(zip(columns, cells)))
    require(result, 'actual XML hardware event table missing')
    return result


def qualify_microarchitecture_rows(header, rows, parsed, target_pid, target_is_atom):
    """Qualify sampled threads; retain idle/undersampled JVM helpers explicitly.

    A PID-filtered report may contain zero rows for sleeping runtime threads.
    Those rows cannot establish microarchitecture percentages. Keep their raw
    values and require >=99% actual retired/clock sample coverage by qualified
    threads, rather than interpreting zero formulas as measured percentages.
    """
    selected, excluded, seen = [], [], set()
    for row in rows:
        match = re.search(r'\(TID:\s*([0-9]+)\)', row.get('Thread', ''))
        require(match is not None, 'VTune formula Thread lacks a real TID')
        tid = int(match[1])
        require(tid in parsed['target_tids'], f'unknown TID in PID-filtered formulas: {tid}')
        require(tid not in seen, f'duplicate formula TID: {tid}')
        seen.add(tid)
        require(core_matches(row.get('Core Type', ''), target_is_atom), 'formula metrics include wrong PMU')
        counts = {event: sum(item['samples'][event] for item in parsed['target_rows']
                             if item['tid'] == tid) for event in EVENTS}
        prefix = 'Efficient-core (E-core):' if target_is_atom else 'Performance-core (P-core):'
        metrics = {}
        for name in ('Retiring', 'Front-End Bound', 'Bad Speculation', 'Back-End Bound'):
            key = prefix + name + ':Self(%)'
            require(key in header, f'VTune formula missing: {key}')
            value = number(row[key])
            require(0 <= value <= 100, f'invalid VTune formula percentage: {key}={value}')
            metrics[name] = value
        item = {'pid_filter': target_pid, 'tid': tid, 'core_type': row['Core Type'],
                'actual_required_event_samples': counts, 'raw_vtune_formula_row': row}
        if any(counts[event] < 10 for event in EVENTS):
            item['reason'] = 'insufficient actual retired/clock samples for this auxiliary TID; no formula qualification'
            excluded.append(item)
            continue
        require(number(row['Instructions Retired:Self']) > 0 and number(row['Clockticks:Self']) > 0,
                'nonzero VTune scaled retired and clock metrics required')
        require(any(value > 0 for value in metrics.values()), 'all qualified VTune top-level metrics are zero')
        item['metrics_percent'] = metrics
        selected.append(item)
    require(selected, 'target VTune microarchitecture formula row required')
    coverage = {}
    for event in EVENTS:
        total = parsed['target_samples'][event]
        covered = sum(item['actual_required_event_samples'][event] for item in selected)
        require(total >= 10 and covered <= total, 'valid target sample denominator required')
        require(covered * 100 >= total * 99,
                f'qualified TIDs cover less than99% actual target samples: {event}: {covered}/{total}')
        coverage[event] = {'qualified': covered, 'all_target': total, 'minimum_percent': 99}
    return selected, excluded, coverage


def hardware_reports(vtune, folder, kind, target_is_atom, run_command=None, *,
                     target_pid=None, target_cpu=None, target_tids=None,
                     result_dir=None, profile_receipt=None):
    """Preserve raw reports, then qualify only requested target sample dimensions.

    ``folder`` is the report output directory; ``result_dir`` permits a separate
    requalification directory without altering an old failed profile receipt.
    The runner must supply its real target PID. CPU defaults to its initial
    receipt's hybrid_pmu.target. Multi-thread targets must supply admitted TIDs.
    """
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    result_dir = Path(result_dir) if result_dir else folder / 'result'
    receipt = Path(profile_receipt) if profile_receipt else folder / 'receipt.json'
    profile = json.loads(receipt.read_text())
    data = {'passed': False, 'scope': 'offline target PID/TID/CPU/PMU sample qualification',
            'kind': kind, 'result': str(result_dir.resolve()), 'profile_receipt': str(receipt),
            'profile_receipt_sha256': digest(receipt), 'helper_sha256': digest(__file__),
            'target_pmu': 'cpu_atom' if target_is_atom else 'cpu_core', 'commands': [],
            'scaled_event_counts_are_actual_samples': False,
            'performance_eligible': profile.get('performance_eligible', False)}

    def report(name, argv):
        out, err = folder / name, folder / (name + '.stderr.log')
        command = [str(vtune), *argv, '-r', str(result_dir)]
        entry = {'argv': command, 'stdout': str(out), 'stderr': str(err)}
        data['commands'].append(entry)
        try:
            if run_command is None:
                # A standalone offline caller also owns/drains its report tree.
                from owned_process import remember_process, wait_process, terminate_group, owned_process_record
                child = None
                try:
                    with out.open('wb') as stdout, err.open('wb') as stderr:
                        child = subprocess.Popen(command, stdout=stdout, stderr=stderr, start_new_session=True,
                                                 env=dict(os.environ, LC_ALL='C', LANG='C'))
                        remember_process(child)
                        code = wait_process(child, 120)
                finally:
                    if child is not None:
                        terminate_group(child)
                        entry['process_ownership'] = owned_process_record(child)
            else:
                code = run_command(command, out, err, 120)
            entry.update(exit=code, stdout_sha256=digest(out), stderr_sha256=digest(err))
            require(code == 0, f'VTune report failed {name}: {err.read_text(errors="replace")[-2000:]}')
        finally:
            for path in (out, err):
                if path.exists():
                    with path.open('rb') as stream:
                        os.fsync(stream.fileno())
        return out

    try:
        require(isinstance(target_pid, int) and target_pid > 0, 'explicit real target_pid required')
        target_cpu = profile['hybrid_pmu']['target'] if target_cpu is None else target_cpu
        require(isinstance(target_cpu, int) and target_cpu >= 0, 'explicit target CPU required')
        if 'target_pid' in profile:
            require(profile['target_pid'] == target_pid, 'target PID differs from profile receipt')
        data.update(target_pid=target_pid, target_cpu=target_cpu)
        data['vtune_sha256'] = digest(vtune)
        require(data['vtune_sha256'] == profile['vtune_sha256'], 'VTune executable differs from collection receipt')
        config = result_dir / 'config/runsa.options'
        require(config.is_file(), 'actual collection runsa.options required')
        options = config.read_text().splitlines()
        require('--target-pid' in options and options[options.index('--target-pid') + 1] == str(target_pid),
                'actual collector target PID differs')
        require('--aggregated-counting' not in options and '--collector=emon' not in options,
                'counting/emon backend cannot establish guest-exclusive sample qualification')
        data['runsa_options_sha256'] = digest(config)
        report('summary.csv', ['-report', 'summary', '-format', 'csv'])
        xml = report('summary.xml', ['-report', 'summary', '-format', 'xml'])
        data['hardware_events'] = xml_hardware_events(xml)
        report('hotspots.csv', ['-report', 'hotspots', '-group-by', 'function', '-limit', '40', '-format', 'csv'])
        dims = ['-group-by', 'process-id,thread-id,cpuid,core-type,address', '-format', 'csv', '-limit', '0']
        report('hw-events-pid-tid-cpu-address-values.csv', ['-report', 'hw-events', *dims, '-show-as', 'values'])
        samples_path = report('hw-events-pid-tid-cpu-address-samples.csv', ['-report', 'hw-events', *dims, '-show-as', 'samples'])
        parsed = sample_table(samples_path, target_pid, target_cpu, target_is_atom, target_tids)
        data.update(parsed)
        for event in EVENTS:
            count = parsed['target_samples'][event]
            require(count >= 10, f'only {count} actual target samples for {event}; increase guest work')
        require(kind in ('hotspots', 'uarch-detailed'),
                'counting/emon uarch-summary cannot establish guest-exclusive hardware metrics')
        if kind == 'uarch-detailed':
            # TID labels are globally unique within the captured result. A PID
            # filter plus the sample table binds every selected Thread to PID.
            path = report('uarch-GEMetrics-process-thread-core.csv', [
                '-report', 'exec-query', '-report-knob', 'row-by=Process/Thread/PMUCoreType',
                '-report-knob', 'column-by=GEMetrics', '-filter', f'process-id={target_pid}', '-format', 'csv'])
            header, rows = table(path, 'Process')
            data['vtune_formula_rows'] = rows
            selected, excluded, coverage = qualify_microarchitecture_rows(
                header, rows, parsed, target_pid, target_is_atom)
            data['target_microarchitecture'] = selected
            data['excluded_microarchitecture_rows'] = excluded
            data['microarchitecture_sample_coverage'] = coverage
            data['metric_scope'] = ('VTune GEMetrics formulas, PID-filtered Thread/PMUCoreType; '
                                    'each qualified TID has >=10 actual samples per required event, '
                                    'qualified TIDs cover >=99% of each required target sample total; '
                                    'auxiliary TIDs with insufficient samples are retained without formula qualification')
        require(digest(config) == data['runsa_options_sha256'], 'collection configuration changed while reporting')
        require(digest(receipt) == data['profile_receipt_sha256'], 'input profile receipt changed while reporting')
        require(digest(vtune) == data['vtune_sha256'] and digest(__file__) == data['helper_sha256'],
                'report executable/helper changed while reporting')
        data['passed'] = True
    except BaseException as error:
        data['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        durable_json(folder / 'hardware-qualification.json', data)
    # Do not overwrite the runner's scope/helper identities/performance flag.
    return {'hardware_qualification': data, 'hardware_qualification_passed': data['passed'],
            **{key: data[key] for key in ('target_pmu', 'target_samples', 'hardware_events')},
            'target_microarchitecture': data.get('target_microarchitecture', [])}
