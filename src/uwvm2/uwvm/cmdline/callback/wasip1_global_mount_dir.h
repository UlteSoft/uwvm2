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
# include <cstring>
# include <cstdlib>
# include <limits>
# include <utility>
# include <atomic>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/imported/wasi/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{

#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

    namespace wasip1_global_mount_dir_details
    {
        // Result for lexical normalization of the guest-visible WASI preopen name.
        // `valid == false` means the path used a relative `..` component that would
        // escape above the relative preopen root. Absolute paths clamp `..` at `/`,
        // matching the usual POSIX lexical shape for absolute paths.
        struct mount_path_normalization_result
        {
            bool valid{};
            ::uwvm2::utils::container::u8string normalized{};
        };

        // Normalize only the WASI guest mount path. This is a lexical transform
        // over the guest-visible preopen name, not a host filesystem operation.
        // Do not use realpath-style canonicalization here: symlinks, case-folding,
        // drive letters, and other host path rules are outside the WASI preopen
        // namespace. The host directory has already been opened as a directory
        // capability before this point.
        //
        // Rules:
        // - collapse repeated `/`;
        // - remove `.` components;
        // - resolve `..` within the guest preopen root;
        // - remove trailing separators, except that `/` stays `/`;
        // - normalize an empty relative result to `.`.
        //
        // The normalized value is used both for storage (unless explicitly
        // disabled) and for component-aware duplicate/overlap checks.
        [[nodiscard]] inline constexpr mount_path_normalization_result normalize_mount_path(::uwvm2::utils::container::u8string_view path) noexcept
        {
            mount_path_normalization_result result{};
            result.normalized.reserve(path.empty() ? 1uz : path.size());

            bool const is_absolute{!path.empty() && path.front_unchecked() == u8'/'};
            if(is_absolute) { result.normalized.push_back(u8'/'); }

            for(::std::size_t curr{}; curr != path.size();)
            {
                while(curr != path.size() && path.index_unchecked(curr) == u8'/') { ++curr; }
                if(curr == path.size()) { break; }

                auto const segment_begin{curr};
                while(curr != path.size() && path.index_unchecked(curr) != u8'/') { ++curr; }
                auto const segment_size{curr - segment_begin};

                ::uwvm2::utils::container::u8string_view const segment{path.data() + segment_begin, segment_size};
                if(segment == u8".") { continue; }

                if(segment == u8"..")
                {
                    if(is_absolute)
                    {
                        auto normalized_size{result.normalized.size()};
                        while(normalized_size > 1uz && result.normalized.index_unchecked(normalized_size - 1uz) != u8'/') { --normalized_size; }

                        if(normalized_size == 1uz) { result.normalized.resize(1uz); }
                        else
                        {
                            result.normalized.resize(normalized_size - 1uz);
                        }
                    }
                    else
                    {
                        if(result.normalized.empty()) [[unlikely]] { return result; }

                        auto normalized_size{result.normalized.size()};
                        while(normalized_size != 0uz && result.normalized.index_unchecked(normalized_size - 1uz) != u8'/') { --normalized_size; }

                        if(normalized_size == 0uz) { result.normalized.clear(); }
                        else
                        {
                            result.normalized.resize(normalized_size - 1uz);
                        }
                    }

                    continue;
                }

                if(is_absolute)
                {
                    if(result.normalized.size() != 1uz) { result.normalized.push_back(u8'/'); }
                }
                else if(!result.normalized.empty())
                {
                    result.normalized.push_back(u8'/');
                }

                result.normalized.append(segment);
            }

            if(result.normalized.empty()) { result.normalized.push_back(u8'.'); }

            result.valid = true;
            return result;
        }

        // Component-aware prefix test for normalized guest mount paths. A plain
        // string prefix is not sufficient: `/usr` must overlap `/usr/lib`, but
        // it must not overlap `/usrbin`.
        [[nodiscard]] inline constexpr bool starts_with_mount_path(::uwvm2::utils::container::u8string_view path,
                                                                   ::uwvm2::utils::container::u8string_view prefix) noexcept
        {
            if(prefix == u8"/") { return !path.empty() && path.front_unchecked() == u8'/'; }
            if(!path.empty() && path.front_unchecked() != u8'/' && prefix == u8".") { return true; }
            if(path.size() < prefix.size()) { return false; }
            if(path.size() == prefix.size()) { return path == prefix; }

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(path.size() <= prefix.size()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

            return (::uwvm2::utils::container::u8string_view{path.data(), prefix.size()} == prefix) && (path.index_unchecked(prefix.size()) == u8'/');
        }

        UWVM_GNU_COLD inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            print_mount_path_normalization_error(::uwvm2::utils::container::u8string_view path) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<wasi dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8": path normalization would escape the preopen root, in ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                path,
                                u8"\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
    }  // namespace wasip1_global_mount_dir_details

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_global_mount_dir_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                            para_begin,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto param_cursor{para_curr + 1u};

        // Check for wasi mount dir
        if(param_cursor == para_end || param_cursor->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_global_mount_dir),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // Parse wasidir
        ::uwvm2::utils::container::u8cstring_view const wasidir{param_cursor->str};

        if(wasidir.empty()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<wasi dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8": cannot be empty\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        param_cursor->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        ++param_cursor;

        // Check system dir argument
        if(param_cursor == para_end || param_cursor->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Missing ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<system dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" after ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<wasi dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" for ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_global_mount_dir),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        ::uwvm2::utils::container::u8cstring_view const system_dir{param_cursor->str};

        // get system dir
        ::fast_io::dir_file entry;  // no initialize

#  if defined(_WIN32) && !defined(_WIN32_WINDOWS)
        if(system_dir.starts_with(u8"::NT::"))
        {
            // nt path

            ::fast_io::u8cstring_view const systemdir_nt_subview{::fast_io::containers::null_terminated, system_dir.subview(6uz)};

            if(::uwvm2::uwvm::io::show_nt_path_warning)
            {
                // Output the main information and memory indication
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    // 1
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Resolve to NT path: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    systemdir_nt_subview,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (nt-path)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::nt_path_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(nt-path)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

#   ifdef UWVM_CPP_EXCEPTIONS
            try
#   endif
            {
                // allow symlink
                ::fast_io::native_file tmp{::fast_io::io_kernel, systemdir_nt_subview, ::fast_io::open_mode::directory | ::fast_io::open_mode::follow};
                entry = ::fast_io::dir_file{tmp.release()};
            }
#   ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Unable to open system dir \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    systemdir_nt_subview,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\": ",
                                    e,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
#   endif
        }
        else
        {
            // win32 path

#   ifdef UWVM_CPP_EXCEPTIONS
            try
#   endif
            {
                // allow symlink
                entry = ::fast_io::dir_file{system_dir, ::fast_io::open_mode::follow};
            }
#   ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Unable to open system dir \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    system_dir,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\": ",
                                    e,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
#   endif
        }

#  else

        // win9x and posix

#   ifdef UWVM_CPP_EXCEPTIONS
        try
#   endif
        {
            // allow symlink
            entry = ::fast_io::dir_file{system_dir, ::fast_io::open_mode::follow};
        }
#   ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error e)
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Unable to open system dir \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                system_dir,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\": ",
                                e,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n"
#    ifndef _WIN32
                                u8"\n"
#    endif
            );
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
#   endif
#  endif

        param_cursor->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        ++param_cursor;

        // check empty
        if(wasidir.empty()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<wasi dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8": cannot be empty\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto& env{::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env};

        // During the command line phase, it is preferable to use `wasi_disable_utf8_check` rather than `env.disable_utf8_check`
        if(!(::uwvm2::uwvm::imported::wasi::storage::wasi_disable_utf8_check || env.disable_utf8_check)) [[likely]]
        {
            // check utf8
            auto const u8res{::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(wasidir.cbegin(),
                                                                                                                                           wasidir.cend())};
            if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Invalid ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    u8"<wasi dir>",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8": invalid UTF-8 (",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    ::uwvm2::utils::utf::get_utf_error_description<char8_t>(u8res.err),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8")\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
        }
        else
        {
            // check utf8
            auto const u8res{::uwvm2::utils::utf::check_has_zero_illegal_unchecked(wasidir.cbegin(), wasidir.cend())};
            if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Invalid ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    u8"<wasi dir>",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8": invalid UTF-8 (",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    ::uwvm2::utils::utf::get_utf_error_description<char8_t>(u8res.err),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8")\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
        }

        auto const disable_mount_path_normalization{
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_disable_mount_path_normalization};
        auto const allow_overlapping_mount_paths{
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_allow_overlapping_mount_paths};

        // The two command-line switches are intentionally independent. The
        // storage policy controls which guest path spelling reaches the WASIp1
        // preopen table, while the overlap policy controls whether uwvm rejects
        // ambiguous preopen layouts before the guest starts:
        // - default: normalize for storage and for duplicate/overlap checking;
        // - disable normalization only: store the raw guest path, but still
        //   normalize a temporary value for the safety check;
        // - allow overlapping only: normalize stored paths, but skip duplicate
        //   and ancestor/child rejection;
        // - both switches: skip the normalization pipeline completely.
        bool const need_normalized_wasidir{!disable_mount_path_normalization || !allow_overlapping_mount_paths};

        ::uwvm2::utils::container::u8string normalized_wasidir_storage{};
        ::uwvm2::utils::container::u8string_view normalized_wasidir{};
        if(need_normalized_wasidir)
        {
            auto normalized_result{wasip1_global_mount_dir_details::normalize_mount_path(wasidir)};
            if(!normalized_result.valid) [[unlikely]] { return wasip1_global_mount_dir_details::print_mount_path_normalization_error(wasidir); }

            normalized_wasidir_storage = ::std::move(normalized_result.normalized);
            normalized_wasidir = ::uwvm2::utils::container::u8string_view{normalized_wasidir_storage.data(), normalized_wasidir_storage.size()};
        }

        ::uwvm2::utils::container::u8string_view wasidir_to_store{wasidir.data(), wasidir.size()};
        if(!disable_mount_path_normalization) { wasidir_to_store = normalized_wasidir; }

        // No pattern processing allowed for security reasons. Any extra free-form args are ignored here.

        // Reject duplicate and ancestor/child mount names by default. WASIp1
        // preopens are independent directory capabilities rather than a POSIX
        // mount namespace, so an overlapping layout such as `/` and `/lib` can
        // make logically equivalent POSIX paths resolve through different
        // preopens. The check is performed on normalized guest names even when
        // raw-name storage is requested, because the protection is about path
        // equivalence, not the spelling stored in the preopen table.
        if(!allow_overlapping_mount_paths)
        {
            for(auto const& mr: env.mount_dir_roots)
            {
                auto const& existing{mr.preload_dir};
                ::uwvm2::utils::container::u8string_view const existing_sv{existing.data(), existing.size()};
                // Existing entries may have been stored raw when normalization
                // was disabled for storage, so compare a temporary normalized
                // value instead of trusting the stored spelling.
                auto existing_normalized_result{wasip1_global_mount_dir_details::normalize_mount_path(existing_sv)};
                if(!existing_normalized_result.valid) [[unlikely]]
                {
                    return wasip1_global_mount_dir_details::print_mount_path_normalization_error(existing_sv);
                }

                auto& existing_norm_storage{existing_normalized_result.normalized};
                ::uwvm2::utils::container::u8string_view const existing_norm{existing_norm_storage.data(), existing_norm_storage.size()};

                if(normalized_wasidir == existing_norm) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Duplicate mount ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        u8"<wasi dir>",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8": ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        normalized_wasidir,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" already mounted. Use ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        u8"--wasip1-allow-overlapping-mount-paths",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" to disable the overlapping mount-path check.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                }

                if(wasip1_global_mount_dir_details::starts_with_mount_path(normalized_wasidir, existing_norm) ||
                   wasip1_global_mount_dir_details::starts_with_mount_path(existing_norm, normalized_wasidir)) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Mount conflict: disallow overlapping prefixes between ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        normalized_wasidir,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" and ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        existing_norm,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8". Use ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        u8"--wasip1-allow-overlapping-mount-paths",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" to disable the overlapping mount-path check.\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                }
            }
        }

        // Record into default_wasi_env (own the string, then move)
        env.mount_dir_roots.emplace_back(::uwvm2::utils::container::u8string{wasidir_to_store}, ::std::move(entry));

        // posix: safe (native fd)
        // windows nt: safe (native handle)
        // windows 9x: unsafe (VxD does not provide directory handles, and multitasking exacerbates the TOCTOU problem.)
        // djgpp: safe (Due to single-task mode + full DJGPP control)

#  if defined(_WIN32) && defined(_WIN32_WINDOWS)
        if(::uwvm2::uwvm::io::show_toctou_warning)
        {
            // show warning
            static ::std::atomic<bool> warned{};  // [global]

            bool const already_warned{warned.exchange(true, ::std::memory_order_relaxed)};
            if(!already_warned) [[unlikely]]
            {
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    // 1
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    u8"[warn]  ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"Due to system limitations in Windows 9x, using the `dir` function may lead to a TOCTOU security vulnerability, potentially causing a sandbox escape. ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                    u8"(toctou)\n",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::toctou_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(toctou)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }
        }
#  endif

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Mounted ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<wasi dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                wasidir_to_store,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"<system dir>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                system_dir,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\".\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }

# endif
#endif

}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
