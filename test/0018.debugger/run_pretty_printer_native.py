#!/usr/bin/env python3
"""Real GDB/LLDB standard-container controls with startup excluded from timing.

Compare these controls to an end-to-end stopped UWVM query of the same shape.
The separate copied-DATA formatter microbenchmark is not a latency competitor.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', type=Path, default=Path('/usr/bin/g++-15'))
    parser.add_argument('--gdb', type=Path, default=Path('/usr/bin/gdb'))
    parser.add_argument('--lldb', type=Path, default=Path('/usr/bin/lldb-21'))
    parser.add_argument('--iterations', type=int, default=2000)
    parser.add_argument('--maximum-memory-bytes', type=int, default=2684354560)
    args = parser.parse_args()
    if not 1 <= args.iterations <= 1000000:
        parser.error('bounded positive iterations required')
    if not 128 << 20 < args.maximum_memory_bytes <= 64 << 30:
        parser.error('allocated total ceiling must exceed 128MiB and remain within the existing 64GiB cgroup')
    spec = importlib.util.spec_from_file_location('pretty_matrix_admission', Path(__file__).with_name('run_pretty_printer_matrix.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    admission = module.admit()
    sys.path.insert(0, str(args.source_root / 'test/0050.high_level_languages'))
    from owned_process import remember_process, wait_process, terminate_group, owned_process_record
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    binary = args.out / 'native.elf'
    fixture = args.source_root / 'test/0018.debugger/pretty_printer_native.cc'
    commands = []

    def budget():
        current = int(Path('/sys/fs/cgroup' + module.CGROUP + '/memory.current').read_text())
        report['observed_memory_peak_bytes'] = max(report.get('observed_memory_peak_bytes', 0), current)
        root_free, output_free = shutil.disk_usage('/').free, shutil.disk_usage(args.out).free
        report['last_disk_observation'] = {'rootfs_free_bytes': root_free, 'outputfs_free_bytes': output_free}
        if current >= report['memory_stop_threshold_bytes']:
            raise RuntimeError('total cgroup memory.current reached the reactive stop threshold 128MiB below the allocated ceiling')
        if min(root_free, output_free) < module.DISK_RESERVE:
            raise RuntimeError('root/output free disk below the 3GiB reserve')

    def cleanup_error(label, issue):
        report['cleanup_errors'].append({'operation': label, 'error': type(issue).__name__ + ': ' + str(issue)})
        report['passed'] = False; report['performance_eligible'] = False

    def run(label, command, timeout):
        started = time.monotonic()
        child = None
        entry = {'label': label, 'argv': list(map(str, command))};commands.append(entry)
        try:
            with (args.out / (label + '.log')).open('wb') as stream:
                budget()
                child = subprocess.Popen(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT,
                                         start_new_session=True, env=dict(os.environ, LC_ALL='C', LANG='C'))
                remember_process(child)
                code = wait_process(child, timeout, observe=budget)
                entry['exit'] = code
        finally:
            try:
                terminate_group(child)
            except BaseException as issue:
                cleanup_error('terminate ' + label, issue)
            entry['process_wall_seconds'] = time.monotonic() - started
            try:
                entry['log_sha256'] = module.sha(args.out / (label + '.log'))
                entry['process_ownership'] = owned_process_record(child)
            except BaseException as issue:
                cleanup_error('record ' + label + ' ownership/log', issue)
        if report['cleanup_errors'] or entry.get('exit') != 0:
            raise RuntimeError(label + ' failed or cleanup incomplete; see log/receipt')

    report = {'passed': False, 'scope': 'real native libstdc++ objects, same three summaries and all sixteen vector children; startup excluded',
              'comparison_limit': 'separate copied-DATA formatter throughput is not an end-to-end debugger latency comparison',
              'admission': admission, 'commands': commands, 'rows': [], 'cleanup_errors': [], 'performance_eligible': True,
              'maximum_memory_bytes': args.maximum_memory_bytes,
              'memory_stop_threshold_bytes': args.maximum_memory_bytes - module.MEMORY_RESERVE,
              'reactive_memory_reserve_bytes': module.MEMORY_RESERVE, 'delegated_memory_cap': False,
              'disk_reserve_bytes': module.DISK_RESERVE,
              'inputs': {str(p.resolve()): module.sha(p) for p in (Path(__file__), fixture, args.cxx, args.gdb, args.lldb)}}
    try:
        run('compile', [args.cxx, '-std=c++20', '-O0', '-g', '-fno-omit-frame-pointer', fixture, '-o', binary], 60)
        gdb_script = args.out / 'gdb-control.py'
        gdb_script.write_text(r'''import gdb,json,time,re
gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('set print elements 200')
gdb.execute('set print pretty off')
gdb.execute('break pretty_printer_ready')
gdb.execute('run')
queries=['print values','print title','print view']
first=[gdb.execute(q,to_string=True) for q in queries]
assert 'length 16' in first[0] and 'capacity 32' in first[0]
match=re.search(r'=\s*\{([^{}]*)\}\s*$',first[0])
assert match is not None,first
items=[item.strip() for item in match[1].split(',')]
assert all(re.fullmatch(r'-?[0-9]+',item) for item in items),first
assert list(map(int,items))==[i*3+1 for i in range(16)],first
assert 'native pretty printer' in first[1] and 'length-delimited view' in first[2]
checksum=0
begin=time.perf_counter_ns()
for i in range(ITERATIONS):
    for q in queries:checksum+=len(gdb.execute(q,to_string=True))
elapsed=time.perf_counter_ns()-begin
print('PRETTY_NATIVE_JSON '+json.dumps({'passed':True,'debugger':'gdb','iterations':ITERATIONS,'queries':len(queries)*ITERATIONS,'nanoseconds':elapsed,'checksum':checksum,'first':first}))
gdb.execute('kill')
gdb.execute('quit')
'''.replace('ITERATIONS', str(args.iterations)))
        lldb_script = args.out / 'lldb_control.py'
        lldb_script.write_text(r'''import lldb,json,time,re
def run(debugger,command,result,internal_dict):
    ci=debugger.GetCommandInterpreter()
    def query(text):
        r=lldb.SBCommandReturnObject();ci.HandleCommand(text,r)
        assert r.Succeeded(),r.GetError()
        return r.GetOutput() or ''
    query('settings set target.max-children-count 200')
    query('settings set target.disable-aslr false')
    query('breakpoint set --name pretty_printer_ready')
    query('run')
    queries=['target variable values','target variable title','target variable view']
    first=[query(q) for q in queries]
    assert 'size=16' in first[0] or 'size = 16' in first[0],first
    pairs=re.findall(r'(?m)^\s*(?:\([^()\n]+\)\s*)?\[([0-9]+)\]\s*=\s*(-?[0-9]+)\s*,?\s*$',first[0])
    indices=re.findall(r'(?m)^\s*(?:\([^()\n]+\)\s*)?\[([0-9]+)\]\s*=',first[0])
    assert len(indices)==len(pairs)==16,first[0]
    assert [(int(i),int(value)) for i,value in pairs]==[(i,i*3+1) for i in range(16)],first
    assert 'native pretty printer' in first[1] and 'length-delimited view' in first[2],first
    checksum=0;begin=time.perf_counter_ns()
    for i in range(ITERATIONS):
        for q in queries:checksum+=len(query(q))
    elapsed=time.perf_counter_ns()-begin
    print('PRETTY_NATIVE_JSON '+json.dumps({'passed':True,'debugger':'lldb','iterations':ITERATIONS,'queries':len(queries)*ITERATIONS,'nanoseconds':elapsed,'checksum':checksum,'first':first}))
    query('process kill')
def __lldb_init_module(debugger,internal_dict):
    debugger.HandleCommand('command script add -f '+__name__+'.run pretty-control')
'''.replace('ITERATIONS', str(args.iterations)))
        run('gdb', [args.gdb, '-q', '-nx', '-batch', '-x', gdb_script, binary], 180)
        run('lldb', [args.lldb, '--no-lldbinit', '--batch', '-o', 'command script import ' + str(lldb_script), '-o', 'pretty-control', binary], 180)
        for debugger in ('gdb', 'lldb'):
            rows = [json.loads(line.split('PRETTY_NATIVE_JSON ', 1)[1]) for line in (args.out / (debugger + '.log')).read_text().splitlines()
                    if line.startswith('PRETTY_NATIVE_JSON ')]
            if len(rows) != 1 or rows[0].get('passed') is not True:
                raise RuntimeError('missing actual ' + debugger + ' result')
            report['rows'].extend(rows)
        report['binary_sha256'] = module.sha(binary)
        report['admission_after'] = module.admit()
        report['passed'] = True
    except BaseException as error:
        report['passed'] = False; report['performance_eligible'] = False
        report['error'] = type(error).__name__ + ': ' + str(error)
        raise
    finally:
        if report['cleanup_errors']:
            report['passed'] = False; report['performance_eligible'] = False
        (args.out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    if not report['passed']:
        raise RuntimeError('native control cleanup qualification failed; see results.json')
    print(json.dumps({'passed': True, 'rows': len(report['rows'])}))


if __name__ == '__main__':
    main()
