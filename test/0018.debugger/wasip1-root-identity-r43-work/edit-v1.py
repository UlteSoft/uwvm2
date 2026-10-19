from pathlib import Path
import hashlib,json,os,tempfile
P=Path('/Users/liyinan/Documents/MacroModel/src');W=P/'uwvm2/test/0018.debugger/wasip1-root-identity-r43-work'
B=json.loads((W/'before.json').read_text());changes={};cpp_hunks=[]
sha=lambda data:hashlib.sha256(data).hexdigest()
def edit(repo,rel,old,new):
 k=repo+'/'+rel;text=changes.get(k,(W/'before'/k).read_text())
 assert text.count(old)==1,(k,old[:100],text.count(old));changes[k]=text.replace(old,new,1)
def cpp(old,new):
 cpp_hunks.append(dict(before=old,after=new))
 for repo in ('uwvm2','uwvm2-ros'):edit(repo,'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp',old,new)
cpp('namespace uwvm2::runtime::lib\n{','namespace uwvm2::runtime::lib\n{\n#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)\n    class runtime_checkpoint_world_transaction;\n#endif')
cpp('#include "uwvm_runtime_checkpoint_materialization_identity.h"\n        struct full_code_publication',
    '#include "uwvm_runtime_checkpoint_materialization_identity.h"\n        struct full_code_publication;\n#include "uwvm_runtime_checkpoint_root_generation_identity_decl.h"\n        struct full_code_publication')
cpp('            ::std::unique_ptr<checkpoint_materialization_identity_data> checkpoint_compilation_identity{};',
    '            ::std::unique_ptr<checkpoint_materialization_identity_data> checkpoint_compilation_identity{};\n            ::std::unique_ptr<checkpoint_root_generation_identity> checkpoint_root_identity{};')
cpp('            // [record-owned complete engine/context] Every symbol resolved before',
    '#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)\n            if(rec.llvm_jit_full_publication->checkpoint_profile)\n            {\n                rec.llvm_jit_full_publication->checkpoint_root_identity =\n                    checkpoint_root_generation_publication_bridge::prepare(rec,materialized_module_id);\n                if(!rec.llvm_jit_full_publication->checkpoint_root_identity) { return false; }\n            }\n#endif\n            // [record-owned complete engine/context] Every symbol resolved before')
cpp('                                                                  ::std::memory_order_release);\n            }\n',
    '                                                                  ::std::memory_order_release);\n            }\n#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)\n            if(rec.llvm_jit_full_publication->checkpoint_profile &&\n               !checkpoint_root_generation_publication_bridge::seal_actual_full(rec)) { return false; }\n#endif\n')
cpp('#include "uwvm_runtime_native_owner_publication_impl.h"\n',
    '#include "uwvm_runtime_native_owner_publication_impl.h"\n#include "uwvm_runtime_checkpoint_root_generation_identity_impl.h"\n')
cpp('        if(generation != 1u)\n        {\n            if(local >= record.llvm_jit_checkpoint_generation_owners.size()',
    '#if defined(UWVM_CPP_EXCEPTIONS)\n        if(!checkpoint_root_generation_publication_bridge::matches_current_root(record,local,generation))\n#else\n        if(generation != 1u)\n#endif\n        {\n            if(local >= record.llvm_jit_checkpoint_generation_owners.size()')
for repo in ('uwvm2','uwvm2-ros'):
 lib='src/uwvm2/runtime/lib/'
 edit(repo,lib+'uwvm_runtime_native_owner_publication_decl.h',
      '        [[nodiscard]] static bool resolve_actual_full(',
      '        [[nodiscard]] static bool has_actual_sealed_full_root(compiled_module_record const&) noexcept;\n        [[nodiscard]] static bool resolve_actual_full(')
 edit(repo,lib+'uwvm_runtime_native_owner_publication_impl.h',
      '    [[nodiscard]] bool native_owner_publication_bridge::resolve_actual_full(',
      '''    [[nodiscard]] bool native_owner_publication_bridge::has_actual_sealed_full_root(compiled_module_record const& supplied) noexcept
    {
        if(get_runtime_state_publication_depth()!=1u) { return false; }
        auto selected=g_runtime.modules.end();
        for(auto it=g_runtime.modules.begin();it!=g_runtime.modules.end();++it)
        { if(::std::addressof(*it)==::std::addressof(supplied)) { selected=it;break; } }
        if(selected==g_runtime.modules.end()) { return false; }
        auto const& record=*selected;auto const* publication=record.llvm_jit_full_publication.get();
        if(!publication || !publication->engine || !publication->context || !publication->actual_native_endpoints ||
           !::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(publication->source)) { return false; }
        auto const& image=*publication->actual_native_endpoints;auto const epoch=current_runtime_generation();
        return epoch!=0u && image.valid_ && image.sealed_ && !image.staged_only_ && !image.attached_ &&
            image.engine_==publication->engine.get() && image.code_publication_==publication &&
            image.module_==record.runtime_module && image.runtime_epoch_==epoch &&
            image.module_id_<g_runtime.modules.size() &&
            ::std::addressof(g_runtime.modules.index_unchecked(image.module_id_))==::std::addressof(record) &&
            image.source_.get()==publication->source.get() && !image.source_.owner_before(publication->source) &&
            !publication->source.owner_before(image.source_) && publication->debug_source_runtime_epoch==epoch &&
            record.llvm_jit_debug_source_fused_epoch==epoch &&
            image.source_->actual_validated_file(image.module_id_,epoch,record.runtime_module)!=nullptr;
    }
    [[nodiscard]] bool native_owner_publication_bridge::resolve_actual_full(''')
 edit(repo,lib+'uwvm_runtime_native_owner_publication_impl.h','        if(generation!=1u)\n        {\n            auto const position',
      '        if(!checkpoint_root_generation_publication_bridge::matches_current_root(rec,local,generation))\n        {\n            auto const position')
 edit(repo,lib+'uwvm_runtime_checkpoint_world_full_code_handoff.h',
      '           publication->debug_source_binding || publication->checkpoint_compilation_identity || record.llvm_jit_ready ||',
      '           publication->debug_source_binding || publication->checkpoint_compilation_identity || record.llvm_jit_ready ||\n           !checkpoint_root_generation_publication_bridge::matches_private_prepared(record,dense) ||')
 edit(repo,lib+'uwvm_runtime_checkpoint_world_full_code_handoff.h',
      '    for(::std::size_t dense{};dense<engines_.size();++dense)\n    {\n        auto& staged=*engines_[dense];auto& record=',
      '''    // Charge all final immutable identity allocations before ownership transfer.
    if(!charge_native(engines_.size(),checkpoint_root_generation_publication_bridge::allocation_bytes()) ||
       !charge_native(prepared_publication_functions_,checkpoint_root_generation_publication_bridge::row_bytes())) { return false; }
    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto& staged=*engines_[dense];auto& record=''')
 edit(repo,lib+'uwvm_runtime_checkpoint_world_full_code_handoff.h',
      '    full_code_handoff_complete_=true;',
      '''    for(::std::size_t dense{};dense<engines_.size();++dense)
    {
        auto& record=indirect_dispatch_modules_.index_unchecked(dense);
        record.llvm_jit_full_publication->checkpoint_root_identity=
            checkpoint_root_generation_publication_bridge::prepare(record,dense);
        if(!record.llvm_jit_full_publication->checkpoint_root_identity) { return fail(preparation_status::allocation_failed); }
    }
    full_code_handoff_complete_=true;''')
 edit(repo,lib+'uwvm_runtime_checkpoint_world_full_code_handoff.h',
      '    out.prepared_full_code_effective_bodies=handed_off_effective_bodies_;',
      '    out.prepared_full_code_effective_bodies=handed_off_effective_bodies_;\n    out.prepared_full_root_code_identities=engines_.size();out.prepared_full_root_code_identity_functions=prepared_publication_functions_;\n    out.runtime_full_root_identities_prepared_privately=true;')
 edit(repo,lib+'uwvm_runtime.h',
      '        ::std::size_t prepared_full_code_owner_modules{}, prepared_full_code_owner_functions{}, prepared_full_code_effective_bodies{};',
      '        ::std::size_t prepared_full_code_owner_modules{}, prepared_full_code_owner_functions{}, prepared_full_code_effective_bodies{};\n        ::std::size_t prepared_full_root_code_identities{}, prepared_full_root_code_identity_functions{};\n        bool runtime_full_root_identities_prepared_privately{}; // Immutable compiler DATA; unsealed, no current-world permission.')
 for name in ('debug_checkpoint_prepared_retirement_runtime.cc','debug_checkpoint_private_code_handoff_generation_runtime.cc','debug_wasip1_prepared_retirement_runtime.cc'):
  edit(repo,'test/0017.runtime/'+name,
       'pending.preparation.runtime_full_code_owners_transferred_privately && pending.preparation.runtime_full_code_targets_allocation_preserved &&',
       '''pending.preparation.runtime_full_code_owners_transferred_privately && pending.preparation.runtime_full_code_targets_allocation_preserved &&
        pending.preparation.runtime_full_root_identities_prepared_privately &&
        pending.preparation.prepared_full_root_code_identities==pending.preparation.modules &&
        pending.preparation.prepared_full_root_code_identity_functions==pending.preparation.functions &&''')
 edit(repo,'test/0017.runtime/debug_checkpoint_active_code_handoff_generation_runtime.cc',
      '        require(prepared.runtime_full_code_owners_transferred_privately &&',
      '''        require(prepared.runtime_full_root_identities_prepared_privately &&
            prepared.prepared_full_root_code_identities==prepared.modules &&
            prepared.prepared_full_root_code_identity_functions==prepared.functions,
            "generation2 root identity is immutable private compiler DATA, never a LIVE seal");
        require(prepared.runtime_full_code_owners_transferred_privately &&''')
 edit(repo,'test/0017.runtime/debug_checkpoint_active_code_handoff_generation_runtime.cc',
      '        ::fast_io::print(::fast_io::out(),"ACTIVE_GENERATION2 private_effective_body_owners=1 active_native_frames=2\\n");',
      '        ::fast_io::print(::fast_io::out(),"ACTIVE_GENERATION2 private_effective_body_owners=1 active_native_frames=2\\nROOT_IDENTITY private_generation2=1 live_seal=0 PASS\\n");')
for k,text in changes.items():
 p=P/k;s=p.stat();b=B[k]
 assert sha(p.read_bytes())==b['sha256'] and (s.st_dev,s.st_ino,s.st_mtime_ns,s.st_mode)==(b['device'],b['inode'],b['mtime_ns'],b['mode']),(k,'peer changed')
for repo in ('uwvm2','uwvm2-ros'):
 for name in ('decl','impl'):
  k=repo+'/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_root_generation_identity_'+name+'.h'
  assert not (P/k).exists();changes[k]=(W/('root_'+name+'.h')).read_text()
for k,text in changes.items():
 p=P/k;fd,tmp=tempfile.mkstemp(prefix='.r43-',dir=p.parent)
 try:
  with os.fdopen(fd,'w') as f:f.write(text);f.flush();os.fsync(f.fileno())
  os.chmod(tmp,0o644);os.replace(tmp,p)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
(W/'cpp-hunks.json').write_text(json.dumps(cpp_hunks,indent=2)+'\n')
(W/'local-delta.json').write_text(json.dumps({k:sha((P/k).read_bytes()) for k in changes},indent=2)+'\n')
print('CAS edited',len(changes),'files; cpp owned hunks',len(cpp_hunks))
