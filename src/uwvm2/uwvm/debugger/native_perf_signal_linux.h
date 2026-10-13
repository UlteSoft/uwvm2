/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && !defined(__ILP32__)
# include <csignal>
# include <sys/syscall.h>
# include <sys/utsname.h>
# include "posix_abi.h"
#endif

// Host-only Linux UAPI decoding, included in module global fragments. Decoded
// DATA is never a trap/owner/cursor capability. In particular, this supplies
// no VM register, stack, memory or code access, and defines no SDK macros.
namespace uwvm2::uwvm::debugger::native_perf_signal_linux
{
    inline constexpr int trap_code{6};
    inline constexpr ::std::uint32_t asynchronous_flag{1u};
    struct payload
    {
        ::std::uint64_t cookie{};
        ::std::uint32_t type{}, flags{};
        bool valid{};
    };

    // Before 5.19, perf SIGTRAP lacks the ASYNC flag needed to distinguish a
    // blocked/deferred notification from a precise pre-execution context.
    // Backports with older release numbers conservatively remain unavailable.
    [[nodiscard]] inline constexpr bool kernel_release_supported(::fast_io::string_view release) noexcept
    {
        if(release.empty() || release.size() > 64u) { return false; }
        ::std::uint32_t components[3]{};
        ::std::size_t offset{};
        for(unsigned part{}; part != 3u; ++part)
        {
            auto const begin{offset};
            while(offset != release.size() && release[offset] >= '0' && release[offset] <= '9') { ++offset; }
            if(offset == begin) { return false; }
            // [safe bounded decimal component] unsafe (one-past)
            // begin/offset are confined to the supplied host DATA view.
            auto const first{release.data() + begin}, last{release.data() + offset};
            auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(components[part]))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return false; }
            if(part != 2u)
            {
                if(offset == release.size() || release[offset] != '.') { return false; }
                ++offset;
            }
        }
        if(offset != release.size())
        {
            if(release[offset] != '-' && release[offset] != '+') { return false; }
            for(; offset != release.size(); ++offset)
            { if(release[offset] < '!' || release[offset] > '~') { return false; } }
        }
        return components[0] > 5u || (components[0] == 5u && components[1] >= 19u);
    }

#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && !defined(__ILP32__)
    // Linux asm-generic siginfo UAPI, with the LP64 x86 layout. Public libc
    // prefix/address offsets and object extent must agree before any read.
    // x32, other architectures and incompatible layouts never use this path.
    inline constexpr bool available{sizeof(::siginfo_t) == 128u && alignof(::siginfo_t) == 8u &&
        sizeof(int) == 4u && sizeof(long) == 8u &&
        offsetof(::siginfo_t, si_signo) == 0u && offsetof(::siginfo_t, si_errno) == 4u &&
        offsetof(::siginfo_t, si_code) == 8u && offsetof(::siginfo_t, si_addr) == 16u};
# if defined(TRAP_PERF)
    static_assert(TRAP_PERF == trap_code);
# endif
# if defined(TRAP_PERF_FLAG_ASYNC)
    static_assert(TRAP_PERF_FLAG_ASYNC == asynchronous_flag);
# endif
# if defined(si_perf_data)
    static_assert(offsetof(::siginfo_t, si_perf_data) == 24u && sizeof(((::siginfo_t*)nullptr)->si_perf_data) == 8u);
# endif
# if defined(si_perf_type)
    static_assert(offsetof(::siginfo_t, si_perf_type) == 32u && sizeof(((::siginfo_t*)nullptr)->si_perf_type) == 4u);
# endif
# if defined(si_perf_flags)
    static_assert(offsetof(::siginfo_t, si_perf_flags) == 36u && sizeof(((::siginfo_t*)nullptr)->si_perf_flags) == 4u);
# endif
    namespace details
    {
        template<typename T, ::std::size_t Offset>
        [[nodiscard]] inline T load(::siginfo_t const& info) noexcept
        {
            static_assert(Offset + sizeof(T) <= sizeof(info));
            ::std::array<unsigned char, sizeof(T)> bytes{};
            // [complete actual kernel-provided siginfo object] end
            // [safe] read only unsigned-char object representation, bounded by
            // sizeof(info); no invented union member/aliased siginfo overlay.
            auto const* source{reinterpret_cast<unsigned char const*>(::std::addressof(info))};
            for(::std::size_t i{}; i != bytes.size(); ++i) { bytes[i] = source[Offset + i]; }
            return ::std::bit_cast<T>(bytes);
        }
    }
    [[nodiscard]] inline payload read(::siginfo_t const* info) noexcept
    {
        if(!available || info == nullptr || info->si_signo != SIGTRAP || info->si_code != trap_code) { return {}; }
        // No allocation, IO, syscall, mutex or kernel-version query here. Keep
        // unknown flags intact: the trap consumer must reject ALL nonzero flags.
        return {details::load<::std::uint64_t, 24u>(*info), details::load<::std::uint32_t, 32u>(*info),
            details::load<::std::uint32_t, 36u>(*info), true};
    }
    [[nodiscard]] inline bool kernel_supports_async_delivery() noexcept
    {
        if(!available) { return false; }
        struct ::utsname actual{};
        // Control-plane admission only: uname copies into a complete host-owned
        // object. An event is never armed based solely on SDK field availability.
        if(posix_abi::syscall_noexcept(SYS_uname, ::std::addressof(actual)) != 0) { return false; }
        ::std::size_t size{};
        while(size != sizeof(actual.release) && actual.release[size] != '\0') { ++size; }
        if(size == sizeof(actual.release)) { return false; }
        return kernel_release_supported(::fast_io::string_view{actual.release, size});
    }
#else
    inline constexpr bool available{false};
    [[nodiscard]] inline bool kernel_supports_async_delivery() noexcept { return false; }
#endif
}
