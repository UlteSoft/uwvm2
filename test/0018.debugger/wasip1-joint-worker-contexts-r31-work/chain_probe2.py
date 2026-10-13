from pathlib import Path
import json,subprocess,sys,hashlib
D=Path(__file__).parent;repo=sys.argv[2];assert repo=='uwvm2'
O=D/'products/linux-integrated'/repo;S=D/'inputs'/repo/'test/0017.runtime/debug_wasip1_environment_group_runtime.cc'
data=S.read_text()
data=data.replace('bool worker_chain{};','bool worker_chain{}; ::std::size_t observed_points{};')
data=data.replace('if(where.code_unit!=self.main_module', 'if(self.worker_chain && self.observed_points++<64u) { ::fast_io::io::perrln("CHAIN_POINT module=",where.code_unit," function=",where.function," offset=",where.offset," target=",self.main_module); }\n        if(where.code_unit!=self.main_module')
data=data.replace('if(actual.status==lib::llvm_jit_checkpoint_capture_status::captured)', 'if(self.worker_chain) { ::fast_io::io::perrln("CHAIN_CAPTURE status=",static_cast<unsigned>(actual.status)); }\n        if(actual.status==lib::llvm_jit_checkpoint_capture_status::captured)')
data=data.replace('auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};', 'auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)}; auto recorded=lib::llvm_jit_observe_checkpoint_recording_host_api(); ::fast_io::io::perrln(\"CHAIN_RECORDING status=\",static_cast<unsigned>(recorded.status),\" frames=\",recorded.native_frames,\" current=\",recorded.at_current_opcode,\" site=\",recorded.site);')
source=D/'chain_probe2.cc';assert not source.exists();source.write_text(data)
rows=json.loads((O/'results.json').read_text());argv=next(r['argv'] for r in rows if r['name']=='environment-group-build')
argv=[str(source) if x==str(S) else str(D/'chain_probe2.d') if x==str(O/'environment-group.d') else str(D/'chain_probe2') if x==str(O/'environment-group') else x for x in argv]
with (D/'chain-probe2-build.log').open('xb') as f:completed=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT,timeout=1200)
assert completed.returncode==0
root=D/'chain-probe2-root';root.mkdir()
with (D/'chain-probe2-native.log').open('xb') as f:p=subprocess.run([str(D/'chain_probe2'),str(O/'worker-chain-main.wasm'),str(O/'worker-chain-provider.wasm'),'instruction',str(root),'worker-chain'],stdout=f,stderr=subprocess.STDOUT,timeout=60)
print((D/'chain-probe2-native.log').read_text(),flush=True)
(D/'chain-probe2.json').write_text(json.dumps(dict(exit=p.returncode,diagnostic_only=True,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),argv=argv),indent=2)+'\n')
