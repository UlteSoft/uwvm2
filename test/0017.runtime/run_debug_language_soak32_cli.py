#!/usr/bin/env python3
"""Sustained actual product CLI regression, designated Linux cgroup only.

Fresh guest and controller for each case; both physical call-stack policies,
real language objects, C++ plain/const methods and reference/type names, Zig numeric reads,
AssemblyScript original-map source positions and physical source steps,
invalid/stale-stop rejection and native worker join.
No idle delay contributes to the requested duration.
"""
from __future__ import annotations
import argparse,hashlib,json,random,subprocess,sys,time
from pathlib import Path

def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--matrix',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
 ap.add_argument('--seconds',type=int,default=1800);a=ap.parse_args()
 if sys.platform!='linux' or not 1800<=a.seconds<=3000:raise RuntimeError('Linux cgroup and 30-50 minute active qualification required')
 root=Path(__file__).resolve().parents[2]
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 a.out.mkdir(parents=True,exist_ok=False);matrix=json.loads(a.matrix.read_text());assert len(matrix)==32
 assert len({label for label,_ in matrix})==32
 pinned={str(a.matrix):sha(a.matrix),str(Path(__file__)):sha(__file__)}
 for _,argv in matrix:
  for key in ('--uwvm','--wasm','--source','--oracle','--map'):
   if key not in argv:continue
   p=Path(argv[argv.index(key)+1]);pinned[str(p)]=sha(p)
  index=argv.index('python3');p=Path(argv[index+1]);pinned[str(p)]=sha(p)
 for p in (root/'test/0017.runtime').glob('run_debug_source_*_cli.py'):pinned[str(p)]=sha(p)
 for p in (root/'test/0017.runtime').glob('run_debug_language_*_cli.py'):pinned[str(p)]=sha(p)
 record={'passed':False,'requested_active_seconds':a.seconds,'inputs_before':pinned,'runs':[],
         'host_expression_execution':False,'guest_memory_modified_by_debugger':False,'random_seed':20261004}
 rng=random.Random(20261004);started=time.monotonic();round_index=0
 try:
  while time.monotonic()-started<a.seconds:
   order=list(range(len(matrix)));rng.shuffle(order)
   for case_index in order:
    label,original=matrix[case_index];argv=list(original);target=a.out/f'round-{round_index:04d}'/label
    argv[argv.index('--out')+1]=str(target);target.parent.mkdir(exist_ok=True)
    began=time.monotonic();log=target.with_suffix('.runner.log')
    with log.open('wb') as stream:result=subprocess.run(argv,stdout=stream,stderr=subprocess.STDOUT,timeout=90,check=False)
    if result.returncode!=0:raise AssertionError(('real soak case failed',label,round_index,result.returncode,str(log)))
    report=target/'summary.json';r=json.loads(report.read_text())
    assert r['passed'] and r['quit_returncode']==0 and r['managed_shutdown_complete'] and r['inputs_before']==r['inputs_after'],(label,round_index,str(report))
    record['runs'].append({'round':round_index,'case':label,'returncode':result.returncode,
      'summary':str(report),'summary_sha256':sha(report),'console_sha256':sha(target/'console.log'),
      'actions':len(r['actions']),'actual_positions':len(r['positions']),'wall_seconds':time.monotonic()-began,
      'real_guest_exit_and_native_worker_join':True})
   round_index+=1;record.update(completed_rounds=round_index,wall_seconds=time.monotonic()-started)
   (a.out/'progress.json').write_text(json.dumps(record,indent=2)+'\n')
   print(json.dumps({'rounds':round_index,'real_cli_runs':len(record['runs']),'active_wall_seconds':record['wall_seconds']}),flush=True)
  record['inputs_after']={p:sha(p) for p in pinned};assert record['inputs_after']==pinned
  subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
  record.update(passed=True,wall_seconds=time.monotonic()-started,total_controller_actions=sum(r['actions'] for r in record['runs']),total_actual_source_positions=sum(r['actual_positions'] for r in record['runs']))
 except BaseException as error:record.update(error=repr(error),wall_seconds=time.monotonic()-started);raise
 finally:(a.out/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
 print('PASS sustained actual language debugger CLI',record['wall_seconds'],len(record['runs']))

if __name__=='__main__':main()
