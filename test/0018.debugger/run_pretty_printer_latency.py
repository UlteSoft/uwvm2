#!/usr/bin/env python3
"""Compare real stopped UWVM print queries with native GDB/LLDB command APIs.

The shared source shape is pretty_printer_native.cc: vector size16/capacity32,
title='native pretty printer', view='length-delimited view'.
Startup, compilation, source breakpoint installation and warmup are excluded.
UWVM includes its UnixBroker round trip, authenticated current source read and
whole-tree formatting; GDB/LLDB use their embedded command APIs. Those boundaries
are deliberately recorded, not called identical. Copied-DATA component throughput
and VTune hardware profiles are separate evidence and are never substituted.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
import run_dap_tinygo_objects as gate

sys.dont_write_bytecode = True


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def distribution(values):
    ordered = sorted(values)
    return {'queries': len(values), 'sum_query_nanoseconds': sum(values),
            'median_query_nanoseconds': statistics.median(values),
            'p95_query_nanoseconds': ordered[max(0, (len(ordered) * 95 + 99) // 100 - 1)]}


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('serve', '__vm_gate'):
        return gate.main()
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'uwvm', 'wasm', 'native', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source', type=Path, help='unchanged shared real producer source, default in source-root')
    parser.add_argument('--gdb', type=Path, default=Path('/usr/bin/gdb'))
    parser.add_argument('--lldb', type=Path, default=Path('/usr/bin/lldb-21'))
    parser.add_argument('--gdb-printer', type=Path, help='optional explicitly selected external Python printer; no policy changes')
    parser.add_argument('--iterations', type=int, default=100)
    parser.add_argument('--warmup', type=int, default=20)
    parser.add_argument('--maximum-memory-bytes', type=int, default=6 << 30)
    parser.add_argument('--maximum-seconds', type=int, default=900)
    parser.add_argument('--disk-reserve-bytes', type=int, default=3 << 30)
    parser.add_argument('--cpu', type=int)
    parser.add_argument('--ros', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.iterations <= 20000 or not 0 <= args.warmup <= 1000 or not 1 <= args.maximum_seconds <= 3600:
        parser.error('bounded iterations/warmup/window required')
    if not (128 << 20) < args.maximum_memory_bytes <= (64 << 30) or args.disk_reserve_bytes < 3 << 30:
        parser.error('existing cgroup total-memory ceiling and >=3GiB disk reserve required')
    for name in ('source_root', 'uwvm', 'wasm', 'native', 'gdb', 'lldb'):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.source = (args.source or args.source_root / 'test/0018.debugger/pretty_printer_native.cc').resolve(strict=True)
    if args.gdb_printer is not None:
        args.gdb_printer = args.gdb_printer.resolve(strict=True)
    if 'PRETTY_NATIVE_STOP' not in args.source.read_text() or args.wasm.read_bytes()[:8] != b'\x00asm\x01\x00\x00\x00':
        parser.error('unchanged marked standard-library source and real core Wasm required')
    admission = load(args.source_root / 'test/0018.debugger/run_pretty_printer_matrix.py', 'pretty_latency_admission')
    initial = admission.admit()
    if args.cpu is not None:
        if args.cpu not in os.sched_getaffinity(0):
            parser.error('selected CPU is outside admitted affinity')
        os.sched_setaffinity(0, {args.cpu})
    sys.path.insert(0, str(args.source_root / 'test/0050.high_level_languages'))
    from owned_process import remember_process, wait_process, terminate_group, owned_process_record
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    args.out = args.out.resolve(strict=True)
    started = time.monotonic()
    report = {'passed': False, 'admission': initial, 'iterations': args.iterations, 'warmup': args.warmup,
              'shape': {'vector_size': 16, 'vector_capacity': 32, 'elements': [i * 3 + 1 for i in range(16)],
                        'title': 'native pretty printer', 'view': 'length-delimited view'},
              'scope': 'actual stopped product/native debugger queries; initialization and warmup excluded',
              'comparison_limit': 'Only this fixed three-query mix: UWVM UnixBroker IPC and whole raw-tree rendering versus in-process GDB/LLDB command APIs. Producer library ABI/provider differences and pooled query distributions retained; not full-container or formatter-only parity.',
              'maximum_memory_bytes': args.maximum_memory_bytes, 'disk_reserve_bytes': args.disk_reserve_bytes,
              'cleanup_errors': [], 'performance_eligible': True,
              'reactive_reserve_bytes': 128 << 20,
              'reactive_stop_threshold_bytes': args.maximum_memory_bytes - (128 << 20),
              'observed_memory_peak_bytes': 0, 'rows': [], 'commands': []}
    paths = [Path(__file__), args.source, args.uwvm, args.wasm, args.native, args.gdb, args.lldb,
             args.source_root / 'tools/debug/dap_adapter.py', args.source_root / 'tools/debug/secure_server.py',
             args.source_root / 'test/0050.high_level_languages/owned_process.py', Path(gate.__file__)]
    if args.gdb_printer is not None:
        paths.append(args.gdb_printer)
    receipt = args.out / 'results.json'

    def save():
        receipt.write_text(json.dumps(report, indent=2) + '\n')

    def cleanup_error(label, issue):
        report['cleanup_errors'].append({'operation': label, 'error': type(issue).__name__ + ': ' + str(issue)})
        report['passed'] = False; report['performance_eligible'] = False

    def budget():
        current = int(Path('/sys/fs/cgroup' + admission.CGROUP + '/memory.current').read_text())
        report['observed_memory_peak_bytes'] = max(report['observed_memory_peak_bytes'], current)
        if current >= args.maximum_memory_bytes - (128 << 20):
            raise RuntimeError('actual total cgroup memory reached reactive stop threshold (128MiB below allocated ceiling)')
        if min(shutil.disk_usage('/').free, shutil.disk_usage(args.out).free) < args.disk_reserve_bytes:
            raise RuntimeError('actual root/output disk below 3GiB reserve')
        if time.monotonic() - started > args.maximum_seconds:
            raise TimeoutError('latency window expired')

    def pins():
        result = {}
        for path in paths:
            digest = hashlib.sha256()
            with path.open('rb') as stream:
                while data := stream.read(1024 * 1024):
                    digest.update(data);budget()
            result[str(path)] = digest.hexdigest()
        return result

    def owned(argv, label, timeout):
        child = None
        entry = {'label': label, 'actual_argv': list(map(str, argv))}
        report['commands'].append(entry)
        try:
            budget()
            with (args.out / (label + '.log')).open('wb') as stream:
                child = subprocess.Popen(entry['actual_argv'], stdout=stream, stderr=subprocess.STDOUT,
                    start_new_session=True, env=dict(os.environ, LC_ALL='C', LANG='C', PYTHONDONTWRITEBYTECODE='1'))
                remember_process(child)
                entry['returncode'] = wait_process(child, timeout, observe=budget)
        finally:
            try:
                terminate_group(child)
            except BaseException as issue:
                cleanup_error('terminate ' + label, issue)
            try:
                entry['ownership'] = owned_process_record(child)
            except BaseException as issue:
                cleanup_error('record ' + label + ' ownership', issue)
            save()
        if report['cleanup_errors'] or entry.get('returncode') != 0:
            raise RuntimeError('actual ' + label + ' command failed or cleanup incomplete; see its log/receipt')

    server = broker = None
    directory = None
    server_log = None
    try:
        budget()
        report['pins_before'] = pins()
        # One retained authenticated client is reused after a real source stop.
        # A new-session broker and descendants are tracked by the existing pidfd
        # owner, including during launch-gate/collector-free startup and cleanup.
        directory = Path(tempfile.mkdtemp(prefix='uwvm-pretty-latency-', dir='/tmp'))
        server_log = (args.out / 'uwvm-broker.log').open('wb')
        mode = [] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
        argv = [sys.executable, str(Path(__file__).resolve()), 'serve', '--uwvm', str(args.uwvm),
                '--socket-dir', str(directory), '--', '-m', 'run', *mode, '-Rct', '0',
                '-Rllvm-call-stack', 'instruction', '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
        server = subprocess.Popen(argv, stdout=server_log, stderr=subprocess.STDOUT, start_new_session=True)
        remember_process(server)
        deadline = time.monotonic() + 25
        while not (directory / 'control.sock').exists():
            remember_process(server);budget()
            if server.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError('actual broker listener failed before launch')
            time.sleep(.01)
        dap = load(args.source_root / 'tools/debug/dap_adapter.py', 'pretty_latency_protocol')
        broker = dap.UnixBroker(str(directory))
        first = broker.request('pause')
        if 'error:' in first or 'guest exited:' in first:
            raise RuntimeError('actual queued first pause failed: ' + first)
        line = next(i for i, text in enumerate(args.source.read_text().splitlines(), 1) if 'PRETTY_NATIVE_STOP' in text)
        point = broker.request(f'breaksource 0 {args.source}:{line}')
        if 'error:' in point or 'breakpoint ' not in point:
            raise RuntimeError('actual standard-library source breakpoint failed: ' + point)
        broker.request('continue')
        for _ in range(64):
            stop = broker.request('wait');budget();remember_process(server)
            if 'stopped: breakpoint' in stop:
                break
            if 'guest exited:' in stop:
                raise RuntimeError('guest exited before pretty benchmark stop')
        else:
            raise TimeoutError('actual standard-library breakpoint not reached')
        status = broker.request('status')
        ids = re.findall(r'(?m)^stop-id ([0-9]+)$', status)
        threads = re.findall(r'(?m)^thread ([0-9]+) module=0 function=[0-9]+ byte-offset=[0-9]+ generation=[0-9]+$', status)
        if len(ids) != 1 or len(threads) != 1:
            raise RuntimeError('one genuinely stopped standard-library participant required: ' + status)
        queries = [f'print {threads[0]} {ids[0]} {name}' for name in ('values', 'title', 'view')]
        first = [broker.request(query) for query in queries]
        trees = [dap.parse_source_object_display(packet, int(ids[0]), int(threads[0])) for packet in first]
        if any(not tree or tree[0].get('pretty') is None for tree in trees):
            raise RuntimeError('actual UWVM query did not return a recognized pretty profile: ' + repr(first))
        if not all(text in trees[0][0]['pretty'] for text in ('size=16', 'capacity=32')):
            raise RuntimeError('actual UWVM vector shape differs: ' + first[0])
        children = [trees[0][index] for index in trees[0][0]['children'] if trees[0][index]['indexed']]
        if [row['value'] for row in children] != list(map(str, report['shape']['elements'])):
            raise RuntimeError('actual UWVM vector did not copy all 16 original elements')
        if 'native pretty printer' not in trees[1][0]['pretty'] or 'length-delimited view' not in trees[2][0]['pretty']:
            raise RuntimeError('actual UWVM string/view differ from native control')
        for _ in range(args.warmup):
            for query in queries:
                broker.request(query)
            budget();remember_process(server)
        samples, checksum = [], 0
        began = time.perf_counter_ns()
        for i in range(args.iterations):
            for query in queries:
                before = time.perf_counter_ns();text = broker.request(query);samples.append(time.perf_counter_ns() - before)
                if text.startswith('error:'):
                    raise RuntimeError('actual timed source query failed: ' + text)
                checksum += len(text.encode())
            if i % 8 == 0:
                budget();remember_process(server)
        elapsed = time.perf_counter_ns() - began
        if broker.request('status') != status:
            raise RuntimeError('real stopped source identity changed during query timing')
        report['rows'].append(dict(debugger='uwvm', nanoseconds=elapsed, checksum=checksum, first=first,
            api='authenticated UnixBroker request; IPC + source read + printer + complete text tree formatting', **distribution(samples)))
        match = re.search(r'breakpoint ([0-9]+)', point)
        broker.request('delete ' + match[1]);broker.request('continue')
        for _ in range(64):
            exited = broker.request('wait')
            if 'guest exited:' in exited:
                if 'guest exited: 0' not in exited:
                    raise RuntimeError('actual benchmark guest oracle failed: ' + exited)
                break
        else:
            raise TimeoutError('actual benchmark guest did not finish')
        broker.close();broker = None
        if wait_process(server, 20, observe=budget) != 0:
            raise RuntimeError('actual broker failed its original guest wait')
        server_log.close();server_log = None
        raw = (args.out / 'uwvm-broker.log').read_text()
        if 'TinyGo DAP original guest wait: 0' not in raw:
            raise RuntimeError('original guest OS wait evidence absent')
        # Native debugger scripts measure command APIs inside their actual
        # stopped process, excluding process launch and all warmup iterations.
        common = r'''import json,time,re
ITERATIONS=__ITERATIONS__
WARMUP=__WARMUP__
def vector_gdb(text):
    match=re.search(r'=\s*\{([^{}]*)\}\s*$',text)
    assert match is not None,text
    items=[item.strip() for item in match[1].split(',')]
    assert all(re.fullmatch(r'-?[0-9]+',item) for item in items),text
    assert list(map(int,items))==[i*3+1 for i in range(16)],text
def vector_lldb(text):
    pairs=re.findall(r'(?m)^\s*(?:\([^()\n]+\)\s*)?\[([0-9]+)\]\s*=\s*(-?[0-9]+)\s*,?\s*$',text)
    indices=re.findall(r'(?m)^\s*(?:\([^()\n]+\)\s*)?\[([0-9]+)\]\s*=',text)
    assert len(indices)==len(pairs)==16,text
    assert [(int(i),int(value)) for i,value in pairs]==[(i,i*3+1) for i in range(16)],text
def finish(name,api,first,checksum,samples,elapsed):
    ordered=sorted(samples)
    print('PRETTY_LATENCY_JSON '+json.dumps(dict(passed=True,debugger=name,api=api,first=first,checksum=checksum,nanoseconds=elapsed,queries=len(samples),sum_query_nanoseconds=sum(samples),median_query_nanoseconds=ordered[len(ordered)//2] if len(ordered)%2 else (ordered[len(ordered)//2-1]+ordered[len(ordered)//2])/2,p95_query_nanoseconds=ordered[max(0,(len(ordered)*95+99)//100-1)])))
'''.replace('__ITERATIONS__', str(args.iterations)).replace('__WARMUP__', str(args.warmup))
        gdb_script = args.out / 'gdb-latency.py'
        gdb_script.write_text(common + '''import gdb
__PRINTER__
gdb.execute('set pagination off');gdb.execute('set confirm off');gdb.execute('set print elements 200');gdb.execute('set print pretty off')
gdb.execute('break pretty_printer_ready');gdb.execute('run')
queries=['print values','print title','print view'];first=[gdb.execute(q,to_string=True) for q in queries]
assert 'length 16' in first[0] and 'capacity 32' in first[0],first
vector_gdb(first[0])
assert 'native pretty printer' in first[1] and 'length-delimited view' in first[2],first
for i in range(WARMUP):
    for q in queries:gdb.execute(q,to_string=True)
checksum=0;samples=[];began=time.perf_counter_ns()
for i in range(ITERATIONS):
    for q in queries:
        before=time.perf_counter_ns();text=gdb.execute(q,to_string=True);samples.append(time.perf_counter_ns()-before);checksum+=len(text.encode())
finish('gdb','embedded gdb.execute(to_string=True); command interpreter/read/format, no per-query IPC',first,checksum,samples,time.perf_counter_ns()-began)
gdb.execute('kill');gdb.execute('quit')
'''.replace('__PRINTER__', 'gdb.execute(' + repr('source ' + json.dumps(str(args.gdb_printer))) + ')' if args.gdb_printer else ''))
        lldb_script = args.out / 'lldb_latency.py'
        lldb_script.write_text(common + '''import lldb
def run(debugger,command,result,internal_dict):
    ci=debugger.GetCommandInterpreter()
    def query(text):
        output=lldb.SBCommandReturnObject();ci.HandleCommand(text,output)
        assert output.Succeeded(),output.GetError()
        return output.GetOutput() or ''
    query('settings set target.max-children-count 200');query('settings set target.disable-aslr false');query('breakpoint set --name pretty_printer_ready');query('run')
    queries=['target variable values','target variable title','target variable view'];first=[query(q) for q in queries]
    assert 'size=16' in first[0] or 'size = 16' in first[0],first
    vector_lldb(first[0])
    assert 'native pretty printer' in first[1] and 'length-delimited view' in first[2],first
    for i in range(WARMUP):
        for q in queries:query(q)
    checksum=0;samples=[];began=time.perf_counter_ns()
    for i in range(ITERATIONS):
        for q in queries:
            before=time.perf_counter_ns();text=query(q);samples.append(time.perf_counter_ns()-before);checksum+=len(text.encode())
    finish('lldb','embedded SBCommandInterpreter; command interpreter/read/format, no per-query IPC',first,checksum,samples,time.perf_counter_ns()-began)
    query('process kill')
def __lldb_init_module(debugger,internal_dict):
    debugger.HandleCommand('command script add -f '+__name__+'.run pretty-latency')
''')
        owned([args.gdb, '-q', '-nx', '-batch', '-x', gdb_script, args.native], 'gdb', args.maximum_seconds)
        owned([args.lldb, '--no-lldbinit', '--batch', '-o', 'command script import ' + json.dumps(str(lldb_script)),
               '-o', 'pretty-latency', args.native], 'lldb', args.maximum_seconds)
        for name in ('gdb', 'lldb'):
            rows = [json.loads(line.split('PRETTY_LATENCY_JSON ', 1)[1]) for line in (args.out / (name + '.log')).read_text().splitlines() if line.startswith('PRETTY_LATENCY_JSON ')]
            if len(rows) != 1 or not rows[0].get('passed') or rows[0]['queries'] != args.iterations * 3:
                raise RuntimeError('missing actual pretty summary/latency receipt from ' + name)
            report['rows'].extend(rows)
        reference = report['rows'][0]['sum_query_nanoseconds']
        report['uwvm_to_native_query_time_ratios'] = {row['debugger']: reference / row['sum_query_nanoseconds'] for row in report['rows'][1:]}
        report['pins_after'] = pins()
        if report['pins_after'] != report['pins_before']:
            raise RuntimeError('producer/product/debugger input bytes changed during timing')
        report['passed'] = True
    except BaseException as error:
        report['error'] = type(error).__name__ + ': ' + str(error)
        report['passed'] = False; report['performance_eligible'] = False
        raise
    finally:
        if broker is not None:
            try:
                broker.close()
            except BaseException as issue:
                cleanup_error('close broker channel', issue)
        try:
            terminate_group(server)
        except BaseException as issue:
            cleanup_error('terminate broker and owned descendants', issue)
        try:
            report['broker_ownership'] = owned_process_record(server)
        except BaseException as issue:
            cleanup_error('record broker ownership', issue)
        if server_log is not None:
            try:
                server_log.close()
            except BaseException as issue:
                cleanup_error('close broker log', issue)
        try:
            if directory is not None and directory.exists():
                remaining = list(directory.iterdir())
                if remaining:
                    raise RuntimeError('owned broker directory retains entries: ' + ', '.join(path.name for path in remaining))
                directory.rmdir()
        except BaseException as issue:
            cleanup_error('retire owned broker directory', issue)
        try:
            report['admission_after'] = admission.admit()
        except BaseException as issue:
            cleanup_error('final cgroup admission', issue)
        report['seconds'] = time.monotonic() - started
        save()
    if not report['passed']:
        raise RuntimeError('latency cleanup/admission qualification failed; see results.json')
    print(json.dumps({'passed': True, 'rows': len(report['rows']), 'ratios': report['uwvm_to_native_query_time_ratios']}))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
