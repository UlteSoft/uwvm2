from pathlib import Path
import argparse, hashlib, json, re, subprocess, sys

ap=argparse.ArgumentParser()
for key in ('uwvm','wasm','source','oracle','out','source-root'):
    ap.add_argument('--'+key,type=Path,required=True)
ap.add_argument('--ros',action='store_true')
ap.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
a=ap.parse_args()
assert sys.platform=='linux'
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
sys.path.insert(0,str(a.source_root/'test/0017.runtime'))
import run_debug_source_step_cli as step
import run_debug_language_experience_cli as language
a.out.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
paths=[a.uwvm,a.wasm,a.source,a.oracle,Path(__file__),Path(step.__file__),Path(language.__file__)]
before={str(p):sha(p) for p in paths}
row={'passed':False,'language':'zig','native_full_parity':False,'inputs_before':before}
s=None
try:
    mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
    argv=[str(a.uwvm),'-m','debug-jit',*mode,'-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm)]
    s=step.Session(argv,a.out/'console.log',step.code_expressions(a.wasm),step.line_sequences(a.oracle.read_text()),a.source)
    row.update(argv=argv,actions=s.actions,positions=s.positions)
    target,_=step.metadata_cli.function(a.wasm,'probe_outer')
    s.begin(target)
    markers={mark:i for i,line in enumerate(a.source.read_text().splitlines(),1) for mark in ('PROBE_VALUES','PROBE_SHADOW','PROBE_CALL','PROBE_AFTER','PROBE_OUTER') if '// '+mark in line}
    assert len(markers)==5
    origin=language.own_seek(s,lambda p:p['function']==target and p['line']==markers['PROBE_VALUES'] and p['is_statement'],'real Zig initialized outer values')
    row['values']=[]
    for expression,value in [('value',3),('packet.tag',3),('packet.grid[1][2]',15),('shadow',7)]:
        reply=language.scalar(s,origin,expression,value)
        row['values'].append({'expression':expression,'expected':value,'reply':reply.decode()})
    command=f'break-source 0 {origin["file"]}:{markers["PROBE_CALL"]}'
    reply=language.query(s,command);bid=re.search(rb'breakpoint (\d+)',reply)
    assert bid is not None,(command,reply)
    table=language.query(s,'info breakpoints')
    bad=language.query(s,f'break-source 0 {origin["file"]}.absent:{markers["PROBE_CALL"]}')
    assert b'error:' in bad and table==language.query(s,'info breakpoints')
    s.send('continue')
    for _ in range(20):
        stopped=s.send('wait')
        if b'stopped: breakpoint' in stopped:break
    else:raise AssertionError('genuine Zig source breakpoint did not stop')
    at=s.position();assert at['function']==target and at['line']==markers['PROBE_CALL'],at
    language.scalar(s,at,'inner_shadow',23)
    s.send('delete '+bid[1].decode())
    leaf,_=step.metadata_cli.function(a.wasm,'probe_leaf')
    for _ in range(16):
        at=s.source_step('into')
        if at['function']==leaf:break
    else:raise AssertionError('source into did not enter genuine Zig leaf')
    for _ in range(16):
        at=s.source_step('out')
        if at['function']==target:break
    else:raise AssertionError('source out did not return to genuine Zig caller')
    stale=language.query(s,f'print {s.thread} {origin["stop_id"]} shadow')
    assert b'error:' in stale and b'source-value stop=' not in stale
    s.finish_guest();row['passed']=True
except BaseException as e:
    row['error']=repr(e)
    raise
finally:
    if s is not None:
        try:s.console.finish()
        except BaseException as e:row.update(passed=False,close_error=repr(e));raise
        finally:
            row['quit_returncode']=s.console.child.returncode
            row['managed_shutdown_complete']=b'managed shutdown complete' in s.console.transcript
            if row['quit_returncode']!=0 or not row['managed_shutdown_complete']:row['passed']=False
    row['inputs_after']={str(p):sha(p) for p in paths}
    if row['inputs_after']!=before:row.update(passed=False,inputs_changed=True)
    (a.out/'summary.json').write_text(json.dumps(row,indent=2)+'\n')
assert row['passed']
print('PASS finite Zig source values, file/line breakpoint and real source into/out')
