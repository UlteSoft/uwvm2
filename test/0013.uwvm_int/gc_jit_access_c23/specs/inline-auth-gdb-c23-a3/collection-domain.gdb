set pagination off
set confirm off
set breakpoint pending off
set $domains=0
set $slow=0
break '_ZN5uwvm27runtime8compiler8llvm_jit21compile_all_from_uwvm7details45llvm_jit_gc_numeric_array_index_domain_bridgeEm'
commands
silent
set $domains=$domains+1
continue
end
break '_ZN5uwvm27runtime8compiler8llvm_jit21compile_all_from_uwvm7details37llvm_jit_gc_typed_array_borrow_bridgeILm4ELb1ELb0EEEomjmj'
commands
silent
set $slow=$slow+1
continue
end
run
printf "AUTHCOUNTS %u %u\n", $domains, $slow
