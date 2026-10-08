/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <memory>
# include <cstdint>
# include <climits>
# include <cerrno>
# include <fast_io.h>
# if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8
#  include <csignal>
#  include <linux/hw_breakpoint.h>
#  include <linux/perf_event.h>
#  include <sys/syscall.h>
#  include "posix_abi.h"
#  include "native_perf_signal_linux.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
    class controller;
    namespace native_continuation_linux
    {
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
        // The runtime must authenticate the current real trap and immutable
        // Wasm continuation before the controller operates this host-only FD.
        // This helper provides an OS resource, never a code/read/frame ticket.
        [[nodiscard]] inline bool make_attributes(::std::uintptr_t continuation,
            ::std::uint64_t cookie, ::perf_event_attr& out) noexcept
        {
            out = {};
            if(!native_perf_signal_linux::available || continuation == 0u || cookie == 0u) { return false; }
            static_assert(sizeof(long) == 8u);
            static_assert(sizeof(::perf_event_attr) >= PERF_ATTR_SIZE_VER7);
            out.type = PERF_TYPE_BREAKPOINT;
            out.size = PERF_ATTR_SIZE_VER7;
            out.sample_period = 1u;
            out.sample_type = PERF_SAMPLE_ADDR;
            out.disabled = true;
            out.pinned = true;
            out.exclude_kernel = true;
            out.exclude_hv = true;
            out.remove_on_exec = true;
            out.sigtrap = true;
            out.bp_type = HW_BREAKPOINT_X;
            out.bp_addr = continuation;
            // Linux x86 instruction breakpoint ABI uses the kernel word length.
            // It matches ONE instruction address; this never reads eight bytes.
            out.bp_len = sizeof(long);
            out.sig_data = cookie;
            return true;
        }
        inline constexpr bool sdk_available{native_perf_signal_linux::available};
#else
        inline constexpr bool sdk_available{false};
#endif
        class event_owner final
        {
            ::fast_io::native_file file_{};
            int error_{};
            bool enabled_{};
            friend class ::uwvm2::uwvm::debugger::controller;
#if defined(UWVM_DEBUG_NATIVE_CONTINUATION_COMPONENT_TEST) && UWVM_DEBUG_NATIVE_CONTINUATION_COMPONENT_TEST == 1
            // Separate trusted kernel component probe; never an enabled runtime
            // or guest/debug-console address request, and never a Wasm proof.
            friend struct trusted_component_probe;
#endif
            [[nodiscard]] bool open_disabled(::std::uint_least64_t actual_thread,
                ::std::uintptr_t actual_continuation, ::std::uint64_t actual_cookie) noexcept
            {
                error_ = 0;
                if(file_) { return false; }
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
                if(!native_perf_signal_linux::kernel_supports_async_delivery())
                { error_ = EOPNOTSUPP; return false; }
                ::perf_event_attr attributes{};
                if(actual_thread == 0u || actual_thread > INT_MAX ||
                   !make_attributes(actual_continuation, actual_cookie, attributes)) { return false; }
                // [complete host-owned perf attributes ... sizeof(attributes)] end
                // [safe] the kernel synchronously copies VER7<=sizeof attributes;
                // no guest cursor, code byte pointer or borrowed field escapes.
                auto const raw{posix_abi::syscall_noexcept(SYS_perf_event_open,
                    ::std::addressof(attributes), static_cast<int>(actual_thread), -1, -1,
                    static_cast<unsigned long>(PERF_FLAG_FD_CLOEXEC))};
                if(raw < 0) { error_ = errno; return false; }
                if(raw > INT_MAX) { ::fast_io::fast_terminate(); } // Linux FD ABI invariant; never narrow silently.
                // Transfer the successful native descriptor immediately to the
                // FastIO RAII owner. No allocation or throwing work intervenes.
                file_.reset(static_cast<int>(raw));
                return true;
#else
                (void)actual_thread; (void)actual_continuation; (void)actual_cookie;
                return false;
#endif
            }
            [[nodiscard]] bool enable() noexcept
            {
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
                if(!file_ || enabled_) { return false; }
                if(posix_abi::syscall_noexcept(SYS_ioctl, file_.native_handle(),
                    static_cast<unsigned long>(PERF_EVENT_IOC_ENABLE), 0ul) != 0)
                { error_ = errno; return false; }
                enabled_ = true;
                return true;
#else
                return false;
#endif
            }
            [[nodiscard]] bool disable_retained() noexcept
            {
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
                if(!file_) { return true; }
                if(posix_abi::syscall_noexcept(SYS_ioctl, file_.native_handle(),
                    static_cast<unsigned long>(PERF_EVENT_IOC_DISABLE), 0ul) != 0)
                { error_ = errno; return false; }
                enabled_ = false;
                return true;
#else
                return !file_;
#endif
            }
            void disable_close() noexcept
            {
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
                if(file_ && enabled_)
                {
                    // Control-plane close only. No code permissions or bytes
                    // change. Caller retains the session until real worker ACK;
                    // closing the FD alone does not drain queued TRAP_PERF work.
                    static_cast<void>(posix_abi::syscall_noexcept(SYS_ioctl, file_.native_handle(),
                        static_cast<unsigned long>(PERF_EVENT_IOC_DISABLE), 0ul));
                }
#endif
                enabled_ = false;
                file_.reset();
            }
        public:
            event_owner() noexcept = default;
            event_owner(event_owner const&) = delete;
            event_owner& operator=(event_owner const&) = delete;
            event_owner(event_owner&&) = delete;
            event_owner& operator=(event_owner&&) = delete;
            ~event_owner() { disable_close(); }
            [[nodiscard]] int native_error() const noexcept { return error_; }
            [[nodiscard]] bool owns_descriptor() const noexcept { return static_cast<bool>(file_); }
        };
    }
}
