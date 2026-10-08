#!/usr/bin/env python3
"""Focused controller tests, only inside the configured SSH Linux cgroup."""
import argparse, hashlib, json, os, resource, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--llvm-build-include',type=Path,
               help='generated include directory of the matching bundled host LLVM')
p.add_argument('--llvm-build-lib',type=Path,
               help='static library directory of that same LLVM build')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
files=[*sorted((root/'src/uwvm2/uwvm/debugger').glob('*')),*sorted((root/'src/uwvm2/utils/control').glob('*')),
       root/'src/uwvm2/utils/thread/cooperative_pause_domain.h',root/'src/uwvm2/runtime/lib/uwvm_runtime.h',
       Path(__file__),Path(__file__).with_name('controller.cc'),Path(__file__).with_name('protocol.cc'),Path(__file__).with_name('wasm_events.cc'), root/'test/0003.utils/control/buffer_helpers.h',root/'test/0003.utils/control/posix_test_abi.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources={str(x.relative_to(root)):sha(x) for x in files if x.is_file()}
(a.out/'sources.json').write_text(json.dumps(sources,indent=2)+'\n')
for relative in sources:
 target=a.out/'sources'/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((root/relative).read_bytes())
common=[os.environ.get('CXX','/toolchain/bin/clang++'),'-std=c++26','-stdlib=libc++','-rtlib=compiler-rt',
        '-unwindlib=libunwind','-fuse-ld=lld','-pthread','-DUWVM_DISABLE_INT','-DUWVM_USE_LLVM_JIT','-I'+str(root/'src'),
        '-I'+str(root/'third-parties/fast_io/include'),'-I'+str(root/'third-parties/bizwen/include'),'-I'+str(root/'third-parties/boost_unordered/include')]
llvm_binding=None
controller_libraries=[]
if (root/'third-parties/llvm/llvm/include').is_dir():
 generated=a.llvm_build_include or os.environ.get('UWVM_TEST_LLVM_BUILD_INCLUDE')
 if generated is None:
  raise RuntimeError('bundled LLVM requires --llvm-build-include')
 generated=Path(generated).resolve(strict=True)
 if not (generated/'llvm/Config/llvm-config.h').is_file():
  raise RuntimeError('missing generated bundled LLVM configuration headers')
 common += ['-I'+str(root/'third-parties/llvm/llvm/include'),'-I'+str(generated)]
 llvm_binding={'source_manifest_sha256':sha(root/'third-parties/llvm/sources.sha256'),
               'generated_config_header':str(generated/'llvm/Config/llvm-config.h'),
               'generated_config_header_sha256':sha(generated/'llvm/Config/llvm-config.h')}
 libraries=(a.llvm_build_lib or generated.parent/'lib').resolve(strict=True)
 controller_libraries=[str(libraries/name) for name in ('libLLVMSupport.a','libLLVMDemangle.a')]
 llvm_binding['static_libraries']={name:sha(Path(name)) for name in controller_libraries}
 controller_libraries+=['-lm','-ldl']
def run(name,cmd):
 (a.out/(name+'.command')).write_text(shlex.join(map(str,cmd))+'\n')
 with (a.out/(name+'.log')).open('w') as log:
  subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
events_exe=a.out/'wasm-events'
run('wasm-events-build',common+['-O2',str(Path(__file__).with_name('wasm_events.cc')),'-o',str(events_exe)])
run('wasm-events-run',[str(events_exe)])
print((a.out/'wasm-events-run.log').read_text(),end='',flush=True)
for profile,flags in [('o3',['-O3']),('sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),
                      ('no-exceptions',['-O2','-fno-exceptions'])]:
 exe=a.out/profile
 run(profile+'-build',common+flags+[str(Path(__file__).with_name('controller.cc')),*controller_libraries,'-o',str(exe)])
 run(profile+'-run',[str(exe)])
 print((a.out/(profile+'-run.log')).read_text(),end='',flush=True)
 run(profile+'-session-build',common+flags+[str(root/'test/0003.utils/control/session.cc'),'-o',str(a.out/(profile+'-session'))])
 run(profile+'-session-run',[str(a.out/(profile+'-session'))])
 print((a.out/(profile+'-session-run.log')).read_text(),end='',flush=True)
 run(profile+'-protocol-build',common+flags+[str(Path(__file__).with_name('protocol.cc')),'-o',str(a.out/(profile+'-protocol'))])
 run(profile+'-protocol-run',[str(a.out/(profile+'-protocol'))])
 print((a.out/(profile+'-protocol-run.log')).read_text(),end='',flush=True)
single=a.out/'single-thread.cc'
single.write_text('#include <uwvm2/uwvm/debugger/impl.h>\nint main(){return uwvm2::uwvm::debugger::parse_console_command("help").kind != uwvm2::uwvm::debugger::console_command_kind::help;}\n')
run('single-thread-build',common+['-D__SINGLE_THREAD__','-O2',str(single),'-o',str(a.out/'single-thread')])
run('single-thread-run',[str(a.out/'single-thread')])
# Recompile the real authorization/protocol module graph and the console grammar
# partition in both feature configurations. Runtime-dependent debugger partitions
# are separately checked for complete imports; actual full graph is not claimed.
module_flags=[x for x in common if x not in ('-rtlib=compiler-rt','-unwindlib=libunwind','-fuse-ld=lld')]
for mode,extra in [('native',[]),('single',['-D__SINGLE_THREAD__'])]:
 fast_io_pcm=a.out/(mode+'-fast_io.pcm')
 run(mode+'-module-fast-io',module_flags+extra+['--precompile',str(root/'third-parties/fast_io/share/fast_io/fast_io.cppm'),'-o',str(fast_io_pcm)])
 bindings=['-fmodule-file=fast_io='+str(fast_io_pcm)]
 for name in ('protocol','sealed_input','session','linux_launch_channel','impl'):
  pcm=a.out/(mode+'-'+name+'.pcm')
  run(mode+'-module-'+name,module_flags+extra+bindings+['--precompile',str(root/'src/uwvm2/utils/control'/(name+'.cppm')),'-o',str(pcm)])
  module='uwvm2.utils.control'+('' if name=='impl' else ':'+name)
  bindings+=['-fmodule-file='+module+'='+str(pcm)]
 events_pcm=a.out/(mode+'-wasm-events.pcm')
 run(mode+'-module-wasm-events',module_flags+extra+bindings+['--precompile',str(root/'src/uwvm2/uwvm/debugger/wasm_events.cppm'),'-o',str(events_pcm)])
 bindings+=['-fmodule-file=uwvm2.uwvm.debugger:wasm_events='+str(events_pcm)]
 run(mode+'-module-command',module_flags+extra+bindings+['--precompile',str(root/'src/uwvm2/uwvm/debugger/command.cppm'),'-o',str(a.out/(mode+'-command.pcm'))])
# Scoped static audit supplements, never substitutes for the above real compiler.
checker=root/'test/0006.check_module/check_uwvm_module.py'
audit=a.out/'audit.py'
audit.write_text("import importlib.util,sys\nfrom pathlib import Path\np=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('module_audit',p);m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)\nm.SRC_ROOT=sys.argv[2]\nraise SystemExit(m.main())\n")
for folder in ('utils/control','uwvm/debugger'):
 run('audit-'+folder.replace('/','-'),['python3',str(audit),str(checker),str(root/'src/uwvm2'/folder)])
assert sources=={str(x.relative_to(root)):sha(x) for x in files if x.is_file()}
(a.out/'summary.json').write_text(json.dumps({'passed':True,'sources':sources,
 'profiles':['O3','ASan+UBSan+leaks','no-exceptions'],'actual_pause_domain':True,
 'test_trace_provider':True,'actual_VM':False,'bundled_llvm':llvm_binding,
 'cgroup':Path('/proc/self/cgroup').read_text()},indent=2)+'\n')
