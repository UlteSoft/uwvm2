/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-14
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
# include <algorithm>
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <memory>
# include <uwvm2/runtime/compiler/shared/strict_float.h>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
#  include <unistd.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Config/llvm-config.h>
#  include <llvm/ADT/StringMap.h>
#  include <llvm/ADT/StringRef.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include "format.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#include "source_provenance_policy.h"

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        namespace posix
        {
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
            // Bind the real libc ABI as noexcept so cache identity setup does
            // not gain a spurious C++ exception edge around the POSIX query.
# if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
            extern "C" ::uid_t libc_getuid() noexcept __asm__("_getuid");
# else
            extern "C" ::uid_t libc_getuid() noexcept __asm__("getuid");
# endif
#endif
#if !(defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__))
            // The direct libc symbol keeps environment lookup available inside the header-only/module-shared implementation.
# if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
            extern char* libc_getenv(char const*) noexcept __asm__("_getenv");
# elif defined(__DJGPP__)
            extern char* libc_getenv(char const*) noexcept __asm__("_getenv");
# else
            extern char* libc_getenv(char const*) noexcept __asm__("getenv");
# endif
#endif
        }  // namespace posix

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string u8string_from_cstr(char const* str) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            if(str == nullptr) { return out; }
            // Cache paths and metadata are normalized to UTF-8 because the on-disk format is byte-stable across platforms.
            auto const len{::std::strlen(str)};
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(str), len});
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string u8string_from_u16(char16_t const* first, char16_t const* last) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            if(first == nullptr || first == last) { return out; }
            // Windows environment data is UTF-16, so converting here avoids code-page-dependent cache paths.
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::fast_io::mnp::code_cvt(::fast_io::mnp::strvw(first, last)));
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string u8string_from_chars(char const* str, ::std::size_t len) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            if(str == nullptr || len == 0uz) { return out; }
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(str), len});
            return out;
        }

        inline constexpr void append_u8(::uwvm2::utils::container::u8string& out, ::uwvm2::utils::container::u8string_view v) noexcept
        {
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, v);
        }

        inline constexpr void append_u8(::uwvm2::utils::container::u8string& out, ::uwvm2::utils::container::u8string const& v) noexcept
        {
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, v);
        }

        inline constexpr void seed_sha256_update(::fast_io::sha256_context& sha, ::std::byte const* first, ::std::byte const* last) noexcept
        {
            // Empty ranges are skipped so optional identity fields do not require sentinel bytes.
            if(first != last) { sha.update(first, last); }
        }

        inline constexpr void seed_sha256_update(::fast_io::sha256_context& sha, ::uwvm2::utils::container::u8string const& v) noexcept
        { seed_sha256_update(sha, reinterpret_cast<::std::byte const*>(v.cbegin()), reinterpret_cast<::std::byte const*>(v.cend())); }

        template <::std::size_t N>
        inline constexpr void seed_sha256_update_literal(::fast_io::sha256_context& sha, char8_t const (&v)[N]) noexcept
        { seed_sha256_update(sha, reinterpret_cast<::std::byte const*>(v), reinterpret_cast<::std::byte const*>(v + N - 1uz)); }

        template <::std::integral T>
        inline constexpr void seed_sha256_update_le(::fast_io::sha256_context& sha, T v) noexcept
        {
            auto const le{::fast_io::little_endian(v)};
            auto const first{reinterpret_cast<::std::byte const*>(::std::addressof(le))};
            seed_sha256_update(sha, first, first + sizeof(le));
        }

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
# ifndef _WIN32_WINDOWS
        template <::std::size_t N>
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string win32_environment_variable(char16_t const (&name)[N]) noexcept
        {
            static_assert(N != 0uz);
            static_assert((N - 1uz) * sizeof(char16_t) <= static_cast<::std::size_t>(UINT_LEAST16_MAX));

            // Querying the process environment directly keeps Unicode values lossless and avoids narrow Win32 fallbacks.
            auto const curr_peb{::fast_io::win32::nt::nt_get_current_peb()};
            if(curr_peb == nullptr || curr_peb->ProcessParameters == nullptr) { return {}; }

            ::fast_io::win32::nt::unicode_string env_us{.Length = static_cast<::std::uint_least16_t>((N - 1uz) * sizeof(char16_t)),
                                                        .MaximumLength = static_cast<::std::uint_least16_t>(N * sizeof(char16_t)),
                                                        .Buffer = const_cast<char16_t*>(name)};

            constexpr ::std::uint_least32_t status_success{};
            constexpr ::std::uint_least32_t status_buffer_too_small{0xC000'0023u};
            ::uwvm2::utils::container::array<char16_t, 260uz> small_buffer{};
            ::fast_io::win32::nt::unicode_string out_us{.Length = 0u,
                                                        .MaximumLength = static_cast<::std::uint_least16_t>(small_buffer.size() * sizeof(char16_t)),
                                                        .Buffer = small_buffer.data()};

            auto const env{curr_peb->ProcessParameters->Environment};
            auto status{::fast_io::win32::nt::RtlQueryEnvironmentVariable_U(env, ::std::addressof(env_us), ::std::addressof(out_us))};
            if(status == status_success)
            {
                // Most cache path variables fit the stack buffer, keeping startup allocation-free on the common path.
                auto const out_len{out_us.Length / sizeof(char16_t)};
                return u8string_from_u16(small_buffer.cbegin(), small_buffer.cbegin() + out_len);
            }
            if(status != status_buffer_too_small) { return {}; }

            auto value_len{static_cast<::std::size_t>(out_us.Length / sizeof(char16_t))};
            if(value_len == 0uz) { value_len = 32767uz; }
            if(value_len > 32767uz) { return {}; }

            ::uwvm2::utils::container::u16string value{};
            value.resize(value_len);
            out_us = ::fast_io::win32::nt::unicode_string{.Length = 0u,
                                                          .MaximumLength = static_cast<::std::uint_least16_t>(value.size() * sizeof(char16_t)),
                                                          .Buffer = value.data()};

            status = ::fast_io::win32::nt::RtlQueryEnvironmentVariable_U(env, ::std::addressof(env_us), ::std::addressof(out_us));
            if(status != status_success) { return {}; }
            value.resize(static_cast<::std::size_t>(out_us.Length / sizeof(char16_t)));
            return u8string_from_u16(value.cbegin(), value.cend());
        }

        template <::std::size_t N>
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(char16_t const (&name)[N],
                                                                                                    ::uwvm2::utils::container::u8string_view suffix) noexcept
        {
            auto out{win32_environment_variable(name)};
            if(out.empty()) { return {}; }
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, suffix);
            return out;
        }
# else
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string win32_environment_variable(char const* name) noexcept
        {
            auto const required{::fast_io::win32::GetEnvironmentVariableA(name, nullptr, 0u)};
            if(required == 0u) { return {}; }

            ::uwvm2::utils::container::string value{};
            value.resize(required);
            auto const written{::fast_io::win32::GetEnvironmentVariableA(name, value.data(), required)};
            if(written == 0u || written >= required) { return {}; }
            value.resize(written);

            ::uwvm2::utils::container::u8string out{};
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref,
                                 ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(value.cbegin()),
                                                                          static_cast<::std::size_t>(value.cend() - value.cbegin())});
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(char const* name,
                                                                                                    ::uwvm2::utils::container::u8string_view suffix) noexcept
        {
            auto out{win32_environment_variable(name)};
            if(out.empty()) { return {}; }
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, suffix);
            return out;
        }
# endif
#else
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string environment_variable(char const* name) noexcept
        {
            auto const value{details::posix::libc_getenv(name)};
            if(value == nullptr || *value == '\0') { return {}; }
            return u8string_from_cstr(value);
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(char const* name,
                                                                                                    ::uwvm2::utils::container::u8string_view suffix) noexcept
        {
            auto out{environment_variable(name)};
            if(out.empty()) { return {}; }
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, suffix);
            return out;
        }
#endif

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string llvm_version_string() noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if defined(LLVM_VERSION_STRING)
            // LLVM is part of the cache context because backend updates can change object layout without IR changes.
            return u8string_from_cstr(LLVM_VERSION_STRING);
# elif defined(LLVM_VERSION_MAJOR) && defined(LLVM_VERSION_MINOR) && defined(LLVM_VERSION_PATCH)
            ::uwvm2::utils::container::u8string out{};
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, LLVM_VERSION_MAJOR, u8".", LLVM_VERSION_MINOR, u8".", LLVM_VERSION_PATCH);
            return out;
# else
            return ::uwvm2::utils::container::u8string{u8"llvm-unknown"};
# endif
#else
            return ::uwvm2::utils::container::u8string{u8"llvm-disabled"};
#endif
        }

#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            llvm_jit_policy_name(::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_policy_t policy) noexcept
        {
            using enum ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_policy_t;
            switch(policy)
            {
                case debug: return u8"debug";
                case default_policy: return u8"default";
                case fast_compile: return u8"fast_compile";
                case balanced: return u8"balanced";
                case max: return u8"max";
                default: return u8"unknown";
            }
        }
#endif
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string default_cache_directory() noexcept
    {
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
# ifndef _WIN32_WINDOWS
        // Prefer per-user cache locations so executable cache objects are not shared between unrelated accounts.
        if(auto out{details::cache_directory_from_env(u"LOCALAPPDATA", u8"/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env(u"USERPROFILE", u8"/AppData/Local/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        // Temporary directories are fallbacks because they may be cleaned aggressively by the OS.
        if(auto out{details::cache_directory_from_env(u"TEMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env(u"TMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
# else
        // Legacy Windows builds use the narrow API path but keep the same per-user preference order.
        if(auto out{details::cache_directory_from_env("LOCALAPPDATA", u8"/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("USERPROFILE", u8"/AppData/Local/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TEMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
# endif
        return ::uwvm2::utils::container::u8string{u8".uwvm2-llvm-jit-cache"};
#elif defined(__APPLE__) && defined(__MACH__)
        // macOS convention keeps large generated artifacts under Library/Caches instead of the project tree.
        if(auto out{details::cache_directory_from_env("HOME", u8"/Library/Caches/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TMPDIR", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        return ::uwvm2::utils::container::u8string{u8"/tmp/uwvm2/llvm-jit"};
#else
        if(auto out{details::environment_variable("XDG_CACHE_HOME")}; !out.empty())
        {
            // XDG_CACHE_HOME is the first choice on Unix-like systems because it is explicitly for regenerable data.
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, u8"/uwvm2/llvm-jit");
            return out;
        }
        if(auto out{details::environment_variable("HOME")}; !out.empty())
        {
            // HOME/.cache mirrors the XDG default when the environment variable is absent.
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, u8"/.cache/uwvm2/llvm-jit");
            return out;
        }
        if(auto out{details::cache_directory_from_env("TMPDIR", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        return ::uwvm2::utils::container::u8string{u8"/tmp/uwvm2/llvm-jit"};
#endif
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string configured_cache_directory() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        using cache_path_mode_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_path_mode_t;
        // Runtime configuration wins over platform defaults so sandboxed embedders can pick an isolated directory.
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_cache_path_mode == cache_path_mode_t::custom_path)
        {
            return ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_cache_path;
        }
#endif
        return default_cache_directory();
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_target_triple() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT)
        // The triple gates object reuse because relocation model, calling convention, and object format depend on it.
        auto triple{::llvm::sys::getDefaultTargetTriple()};
        return details::u8string_from_chars(triple.data(), triple.size());
#else
        return ::uwvm2::utils::container::u8string{u8"llvm-jit-disabled"};
#endif
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_target_triple(::llvm::TargetMachine const& target_machine) noexcept
    {
        auto triple{target_machine.getTargetTriple().str()};
        return details::u8string_from_chars(triple.data(), triple.size());
    }
#endif

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_cpu_name() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT)
        // The CPU name captures backend tuning choices that may not be represented by feature strings alone.
        auto cpu{::llvm::sys::getHostCPUName()};
        return details::u8string_from_chars(cpu.data(), cpu.size());
#else
        return ::uwvm2::utils::container::u8string{u8"llvm-jit-disabled"};
#endif
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_cpu_name(::llvm::TargetMachine const& target_machine) noexcept
    {
        auto cpu{target_machine.getTargetCPU()};
        return details::u8string_from_chars(cpu.data(), cpu.size());
    }
#endif

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_cpu_features() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT)
        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> storage{};
        auto features{::llvm::sys::getHostCPUFeatures()};
        storage.reserve(features.size());
        for(auto const& [name, enabled]: features)
        {
            auto feature_name{details::u8string_from_chars(name.data(), name.size())};
            ::uwvm2::utils::container::u8string item{};
            ::uwvm2::utils::container::u8string_ref_uwvm item_ref{::std::addressof(item)};
            ::fast_io::io::print(item_ref, enabled ? u8"+" : u8"-", feature_name);
            storage.push_back(::std::move(item));
        }
        // LLVM exposes features through a map, so sorting makes the fingerprint deterministic across library builds.
        ::std::sort(storage.begin(), storage.end());

        ::uwvm2::utils::container::u8string out{};
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
        for(auto const& feature: storage)
        {
            if(!out.empty()) { ::fast_io::io::print(ref, u8","); }
            ::fast_io::io::print(ref, feature);
        }
        if(out.empty()) { ::fast_io::io::print(ref, u8"generic"); }
        return out;
#else
        return ::uwvm2::utils::container::u8string{u8"llvm-jit-disabled"};
#endif
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string collect_cpu_features(::llvm::TargetMachine const& target_machine) noexcept
    {
        auto features{target_machine.getTargetFeatureString()};
        return details::u8string_from_chars(features.data(), features.size());
    }
#endif

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string default_codegen_policy_name() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        auto out{details::make_cache_key(u8"codegen-policy")};
        // Optimization policy is hashed because different policies can emit different native code for the same module.
        details::append_cache_key_value(out, u8"backend", u8"llvm-jit");
        details::append_cache_key_value(out, u8"policy", details::llvm_jit_policy_name(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_policy));
        return out;
#else
        auto out{details::make_cache_key(u8"codegen-policy")};
        details::append_cache_key_value(out, u8"backend", u8"llvm-jit-disabled");
        return out;
#endif
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string uwvm_runtime_abi_fingerprint() noexcept
    {
        auto out{details::make_cache_key(u8"uwvm-runtime-abi")};
        // The schema version separates intentional ABI-fingerprint changes from ordinary project version changes.
        details::append_cache_key_value(out, u8"schema", u8"uwvm2-runtime-abi-v27");
        // Core 3 call_ref/return_call_ref now share the authoritative rich-type
        // matcher in the pure validator and both fused compilers. Keep this
        // validation-policy identity independent of project/source IDs: an
        // embedder may reuse them, and unchanged emitted IR alone cannot name
        // the admission rules under which an older object was compiled. Full,
        // parallel-full, lazy-single/group and tiered materialization all derive
        // their actual uwvm_abi from this common fingerprint before disk lookup.
        // The checkpoint publisher hashes these exact context bytes as well.
        details::append_cache_key_value(out, u8"wasm-core3-validation", u8"fused-rich-call-reference-v1");
        // Typed scalar entries use LLVM tailcc on the qualified backend family;
        // raw C++ entry pointers retain their old convention. Old native objects
        // must not mix these private typed ABIs even with an identical source ID.
        details::append_cache_key_value(out, u8"typed-call-abi", u8"qualified-tailcc-explicit-tuple-buffer-v3");
        // X86 admission follows the actual aligned-frame repair capability,
        // never LLVM version alone. Record both outcomes and the explicit
        // backport opt-in in every cache/checkpoint context.
        details::append_cache_key_value(out, u8"x86-tailcc-admission", u8"aligned-frame-capability-v1");
#if (defined(LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED) && LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED == 1) || \
    (defined(UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED) && UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED == 1)
        details::append_cache_key_value(out, u8"x86-tailcc-aligned-frame", u8"fixed");
#else
        details::append_cache_key_value(out, u8"x86-tailcc-aligned-frame", u8"unfixed");
#endif
#if defined(UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED) && UWVM_LLVM_X86_TAILCC_ALIGNED_FRAME_FIXED == 1
        details::append_cache_key_value(out, u8"x86-tailcc-aligned-frame-backport", u8"qualified");
#else
        details::append_cache_key_value(out, u8"x86-tailcc-aligned-frame-backport", u8"unqualified");
#endif
#if defined(UWVM_LLVM_X86_TAILCC_FIXED)
        details::append_cache_key_value(out, u8"x86-tailcc-backport", u8"qualified");
#else
        details::append_cache_key_value(out, u8"x86-tailcc-backport", u8"version-policy");
#endif
#if defined(__riscv)
        // The ordinary build can opt into a repaired RISC-V TailCC provider without
        // changing LLVM_VERSION_STRING or its source ID. A cached typed entry from
        // the other setting has an incompatible native calling convention.
# if defined(UWVM_LLVM_RISCV_TAILCC_FIXED) && UWVM_LLVM_RISCV_TAILCC_FIXED == 1
        details::append_cache_key_value(out, u8"riscv-tailcc-backport", u8"qualified");
# else
        details::append_cache_key_value(out, u8"riscv-tailcc-backport", u8"unqualified");
# endif
#endif
#if defined(__loongarch_grlen) && __loongarch_grlen == 64
        // The repaired provider has a different private typed ABI even if its
        // version/source labels match an older C-ABI provider. Cache both
        // configurations normally, but never reuse objects across them.
# if (defined(UWVM_LLVM_LOONGARCH64_TAILCC_FIXED) && UWVM_LLVM_LOONGARCH64_TAILCC_FIXED == 1) || \
     (defined(LLVM_UWVM_ROS_LOONGARCH64_TAILCC) && LLVM_UWVM_ROS_LOONGARCH64_TAILCC == 1)
        details::append_cache_key_value(out, u8"loongarch64-tailcc-backport", u8"qualified-v1");
# else
        details::append_cache_key_value(out, u8"loongarch64-tailcc-backport", u8"unqualified");
# endif
#endif
        // TailCC and the legacy C typed ABI both support authenticated caches.
        // Their parameter-area ownership differs even with identical SDK labels.
#if (defined(__linux__) && defined(__mips__) && defined(__mips64) && defined(__MIPSEL__) && __SIZEOF_POINTER__ == 8 && \
     defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips) && \
     ((defined(UWVM_LLVM_MIPS_N64EL_R2_TAILCC_FIXED) && UWVM_LLVM_MIPS_N64EL_R2_TAILCC_FIXED == 1) || \
      (defined(LLVM_UWVM_ROS_MIPS_N64EL_R2_TAILCC) && LLVM_UWVM_ROS_MIPS_N64EL_R2_TAILCC == 1)))
        details::append_cache_key_value(out, u8"mips-n64el-r2-tailcc", u8"callee-pop-v1");
#else
        details::append_cache_key_value(out, u8"mips-n64el-r2-tailcc", u8"producer-default");
#endif
        // A limited Memory64 reservation uses software bounds instead of the
        // fixed partial-guard threshold. Retire older generated address checks.
        details::append_cache_key_value(out, u8"memory64-reservation-bounds", u8"declared-maximum-v1");
        // Version the SIMD sign lowering uniformly across cache configurations and native targets.
        details::append_cache_key_value(out, u8"simd-sign-lowering", u8"mips64-scalar-mask-v1");
        details::append_cache_key_value(out, u8"simd-phi-lowering", u8"mips64-no-msa-word-vector-o0-v1");
        details::append_cache_key_value(out, u8"mips-debug-long-branch", u8"static-pcrel-range-and-contained-absolute-v1");
        // Explicit table initializers add a borrowed expression pointer to local table records. Cached native
        // code must never use the previous record stride/owner offset, even with an unchanged embedder source ID.
        details::append_cache_key_value(out, u8"local-table-layout", u8"typed-u64-limits-initializer-expression-v2");
        // ref.func objects now relocate directly to stable VM function records. Older emitters do not bind these symbols.
        details::append_cache_key_value(out, u8"function-reference-emission", u8"relocated-record-ssa-v1");
        // Native indirect/reference calls admit declared Core 3 function
        // subtypes through a caller-local preorder interval. Old exact-ID
        // objects must not be replayed against these new target encodings.
        details::append_cache_key_value(out, u8"function-call-type-check", u8"caller-forest-interval-v1");
        details::append_cache_key_value(out, u8"tag-instance-storage", u8"local-and-imported-v1");
        // Typed native catches use relocatable typeinfo/personality declarations and cleanup edges.
        // Reject pre-native-EH objects even when an embedder reuses the same source identifier.
        details::append_cache_key_value(out, u8"exception-control", u8"numeric-itanium-dwarf-native-v2");
        details::append_cache_key_value(out, u8"exception-cleanup", u8"typed-guest-invoke-v1");
        details::append_cache_key_value(out,u8"pending-numeric-memory",u8"validated-owned-scalar-preserve-native-unwind-v1");
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
        details::append_cache_key_value(out,u8"pending-numeric-payload",u8"immutable-schema-packed-tuple-fused-catch-v6-256");
#endif
        // COFF partitions must retain an executable local SEH personality;
        // reject objects emitted by the earlier declaration-only partitioner.
        details::append_cache_key_value(out, u8"seh-personality-partition", u8"local-definition-v1");
#if defined(UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT) && UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT == 1
        // The candidate may emit typed invoke/landingpad and local SEH personality
        // relocations where the old A64 build rejected native guest EH. An identical
        // embedder source ID must not let either profile load the other's objects.
        // This identity separates the opt-in; it is not proof of provider support.
        details::append_cache_key_value(out, u8"windows-arm64-gnu-native-eh", u8"qualified-provider-itanium-seh-v1");
#endif
        // mmap records now carry a per-instance reservation extent. Small 32-bit memories require
        // software checks; old fixed-window objects must not survive an unchanged embedder source ID.
        details::append_cache_key_value(out, u8"native-memory-layout", u8"bounded-reservation-aligned64-owner-ordered-size-v4");
        // mmap emits direct accesses; moving allocations use pinning bridges.
        // Source/version strings can be identical across these build configurations.
#if defined(UWVM_SUPPORT_MMAP)
        details::append_cache_key_value(out, u8"native-memory-backend", u8"mmap");
#elif defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
        details::append_cache_key_value(out, u8"native-memory-backend", u8"allocator-concurrent");
#else
        details::append_cache_key_value(out, u8"native-memory-backend", u8"allocator-single");
#endif
        // Do not rely on git/source ids to distinguish products: both builds
        // may deliberately receive the same id from an embedding application.
        details::append_cache_key_value(out, u8"product", cache_product_name);
        // Declaration limits and compiled memory64 immediates preserve all address bits.
        details::append_cache_key_value(out, u8"wasm-memory-addresses", u8"typed-u64-limits-and-memarg-v1");
        // v128 now crosses private Wasm-to-Wasm calls as an LLVM byte vector, not an i128 integer pair.
        // Also reject old objects that could partially write a crossing store before its guard fault.
        details::append_cache_key_value(out, u8"llvm-wasm-v128-abi", u8"ssa-byte-vector16-v1");
        // Reject objects emitted before the no-NEON AArch64 bitmask, i386
        // no-x87/PPC32 minmax, and SPARC demotion repairs. Identical Wasm,
        // LLVM version and CPU features alone do not identify a safe lowering.
        // Keep this independent of project version/source-id discipline.
        // v3 additionally excludes x86_64 no-SSE FP-return libcalls and VE's
        // unsupported short-vector VPU lowering. A source-id escape hatch must
        // not let a previously cached unsafe object bypass these repairs.
        details::append_cache_key_value(out, u8"llvm-simd-scalar-lowering", u8"scalar-target-contract-v3");
        details::append_cache_key_value(out, u8"guarded-store", u8"cross-custom-page-last-byte-preflight-v1");
        // Native objects may omit the u32-sum overflow branch only with the matching unsigned-domain mmap layout.
        // Never reuse such code with the former 2-GiB-front-guard reservation, even when project version fields match.
        details::append_cache_key_value(out, u8"wasm32-mmap-layout", u8"unsigned-8g-domain-tail64-v1");
        // Older native policies retained logical instrumentation; never reuse those objects as native-only code.
        details::append_cache_key_value(out, u8"native-call-stack", u8"physical-activation-no-logical-jit-v1");
        // The successful live probe now uses unique ELF temporary labels on
        // RISC-V/LoongArch. Replaying an older object with aliased FDE labels
        // after that probe would lose frames. Key this independently of source
        // IDs and external LLVM's unchanged version string, for full AND lazy.
        details::append_cache_key_value(out, u8"llvm-elf-local-symbols", u8"unique-temporary-labels-v1");
        // Full/lazy native MIPS selection requires full-width register calls.
        // v2 adds noabicalls: O32/N32 ignored v1's long-calls alone, retaining
        // non-t9 loader stubs. A new probe must not admit those cached objects.
        details::append_cache_key_value(out, u8"llvm-mips-call-relocations", u8"full-width-noabicalls-c-abi-v2");
        // A new successful probe must not admit objects whose async epilogue
        // rows leaked into a later live block, even with a reused source ID.
        details::append_cache_key_value(out, u8"llvm-native-unwind-cfi", u8"cfg-epilogue-state-v1");
        // Same-source-id embedders must reject earlier preemptible MIPS tail targets.
        details::append_cache_key_value(out, u8"llvm-owned-tail-targets", u8"mips-n64-r2-dso-local-v1");
#if defined(__linux__) && __SIZEOF_POINTER__ == 8 && defined(__mips__) && defined(__mips64) && \
    defined(__mips_isa_rev) && __mips_isa_rev == 2 && \
    !defined(__mips16) && !defined(__mips_micromips) && defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && \
    UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1 && defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 23
        details::append_cache_key_value(out, u8"llvm-mips-n64-cfi-fixup", u8"enabled");
#else
        details::append_cache_key_value(out, u8"llvm-mips-n64-cfi-fixup", u8"producer-default");
#endif
#if defined(UWVM_VERSION_X)
        details::append_cache_key_value_u64(out, u8"version-x", static_cast<::std::uint_least64_t>(UWVM_VERSION_X));
#else
        details::append_cache_key_value(out, u8"version-x", u8"unknown");
#endif
#if defined(UWVM_VERSION_Y)
        details::append_cache_key_value_u64(out, u8"version-y", static_cast<::std::uint_least64_t>(UWVM_VERSION_Y));
#else
        details::append_cache_key_value(out, u8"version-y", u8"unknown");
#endif
#if defined(UWVM_VERSION_Z)
        details::append_cache_key_value_u64(out, u8"version-z", static_cast<::std::uint_least64_t>(UWVM_VERSION_Z));
#else
        details::append_cache_key_value(out, u8"version-z", u8"unknown");
#endif
#if defined(UWVM_VERSION_S)
        details::append_cache_key_value_u64(out, u8"version-s", static_cast<::std::uint_least64_t>(UWVM_VERSION_S));
#else
        details::append_cache_key_value(out, u8"version-s", u8"unknown");
#endif
#if defined(UWVM_GIT_COMMIT_ID)
        details::append_cache_key_value(out, u8"git-commit", UWVM_GIT_COMMIT_ID);
#else
        details::append_cache_key_value(out, u8"git-commit", u8"unknown");
#endif
#if defined(UWVM2_BUILD_SOURCE_ID)
        details::append_cache_key_value(out, u8"build-source-id", UWVM2_BUILD_SOURCE_ID);
#else
        details::append_cache_key_value(out, u8"build-source-id", u8"unknown");
#endif
#if defined(UWVM_GIT_COMMIT_DATA)
        details::append_cache_key_value(out, u8"git-commit-date", UWVM_GIT_COMMIT_DATA);
#else
        details::append_cache_key_value(out, u8"git-commit-date", u8"unknown");
#endif
#if defined(UWVM_GIT_HAS_UNCOMMITTED_MODIFICATIONS)
        details::append_cache_key_value(out, u8"git-dirty", u8"1");
#elif defined(UWVM_GIT_COMMIT_ID)
        details::append_cache_key_value(out, u8"git-dirty", u8"0");
#else
        details::append_cache_key_value(out, u8"git-dirty", u8"unknown");
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        details::append_cache_key_value(out, u8"runtime-jit", u8"uwvm-int-llvm-jit-tiered");
#elif defined(UWVM_RUNTIME_LLVM_JIT)
        details::append_cache_key_value(out, u8"runtime-jit", u8"llvm-jit");
#else
        details::append_cache_key_value(out, u8"runtime-jit", u8"none");
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        // Keep cached native objects separated when runtime bridge symbol naming or bridge-call ABI details change.
        // v3 also makes generated raw-call operands and status values register-wide, avoiding target-specific narrow
        // integer extension attributes at the handwritten LLVM/C++ ABI boundary.
        details::append_cache_key_value(out, u8"llvm-jit-bridge-symbol-abi", u8"generated-register-wide-internal-entry-v3");
        // Generated status CFG changes in every GC profile. Native bridge ABI
        // is unchanged, but old multi-guard objects cannot bypass this version.
        details::append_cache_key_value(out, u8"llvm-gc-status-dispatch", u8"zero-fast-cold-v1");
        details::append_cache_key_value(out,u8"gc-field-classification",u8"immutable-any-reference-bit-v1");
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        details::append_cache_key_value(out,u8"gc-array-construction-fill",u8"hybrid-native-seed512-libc-copy-v2");
#endif
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
        details::append_cache_key_value(out,u8"gc-array-pressure",u8"count4096-wide-numeric-carrier-1mib-actual-entry-v2");
#endif
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
        details::append_cache_key_value(out,u8"gc-general-slab",u8"pow2-carrier-units-reference-and-packed-one-lock-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
        // Host-owned type-layout/tracing profiles differ even when an embedder
        // deliberately reuses its source ID; never share native cache context.
        // This identity does not authorize mixing differently built C++ objects.
        details::append_cache_key_value(out, u8"gc-precise-trace-metadata", u8"owned-canonical-reference-plan-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
        details::append_cache_key_value(out, u8"gc-inline-trace-metadata", u8"owned-small-struct-bitmap64-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1
        // Conservative native allocator profile identity; no IR or ABI change.
        details::append_cache_key_value(out, u8"gc-numeric-slab-locking", u8"reserve-construct-commit-one-lock-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION) && UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION == 1
        details::append_cache_key_value(out, u8"gc-header-publication", u8"global-stripe-local-store-single-list-cas-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
        details::append_cache_key_value(out, u8"gc-collection-local-membership", u8"closed-canonical-local-v1");
#endif
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI) && UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI == 1 && !defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        details::append_cache_key_value(out, u8"gc-array-set-raw-carrier-abi", u8"six-scalars-u64-index-status-native-byte-chunks-v3-inline-checked-store-and-payload");
#endif
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
        details::append_cache_key_value(out, u8"gc-array-set-reference-auth", u8"ordinary-local-same-operation-v8-fused-local-type-check-subtree-intervals-scalar-bypass-inline-membership-scalar-no-lease");
#endif
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
        // Independent default-off candidate: two-carrier generic setters and
        // five-uintptr_t/status-only setters must not share native cache context.
        // Symbol names additionally bind actual C++ function, FunctionType and
        // gc_struct_set32_registerwide_v1; no experimental six-switch profile implies it.
        details::append_cache_key_value(out, u8"llvm-gc-struct-set32-abi", u8"registerwide-status0-cold-v2");
#endif
        details::append_cache_key_value(out, u8"llvm-wasm-typed-result-abi", u8"void-scalar-tuple-struct-v1");
        details::append_cache_key_value(out, u8"llvm-wasm-nan-arithmetic", u8"native-constrained-v1");
        details::append_cache_key_value(out, u8"llvm-native-stack-probes", u8"inline-supported-targets-riscv-half-page-v2");
        // Reject objects emitted with process-address immediates/carriers,
        // even in builds without a git revision or verified source identity.
        details::append_cache_key_value(out, u8"llvm-host-symbol-address", u8"relocatable-pointer-carrier-v1");
        // These keys invalidate machine code, not merely diagnostics. A build can
        // lack an embedded git revision yet load old objects with ST0 returns,
        // double rounding or unnormalized native NaNs. Keep independent ABI and
        // arithmetic-semantic revisions so cache hits cannot bypass a source fix.
        // Native globals use integer carriers on every target. i386 additionally
        // uses integer FP results even on SSE2 hosts; old ST0 objects are incompatible.
        // The x86_64 no-SSE byte-buffer path now also excludes x87 transport
        // and FP-return rounding libcalls. Keep ordinary host C FP ABIs intact.
        details::append_cache_key_value(out, u8"llvm-wasm-fp-bit-abi", u8"integer-globals-x86-no-sse-transport-v2");
        details::append_cache_key_value(out, u8"llvm-wasm-native-nan", u8"canonical-native-rounding-v1");
        // RV32 f64 nearest must not reload an object requiring an unavailable
        // C23 roundeven libcall (e.g. musl), even without a git revision in the key.
        details::append_cache_key_value(out, u8"llvm-wasm-rv32-nearest", u8"fixed-rne-d-or-integer-v1");
        // Separates pre-fix objects even when a build has no embedded git revision.
        details::append_cache_key_value(out, u8"llvm-wasm-fp-rounding", u8"extended-round-to-odd-v1");
        details::append_cache_key_value(out, u8"llvm-wasm-fp-rounding-mode",
            ::uwvm2::runtime::compiler::shared::strict_float::needs_extended_rounding
                ? ::uwvm2::utils::container::u8string_view{u8"extended"}
                : ::uwvm2::utils::container::u8string_view{u8"native"});
#endif
        return out;
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::array<::std::byte, cache_ed25519_seed_size> collect_signature_seed(
        cache_context const& ctx) noexcept
    {
        ::fast_io::sha256_context sha{};
        // Separate the deterministic integrity domains as well as disk paths.
        // This is not a secret-key boundary against a same-account attacker.
        details::seed_sha256_update_literal(sha, u8"uwvm2-llvm-jit-cache-ed25519-seed-v2");
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
        // The user id prevents one local account from producing cache signatures accepted as another account.
        details::seed_sha256_update_literal(sha, u8"posix-user");
        details::seed_sha256_update_le(sha, static_cast<::std::uint_least64_t>(details::posix::libc_getuid()));
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
        // The Windows user name is the available per-user identity for the deterministic cache signature seed.
        details::seed_sha256_update_literal(sha, u8"win32-user");
# ifndef _WIN32_WINDOWS
        details::seed_sha256_update(sha, details::win32_environment_variable(u"USERNAME"));
# else
        details::seed_sha256_update(sha, details::win32_environment_variable("USERNAME"));
# endif
#else
        details::seed_sha256_update_literal(sha, u8"unknown-user");
#endif
        // The seed includes the same compatibility inputs as the object key so signatures cannot be replayed across contexts.
        details::seed_sha256_update_literal(sha, u8"target");
        details::seed_sha256_update(sha, ctx.target_triple);
        details::seed_sha256_update_literal(sha, u8"cpu");
        details::seed_sha256_update(sha, ctx.cpu_name);
        details::seed_sha256_update_literal(sha, u8"features");
        details::seed_sha256_update(sha, ctx.cpu_features);
        details::seed_sha256_update_literal(sha, u8"codegen");
        details::seed_sha256_update(sha, ctx.codegen_policy);
        sha.do_final();

        ::uwvm2::utils::container::array<::std::byte, cache_ed25519_seed_size> out{};
        sha.digest_to_byte_ptr(out.data());
        return out;
    }

    [[nodiscard]] inline constexpr cache_policy default_cache_policy() noexcept
    {
        cache_policy policy{};
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        // Policy mirrors runtime flags so the cache can be disabled or relaxed without changing call-site code.
        policy.enable = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_cache_path_mode !=
                        ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_path_mode_t::disabled;
        policy.generate_signature = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_no_sign;
        policy.verify_signature = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_no_verify;
#endif
        constexpr source_provenance_policy_inputs provenance_policy{
#if defined(UWVM_GIT_COMMIT_ID)
            .has_git_commit = true,
#endif
#if defined(UWVM2_BUILD_SOURCE_ID)
            .has_verified_build_source_id = true,
#endif
#if defined(UWVM_GIT_HAS_UNCOMMITTED_MODIFICATIONS)
            .git_worktree_is_dirty = true,
#endif
#if defined(UWVM2_ALLOW_UNSAFE_DIRTY_LLVM_JIT_CACHE)
            .allow_unsafe_dirty_cache = true,
#endif
#if defined(UWVM2_ALLOW_UNSAFE_UNPROVENANCED_LLVM_JIT_CACHE)
            .allow_unsafe_unprovenanced_cache = true,
#endif
        };
        // The generated module hash does not cover host bridge/runtime/unwind semantics. Likewise, a deterministic
        // signature provides integrity and context binding, not source provenance. Therefore dirty or unidentified
        // source builds cannot publish or reuse persistent native objects unless their distinct developer-only
        // escape hatch was explicitly selected at build time.
        if(!source_provenance_allows_persistent_cache(provenance_policy)) { policy.enable = false; }
        // If signing support is missing, disabling the whole cache is safer than silently accepting unsigned native code.
        if(policy.enable && (policy.generate_signature || policy.verify_signature) && !cache_ed25519_identity_signature_available) { policy.enable = false; }
        return policy;
    }

    [[nodiscard]] inline constexpr cache_context default_cache_context(::uwvm2::utils::container::u8string_view cache_key,
                                                                       ::uwvm2::utils::container::u8string_view codegen_policy = {}) noexcept
    {
        cache_context ctx{};
        ctx.cache_dir = configured_cache_directory();
        // The caller's key is copied into owned storage because contexts can outlive the original module strings.
        ::uwvm2::utils::container::u8string_ref_uwvm cache_key_ref{::std::addressof(ctx.cache_key)};
        ::fast_io::io::print(cache_key_ref, cache_key);
        ctx.target_triple = collect_target_triple();
        ctx.cpu_name = collect_cpu_name();
        ctx.cpu_features = collect_cpu_features();
        ctx.llvm_version = details::llvm_version_string();
        ctx.uwvm_abi = uwvm_runtime_abi_fingerprint();
        if(codegen_policy.empty()) { ctx.codegen_policy = default_codegen_policy_name(); }
        else
        {
            ::uwvm2::utils::container::u8string_ref_uwvm codegen_policy_ref{::std::addressof(ctx.codegen_policy)};
            ::fast_io::io::print(codegen_policy_ref, codegen_policy);
        }
        ctx.signature_seed = collect_signature_seed(ctx);
        // A generated seed is marked explicitly so store/load code can fail closed if future constructors omit it.
        ctx.has_signature_seed = true;
        return ctx;
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    [[nodiscard]] inline constexpr cache_context default_cache_context(::uwvm2::utils::container::u8string_view cache_key,
                                                                       ::uwvm2::utils::container::u8string_view codegen_policy,
                                                                       ::llvm::TargetMachine const& target_machine) noexcept
    {
        cache_context ctx{};
        ctx.cache_dir = configured_cache_directory();
        // TargetMachine-derived fields match the actual backend instance rather than the host default guess.
        ::uwvm2::utils::container::u8string_ref_uwvm cache_key_ref{::std::addressof(ctx.cache_key)};
        ::fast_io::io::print(cache_key_ref, cache_key);
        ctx.target_triple = collect_target_triple(target_machine);
        ctx.cpu_name = collect_cpu_name(target_machine);
        ctx.cpu_features = collect_cpu_features(target_machine);
        ctx.llvm_version = details::llvm_version_string();
        ctx.uwvm_abi = uwvm_runtime_abi_fingerprint();
        if(codegen_policy.empty()) { ctx.codegen_policy = default_codegen_policy_name(); }
        else
        {
            ::uwvm2::utils::container::u8string_ref_uwvm codegen_policy_ref{::std::addressof(ctx.codegen_policy)};
            ::fast_io::io::print(codegen_policy_ref, codegen_policy);
        }
        ctx.signature_seed = collect_signature_seed(ctx);
        // The signature seed is recomputed after TargetMachine fields so cross-target JITs do not share identities.
        ctx.has_signature_seed = true;
        return ctx;
    }
#endif
}  // namespace uwvm2::runtime::llvm_jit_cache

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
