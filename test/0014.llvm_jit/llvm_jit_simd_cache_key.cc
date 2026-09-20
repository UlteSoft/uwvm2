// A fixed emitter must not accept an object cached by the former miscompiling
// scalar fallback merely because project version/LLVM/ISA strings still match.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/llvm_jit_cache/environment.h>
#include <string_view>

int main()
{
    auto key{uwvm2::runtime::llvm_jit_cache::uwvm_runtime_abi_fingerprint()};
    std::u8string_view text{key.data(), key.size()};
    if(text.find(u8"llvm-simd-scalar-lowering") == std::u8string_view::npos ||
       text.find(u8"scalar-target-contract-v3") == std::u8string_view::npos ||
       text.find(u8"integer-globals-x86-no-sse-transport-v2") == std::u8string_view::npos) { return 1; }
    // Both changes are semantic, even with an unchanged source ID: VE now
    // selects its scalar TargetMachine, and no-SSE x86 transport must not
    // quiet a signaling NaN through x87. Neither former object is reusable.
    if(text.find(u8"scalar-target-contract-v2") != std::u8string_view::npos ||
       text.find(u8"integer-globals-v1") != std::u8string_view::npos) { return 3; }
    auto repeated{uwvm2::runtime::llvm_jit_cache::uwvm_runtime_abi_fingerprint()};
    return text == std::u8string_view{repeated.data(), repeated.size()} ? 0 : 2;
}
