#!/usr/bin/env python3
"""Check ROS's production job resolver/build command with real Xmake/CMake/Ninja.

POSIX worker accounting measures actual concurrency. A tiny custom-command
project replaces LLVM compilation; this does not qualify a full LLVM/ROS build.
Run under the host's test resource supervisor.
"""
import argparse, hashlib, json, os, re, shutil, subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',required=True,type=Path)
parser.add_argument('--xmake',default=shutil.which('xmake'))
args=parser.parse_args()
if os.name!='posix': parser.error('POSIX worker accounting is required')
if not args.xmake: parser.error('provide --xmake or put Xmake on PATH')
repo=Path(__file__).resolve().parents[2]
w=args.out.resolve();w.mkdir(exist_ok=False)
shutil.copytree(repo/'test/0018.build/fixtures/bundled_llvm_jobs',w/'fixture')
# Exercise the actual product default, rather than masking it with a fixture default.
block=re.search(r'option\("llvm-build-jobs", function\(\)(.*?)\nend\)',(repo/'xmake/option.lua').read_text(),re.S)
assert block
value=re.search(r'set_default\("([^"\n]+)"\)',block.group(1)).group(1)
assert value=='inherit'
xmake=args.xmake
production=repo/'xmake/utility/bundled_llvm.lua'
source_sha=hashlib.sha256(production.read_bytes()).hexdigest()
env=dict(os.environ,UWVM_TEST_JOB_MODULE_ROOT=str(repo/'xmake'),UWVM_TEST_JOB_DEFAULT=value,PYTHONDONTWRITEBYTECODE='1',XMAKE_STATS='false')
rows=[]
cases=[('fresh-short',['-j1'],1),('same-cache-4',['-j4'],4),('build-long',['build','--jobs=3'],3),('target-default',['build','probe'],'default'),('alias-b',['b','-j2'],2),('above-old-limit',['-j32'],32),('standalone-inherit',['f','-y','--llvm-build-jobs=inherit'],1),('explicit-2',['f','-y','--llvm-build-jobs=2'],2),('override-outer-4',['-j4'],2),('invalid-zero',['f','-y','--llvm-build-jobs=0'],None),('invalid-false',['f','-y','--llvm-build-jobs=n'],None),('invalid-negative',['f','-y','--llvm-build-jobs=-1'],None),('invalid-overflow',['f','-y','--llvm-build-jobs=2147483648'],None),('invalid-fraction',['f','-y','--llvm-build-jobs=1.5'],None),('invalid-infinite',['f','-y','--llvm-build-jobs=inf'],None),('restore-inherit',['f','-y','--llvm-build-jobs=inherit'],1),('same-cache-1',['--jobs=1'],1)]
for label,args,expected in cases:
 trace=w/(label+'.json');env['UWVM_TEST_JOB_TRACE']=str(trace)
 run=subprocess.run([xmake,*args],cwd=w/'fixture',env=env,capture_output=True,timeout=90)
 (w/(label+'.log')).write_bytes(run.stdout+run.stderr)
 if expected is None:
  assert run.returncode and b'must be a positive integer' in run.stdout+run.stderr,(label,run.stdout,run.stderr)
  result=dict(rejected=True)
 else:
  assert run.returncode==0,(label,run.stdout.decode()[-3000:],run.stderr.decode()[-3000:])
  result=json.loads(trace.read_text());expected=result['default_jobs'] if expected=='default' else expected;assert result['jobs']==expected,(label,result)
  for phase in ['llvm','runtime']:
   observed=result[phase]
   assert observed==dict(current=0,peak=min(expected,8),started=8,finished=8),(label,phase,observed)
 rows.append(dict(label=label,args=args,expected=expected,returncode=run.returncode,result=result))
 (w/'results.json').write_text(json.dumps(dict(production_sha256=source_sha,cases=rows),indent=2)+'\n')
 print(label,'PASS',flush=True)
checked=json.loads((w/'fresh-short.json.check').read_text())
assert checked['jobs']==1 and checked.get('current_jobs') is None
if 'task' in checked: assert checked['task']=='config'
# The same CMake directory served every case: changing jobs needs neither a
# second cache nor a configure with a different persistent compile pool.
assert hashlib.sha256(production.read_bytes()).hexdigest()==source_sha
print('PASS: nested automatic config, same-cache job changes, explicit overrides, invalid requests, real CMake/Ninja concurrency',flush=True)
