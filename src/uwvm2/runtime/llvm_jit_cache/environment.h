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

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache
{
    namespace details
    {
        namespace posix
        {
#if !(defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__))
# if defined(__DARWIN_C_LEVEL) || defined(__DJGPP__)
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
            auto const len{::std::strlen(str)};
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(str), len});
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string u8string_from_u16(char16_t const* first, char16_t const* last) noexcept
        {
            ::uwvm2::utils::container::u8string out{};
            if(first == nullptr || first == last) { return out; }
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
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(
            char16_t const (&name)[N], ::uwvm2::utils::container::u8string_view suffix) noexcept
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

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(
            char const* name, ::uwvm2::utils::container::u8string_view suffix) noexcept
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

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string cache_directory_from_env(
            char const* name, ::uwvm2::utils::container::u8string_view suffix) noexcept
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
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view llvm_jit_policy_name(
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_policy_t policy) noexcept
        {
            using enum ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_policy_t;
            switch(policy)
            {
                case debug:
                    return u8"debug";
                case default_policy:
                    return u8"default";
                case fast_compile:
                    return u8"fast_compile";
                case balanced:
                    return u8"balanced";
                case max:
                    return u8"max";
                default:
                    return u8"unknown";
            }
        }
#endif
    }  // namespace details

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string default_cache_directory() noexcept
    {
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
# ifndef _WIN32_WINDOWS
        if(auto out{details::cache_directory_from_env(u"LOCALAPPDATA", u8"/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env(u"USERPROFILE", u8"/AppData/Local/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env(u"TEMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env(u"TMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
# else
        if(auto out{details::cache_directory_from_env("LOCALAPPDATA", u8"/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("USERPROFILE", u8"/AppData/Local/UlteSoft/uwvm2/cache/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TEMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TMP", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
# endif
        return ::uwvm2::utils::container::u8string{u8".uwvm2-llvm-jit-cache"};
#elif defined(__APPLE__) && defined(__MACH__)
        if(auto out{details::cache_directory_from_env("HOME", u8"/Library/Caches/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        if(auto out{details::cache_directory_from_env("TMPDIR", u8"/uwvm2/llvm-jit")}; !out.empty()) { return out; }
        return ::uwvm2::utils::container::u8string{u8"/tmp/uwvm2/llvm-jit"};
#else
        if(auto out{details::environment_variable("XDG_CACHE_HOME")}; !out.empty())
        {
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            ::fast_io::io::print(ref, u8"/uwvm2/llvm-jit");
            return out;
        }
        if(auto out{details::environment_variable("HOME")}; !out.empty())
        {
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
        ::uwvm2::utils::container::u8string out{};
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
        ::fast_io::io::print(ref,
                             u8"llvm-jit;policy=",
                             details::llvm_jit_policy_name(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_policy));
        return out;
#else
        return ::uwvm2::utils::container::u8string{u8"llvm-jit-disabled"};
#endif
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::array<::std::byte, cache_ed25519_seed_size>
        collect_signature_seed(cache_context const& ctx) noexcept
    {
        ::fast_io::sha256_context sha{};
        details::seed_sha256_update_literal(sha, u8"uwvm2-llvm-jit-cache-ed25519-seed-v1");
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
        details::seed_sha256_update_literal(sha, u8"posix-user");
        details::seed_sha256_update_le(sha, static_cast<::std::uint_least64_t>(::getuid()));
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
        details::seed_sha256_update_literal(sha, u8"win32-user");
# ifndef _WIN32_WINDOWS
        details::seed_sha256_update(sha, details::win32_environment_variable(u"USERNAME"));
# else
        details::seed_sha256_update(sha, details::win32_environment_variable("USERNAME"));
# endif
#else
        details::seed_sha256_update_literal(sha, u8"unknown-user");
#endif
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
        policy.enable = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_cache_path_mode !=
                        ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_path_mode_t::disabled;
        policy.generate_signature = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_no_sign;
        policy.verify_signature = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_cache_no_verify;
#endif
        if(policy.enable && (policy.generate_signature || policy.verify_signature) && !cache_ed25519_identity_signature_available) { policy.enable = false; }
        return policy;
    }

    [[nodiscard]] inline constexpr cache_context default_cache_context(::uwvm2::utils::container::u8string_view cache_key,
                                                                      ::uwvm2::utils::container::u8string_view codegen_policy = {}) noexcept
    {
        cache_context ctx{};
        ctx.cache_dir = configured_cache_directory();
        ::uwvm2::utils::container::u8string_ref_uwvm cache_key_ref{::std::addressof(ctx.cache_key)};
        ::fast_io::io::print(cache_key_ref, cache_key);
        ctx.target_triple = collect_target_triple();
        ctx.cpu_name = collect_cpu_name();
        ctx.cpu_features = collect_cpu_features();
        ctx.llvm_version = details::llvm_version_string();
        ctx.uwvm_abi = ::uwvm2::utils::container::u8string{u8"uwvm2-runtime-abi-v1"};
        if(codegen_policy.empty()) { ctx.codegen_policy = default_codegen_policy_name(); }
        else
        {
            ::uwvm2::utils::container::u8string_ref_uwvm codegen_policy_ref{::std::addressof(ctx.codegen_policy)};
            ::fast_io::io::print(codegen_policy_ref, codegen_policy);
        }
        ctx.signature_seed = collect_signature_seed(ctx);
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
        ::uwvm2::utils::container::u8string_ref_uwvm cache_key_ref{::std::addressof(ctx.cache_key)};
        ::fast_io::io::print(cache_key_ref, cache_key);
        ctx.target_triple = collect_target_triple(target_machine);
        ctx.cpu_name = collect_cpu_name(target_machine);
        ctx.cpu_features = collect_cpu_features(target_machine);
        ctx.llvm_version = details::llvm_version_string();
        ctx.uwvm_abi = ::uwvm2::utils::container::u8string{u8"uwvm2-runtime-abi-v1"};
        if(codegen_policy.empty()) { ctx.codegen_policy = default_codegen_policy_name(); }
        else
        {
            ::uwvm2::utils::container::u8string_ref_uwvm codegen_policy_ref{::std::addressof(ctx.codegen_policy)};
            ::fast_io::io::print(codegen_policy_ref, codegen_policy);
        }
        ctx.signature_seed = collect_signature_seed(ctx);
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
