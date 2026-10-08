from pathlib import Path
import hashlib,json,difflib
B=Path('/Users/liyinan/Documents/MacroModel/src');L=Path("/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0018.debugger/wasip1-source-initializer-r38-work")
before=json.loads((L/'before-edit-sha256.json').read_text());updates={}
sha=lambda b:hashlib.sha256(b).hexdigest()
def change(key,old,new):
 s=updates.get(key,(B/key).read_text());assert s.count(old)==1,(key,old)
 updates[key]=s.replace(old,new)
for repo in ('uwvm2','uwvm2-ros'):
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime.h'
 change(k,'dispatch_binding_preparation_declined, indirect_binding_preparation_declined };','dispatch_binding_preparation_declined, indirect_binding_preparation_declined, source_initializer_binding_preparation_declined };')
 change(k,'::std::size_t maximum_private_native_endpoint_functions{65536u};','::std::size_t maximum_private_native_endpoint_functions{65536u};\n        // Final main/preload initializer correspondence; no source seal is issued.\n        ::std::size_t maximum_private_source_initializer_modules{4096u};')
 change(k,'bool runtime_native_endpoint_capture_prepared{}; // Private DATA; LIVE seal/epoch are not issued.','bool runtime_native_endpoint_capture_prepared{}; // Private DATA; LIVE seal/epoch are not issued.\n        ::std::size_t prepared_source_initializer_modules{}, prepared_source_initializer_preloads{};\n        bool runtime_source_initializer_bindings_prepared{}; // Fixed private DATA, no initializer serial.')
 change(k,'bool candidate_world_retained{}; // Diagnostic only; no world/entry is exposed.','bool candidate_world_retained{}; // Diagnostic only; no world/entry is exposed.\n        bool source_initializer_bindings_rechecked_after_physical_join{}; // Actual closed-world preflight only.')
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_resources.h'
 change(k,'# include "uwvm_runtime_checkpoint_world_frames.h"','# include "uwvm_runtime_checkpoint_world_source_initializer.h"\n# include "uwvm_runtime_checkpoint_world_frames.h"')
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_preparation_api.h'
 change(k,'            out.modules=prepared->modules_.size();','            out.modules=prepared->modules_.size();\n            out.status=llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined;\n            if(!prepared->prepare_private_source_initializer_bindings(source,epoch,request.maximum_private_source_initializer_modules,out))\n            { out.native_payload_bytes=prepared->native_bytes_;out.resource_diagnostic=static_cast<unsigned>(prepared->phase_);return; }')
 change(k,'            out.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded;','            if(!prepared->preflight_private_source_initializer_bindings())\n            { out.status=llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined;return; }\n            out.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded;')
 k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_native_retirement_state.h'
 change(k,'        actual.result.candidate_world_retained=true;\n        actual.result.status=outcome::prepared_world_ready_closed;','        // Recheck the complete fixed source/parser/dense binding roster and\n        // unsealed candidate GC state AFTER this authentic drain and OS/TLS\n        // join. A copied diagnostic cannot satisfy these ownership checks.\n        if(!actual.candidate->preflight_private_source_initializer_bindings())\n        { actual.result.status=outcome::failed_closed;return; }\n        actual.result.source_initializer_bindings_rechecked_after_physical_join=true;\n        actual.result.candidate_world_retained=true;\n        actual.result.status=outcome::prepared_world_ready_closed;')
 k=repo+'/test/0017.runtime/debug_checkpoint_prepared_retirement_runtime.cc'
 change(k,'    invalid=prepare_request;invalid.maximum_private_dispatch_bindings=1u;','    invalid=prepare_request;invalid.maximum_private_source_initializer_modules=0u;\n    denied=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline());\n    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined &&\n        !denied.operation && !denied.candidate_world_retained && !denied.preparation.runtime_source_initializer_bindings_prepared && denied.preparation.engines==0u);\n    REQUIRE(state->control->capture(state->ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch);\n    invalid=prepare_request;invalid.maximum_private_dispatch_bindings=1u;')
 change(k,'    REQUIRE(pending.preparation.runtime_dispatch_bindings_prepared &&','    REQUIRE(pending.preparation.runtime_source_initializer_bindings_prepared &&\n        pending.preparation.prepared_source_initializer_modules==pending.preparation.modules && pending.preparation.prepared_source_initializer_preloads==0u &&\n        !pending.source_initializer_bindings_rechecked_after_physical_join);\n    REQUIRE(pending.preparation.runtime_dispatch_bindings_prepared &&')
 change(k,'joined.status==outcome::prepared_world_ready_closed && joined.operation==operation &&','joined.status==outcome::prepared_world_ready_closed && joined.operation==operation && joined.source_initializer_bindings_rechecked_after_physical_join &&')
 k=repo+'/test/0017.runtime/debug_wasip1_prepared_retirement_runtime.cc'
 change(k,'    auto limited=request;limited.maximum_private_dispatch_bindings=1u;','    auto limited=request;limited.maximum_private_source_initializer_modules=0u;\n    auto source_refused{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,limited,deadline())};\n    require(source_refused.status==outcome::preparation_declined && source_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined &&\n        !source_refused.operation && !source_refused.candidate_world_retained && !source_refused.preparation.runtime_source_initializer_bindings_prepared && source_refused.preparation.engines==0u,\n        "source initializer quota refuses before engine allocation and old WASI retirement");\n    require(state->control->capture(state->ticket).result==::uwvm2::utils::thread::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"source binding refusal preserves the same pause and runtime epoch");\n    limited=request;limited.maximum_private_dispatch_bindings=1u;')
 change(k,'    require(pending.preparation.runtime_dispatch_bindings_prepared &&','    require(pending.preparation.runtime_source_initializer_bindings_prepared &&\n        pending.preparation.prepared_source_initializer_modules==pending.preparation.modules && pending.preparation.prepared_source_initializer_preloads==0u &&\n        !pending.source_initializer_bindings_rechecked_after_physical_join,"source initializer bindings remain private before physical join");\n    require(pending.preparation.runtime_dispatch_bindings_prepared &&')
 change(k,'ready.status==outcome::prepared_world_ready_closed && ready.native_workers_joined && ready.candidate_world_retained','ready.status==outcome::prepared_world_ready_closed && ready.native_workers_joined && ready.candidate_world_retained && ready.source_initializer_bindings_rechecked_after_physical_join')
 k=repo+'/src/uwvm2/uwvm/debugger/wasip1_checkpoint.md'
 s=(B/k).read_text();updates[k]=s+"""

候选整体 world 还会预先准备 main/preload 的 source initializer 对应表：每个记录必须匹配新 source 实际 parser 文件、不可移动 registry 成员及真实 dense 顺序，GC staging 资源全部填充且保持未发布。maximum_private_source_initializer_modules 默认 4096，并在创建新 engine 前拒绝超额；内存由 maximum_native_payload_bytes 计费。真实旧 guest drain 和 OS/TLS join 完成后，管理器再次检查该表、旧 world 的当前 source/epoch/initializer serial 和实际 builtin WASIp1 loader 归属，才报告 prepared_world_ready_closed。两个 source/epoch 的完整发布与恢复线程启动仍是独立未完成步骤；这里没有发放新 initializer serial、source seal、执行权限或 VM/host ASM 调试上下文。Wasm 与 WASIp1 仍须在同一停止点一起 checkpoint。
"""
assert set(updates)==set(before)
assert all(sha((B/k).read_bytes())==h for k,h in before.items()),'Concurrent source edits detected'
header='src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_world_source_initializer.h'
data=(B/'uwvm2'/header).read_bytes();assert not (B/'uwvm2-ros'/header).exists()
for k,s in updates.items():(B/k).write_text(s)
(B/'uwvm2-ros'/header).write_bytes(data)
patch=[]
for key in sorted(updates):
 patch+=difflib.unified_diff((L/'before'/key).read_text().splitlines(True),(B/key).read_text().splitlines(True),fromfile='a/'+key,tofile='b/'+key)
for repo in ('uwvm2','uwvm2-ros'):
 key=repo+'/'+header;patch+=difflib.unified_diff([],data.decode().splitlines(True),fromfile='/dev/null',tofile='b/'+key)
(L/'owned-source-changes.patch').write_text(''.join(patch))
print('Updated 14 existing files and 2 new headers with source CAS')
