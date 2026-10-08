from pathlib import Path
import json
import subprocess
import sys
import time

L = Path(__file__).parent
E = Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17')
D = E / 'rounds/wasip1-prepared-retirement-20261008-r34'
host = 'macromodel@100.123.133.75'
ssh = ['ssh', '-i', str(Path.home() / '.ssh/id_ed25519'), host]
scp = ['scp', '-i', str(Path.home() / '.ssh/id_ed25519')]
ledger = L / 'finish-v8-pipeline.json'
rows = json.loads(ledger.read_text()) if ledger.exists() else []


def run(label, argv):
    previous = next((r for r in rows if r['label'] == label and r['exit'] == 0), None)
    if previous:
        assert previous['argv'] == argv
        print('completed V8 remaining stage retained', label, flush=True)
        return
    print('R34 V8 REMAINING', label, flush=True)
    start = time.monotonic()
    result = subprocess.run(argv)
    rows.append(dict(label=label, argv=argv, exit=result.returncode, seconds=time.monotonic() - start))
    ledger.write_text(json.dumps(rows, indent=2) + '\n')
    assert result.returncode == 0, label


# This controller performs no tests outside the established cgroup guards.
# Wait for the compaction guard's physical retirement proof, not just its archive.
wait = """from pathlib import Path
import json,time
p=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/guard-joint-native-v8-products-consolidate-r34-windows-all.json')
start=time.monotonic();last=0
while not p.exists():
    assert time.monotonic()-start<3600
    if time.monotonic()-last>45:
        print('waiting for verified V8 native product consolidation',flush=True);last=time.monotonic()
    time.sleep(2)
q=json.loads(p.read_text())
assert q['passed'] and q['actual_root_exit']==0 and all(r['pidfd_retired'] for r in q['retirement'])
print('V8 compaction and actual process retirement verified',flush=True)
"""
import shlex
run('wait-verified-consolidation', ssh + ['python3 -u -c ' + shlex.quote(wait)])
wait_remaining = """from pathlib import Path
import json,time
E=Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17')
paths=[E/'guard-joint-historical-r29-windows-products-consolidate-r34-windows-all.json',
    E/'guard-joint-local-inputs-custody-r34-linux-all.json']
start=time.monotonic();last=0
while not all(p.exists() for p in paths):
    assert time.monotonic()-start<3600
    if time.monotonic()-last>45:
        print('waiting for historical compaction and original source archive custody proof',flush=True);last=time.monotonic()
    time.sleep(2)
for p in paths:
    q=json.loads(p.read_text())
    assert q['passed'] and q['actual_root_exit']==0 and all(r['pidfd_retired'] for r in q['retirement'])
print('Historical compaction and original source archive custody physically retired',flush=True)
"""
run('wait-verified-historical-compaction-and-custody', ssh + ['python3 -u -c ' + shlex.quote(wait_remaining)])
run('linux-windows-freebsd', ssh + ['python3', '-u', str(D / 'pipeline_v8_linux.py')])
for repo in ('uwvm2', 'uwvm2-ros'):
    guard = 'guard-joint-cross-macos-ordinary-12g-r34.py' if repo == 'uwvm2' else 'guard-joint-cross-r34.py'
    run('cross-macos-' + repo, ssh + ['python3', '-u', str(E / guard), 'joint-cross-r34', 'macos', repo])
    run('qualification-macos-' + repo, scp + [host + ':' + str(D / 'products-v8/macos' / repo / 'qualified.json'),
        str(L / (repo + '-macos-cross-qualified.json'))])
    run('native-macos-' + repo, [sys.executable, '-u', str(L / 'pipeline_macos.py'), repo])
print('V8 all 48 native executions complete; independent final report readback remains', flush=True)
