from pathlib import Path
import json,hashlib,os,difflib,tarfile,io
B=Path('/Users/liyinan/Documents/MacroModel/src');W=B/'uwvm2/test/0018.debugger/wasip1-source-initializer-r38-work'
M=json.loads((W/'syntax-overlay-v2.json').read_text());assert all(hashlib.sha256((B/k).read_bytes()).hexdigest()==v for k,v in M.items())
plans=[]
def add(repo,relative,old,new):
 p=B/repo/relative;data=p.read_bytes();s=data.decode();assert s.count(old)==1,(str(p),s.count(old));t=s.replace(old,new,1).encode();plans.append((p,data,t))
for repo in ('uwvm2','uwvm2-ros'):
 rel='src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_staged_llvm_full_engine.h'
 p=B/repo/rel;data=p.read_bytes();s=data.decode()
 old='{ ok, invalid_input, invalid_body, unsupported_native, lowering_declined, exhausted, native_failure, native_endpoint_failure };'
 new='{ ok, invalid_input, invalid_body, unsupported_native, lowering_declined, exhausted, native_failure, native_endpoint_failure,\n          native_endpoint_observer_failure, native_endpoint_freeze_failure };'
 assert s.count(old)==1;s=s.replace(old,new)
 old='{ return {{},preparation_status::native_endpoint_failure}; }\n                }';assert s.count(old)==1;s=s.replace(old,'{ return {{},preparation_status::native_endpoint_observer_failure}; }\n                }')
 old='if(!data->freeze_native_endpoint_capture())\n                { return {{},preparation_status::native_endpoint_failure}; }';assert s.count(old)==1;s=s.replace(old,'if(!data->freeze_native_endpoint_capture())\n                { return {{},preparation_status::native_endpoint_freeze_failure}; }')
 plans.append((p,data,s.encode()))
 add(repo,'src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_resources.h',
 '                { if(phase_==preparation_status::resources_prepared) { (void)fail(preparation_status::type_mismatch); }return false; }',
 '                {\n                    // The actual loader calls THIS world\'s quota callback. Keep\n                    // that refusal instead of reporting a broken endpoint table.\n                    if(phase_==preparation_status::quota_exceeded)\n                    { engine_diagnostic_=static_cast<unsigned>(engine_type::preparation_status::exhausted); }\n                    else if(phase_==preparation_status::resources_prepared) { (void)fail(preparation_status::type_mismatch); }\n                    return false;\n                }')
 add(repo,'src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_preparation_api.h',
 '            { out.engine_diagnostic=prepared->engine_diagnostic_;out.native_payload_bytes=prepared->native_bytes_;return; }',
 '            {\n                out.engine_diagnostic=prepared->engine_diagnostic_;out.native_payload_bytes=prepared->native_bytes_;\n                out.resource_diagnostic=static_cast<unsigned>(prepared->phase_);return;\n            }')
 for filename in ('debug_checkpoint_complete_instance_runtime.cc','debug_checkpoint_complete_preload_runtime.cc'):
  rel='test/0017.runtime/'+filename
  add(repo,rel,'                    prepare.recording_label=label;prepare.graph_budget=cap;',
   '                    prepare.recording_label=label;prepare.graph_budget=cap;\n                    // Thousands of native resume landings also reserve the\n                    // complete object/symbol workspace. This explicit test\n                    // budget preserves the public 256 MiB default and all\n                    // real process/cgroup limits.\n                    prepare.maximum_native_payload_bytes=512u*1024u*1024u;')
 # Add one authentic native object quota failure per policy, after proving a\n # full candidate fits and before testing worker/root quotas.
 p=B/repo/'test/0017.runtime/debug_checkpoint_complete_instance_runtime.cc'
 entry=next(i for i,(x,_,_) in enumerate(plans) if x==p);_,data,t=plans[entry];s=t.decode()
 old='                    auto workers_small=prepare;workers_small.maximum_private_root_workers=1u;'
 new='''#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                    if(positives==0u)
                    {
                        auto object_small=prepare;object_small.maximum_native_payload_bytes=1024u*1024u;
                        auto object_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,object_small);
                        REQUIRE(object_refused.status==lib::llvm_jit_checkpoint_prepare_status::engine_preparation_declined &&
                            object_refused.engine_diagnostic==5u && object_refused.resource_diagnostic==9u &&
                            object_refused.engines==0u && !object_refused.runtime_native_endpoint_capture_prepared &&
                            object_refused.native_payload_bytes<=object_small.maximum_native_payload_bytes);
                        REQUIRE(gc::current_root_frames()==inherited &&
                            lib::observe_compiler_runtime_generation_host_api()==source_epoch &&
                            gc::published_initializer_serial.load(::std::memory_order_acquire)==source_serial);
                    }
#endif
'''+old
 assert s.count(old)==1;s=s.replace(old,new);plans[entry]=(p,data,s.encode())
 rel='src/uwvm2/uwvm/debugger/wasip1_checkpoint.md';p=B/repo/rel;data=p.read_bytes()
 appendix='''

R38 native object quota diagnostics: the private actual-object/symbol ledger
reserves 16 times the loaded object byte count, plus individual symbol storage,
before decoding. The public native payload default stays 256 MiB. Large modules
with thousands of instruction resume landings may need an explicitly larger
request budget; the complete instance/preload tests use 512 MiB and continue to
exercise smaller budget refusal. A loader quota refusal now reports the actual
resource phase (quota_exceeded, diagnostic 9) and engine exhausted (diagnostic
5), while malformed endpoint declarations, observer setup and freeze checks
retain distinct engine diagnostics (7, 8 and 9). These are private preparation
results; no candidate is published and the original paused world stays intact.
'''
 plans.append((p,data,data+appendix.encode()))
assert len(plans)==12
backup=W/'before-native-quota-fix';backup.mkdir()
for p,data,t in plans:
 q=backup/p.relative_to(B);q.parent.mkdir(parents=True,exist_ok=True)
 with q.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
 assert p.read_bytes()==data
 temp=p.with_name(p.name+'.r38-native-quota.tmp');assert not temp.exists()
 with temp.open('xb') as f:f.write(t);f.flush();os.fsync(f.fileno())
 os.chmod(temp,p.stat().st_mode&0o777)
 assert p.read_bytes()==data;os.replace(temp,p)
m={k:hashlib.sha256((B/k).read_bytes()).hexdigest() for k in M}
(W/'syntax-overlay-v3.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(W/'syntax-overlay-v3.tar.gz','w:gz') as archive:
 for k in m:
  data=(B/k).read_bytes();info=tarfile.TarInfo(k);info.size=len(data);info.mode=0o444;info.mtime=0;archive.addfile(info,io.BytesIO(data))
changed={}
for p in (W/'before').rglob('*'):
 if p.is_file():
  k=str(p.relative_to(W/'before'));changed[k]=(p.read_text(),(B/k).read_text())
for repo in ('uwvm2','uwvm2-ros'):
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_source_initializer.h';changed[k]=('',(B/k).read_text())
patch=''.join(''.join(difflib.unified_diff(a.splitlines(True),z.splitlines(True),fromfile='a/'+k,tofile='b/'+k)) for k,(a,z) in sorted(changed.items()))
(W/'owned-source-changes.patch').write_text(patch)
print('atomic source updates',len(plans),'frozen inputs',len(m),'manifest sha',hashlib.sha256((W/'syntax-overlay-v3.json').read_bytes()).hexdigest())
