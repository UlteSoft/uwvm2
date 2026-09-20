// This build explicitly supplies the fixed-environment trust contract. Verify
// the elided guard path without interpreting it as runtime safety detection:
// callers must establish/preserve controls, and numerical/ABI fixes stay enabled.
// This mode is a whole-thread embedding promise, not a check of arbitrary hostile callbacks.
#define UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT 1
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>

namespace fp = ::uwvm2::runtime::lib::details;
static_assert(fp::wasm_fp_environment_is_fixed);
namespace { unsigned calls{}; }

extern "C" void uwvm_fp_fixed_callback_probe(void (*callback)())
{
    fp::scoped_wasm_host_fp_control_restore control{};
    fp::scoped_llvm_wasm_host_fp_environment_restore environment{true};
    callback();
}

int main()
{
    bool active{};
    {
        fp::scoped_llvm_wasm_fp_environment entry{active};
        if(!active || !entry.ready()) { return 1; }
        {
            fp::scoped_llvm_wasm_fp_environment nested{active};
            if(!active || !nested.ready()) { return 2; }
        }
        if(!active) { return 3; }
    }
    if(active) { return 4; }
    {
        fp::scoped_llvm_wasm_fp_environment disabled{active, false};
        if(active || !disabled.ready()) { return 5; }
    }
    fp::scoped_wasm_host_fp_control_restore control{};
    if(!control.ready()) { return 6; }
    uwvm_fp_fixed_callback_probe(+[]() { ++calls; });
    return calls == 1u ? 0 : 7;
}
