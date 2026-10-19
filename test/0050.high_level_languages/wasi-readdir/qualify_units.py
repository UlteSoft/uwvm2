#!/usr/bin/env python3
"""Offline strict qualification of retained fd_readdir unit logs, no subprocess.

In particular an OOB signal alone is insufficient: require the exact production
memory diagnostic for byte0x20000 beyond the two-page memory.
"""
import argparse
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from run_units import admission, digest, durable, require, MEMBERSHIP, FALSE_EOF


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    current = admission()
    require(current['affinity'] == [5], 'offline qualification reserved CPU5 required')
    a.run = a.run.resolve(strict=True)
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    result = {'passed': False, 'scope': 'offline requalification of native unit evidence only; no guest/backend claim',
              'admission': current, 'source_run': str(a.run), 'source_receipt_sha256': digest(a.run / 'receipt.json'),
              'qualifier_sha256': digest(__file__), 'rows': []}
    try:
        raw = json.loads((a.run / 'receipt.json').read_text())
        require(raw['passed'] is True, 'source unit driver did not complete')
        for side in ['admission_before', 'admission_after']:
            require(raw[side]['membership'] == MEMBERSHIP, 'source cgroup mismatch')
            require(raw[side]['boot'] == current['boot'], 'source and offline boot differ')
            require(raw[side]['events'] == current['events'], 'source/current memory events differ')
        rows = {row['name']: row for row in raw['runs']}
        require(len(rows) == len(raw['runs']) == 30, 'complete unique 12compile+6negative+6positive+6trap matrix required')
        for path, sha in raw['inputs'].items():
            require(digest(path) == sha, 'tested input changed: ' + path)
        expected = set()
        for repository in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            for width in [32, 64]:
                prefix = f'{repository}-wasm{width}-'
                for suffix, kind, code in [('before-compile', 'compile', 0), ('candidate-compile', 'compile', 0),
                                           ('before-false-eof', 'targeted-baseline-false-EOF', 42),
                                           ('candidate-boundary', 'candidate-positive', 0),
                                           ('candidate-oob', 'intentional-production-OOB-trap', -4)]:
                    name = prefix + suffix
                    expected.add(name)
                    row = rows[name]
                    require(row['qualified'] and row['retired'] and row['ownership']['closed'], 'owned retirement/qualification incomplete')
                    require(row['exit'] == code, 'unexpected exit: ' + name)
                    folder = a.run / 'commands' / name
                    require(json.loads((folder / 'receipt.json').read_text()) == row, 'command receipt differs from final row')
                    for filename in ['stdout.log', 'stderr.log']:
                        require(digest(folder / filename) == row[filename + '_sha256'], 'raw log changed: ' + name)
                    stdout = (folder / 'stdout.log').read_text()
                    stderr = (folder / 'stderr.log').read_text()
                    count = None
                    if kind == 'targeted-baseline-false-EOF':
                        require(stdout == FALSE_EOF and not stderr, 'baseline failure is not the precise false EOF')
                    elif kind == 'candidate-positive':
                        match = re.fullmatch(rf'fd_readdir wasm{width} partial-page contract: PASS checks=(\d+) records=42 tail=22\n', stdout)
                        require(match is not None and not stderr, 'candidate boundary witness invalid')
                        count = int(match.group(1))
                        require(count > 1000, 'boundary assertions unexpectedly absent')
                    elif kind == 'intentional-production-OOB-trap':
                        plain = re.sub(r'\x1b\[[0-9;]*m', '', stderr)
                        require(stdout == '' and plain == 'ABOUT_TO_CALL_OOB\nuwvm: [fatal] memory[0] page protection fault: '
                                '0x0000000000020000 (fault offset) >= 0x0000000000020000 (allocated)\n\n',
                                'signal lacks the exact production out-of-bounds diagnostic')
                    for member in row['ownership']['members']:
                        identity = member['identity']
                        path = Path('/proc') / str(identity['pid']) / 'stat'
                        try:
                            now = path.read_text().rpartition(')')[2].split()
                        except (FileNotFoundError, ProcessLookupError):
                            continue
                        require(int(now[19]) != identity['start'], 'retained owned identity still exists')
                    result['rows'].append({'name': name, 'class': kind, 'exit': code, 'assertions': count,
                                           'receipt_sha256': digest(folder / 'receipt.json')})
        require(set(rows) == expected, 'unexpected matrix row')
        result['counts'] = {kind: sum(row['class'] == kind for row in result['rows'])
                            for kind in ['compile', 'targeted-baseline-false-EOF', 'candidate-positive', 'intentional-production-OOB-trap']}
        result['owned_peak_observed_rss_bytes'] = max(row['rss_peak_observed_bytes'] for row in rows.values())
        after = admission()
        require(after['boot'] == current['boot'] and after['events'] == current['events'], 'offline admission changed')
        result['admission_after'] = after
        result['passed'] = True
    except BaseException as error:
        result['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        durable(a.out / 'receipt.json', result)
    print(json.dumps({'passed': result['passed'], 'counts': result['counts'], 'out': str(a.out)}))


if __name__ == '__main__':
    main()
