#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/llvm_jit_cache/environment.h>
#include <string_view>

int main()
{
    auto key{uwvm2::runtime::llvm_jit_cache::uwvm_runtime_abi_fingerprint()};
    std::u8string_view text{key.data(), key.size()};
    bool const present{text.find(u8"llvm-riscv64-host-address") != std::u8string_view::npos &&
                       text.find(u8"inline-li-no-data-relocation-v2") != std::u8string_view::npos};
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64) && \
    (defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED))
    return present ? 0 : 1;
#else
    return present ? 1 : 0;
#endif
}
