#!/usr/bin/env python3
"""Exercise real journal/locking and replay kernel alerts; never start PMU sampling."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from unittest.mock import patch

from debug_sampling_safety import SamplingSession, durable_write, sampling_options


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    args.out.mkdir(parents=True, exist_ok=False)
    result = {'passed': False, 'pmu_collection_started': False, 'checks': []}
    durable_write(args.out / 'result.json', json.dumps(result))
    first = SamplingSession(args.out)
    try:
        first.check(force=True)
        assert first.predicate(lambda data: data == b'done')(b'done')
        result['checks'].append('actual current-boot journal cursor and guarded wait')
        try:
            SamplingSession(args.out)
        except BlockingIOError:
            result['checks'].append('concurrent collection rejected')
        else:
            raise AssertionError('concurrent lock admitted')
        for message in ['perf: interrupt took too long (2516 > 2500)',
                        'INFO: NMI handler (perf_event_nmi_handler) took too long to run: 2.254 msecs',
                        'WARNING: kernel stack frame pointer has bad value',
                        'watchdog: BUG: soft lockup - CPU#20 stuck',
                        'rcu: INFO: rcu_preempt detected stalls',
                        'Out of memory: Killed process 1']:
            row = {'__CURSOR': 'replayed', 'MESSAGE': message}
            response = subprocess.CompletedProcess([], 0, (json.dumps(row) + '\n-- cursor: replayed\n').encode(), b'')
            old_cursor = first.cursor
            with patch('debug_sampling_safety.subprocess.run', return_value=response):
                try:
                    first.check(force=True)
                except RuntimeError as error:
                    assert 'Collection stopped after kernel alert:' in str(error)
                else:
                    raise AssertionError('kernel alert ignored: ' + message)
            assert json.loads((args.out / 'kernel-alerts.json').read_text())[-1]['MESSAGE'] == message
            first.cursor = old_cursor
            first.events.clear()
            result['checks'].append('replay abort: ' + message)
        response = subprocess.CompletedProcess([], 1, b'', b'journal unavailable')
        with patch('debug_sampling_safety.subprocess.run', return_value=response):
            try:
                first.check(force=True)
            except RuntimeError as error:
                assert 'monitoring unavailable' in str(error)
            else:
                raise AssertionError('unavailable monitoring admitted')
        result['checks'].append('monitor failure rejects capture')
    finally:
        first.close()
    reopened = SamplingSession(args.out)
    reopened.close()
    result['checks'].append('lock released and reacquired')
    target = args.out / 'atomic.json'
    durable_write(target, '{"old":true}')
    with patch('debug_sampling_safety.os.replace', side_effect=OSError('injected publication failure')):
        try:
            durable_write(target, '{"new":true}')
        except OSError:
            pass
        else:
            raise AssertionError('publication failure ignored')
    assert json.loads(target.read_text()) == {'old': True}
    assert not list(args.out.glob('atomic.json.*'))
    durable_write(target, '{"new":true}')
    assert json.loads(target.read_text()) == {'new': True}
    result['checks'].append('failed atomic publication preserves previous receipt; retry succeeds')
    for kind in ['hotspots', 'uarch-exploration']:
        for mode in ['summary', 'detailed']:
            options = sampling_options(kind, mode, next(iter(os.sched_getaffinity(0))))
            assert options[:4] == ['-duration', '30', '-knob', 'sampling-interval=10']
            assert options[4:6] == ['-cpu-mask', str(next(iter(os.sched_getaffinity(0))))]
            assert 'enable-stack-collection=false' in options
            assert 'enable-stack-collection=true' not in options
            if kind == 'uarch-exploration':
                assert 'pmu-collection-mode=' + mode in options
    result['checks'].append('bounded no-stack single-CPU configuration for both hardware analyses')
    for kind in ['hotspots','uarch-exploration']:
        for mode in ['summary','detailed']:
            options=sampling_options(kind,mode,next(iter(os.sched_getaffinity(0))),50)
            assert options[:4]==['-duration','30','-knob','sampling-interval=50']
            assert 'enable-stack-collection=false' in options
    low_rate=SamplingSession(args.out,sampling_interval_ms=50)
    try:
        assert json.loads((args.out/'sampling-safety.json').read_text())['sampling_interval_ms']==50
        low_rate.check(force=True)
    finally:low_rate.close()
    result['checks'].append('lower-rate 50ms options and actual journal receipt, no collection')
    for interval in [0,1,101,1000,True,50.0]:
        for invoke in [lambda:sampling_options('hotspots','summary',next(iter(os.sched_getaffinity(0))),interval),
                       lambda:SamplingSession(args.out,sampling_interval_ms=interval)]:
            try:invoke()
            except ValueError:pass
            else:raise AssertionError('unsafe sampling interval admitted')
    result['checks'].append('invalid or unbounded sampling intervals rejected before lock/collection')

    # Persisted replay data is explicitly synthetic and never a real kernel fault.
    result.update(passed=True, alert_input='unit replay; actual kernel unchanged',
                  boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
                  cgroup=Path('/proc/self/cgroup').read_text().strip(),
                  affinity=sorted(os.sched_getaffinity(0)))
    durable_write(args.out / 'result.json', json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
