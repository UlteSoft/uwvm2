#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/object/memory/signal/signal.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using mmap_memory_error_t = ::uwvm2::object::memory::error::mmap_memory_error_t;

    struct synthetic_context
    {
        ::std::uintptr_t instruction_pointer;
        ::std::uintptr_t stack_pointer;
        ::std::uintptr_t frame_pointer;
        ::std::uintptr_t link_register;
    };

    inline mmap_memory_error_t captured_error{};
    inline synthetic_context captured_context{};
    inline void const* captured_context_pointer{};
    inline bool context_handler_called{};

    void capture_fault_context(mmap_memory_error_t const& error, void const* platform_context) noexcept
    {
        captured_error = error;
        captured_context_pointer = platform_context;
        context_handler_called = true;
        if(platform_context != nullptr) { ::std::memcpy(::std::addressof(captured_context), platform_context, sizeof(captured_context)); }
    }

    void capture_legacy_fault(mmap_memory_error_t const&) noexcept {}
}

int main()
{
    static_assert(sizeof(::std::uintptr_t) != 8uz || alignof(mmap_memory_error_t) == 8uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || sizeof(mmap_memory_error_t) == 48uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, memory_idx) == 0uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, memory_offset) == 8uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, memory_length) == 16uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, instruction_address) == 24uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, frame_address) == 32uz);
    static_assert(sizeof(::std::uintptr_t) != 8uz || offsetof(mmap_memory_error_t, stack_pointer) == 40uz);

#if !defined(UWVM_SUPPORT_MMAP)
    return 0;
#else
    namespace signal = ::uwvm2::object::memory::signal;
    using public_handler_t = void (*)(mmap_memory_error_t const&) noexcept;
    using internal_handler_t = void (*)(mmap_memory_error_t const&, void const*) noexcept;
    static_assert(::std::is_same_v<signal::mmap_memory_out_of_bounds_func_t, public_handler_t>);
    static_assert(::std::is_same_v<signal::detail::mmap_memory_out_of_bounds_with_context_func_t, internal_handler_t>);

    ::std::byte storage[16]{};
    signal::protected_memory_segment_t const segment{.begin = storage, .end = storage + 16, .length_p = nullptr, .memory_idx = 7uz};
    synthetic_context const context_token{0x111u, 0x333u, 0x222u, 0x444u};

    auto const error{signal::detail::make_mmap_memory_error(segment, storage + 3, 0x11u, 0x22u, 0x33u)};
    signal::detail::set_mmap_memory_out_of_bounds_with_context_handler(capture_fault_context);
    auto const context_handler{signal::detail::mmap_memory_out_of_bounds_with_context_func};
    if(context_handler == nullptr) { return 1; }
    context_handler(error, ::std::addressof(context_token));
    if(!context_handler_called || captured_error.memory_idx != 7uz || captured_error.memory_offset != 3u || captured_error.memory_length != 16u)
    { return 2; }
    if(captured_error.instruction_address != 0x11u || captured_error.frame_address != 0x22u || captured_error.stack_pointer != 0x33u)
    { return 3; }
    if(captured_context_pointer != ::std::addressof(context_token) || captured_context.instruction_pointer != 0x111u ||
       captured_context.stack_pointer != 0x333u || captured_context.frame_pointer != 0x222u || captured_context.link_register != 0x444u)
    { return 4; }

    // The legacy public setter must retain its original ABI and override the internal runtime hook.
    context_handler_called = false;
    signal::set_mmap_memory_out_of_bounds_handler(capture_legacy_fault);
    if(signal::detail::mmap_memory_out_of_bounds_with_context_func != nullptr ||
       signal::detail::mmap_memory_out_of_bounds_func != capture_legacy_fault)
    { return 5; }

    // The internal runtime setter overrides the legacy slot and preserves a null native context unchanged.
    captured_context_pointer = ::std::addressof(context_token);
    context_handler_called = false;
    signal::detail::set_mmap_memory_out_of_bounds_with_context_handler(capture_fault_context);
    if(signal::detail::mmap_memory_out_of_bounds_func != nullptr ||
       signal::detail::mmap_memory_out_of_bounds_with_context_func != capture_fault_context)
    { return 6; }
    signal::detail::mmap_memory_out_of_bounds_with_context_func(error, nullptr);
    if(!context_handler_called || captured_context_pointer != nullptr) { return 7; }
    signal::set_mmap_memory_out_of_bounds_handler(nullptr);

# if defined(_WIN32) && !defined(__CYGWIN__) && defined(_WIN64) &&                                                                            \
     ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
    ::fast_io::win32::win_current_context context{};
    context.Rip = 0x111u;
    context.Rbp = 0x222u;
    context.Rsp = 0x333u;
    auto const view{signal::detail::get_windows_fault_context(::std::addressof(context))};
    if(view.instruction_address != 0x111u || view.frame_address != 0x222u || view.stack_pointer != 0x333u) { return 10; }
    if(view.platform_context != ::std::addressof(context)) { return 11; }
# elif defined(_WIN32) && !defined(__CYGWIN__) && defined(_WIN64) &&                                                                         \
     (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
    ::fast_io::win32::win_current_context context{};
    context.Pc = 0x111u;
    context.X[29u] = 0x222u;
    context.X[30u] = 0x444u;
    context.Sp = 0x333u;
    auto const view{signal::detail::get_windows_fault_context(::std::addressof(context))};
    if(view.instruction_address != 0x111u || view.frame_address != 0x222u || view.stack_pointer != 0x333u) { return 12; }
    if(view.platform_context != ::std::addressof(context)) { return 13; }
    ::fast_io::win32::win_current_context copied_context{};
    ::std::memcpy(::std::addressof(copied_context), view.platform_context, sizeof(copied_context));
    if(copied_context.X[30u] != 0x444u) { return 14; }
# elif defined(_WIN32) && !defined(__CYGWIN__) && (defined(__i386__) || defined(_M_IX86))
    ::fast_io::win32::win_current_context context{};
    context.Eip = 0x111u;
    context.Ebp = 0x222u;
    context.Esp = 0x333u;
    auto const view{signal::detail::get_windows_fault_context(::std::addressof(context))};
    if(view.instruction_address != 0x111u || view.frame_address != 0x222u || view.stack_pointer != 0x333u) { return 15; }
    if(view.platform_context != ::std::addressof(context)) { return 16; }
# endif

    return 0;
#endif
}
