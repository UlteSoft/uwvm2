#!/usr/bin/env python3
"""Link focused fixtures against an already-qualified, unchanged runtime object.

This avoids recompiling the runtime for each development test. The source/dependency
fingerprint must equal the original build and remain unchanged throughout the run.
"""
import argparse,hashlib,json,os,re,shlex,subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runtime-build',type=Path,required=True)
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--define',action='append',default=[])
parser.add_argument('--success-marker',help='require this fixture-specific output in each successful run')
parser.add_argument('--ir-env',choices=['UWVM_ATOMIC_IR_DIR','UWVM_WAIT_IR_DIR','UWVM_SHARED_IR_DIR'])
args=parser.parse_args()
root=Path(__file__).resolve().parents[2];os.chdir(root)
subprocess.run(['bash','tools/ci/require_wasm3_test_cgroup.sh'],check=True)
out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
build=args.runtime_build.resolve()
def digest(path):return hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
def fingerprint(path):
 return subprocess.check_output(['python3','tools/ci/wasm3_source_fingerprint.py','.',str(path)],text=True).strip()
before=fingerprint(out/'source-before.json')
assert before==json.loads((build/'source-before.json').read_text())['source_id'],'runtime source/dependencies changed'
command=shlex.split((build/'test.command').read_text())
source=next(x for x in command if x.startswith('test/') and x.endswith('.cc'))
command[command.index(source)]=str(args.source)
command[command.index('-o')+1]=str(out/'fixture')
for define in args.define:
 assert re.fullmatch(r'UWVM2TEST_[A-Za-z_0-9]+(=[A-Za-z_0-9]+)?',define), 'only fixture-local defines may differ'
 command.append('-D'+define)
source_hash=digest(args.source);object_hash=digest(build/'runtime.o')
(out/'test.command').write_text(shlex.join(command)+'\n')
with (out/'build.log').open('w') as f:subprocess.run(command,stdout=f,stderr=f,check=True)
env=os.environ.copy()
if args.ir_env:env[args.ir_env]=str(out)
rows=[]
for policy in ['instruction','unwind']:
 command=['timeout','90',str(out/'fixture'),policy]
 with (out/(policy+'.run.log')).open('w') as f:result=subprocess.run(command,stdout=f,stderr=f,env=env)
 rows.append(dict(command=command,returncode=result.returncode))
 (out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 if result.returncode:raise RuntimeError(f'{policy} failed; inspect {out}')
 if args.success_marker and args.success_marker not in (out/(policy+'.run.log')).read_text():
  raise RuntimeError(f'{policy}: missing success marker; inspect {out}')
 print((out/(policy+'.run.log')).read_text())
subprocess.run(['bash','tools/ci/require_wasm3_test_cgroup.sh'],check=True)
assert before==fingerprint(out/'source-after.json')
assert source_hash==digest(args.source) and object_hash==digest(build/'runtime.o')
(out/'summary.json').write_text(json.dumps(dict(passed=True,source_id=before,test_sha256=source_hash,
 runtime_object_sha256=object_hash,runtime_build=str(build),binary_sha256=digest(out/'fixture')),indent=2)+'\n')
