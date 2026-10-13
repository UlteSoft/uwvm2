// Actual runtime API negative capability tests, not a fabricated trap model.
// Positive executed CALL evidence is the separate actual controller fixture.
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <fast_io.h>

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
static void check(bool value, char const* message) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("native-call-capability: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
static void never_invoked(void* context, lib::llvm_jit_debug_native_call_continuation_view const&,
    ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow&) noexcept
{ *static_cast<bool*>(context) = true; } // real host-owned flag, never a guest address.
int main()
{
    using borrow = ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow;
    static_assert(!::std::is_copy_constructible_v<borrow> && !::std::is_move_constructible_v<borrow> &&
                  !::std::is_default_constructible_v<borrow>);
    static_assert(::std::is_same_v<decltype(&lib::llvm_jit_debug_resume_native_call_continuation_host_api),
        bool (*)(lib::llvm_jit_debug_native_call_continuation_owner const&, void const*, void*,
                 lib::llvm_jit_debug_native_call_resume_callback) noexcept>);
    static_assert(::std::is_same_v<decltype(&lib::llvm_jit_debug_mint_native_call_continuation_host_api),
        lib::llvm_jit_debug_native_call_continuation_owner (*)(
            lib::llvm_jit_debug_activation_capture_owner const&, lib::llvm_jit_debug_native_activation_cursor_owner const&, void const*) noexcept>);
    static_assert(::std::is_same_v<decltype(&lib::llvm_jit_debug_native_call_continuation_host_api),
        bool (*)(lib::llvm_jit_debug_native_call_continuation_owner const&, void const*,
                 lib::llvm_jit_debug_native_call_continuation_view&) noexcept>);
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    auto dummy{::std::make_shared<::std::array<::std::byte, 64u>>()};
    // [owned dummy allocation64 bytes] allocation_end
    // [safe] comparison-only alias pointer formation; the genuine runtime
    // registry MUST reject these control blocks BEFORE any class field read.
    // None of these aliases names a constructed capture/cursor/call capability.
    auto const* opaque{static_cast<void const*>(dummy->data())};
    lib::llvm_jit_debug_activation_capture_owner capture{dummy,
        static_cast<lib::llvm_jit_debug_activation_capture const*>(opaque)};
    lib::llvm_jit_debug_native_activation_cursor_owner cursor{dummy,
        static_cast<lib::llvm_jit_debug_native_activation_cursor const*>(opaque)};
    lib::llvm_jit_debug_native_call_continuation_owner cap{dummy,
        static_cast<lib::llvm_jit_debug_native_call_continuation const*>(opaque)};
    lib::llvm_jit_debug_native_call_continuation_view view{1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u};
    check(!lib::llvm_jit_debug_native_call_continuation_host_api({}, opaque, view) && view == decltype(view){},
          "empty owner rejected and DATA output reset");
    view = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u};
    check(!lib::llvm_jit_debug_native_call_continuation_host_api(cap, opaque, view) && view == decltype(view){},
          "unconstructed alias rejected before dereference");
    check(!lib::llvm_jit_debug_mint_native_call_continuation_host_api(capture, cursor, opaque),
          "unconstructed capture/cursor aliases cannot mint");
    check(!lib::llvm_jit_debug_mint_native_call_continuation_host_api({}, {}, opaque),
          "non-null numeric session without a real capture cannot mint");
    check(!lib::llvm_jit_debug_mint_native_call_continuation_host_api(capture, cursor, nullptr),
          "null session cannot mint");
    bool invoked{};
    check(!lib::llvm_jit_debug_resume_native_call_continuation_host_api(cap, opaque, ::std::addressof(invoked), never_invoked) && !invoked,
          "alias cannot invoke protected host resume callback");
    check(!lib::llvm_jit_debug_resume_native_call_continuation_host_api({}, opaque, ::std::addressof(invoked), never_invoked) && !invoked,
          "missing owner cannot invoke protected host resume callback");
    check(!lib::llvm_jit_debug_resume_native_call_continuation_host_api(cap, opaque, ::std::addressof(invoked), nullptr) && !invoked,
          "missing callback retains stop");
    ::fast_io::io::println("native-call-capability: actual API rejects unowned aliases; no executed CALL qualification");
}
#else
int main() { return 77; }
#endif
