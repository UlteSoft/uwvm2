#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/llvm_jit_cache/environment.h>
#include "llvm_jit_cache_key_test_helpers.h"
int main()
{
    auto key{::uwvm2::runtime::llvm_jit_cache::uwvm_runtime_abi_fingerprint()};
    ::std::u8string_view text{key.data(),key.size()};
    auto const entry{cache_key_test::lookup(text,u8"gc-collection-local-membership")};
    if(!entry.valid) { return 2; }
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1 && (defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED))
    return entry.matches==1uz && entry.value==u8"closed-canonical-local-v1" ? 0 : 1;
#else
    return entry.matches==0uz ? 0 : 1;
#endif
}
