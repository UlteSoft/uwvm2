#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
static_assert(uwvm2::runtime::lib::details::wasm_fp_environment_is_fixed);
extern "C" void fixed_fp_boundary(void (*callback)() noexcept) noexcept
{
    uwvm2::runtime::lib::details::scoped_wasm_host_fp_control_restore control{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_host_fp_environment_restore environment{true};
    callback();
}
int main()
{
    bool active{};
    {
        uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment entry{active};
        if(!active || !entry.ready()) { return 1; }
        {
            uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment nested{active};
            if(!active || !nested.ready()) { return 2; }
        }
        if(!active) { return 3; }
    }
    return active ? 4 : 0;
}
