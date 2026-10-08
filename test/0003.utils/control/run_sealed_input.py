#!/usr/bin/env python3
"""Linux identity/alias input sealing plus real WASI32/64 path_open cold paths."""
import argparse, hashlib, json, os, resource, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[3];a.out=a.out.resolve()
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
files=[*sorted((root/'src/uwvm2/utils/control').glob('*')),Path(__file__),Path(__file__).with_name('sealed_input.cc'),Path(__file__).with_name('sealed_input_wasi.cc'),Path(__file__).with_name('sealed_input_query_errors.cc'), Path(__file__).with_name('posix_test_abi.h')]
files.append(Path(__file__).with_name('linux_console_socket_seal.cc'))
for name in ('path_open','path_open_wasm64'):
 for ext in ('.h','.cppm'):files.append(root/'src/uwvm2/imported/wasi/wasip1/func'/(name+ext))
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
sources={str(f.relative_to(root)):sha(f) for f in files if f.is_file()}
for path in sources:
 target=a.out/'sources'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((root/path).read_bytes())
(a.out/'sources.json').write_text(json.dumps(sources,indent=2)+'\n')
common=['/toolchain/bin/clang++','-std=c++26','-stdlib=libc++','-rtlib=compiler-rt','-unwindlib=libunwind','-fuse-ld=lld','-pthread',
 '-DUWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64','-I'+str(root/'src'),'-I'+str(root/'third-parties/fast_io/include'),
 '-I'+str(root/'third-parties/boost_unordered/include'),'-I'+str(root/'third-parties/bizwen/include')]
def run(name,cmd,cwd=None):
 (a.out/(name+'.command')).write_text(shlex.join(map(str,cmd))+'\n')
 with (a.out/(name+'.log')).open('w') as log:
  subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=cwd,timeout=180,
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
for profile,flags in [('o3',['-O3']),('sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),('no-exceptions',['-O2','-fno-exceptions'])]:
 exe=a.out/profile
 run(profile+'-build',common+flags+[str(Path(__file__).with_name('sealed_input.cc')),'-o',str(exe)])
 run(profile+'-run',[str(exe)])
 print((a.out/(profile+'-run.log')).read_text(),end='',flush=True)
 query=a.out/(profile+'-query')
 run(profile+'-query-build',common+flags+[str(Path(__file__).with_name('sealed_input_query_errors.cc')),'-o',str(query)])
 run(profile+'-query-run',[str(query)])
 print((a.out/(profile+'-query-run.log')).read_text(),end='',flush=True)
 socket=a.out/(profile+'-socket')
 run(profile+'-socket-build',common+flags+[str(Path(__file__).with_name('linux_console_socket_seal.cc')),'-o',str(socket)])
 run(profile+'-socket-run',[str(socket)])
 print((a.out/(profile+'-socket-run.log')).read_text(),end='',flush=True)
 if profile=='no-exceptions':continue
 wasi=a.out/(profile+'-wasi');work=a.out/(profile+'-work');work.mkdir()
 run(profile+'-wasi-build',common+flags+[str(Path(__file__).with_name('sealed_input_wasi.cc')),'-o',str(wasi)])
 run(profile+'-wasi-run',[str(wasi)],work)
 print((a.out/(profile+'-wasi-run.log')).read_text(),end='',flush=True)
module_flags=[x for x in common if x not in ('-rtlib=compiler-rt','-unwindlib=libunwind','-fuse-ld=lld')]
fast_io_pcm=a.out/'fast_io.pcm'
run('module-fast-io',module_flags+['--precompile',str(root/'third-parties/fast_io/share/fast_io/fast_io.cppm'),'-o',str(fast_io_pcm)])
module_flags+=['-fmodule-file=fast_io='+str(fast_io_pcm)]
run('module-sealed-input',module_flags+['--precompile',str(root/'src/uwvm2/utils/control/sealed_input.cppm'),'-o',str(a.out/'sealed_input.pcm')])
assert sources=={str(f.relative_to(root)):sha(f) for f in files if f.is_file()}
(a.out/'summary.json').write_text(json.dumps({'passed':True,'sources':sources,'actual_linux_file_identity':True,
 'actual_PTY_alias':True,'actual_wasi32_64_functions':True,'full_CLI':False,'cgroup':Path('/proc/self/cgroup').read_text()},indent=2)+'\n')
