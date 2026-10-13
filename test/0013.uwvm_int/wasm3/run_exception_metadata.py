#!/usr/bin/env python3
"""Run numeric EH metadata bounds/ownership checks in the constrained remote Linux build."""
import argparse,hashlib,json,os,resource,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--base-source-root',type=Path,help='Frozen complete source tree beneath a small test overlay')
a=p.parse_args();root=Path(__file__).resolve().parents[3];base=a.base_source_root or root
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
subprocess.run(['bash',str(base/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
a.out.mkdir(parents=True,exist_ok=False);out=a.out.resolve()
source=root/'test/0013.uwvm_int/wasm3/exception_metadata.cc'
headers=root/'src/uwvm2/runtime/compiler/uwvm_int/optable'
inputs=[source,Path(__file__).resolve(),*[headers/x for x in ('exception_metadata.h','exception_metadata.cppm','define.h','exception.h','exception_throw.h','call.h','storage.h')]]
def sha(path):
 with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
inputs += [root/('src/uwvm2/runtime/exception/'+x) for x in ('value.h','value.cppm','impl.h','impl.cppm')]
before={str(x.relative_to(root)):sha(x) for x in inputs}
flags=[os.environ.get('CXX','clang++'),'-std=c++26','-stdlib=libc++','-fexceptions','-fno-rtti','-O1','-g0','-fno-omit-frame-pointer','-fsanitize=address,undefined',
 '-fuse-ld=lld','-rtlib=compiler-rt','-unwindlib=libunwind','-DUWVM=2','-DUWVM_USE_UWVM_INT','-DUWVM_DISABLE_JIT','-DUWVM_USE_THREAD_LOCAL']
for path in [root/'src',base/'src',base/'third-parties/fast_io/include',base/'third-parties/bizwen/include',base/'third-parties/boost_unordered/include']:
 flags+=['-I',str(path)]
command=[*flags,str(source),'-o',str(out/'fixture'),'-pthread','-lm']
(out/'build.command').write_text(shlex.join(command)+'\n')
with (out/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
with (out/'run.log').open('w') as log:subprocess.run([str(out/'fixture')],stdout=log,stderr=subprocess.STDOUT,env=env,timeout=60,check=True)
assert 'PASS numeric exception metadata:' in (out/'run.log').read_text()
assert before=={str(x.relative_to(root)):sha(x) for x in inputs}
subprocess.run(['bash',str(base/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
(out/'summary.json').write_text(json.dumps(dict(passed=True,source=before,base=str(base),binary_sha256=sha(out/'fixture'),sanitizers=['address','undefined'],scope='cold numeric metadata dispatch only; compiler publication is tested separately'),indent=2)+'\n')
print((out/'run.log').read_text(),end='')
