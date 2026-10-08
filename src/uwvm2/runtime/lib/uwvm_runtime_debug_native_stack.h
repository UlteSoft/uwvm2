/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Copyright (c) 2025-present UlteSoft. All rights reserved.
 * Licensed under the APL-2.0 License (see LICENSE file).
 *************************************************************/
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "uwvm_runtime_native_stack_guard.h"
#if defined(__linux__) && (((defined(__i386__) || (defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__))) && __SIZEOF_POINTER__ == 4) || ((defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))) && __SIZEOF_POINTER__ == 8))
# include <cerrno>
# include <sys/syscall.h>
# include <sys/uio.h>
# include <fast_io.h>
#endif

namespace uwvm2::runtime::lib::details::debug_native_stack
{
    // Implementation-private scope identity. This is NOT ownership of an OS
    // stack allocation, a public memory capability, or proof of a Wasm frame.
    // The caller must keep the real worker trapped under domain/publication/
    // native-transition guards before using even one of these private reads.
    class registration final
    {
        friend class scope;
        ::std::uintptr_t low_{}, high_{};
        ::std::uint_least64_t thread_{};
        int process_{};
        ::std::atomic_bool live_{true};
        registration(::std::uintptr_t low, ::std::uintptr_t high,
            ::std::uint_least64_t thread, int process) noexcept
            : low_{low}, high_{high}, thread_{thread}, process_{process} {}
        template<::std::size_t Width>
        [[nodiscard]] bool copy_slot(::std::uint_least64_t thread, ::std::uintptr_t sp,
            ::std::uintptr_t cfa, ::std::uintptr_t address, ::std::byte* owned_word) const noexcept
        {
#if defined(__linux__) && (((defined(__i386__) || (defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__))) && __SIZEOF_POINTER__ == 4) || ((defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))) && __SIZEOF_POINTER__ == 8)) && defined(__NR_process_vm_readv)
            static_assert(Width == 4u || Width == 8u);
            if(Width != sizeof(::std::uintptr_t) || owned_word == nullptr || !contains_frame(thread, sp, cfa) ||
               address < sp || address >= cfa || cfa - address < Width) { return false; }
            // Only the ABI-sized, separately authenticated CFI slot. On 32-bit targets
            // eight bytes could include a parent/host word and always refuse.
            ::iovec local{owned_word, Width};
            ::iovec remote{reinterpret_cast<void*>(address), Width};
            auto const copied{::fast_io::system_call<__NR_process_vm_readv, long>(
                process_, ::std::addressof(local), 1u, ::std::addressof(remote), 1u, 0u)};
#if (defined(__i386__) || (defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__) && __SIZEOF_POINTER__ == 4) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))) && defined(__NR_pread64) && defined(UWVM_CPP_EXCEPTIONS)
            // QEMU may omit process_vm_readv; only ENOSYS permits this fixed
            // self-mem fallback. No protection/read error opens another path.
            if(copied == -ENOSYS && address <= static_cast<::std::uintptr_t>(INT64_MAX))
            {
                try
                {
                    ::fast_io::native_file memory{"/proc/self/mem", ::fast_io::open_mode::in};
#if defined(__i386__) && __SIZEOF_POINTER__ == 4
                    // i386 pread64 takes two offset words. Passing one 32-bit
                    // word loses the high argument; libc pread can narrow a
                    // guest address above INT32_MAX when off_t is 32-bit.
                    auto const read{::fast_io::system_call<__NR_pread64, long>(
                        memory.native_handle(), owned_word, Width, static_cast<::std::uint32_t>(address), 0u)};
#elif defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__) && __SIZEOF_POINTER__ == 4
                    // ARM EABI aligns the 64-bit offset to r4/r5. r3 is
                    // padding, not the low offset word (unlike i386). Keep
                    // the full unsigned 32-bit guest address above INT32_MAX.
                    auto const read{::fast_io::system_call<__NR_pread64, long>(
                        memory.native_handle(), owned_word, Width, 0u, static_cast<::std::uint32_t>(address), 0u)};
#else
                    auto const read{::fast_io::system_call<__NR_pread64, long>(
                        memory.native_handle(), owned_word, Width, address)};
#endif
                    return read == static_cast<long>(Width) && live_.load(::std::memory_order_acquire);
                }
                catch(...) { return false; }
            }
#endif
            return copied == static_cast<long>(Width) && live_.load(::std::memory_order_acquire);
#else
            (void)thread; (void)sp; (void)cfa; (void)address; (void)owned_word; return false;
#endif
        }
    public:
        // Actual registered worker-stack bounds, not a fixed frame-size guess.
        // Only separately authenticated CFI words may be copied from this span.
        // A zero-size leaf has CFA==SP and admits no ABI-sized slot.
        [[nodiscard]] bool contains_frame(::std::uint_least64_t thread,
            ::std::uintptr_t sp, ::std::uintptr_t cfa) const noexcept
        {
            return thread != 0u && thread == thread_ && live_.load(::std::memory_order_acquire) &&
                sp >= low_ && sp < high_ && cfa >= sp && cfa <= high_;
        }
        [[nodiscard]] bool copy_word(::std::uint_least64_t thread, ::std::uintptr_t sp,
            ::std::uintptr_t cfa, ::std::uintptr_t address, ::std::byte* owned_word) const noexcept
        {
            return copy_slot<8u>(thread, sp, cfa, address, owned_word);
        }
        [[nodiscard]] bool copy_word32(::std::uint_least64_t thread, ::std::uintptr_t sp,
            ::std::uintptr_t cfa, ::std::uintptr_t address, ::std::byte* owned_word) const noexcept
        { return copy_slot<4u>(thread, sp, cfa, address, owned_word); }
    };
    using owner = ::std::shared_ptr<registration const>;
    class scope final
    {
        ::std::shared_ptr<registration> owner_{};
    public:
        scope() noexcept
        {
#if defined(__linux__) && (((defined(__i386__) || (defined(__arm__) && defined(__ARM_EABI__) && defined(__ARMEL__))) && __SIZEOF_POINTER__ == 4) || ((defined(__x86_64__) || (defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__)) || defined(__loongarch64) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))) && __SIZEOF_POINTER__ == 8)) && defined(UWVM_CPP_EXCEPTIONS)
            // Cold debug-only entry, on the actual admitted worker. Never query
            // a manager's stack or register a caller-populated bound/TID tuple.
            auto const page{::uwvm2::runtime::lib::posix_abi::sysconf_noexcept(_SC_PAGESIZE)};
            native_stack::bounds actual{};
            ::std::byte here{};
            if(page <= 0 || !native_stack::read_bounds(reinterpret_cast<::std::uintptr_t>(::std::addressof(here)),
                static_cast<::std::size_t>(page), actual)) { return; }
            auto const thread{::fast_io::system_call<__NR_gettid, long>()};
            auto const process{::fast_io::system_call<__NR_getpid, int>()};
            if(thread <= 0 || process <= 0) { return; }
            try { owner_.reset(new registration{actual.low, actual.high,
                static_cast<::std::uint_least64_t>(thread), process}); }
            catch(...) { owner_.reset(); }
#endif
        }
        scope(scope const&) = delete;
        scope& operator=(scope const&) = delete;
        ~scope() { if(owner_) { owner_->live_.store(false, ::std::memory_order_release); } }
        [[nodiscard]] owner pin() const noexcept { return owner_; }
    };
}
