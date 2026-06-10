/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <limits>
# include <concepts>
# include <bit>
# include <vector>
# include <atomic>
# include <memory>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platfrom
# include <signal.h>
# pragma push_macro("UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT")
# undef UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
# if !(defined(_WIN32) || defined(__CYGWIN__)) && defined(__has_include)
#  if (defined(__APPLE__) || defined(__DARWIN_C_LEVEL)) && __has_include(<sys/ucontext.h>)
/// Darwin's <ucontext.h> exposes deprecated context routines and errors without _XOPEN_SOURCE; <sys/ucontext.h> still provides signal ucontext_t.
#   include <sys/ucontext.h>
#   define UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
#  elif __has_include(<ucontext.h>)
#   include <ucontext.h>
#   define UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
#  endif
# endif
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/object/memory/error/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_SUPPORT_MMAP)

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::signal
{
# if !(defined(_WIN32) || defined(__CYGWIN__))
    /// @brief      POSIX C signal APIs imported with their platform ABI symbol names.
    /// @details    This namespace avoids accidental calls through macro-expanded or wrapped names while
    ///             still letting the signal layer chain to the process' previous handlers.
    namespace posix
    {
        extern int raise(int) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("raise")
#  else
            __asm__("_raise")
#  endif
                ;

        extern int sigaction(int, struct ::sigaction const*, struct ::sigaction*) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("sigaction")
#  else
            __asm__("_sigaction")
#  endif
                ;

        extern void (*signal(int, void (*)(int)))(int) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("signal")
#  else
            __asm__("_signal")
#  endif
                ;
    }  // namespace posix
# endif

    /// @brief      Reserved virtual-memory interval protected by the mmap-backed wasm memory runtime.
    /// @details    The interval [begin, end) is the complete reserved address range, including committed
    ///             pages and guard/uncommitted pages. A fault inside this interval is considered a wasm
    ///             memory bounds fault and is converted to mmap_memory_error_t.
    /// @note       @p length_p points to the currently allocated/committed byte length when the memory can
    ///             grow at runtime. When it is null, the fixed segment size is derived from end - begin.
    struct protected_memory_segment_t
    {
        /// @brief  First byte of the reserved virtual address interval.
        ::std::byte const* begin{};

        /// @brief  One-past-the-last byte of the reserved virtual address interval.
        ::std::byte const* end{};

        /// @brief  Optional live memory length used to report the valid wasm memory size.
        ::std::atomic_size_t const* length_p{};

        /// @brief  Wasm linear-memory index used in diagnostics.
        ::std::size_t memory_idx{};
    };

    /// @brief      Callback type invoked when a protected mmap segment is faulted.
    /// @details    The callback receives a fully populated fault report. It must not throw and it is
    ///             expected to be terminal; the dispatcher terminates immediately after invoking it.
    using mmap_memory_out_of_bounds_func_t = void (*)(::uwvm2::object::memory::error::mmap_memory_error_t const&) noexcept;

    namespace detail
    {
        /// @brief  Global registry of protected mmap-backed memory intervals.
        /// @note   Structural mutations are intentionally restricted to VM/module setup and teardown.
        ///         The platform fault handler reads this container without taking locks.
        inline ::uwvm2::utils::container::vector<protected_memory_segment_t> segments{};  // [global]

        /// @brief  Optional process-wide hook for translating mmap memory faults to a runtime-specific action.
        inline mmap_memory_out_of_bounds_func_t mmap_memory_out_of_bounds_func{};  // [global]

        /// @brief      Previous platform handlers saved when UWVM installs its own fault handler.
        /// @details    The signal layer only consumes faults that belong to registered protected segments.
        ///             Unrelated process faults are forwarded to the saved handlers when possible.
        struct signal_handlers_t
        {
# if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
            /// @brief  Previous Win32 unhandled-exception filter used on old Windows targets.
            ::fast_io::win32::pvectored_exception_handler previous_unhandled_exception_filter{};
#  else
            /// @brief  Handle returned by AddVectoredExceptionHandler for modern Windows targets.
            void* vectored_handler_handle{};
#  endif
# else
            /// @brief  SIGSEGV handler active before UWVM installed its handler.
            struct ::sigaction previous_sigsegv{};

            /// @brief  SIGBUS handler active before UWVM installed its handler.
            struct ::sigaction previous_sigbus{};

            /// @brief  True after previous_sigsegv contains a valid saved handler.
            bool has_previous_sigsegv{};

            /// @brief  True after previous_sigbus contains a valid saved handler.
            bool has_previous_sigbus{};
# endif
        };

        /// @brief  Process-wide storage for saved signal/exception handlers.
        inline signal_handlers_t signal_handlers{};  // [global]

        /// @brief      Return the valid wasm memory length associated with a protected segment.
        /// @details    Growable memories publish their current length through length_p. Fixed-size
        ///             segments use the reserved interval length as the reported memory length.
        inline constexpr ::std::size_t get_memory_length(protected_memory_segment_t const& seg) noexcept
        {
            if(seg.length_p != nullptr) [[likely]] { return static_cast<::std::size_t>(seg.length_p->load(::std::memory_order_acquire)); }

            return static_cast<::std::size_t>(seg.end - seg.begin);
        }

        /// @brief      Build a user-facing mmap fault report from a segment and a raw fault address.
        /// @param      seg                 Registered protected segment containing the fault address.
        /// @param      fault_addr          Address reported by the platform fault mechanism.
        /// @param      instruction_address Best-effort program counter of the faulting instruction.
        /// @return     Diagnostic data consumed by the default reporter or a custom out-of-bounds hook.
        inline constexpr ::uwvm2::object::memory::error::mmap_memory_error_t
            make_mmap_memory_error(protected_memory_segment_t const& seg, ::std::byte const* fault_addr, ::std::uintptr_t instruction_address) noexcept
        {
            auto const offset{static_cast<::std::size_t>(fault_addr - seg.begin)};
            auto const memory_length{get_memory_length(seg)};

            return {.memory_idx = seg.memory_idx,
                    .memory_offset = static_cast<::std::uint_least64_t>(offset),
                    .memory_length = static_cast<::std::uint_least64_t>(memory_length),
                    .instruction_address = instruction_address};
        }

        /// @brief      Try to consume a platform fault as a wasm mmap memory fault.
        /// @return     False when the address is null or outside every registered protected segment.
        /// @details    For registered protected segments this function is terminal: it invokes the custom
        ///             handler when present, otherwise prints the default mmap memory error, and then
        ///             terminates. The boolean return exists for the platform handler's pass-through path.
        inline constexpr bool handle_fault_address(::std::byte const* fault_addr, ::std::uintptr_t instruction_address) noexcept
        {
            if(fault_addr == nullptr) [[unlikely]] { return false; }

            for(auto const& seg: segments)
            {
                if(seg.begin <= fault_addr && fault_addr < seg.end)
                {
                    auto const mmapmemerr{make_mmap_memory_error(seg, fault_addr, instruction_address)};
                    if(auto const handler{mmap_memory_out_of_bounds_func}; handler != nullptr) [[likely]]
                    {
                        handler(mmapmemerr);
                        ::fast_io::fast_terminate();
                        ::std::unreachable();
                    }

                    ::uwvm2::object::memory::error::output_mmap_memory_error_and_terminate(mmapmemerr);
                    ::std::unreachable();
                }
            }

            return false;
        }

# if defined(_WIN32) || defined(__CYGWIN__)
        /// @brief      Windows vectored exception entry point used to intercept access violations.
        /// @details    Access violations are translated only when the fault address falls inside a
        ///             registered protected segment. All other exceptions continue through the normal
        ///             Windows exception-dispatch chain.
        inline constexpr ::std::int_least32_t UWVM_WINSTDCALL vectored_exception_handler(::fast_io::win32::exception_pointers* exception_pointers) noexcept
        {
            if(exception_pointers == nullptr || exception_pointers->ExceptionRecord == nullptr) [[unlikely]] { return 0 /*EXCEPTION_CONTINUE_SEARCH*/; }

            auto const code{exception_pointers->ExceptionRecord->ExceptionCode};
            if(code ==
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
               998 /*EXCEPTION_ACCESS_VIOLATION*/
#  else
               0xC0000005u /*STATUS_ACCESS_VIOLATION*/
#  endif
            )
            {
                if(exception_pointers->ExceptionRecord->NumberParameters >= 2u)
                {
                    auto const fault_addr{reinterpret_cast<::std::byte const*>(exception_pointers->ExceptionRecord->ExceptionInformation[1u])};

                    // Windows reports the faulting instruction address directly in the exception record, so
                    // the signal/ucontext architecture table below is only needed for POSIX targets.
                    auto const instruction_address{reinterpret_cast<::std::uintptr_t>(exception_pointers->ExceptionRecord->ExceptionAddress)};

                    if(handle_fault_address(fault_addr, instruction_address)) { return -1 /*EXCEPTION_CONTINUE_EXECUTION*/; }
                }
            }

            return 0 /*EXCEPTION_CONTINUE_SEARCH*/;
        }

#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
        /// @brief      Compatibility unhandled-exception filter for old Windows targets.
        /// @details    Old targets do not provide vectored exception handling, so the same access-violation
        ///             translation is installed through SetUnhandledExceptionFilter and chained manually.
        inline constexpr ::std::int_least32_t UWVM_WINSTDCALL unhandled_exception_filter(::fast_io::win32::exception_pointers* exception_pointers) noexcept
        {
            auto const ret{vectored_exception_handler(exception_pointers)};
            if(ret != 0) { return ret; }

            auto const prev{signal_handlers.previous_unhandled_exception_filter};
            if(prev != nullptr && prev != unhandled_exception_filter) { return prev(exception_pointers); }

            return 0 /*EXCEPTION_CONTINUE_SEARCH*/;
        }
#  endif

        /// @brief      Install the Windows access-violation handler once for the process.
        /// @details    The handler is process-wide because Windows exception dispatch is process/thread
        ///             global. register_protected_segment calls this lazily before publishing the first
        ///             protected interval.
        inline constexpr void install_signal_handler() noexcept
        {
            static ::std::atomic_bool signal_installed{};  // [global]

            // call once
            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            auto& handlers{signal_handlers};

            // NOTE: In C++17 and later, for `E1 = E2`, `E2` is sequenced before `E1`.
            // Keep handler-state evaluation separated from installing the handler to avoid re-entrancy
            // pitfalls if `signal_handlers` ever becomes lazily initialized.
#  if defined(_WIN32_WINDOWS) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
            handlers.previous_unhandled_exception_filter = ::fast_io::win32::SetUnhandledExceptionFilter(unhandled_exception_filter);
#  else
            handlers.vectored_handler_handle = ::fast_io::win32::AddVectoredExceptionHandler(1u, vectored_exception_handler);
            if(handlers.vectored_handler_handle == nullptr) [[unlikely]]
            {
#   ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler.\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#   else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler.\n\n");
#   endif
                ::fast_io::fast_terminate();
            }
#  endif
        }

# else
        /// @brief      Extract the faulting instruction address from a POSIX ucontext, when supported.
        /// @details    The exact register field is OS and architecture specific. Unsupported targets return
        ///             zero, which keeps diagnostics valid while omitting the instruction address.
        [[nodiscard]] inline constexpr ::std::uintptr_t get_signal_instruction_address([[maybe_unused]] void* context) noexcept
        {
#  ifdef UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT
            if(context == nullptr) [[unlikely]] { return 0u; }
            [[maybe_unused]] auto const uctx{static_cast<::ucontext_t const*>(context)};

#   if defined(__linux__) && defined(__x86_64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_RIP]);
#   elif defined(__linux__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_EIP]);
#   elif defined(__linux__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.pc);
#   elif defined(__linux__) && defined(__arm__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.arm_pc);
#   elif defined(__linux__) && defined(__riscv)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[REG_PC]);
#   elif defined(__linux__) && (defined(__loongarch64) || (defined(__loongarch__) && defined(__loongarch_grlen) && __loongarch_grlen == 64))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__pc);
#   elif defined(__linux__) && (defined(__mips__) || defined(__mips))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.pc);
#   elif defined(__linux__) && (defined(__powerpc64__) || defined(__PPC64__))
            // Linux PowerPC stores NIP (next instruction pointer) at general-register slot 32.
            // This is PT_NIP in the kernel uapi ptrace register layout.
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gp_regs[32u]);
#   elif defined(__linux__) && (defined(__powerpc__) || defined(__PPC__))
            // Linux PowerPC stores NIP (next instruction pointer) at general-register slot 32.
            // This is PT_NIP in the kernel uapi ptrace register layout.
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[32u]);
#   elif defined(__linux__) && (defined(__s390__) || defined(__s390x__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.psw.addr);
#   elif defined(__linux__) && (defined(__sparc__) || defined(__sparc))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_PC]);
#   elif defined(__linux__) && defined(__alpha__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.sc_pc);
#   elif defined(__linux__) && defined(__arc__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__stop_pc);
#   elif defined(__linux__) && defined(__csky__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs.__pc);
#   elif defined(__linux__) && defined(__hppa__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.sc_iaoq[0u]);
#   elif defined(__linux__) && defined(__m68k__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[R_PC]);
#   elif defined(__linux__) && defined(__microblaze__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.regs.pc);
#   elif defined(__linux__) && defined(__or1k__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__pc);
#   elif defined(__linux__) && (defined(__sh__) || defined(__SH__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.pc);
#   elif defined(__OpenBSD__) && (defined(__amd64__) || defined(__x86_64__))
            return static_cast<::std::uintptr_t>(uctx->sc_rip);
#   elif defined(__OpenBSD__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->sc_eip);
#   elif defined(__OpenBSD__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->sc_elr);
#   elif defined(__OpenBSD__) && defined(__arm__)
            return static_cast<::std::uintptr_t>(uctx->sc_pc);
#   elif defined(__OpenBSD__) && defined(__riscv)
            return static_cast<::std::uintptr_t>(uctx->sc_sepc);
#   elif defined(__OpenBSD__) && (defined(__powerpc64__) || defined(__PPC64__))
            return static_cast<::std::uintptr_t>(uctx->sc_pc);
#   elif defined(__OpenBSD__) && (defined(__powerpc__) || defined(__PPC__))
            return static_cast<::std::uintptr_t>(uctx->sc_frame.srr0);
#   elif defined(__OpenBSD__) && (defined(__sparc__) || defined(__sparc))
            return static_cast<::std::uintptr_t>(uctx->sc_pc);
#   elif defined(__OpenBSD__) && (defined(__mips__) || defined(__mips))
            return static_cast<::std::uintptr_t>(uctx->sc_pc);
#   elif defined(__OpenBSD__) && defined(__hppa__)
            return static_cast<::std::uintptr_t>(uctx->sc_pcoqh);
#   elif defined(__OpenBSD__) && defined(__alpha__)
            return static_cast<::std::uintptr_t>(uctx->sc_pc);
#   elif defined(__OpenBSD__) && defined(__m88k__)
            // OpenBSD/m88k sigcontext stores struct reg inline: r[32], epsr, fpsr, fpcr, sxip.
            // sxip carries low valid/exception flag bits, so apply the XIP_ADDR mask.
            return static_cast<::std::uintptr_t>(uctx->sc_regs[35u] & 0xfffffffcUL);
#   elif defined(__FreeBSD__) && (defined(__amd64__) || defined(__x86_64__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_rip);
#   elif defined(__FreeBSD__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_eip);
#   elif defined(__FreeBSD__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_gpregs.gp_elr);
#   elif defined(__FreeBSD__) && defined(__arm__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__FreeBSD__) && defined(__riscv)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_gpregs.gp_sepc);
#   elif defined(__FreeBSD__) && (defined(__powerpc__) || defined(__powerpc64__) || defined(__PPC__) || defined(__PPC64__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_srr0);
#   elif defined(__NetBSD__) && (defined(__amd64__) || defined(__x86_64__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_RIP]);
#   elif defined(__NetBSD__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_EIP]);
#   elif defined(__NetBSD__) && (defined(__aarch64__) || defined(__arm__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && defined(__riscv)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && (defined(__powerpc__) || defined(__powerpc64__) || defined(__PPC__) || defined(__PPC64__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && (defined(__mips__) || defined(__mips))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_EPC]);
#   elif defined(__NetBSD__) && defined(__hppa__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PCOQH]);
#   elif defined(__NetBSD__) && (defined(__sparc__) || defined(__sparc))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && defined(__alpha__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && defined(__m68k__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && (defined(__sh__) || defined(__SH__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__NetBSD__) && defined(__vax__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.__gregs[_REG_PC]);
#   elif defined(__DragonFly__) && (defined(__amd64__) || defined(__x86_64__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.mc_rip);
#   elif defined(__sun) && (defined(__i386__) || defined(__amd64) || defined(__amd64__) || defined(__x86_64__) || defined(__sparc__) || defined(__sparc))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.gregs[REG_PC]);
#   elif defined(__serenity__) && defined(__x86_64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.rip);
#   elif defined(__serenity__) && (defined(__aarch64__) || defined(__riscv))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext.pc);
#   elif defined(__APPLE__) && defined(__x86_64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__rip);
#   elif defined(__APPLE__) && defined(__i386__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__eip);
#   elif defined(__APPLE__) && (defined(__ppc__) || defined(__POWERPC__))
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__srr0);
#   elif defined(__APPLE__) && defined(__aarch64__)
            return static_cast<::std::uintptr_t>(uctx->uc_mcontext->__ss.__pc);
#   else
            // static_cast<void>(uctx);
            return 0u;
#   endif
#  else
            // static_cast<void>(context);
            return 0u;
#  endif
        }

        /// @brief      Forward an unhandled POSIX signal to the handler that was installed before UWVM.
        /// @details    Both SA_SIGINFO handlers and classic one-argument handlers are supported. Default
        ///             handlers are restored and re-raised so the process observes the platform's normal
        ///             crash behavior.
        inline constexpr void dispatch_previous_handler(int signal, ::siginfo_t* siginfo, void* context, struct ::sigaction const& previous) noexcept
        {
            if(previous.sa_flags & SA_SIGINFO)
            {
                if(reinterpret_cast<void*>(previous.sa_sigaction) == reinterpret_cast<void*>(SIG_DFL))
                {
                    posix::signal(signal, SIG_DFL);
                    posix::raise(signal);
                    return;
                }

                if(reinterpret_cast<void*>(previous.sa_sigaction) == reinterpret_cast<void*>(SIG_IGN)) { return; }

                previous.sa_sigaction(signal, siginfo, context);
                return;
            }

            if(reinterpret_cast<void*>(previous.sa_handler) == reinterpret_cast<void*>(SIG_DFL))
            {
                posix::signal(signal, SIG_DFL);
                posix::raise(signal);
                return;
            }

            if(reinterpret_cast<void*>(previous.sa_handler) == reinterpret_cast<void*>(SIG_IGN)) { return; }

            previous.sa_handler(signal);
        }

        /// @brief      POSIX SIGSEGV/SIGBUS entry point used to intercept protected-memory faults.
        /// @details    Faults inside registered protected segments are converted to wasm memory diagnostics.
        ///             Other faults are delegated to the previous handler, or to the platform default action
        ///             when no previous handler is available.
        inline constexpr void posix_signal_handler(int signal, ::siginfo_t* siginfo, void* context) noexcept
        {
            auto const fault_addr{siginfo == nullptr ? nullptr : reinterpret_cast<::std::byte const*>(siginfo->si_addr)};
            auto const instruction_address{get_signal_instruction_address(context)};

            if(handle_fault_address(fault_addr, instruction_address)) { return; }

            if(signal == SIGSEGV && signal_handlers.has_previous_sigsegv)
            {
                dispatch_previous_handler(signal, siginfo, context, signal_handlers.previous_sigsegv);
                return;
            }

#  ifdef SIGBUS
            if(signal == SIGBUS && signal_handlers.has_previous_sigbus)
            {
                dispatch_previous_handler(signal, siginfo, context, signal_handlers.previous_sigbus);
                return;
            }
#  endif

            posix::signal(signal, SIG_DFL);
            posix::raise(signal);
        }

        /// @brief      Install POSIX SIGSEGV and, where available, SIGBUS handlers once for the process.
        /// @details    The handler is installed lazily by register_protected_segment. It uses SA_SIGINFO so
        ///             the fault address and optional machine context are available for diagnostics.
        inline constexpr void install_signal_handler() noexcept
        {
            static ::std::atomic_bool signal_installed{};  // [global]

            bool expected{false};
            if(!signal_installed.compare_exchange_strong(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return; }

            struct ::sigaction act{};
            act.sa_sigaction = posix_signal_handler;
            sigemptyset(::std::addressof(act.sa_mask));
            act.sa_flags = SA_SIGINFO;

            if(posix::sigaction(SIGSEGV, ::std::addressof(act), ::std::addressof(signal_handlers.previous_sigsegv)) != 0) [[unlikely]]
            {
#  ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler (SIGSEGV).\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#  else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler (SIGSEGV).\n\n");
#  endif
                ::fast_io::fast_terminate();
            }
            signal_handlers.has_previous_sigsegv = true;

#  ifdef SIGBUS
            if(posix::sigaction(SIGBUS, ::std::addressof(act), ::std::addressof(signal_handlers.previous_sigbus)) != 0) [[unlikely]]
            {
#   ifdef UWVM
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Failed to install signal handler (SIGBUS).\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#   else
                ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Failed to install signal handler (SIGBUS).\n\n");
#   endif
                ::fast_io::fast_terminate();
            }
            signal_handlers.has_previous_sigbus = true;
#  endif
        }
# endif
    }  // namespace detail

    /// @brief      Set the process-wide callback used for protected mmap memory faults.
    /// @note       The callback should be installed before guest execution begins. Updating it concurrently
    ///             with fault handling is not synchronized.
    inline constexpr void set_mmap_memory_out_of_bounds_handler(mmap_memory_out_of_bounds_func_t func) noexcept
    { detail::mmap_memory_out_of_bounds_func = func; }

    /// @brief      Register a reserved memory interval whose guard faults should be reported as wasm mmap faults.
    /// @param      begin      First byte of the reserved virtual address interval.
    /// @param      end        One-past-the-last byte of the reserved virtual address interval.
    /// @param      length_p   Optional pointer to the live committed/valid length for growable memories.
    /// @param      memory_idx Wasm linear-memory index used in diagnostics.
    /// @note       Protected memory segments are expected to be registered during VM/module initialization,
    ///             before any guest code that may trigger memory access violations is executed. After
    ///             initialization, the set of segments is treated as structurally immutable (only the
    ///             dynamic length, when provided via @p length_p, may change), and no further calls to
    ///             register_protected_segment, unregister_protected_segment, or clear_protected_segments
    ///             are performed. This design ensures that the signal/exception handler can traverse the
    ///             global segments container without additional synchronization, as there are no
    ///             concurrent structural modifications.
    inline constexpr void register_protected_segment(::std::byte const* begin,
                                                     ::std::byte const* end,
                                                     ::std::atomic_size_t const* length_p = nullptr,
                                                     ::std::size_t memory_idx = 0uz) noexcept
    {
        if(begin == nullptr || end == nullptr || begin >= end) [[unlikely]]
        {
# ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid protected memory segment.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [fatal] Invalid protected memory segment.\n\n");
# endif
            ::fast_io::fast_terminate();
        }

        detail::install_signal_handler();
        detail::segments.emplace_back(begin, end, length_p, memory_idx);
    }

    /// @brief      Remove a previously registered protected memory interval.
    /// @details    The interval is matched by the exact begin/end pair used at registration time. Missing
    ///             entries are ignored because teardown paths may be reached after partial initialization.
    /// @note       This must not run concurrently with guest execution or with platform fault handling.
    inline constexpr void unregister_protected_segment(::std::byte const* begin, ::std::byte const* end) noexcept
    {
        if(begin == nullptr || end == nullptr) [[unlikely]] { return; }

        auto& segments{detail::segments};
        for(auto it{segments.begin()}; it != segments.end(); ++it)
        {
            if(it->begin == begin && it->end == end)
            {
                segments.erase(it);
                return;
            }
        }
    }

    /// @brief      Remove all protected memory intervals from the global registry.
    /// @note       Intended for whole-runtime teardown or reinitialization outside guest execution.
    inline constexpr void clear_protected_segments() noexcept { detail::segments.clear(); }

}  // namespace uwvm2::object::memory::signal

#endif

#ifndef UWVM_MODULE
// platfrom
# pragma pop_macro("UWVM2_OBJECT_MEMORY_SIGNAL_HAS_UCONTEXT")
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
