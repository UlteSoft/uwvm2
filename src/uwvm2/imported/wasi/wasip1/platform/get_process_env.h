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
# include <memory>
# include <new>
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::platform
{
    namespace posix
    {
#if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
        // Darwin does not provide an `environ` function; here we use `_NSGetEnviron` to obtain it.
        extern char*** _NSGetEnviron() noexcept __asm__("__NSGetEnviron");
#elif defined(__MSDOS__) || defined(__DJGPP__)
        // djgpp only provides `char** _environ`. For consistency, a symbolic link is used here.
        extern char** environ __asm__("__environ");
#elif !(defined(_WIN32) || defined(__CYGWIN__))
        // Reference to the global `environ` variable
        extern "C" char** environ;
#endif
    }  // namespace posix

    struct buf_guard_t
    {
        using allocator_t = ::fast_io::native_thread_local_allocator;

        void* buf{};

        inline constexpr buf_guard_t(void* buf_o) noexcept : buf{buf_o} {}

        inline constexpr buf_guard_t(buf_guard_t const& other) noexcept = delete;
        inline constexpr buf_guard_t& operator= (buf_guard_t const& other) noexcept = delete;
        inline constexpr buf_guard_t(buf_guard_t&& other) noexcept = delete;
        inline constexpr buf_guard_t& operator= (buf_guard_t&& other) noexcept = delete;

        inline constexpr ~buf_guard_t()
        {
            if(buf) [[likely]] { allocator_t::deallocate(buf); }
        }
    };

    inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> get_process_env() noexcept
    {
        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> result{};

#if defined(_WIN32) || defined(__CYGWIN__)
# if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)
        // Windows 9x

        // Use raii to manage environment strings
        struct fast_io_get_environment_string_a_guard_t
        {
            char* tmp_environ{};

            inline constexpr fast_io_get_environment_string_a_guard_t(char* tmp_environ_o) noexcept : tmp_environ{tmp_environ_o} {}

            inline constexpr fast_io_get_environment_string_a_guard_t(fast_io_get_environment_string_a_guard_t const& other) noexcept = delete;
            inline constexpr fast_io_get_environment_string_a_guard_t& operator= (fast_io_get_environment_string_a_guard_t const& other) noexcept = delete;
            inline constexpr fast_io_get_environment_string_a_guard_t(fast_io_get_environment_string_a_guard_t&& other) noexcept = delete;
            inline constexpr fast_io_get_environment_string_a_guard_t& operator= (fast_io_get_environment_string_a_guard_t&& other) noexcept = delete;

            inline constexpr ~fast_io_get_environment_string_a_guard_t()
            {
                if(tmp_environ) [[likely]] { ::fast_io::win32::FreeEnvironmentStringsA(tmp_environ); }
                // multi-call to destructor is ub
            }
        };

        auto env_str{::fast_io::win32::GetEnvironmentStringsA()};
        fast_io_get_environment_string_a_guard_t env_str_guard{env_str};

        // Do not perform APC escaping from ANSI to UTF-8 under Windows 9x.
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        for(auto env_str_curr{reinterpret_cast<char8_t_const_may_alias_ptr>(env_str)};;)
        {
            auto const len{::fast_io::cstr_len(env_str_curr)};
            // When encountering a double null character (env terminator), execution will terminate here.
            if(len == 0uz) [[unlikely]] { break; }

            auto const next{env_str_curr + len};
            result.emplace_back(env_str_curr, next);

            env_str_curr = next + 1u;

            // The ending consists of two null characters.
            if(*env_str_curr == u8'\0') { break; }
        }

        return result;

# else
        auto const c_peb{::fast_io::win32::nt::nt_get_current_peb()};
        // c_peb->ProcessParameters cannot be null.
        auto const c_peb_environment{c_peb->ProcessParameters->Environment};
        auto const c_peb_environment_size{c_peb->ProcessParameters->EnvironmentSize};
        auto const c_peb_environment_end{c_peb_environment + c_peb_environment_size / sizeof(char16_t)};

        // Converting everything at once is faster than converting one by one.
        auto const env_str{
            ::uwvm2::utils::container::tlc::u8concat_uwvm_tlc(::fast_io::mnp::code_cvt(::fast_io::mnp::strvw(c_peb_environment, c_peb_environment_end)))};

        auto const env_str_end{env_str.cend()};

        for(auto env_str_curr{env_str.cbegin()};;)
        {
            auto const len{::fast_io::cstr_nlen(env_str_curr, static_cast<::std::size_t>(env_str_end - env_str_curr))};
            // Length 0 is not included in envs.
            if(len == 0uz) [[unlikely]] { break; }

            auto const next{env_str_curr + len};
            result.emplace_back(env_str_curr, next);

            // Prevent reaching the end (Under normal circumstances, it will not hit)
            // no necessary to check `next == env_str_end`
            env_str_curr = next + 1u;

            // The ending consists of two null characters.
            if(*env_str_curr == u8'\0') { break; }
        }

        return result;

# endif
#elif defined(__APPLE__) || defined(__DARWIN_C_LEVEL)

        auto const envpp{posix::_NSGetEnviron()};
        if(envpp == nullptr) [[unlikely]] { return result; }

        auto const environ_begin{*envpp};
        if(environ_begin == nullptr) [[unlikely]] { return result; }

        for(auto environ_curr{environ_begin}; *environ_curr != nullptr; ++environ_curr)
        {
            result.push_back(::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt_os_c_str(*environ_curr)));
        }

        return result;

#else

        auto const environ_begin{posix::environ};
        if(environ_begin == nullptr) [[unlikely]] { return result; }

        for(auto environ_curr{environ_begin}; *environ_curr != nullptr; ++environ_curr)
        {
            result.push_back(::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt_os_c_str(*environ_curr)));
        }

        return result;

#endif
    }

}  // namespace uwvm2::imported::wasi::wasip1::platform

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
