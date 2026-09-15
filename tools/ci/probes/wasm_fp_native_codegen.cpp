// Inspect callback control save/restore independently of target libc. The
// hot boundary must preserve required controls without adding per-op guards;
// cross-assembly evidence is not a substitute for target execution tests.
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_native_control.h>
#if defined(UWVM_TEST_EXPECT_NATIVE_FP_CONTROL)
static_assert(uwvm2::runtime::lib::details::wasm_fp_has_native_control_guard);
extern "C" void native_fp_boundary(void (*callback)() noexcept) noexcept
{
    uwvm2::runtime::lib::details::scoped_wasm_native_fp_control_restore guard{};
    callback();
}
#else
static_assert(!uwvm2::runtime::lib::details::wasm_fp_has_native_control_guard);
#endif
