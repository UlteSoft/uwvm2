from pathlib import Path
import os,sys,json,shutil
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
S=R/'source/uwvm2-candidate-c23-a3';binding=json.loads((S/'source-binding-c23-a3.json').read_text())
for f,h in binding['files'].items():assert N.sha(S/f)==h,f
T=R/'source/uwvm2-candidate-c25-a1';disk=N.BASE/'artifacts/c25-candidate-a1/source';assert not T.exists() and not disk.exists();disk.mkdir(parents=True);T.symlink_to(disk,target_is_directory=True)
for p in S.rglob('*'):
 q=disk/p.relative_to(S)
 if p.is_dir():q.mkdir(exist_ok=True)
 elif p.is_file():shutil.copy2(p,q)
base='src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/'
patches=[]
new="""::llvm::Value* length{builder.CreateLoad(intptr, member(storage::gc_object_store::jit_array_length_offset))};
#if defined(UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES) && UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES == 1
        // Use a logical length associated with THIS complete reference SSA.
        // Authentication/status success still dominates the native load and
        // bounds check. This is not a cached object address or a new lease.
        // Unknown calls/PHIs/selects/null/local assignments carry valid=false
        // through the existing length-fact protocol and retain the header load.
        // Known allocation lengths become constants after mem2reg, allowing
        // LLVM to remove the unused load and prove bounded numeric indices.
        auto const fact{llvm_jit_find_array_length_value(state, operand.value)};
        if(fact.length != nullptr && fact.valid != nullptr &&
           fact.length->getType()->isIntegerTy(32u) && fact.valid->getType()->isIntegerTy(1u))
        {
            length = builder.CreateSelect(fact.valid, builder.CreateZExt(fact.length, intptr),
                length, "gc.array.bounds.length");
        }
#endif"""
for fn,old in [('single_func_gc_typed_array_load.h','auto const length{builder.CreateLoad(intptr,member(storage::gc_object_store::jit_array_length_offset))};'),('single_func_gc_typed_array_store.h','auto const length{builder.CreateLoad(intptr, member(storage::gc_object_store::jit_array_length_offset))};')]:
 p=T/base/fn;before=p.read_text();assert before.count(old)==1
 p.write_text(before.replace(old,new));patches.append(dict(file=base+fn,old=old,new=new,before_sha=N.sha(S/base/fn),after_sha=N.sha(p)))
p=T/'src/uwvm2/runtime/llvm_jit_cache/environment.h';before=p.read_text();anchor='        details::append_cache_key_value(out,u8"gc-field-classification",u8"immutable-any-reference-bit-v1");';assert before.count(anchor)==1
addition="""#if defined(UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES) && UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES == 1 && \\
    ((defined(UWVM_EXPERIMENTAL_TYPED_ARRAY_LOAD) && UWVM_EXPERIMENTAL_TYPED_ARRAY_LOAD == 1) || \\
     (defined(UWVM_EXPERIMENTAL_TYPED_ARRAY_STORE) && UWVM_EXPERIMENTAL_TYPED_ARRAY_STORE == 1))
        // Typed element bounds now consume the existing logical length fact.
        // Invalidate old code even for source-ID-free signed cache builds.
        details::append_cache_key_value(out, u8"llvm-gc-typed-array-bounds",
            u8"logical-length-after-auth-unknown-retains-header-v1");
#endif
"""
p.write_text(before.replace(anchor,addition+anchor));patches.append(dict(file=str(p.relative_to(T)),old=anchor,new=addition+anchor,before_sha=N.sha(S/p.relative_to(T)),after_sha=N.sha(p)))
files={str(p.relative_to(T)):N.sha(p) for p in sorted(T.rglob('*')) if p.is_file()};diff=[f for f,h in binding['files'].items() if files[f]!=h];assert set(diff)=={p['file'] for p in patches},diff
N.save(T/'source-binding-c25-a1.json',dict(files=files,parent=str(S),parent_binding_sha=N.sha(S/'source-binding-c23-a3.json'),changed_files=diff,status='Private candidate UNCOMPILED, no C24 loop-root elision; C23 roots/locks/authentication retained. Only typed numeric accesses use existing logical length facts; new signed cache identity. Native candidate correctness/performance pending.'))
N.save(R/'source/scoped-c25-known-length-patches-a1.json',dict(patches=patches,parent_binding_sha=N.sha(S/'source-binding-c23-a3.json'),candidate_binding_sha=N.sha(T/'source-binding-c25-a1.json'),status='Uncompiled private candidate, not production qualified'))
plan=json.loads((R/'source/build-plan-typed-array-access-inline-static32-c23-a3.json').read_text());flags=next(iter(plan['variants'].values()));plan['source']=str(T);plan['variants']={'deferred-typed-array-access-known-length':flags};N.save(R/'source/build-plan-typed-array-access-known-length-c25-a1.json',plan)
print('C25_PREPARED',diff,N.sha(T/'source-binding-c25-a1.json'),flush=True)
