/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_mount_dir_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
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
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_mount_dir),
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
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_mount_dir),
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

        // Validate wasidir (absolute and relative modes):
        // Absolute: POSIX-like absolute path; forbid '//' and any path segment equal to '.' or '..'.
        //           '.' is allowed only when used alone as the entire path. Characters other than '/' are not restricted.
        //           Additionally, require valid UTF-8 and forbid embedded NUL.
        // Relative: allow '.' alone, or pure relative paths without any '.' or '..' segments (e.g. "b/xxx").
        //           './name' is NOT allowed; '//' is forbidden.

        // Detect relative mode: any path not starting with '/' is treated as relative (including '.')
        bool const wasidir_is_relative{wasidir.front_unchecked() != u8'/'};

        if(wasidir_is_relative)
        {
            // check not "."
            if(wasidir != u8".")
            {
                bool prev_slash{};  // we don't start with '/'
                ::std::size_t seg_len{};
                bool seg_only_dots{true};

                for(::std::size_t i{}; i != wasidir.size(); ++i)
                {
                    auto const ch{wasidir.index_unchecked(i)};
                    if(ch == u8'/')
                    {
                        if(prev_slash) [[unlikely]]
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
                                                u8": must not contain '//', in ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                wasidir,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8" at offset ",
                                                i,
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;  // forbid '//'
                        }

                        if(seg_len != 0uz)
                        {
                            // forbid '.' or '..' segment
                            if(seg_only_dots && (seg_len == 1uz || seg_len == 2uz)) [[unlikely]]
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
                                                    u8": path segments './', '/.' or '..' are not allowed, in ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                    wasidir,
                                                    u8"\n\n",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                            }
                        }

                        seg_len = 0uz;
                        seg_only_dots = true;
                        prev_slash = true;
                    }
                    else
                    {
                        // POSIX/WASI: do not restrict characters beyond '/' and dot-segment rules

                        prev_slash = false;
                        ++seg_len;
                        if(seg_only_dots && ch != u8'.') { seg_only_dots = false; }
                    }
                }

                if(seg_len != 0uz)
                {
                    if(seg_only_dots && (seg_len == 1uz || seg_len == 2uz)) [[unlikely]]
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
                                            u8": path segments './', '/.' or '..' are not allowed, in ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            wasidir,
                                            u8"\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                    }
                }
            }
        }
        else
        {
            bool prev_slash{};
            ::std::size_t seg_len{};
            bool seg_only_dots{true};

            for(auto const ch: wasidir)
            {
                if(ch == u8'/')
                {
                    if(prev_slash) [[unlikely]]
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
                                            u8": must not contain '//', in ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            wasidir,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                    }

                    if(seg_len != 0uz)
                    {
                        if(seg_only_dots && (seg_len == 1uz || seg_len == 2uz)) [[unlikely]]
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
                                                u8": path segments '.' or '..' are not allowed in absolute paths, in ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                wasidir,
                                                u8"\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                        }
                    }

                    seg_len = 0uz;
                    seg_only_dots = true;
                    prev_slash = true;
                }
                else
                {
                    // POSIX/WASI: do not restrict characters beyond '/' and dot-segment rules

                    prev_slash = false;
                    ++seg_len;
                    if(seg_only_dots && ch != u8'.') { seg_only_dots = false; }
                }
            }

            if(seg_len != 0uz)
            {
                if(seg_only_dots && (seg_len == 1uz || seg_len == 2uz)) [[unlikely]]
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
                                        u8": path segments '.' or '..' are not allowed in absolute paths, in ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        wasidir,
                                        u8"\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                }
            }
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

        // Canonicalize wasidir: remove trailing '/' (except root "/") for stable comparisons
        constexpr auto canonicalize_wasidir{[](::uwvm2::utils::container::u8string_view p) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                                            {
                                                if(p == u8".") { return p; }
                                                auto n{p.size()};
                                                while(n > 1uz && p.index_unchecked(n - 1uz) == u8'/') { --n; }
                                                return ::uwvm2::utils::container::u8string_view{p.data(), n};
                                            }};

        // Canonicalize relative wasidir: forbid './' prefix; keep '.' as is; trim trailing '/'.
        constexpr auto canonicalize_wasidir_rel{[](::uwvm2::utils::container::u8string_view p) constexpr noexcept -> ::uwvm2::utils::container::u8string_view
                                                {
                                                    if(p == u8".") { return p; }
                                                    // p is a pure relative path like 'b/xxx'; remove trailing '/'
                                                    auto n{p.size()};
                                                    while(n != 0uz && p.index_unchecked(n - 1uz) == u8'/') { --n; }
                                                    if(n == 0uz) { return ::uwvm2::utils::container::u8string_view{u8".", 1uz}; }
                                                    return ::uwvm2::utils::container::u8string_view{p.data(), n};
                                                }};

        auto const wasidir_norm{wasidir_is_relative ? canonicalize_wasidir_rel(wasidir) : canonicalize_wasidir(wasidir)};

        // No pattern processing allowed for security reasons. Any extra free-form args are ignored here.

        // Conflict check with existing mounts: disallow prefix conflicts both directions
        for(auto const& mr: env.mount_dir_roots)
        {
            auto const& existing{mr.preload_dir};
            // Convert to view for normalization and comparisons
            ::uwvm2::utils::container::u8string_view const existing_sv{existing.data(), existing.size()};
            // Supports normalization of both absolute paths and relative paths (including “.”).
            auto const existing_norm{(!existing_sv.empty() && existing_sv.front_unchecked() == u8'/') ? canonicalize_wasidir(existing_sv)
                                                                                                      : canonicalize_wasidir_rel(existing_sv)};
            // Note: stored mount points are validated; they may be absolute or relative.
            constexpr auto starts_with{[](::uwvm2::utils::container::u8string_view a, ::uwvm2::utils::container::u8string_view b) constexpr noexcept -> bool
                                       {
                                           // Special case: '/' is the prefix of any absolute path
                                           if(b == u8"/") { return (!a.empty() && a.front_unchecked() == u8'/'); }

                                           // Special case: for relative paths, '.' is the prefix of any relative path
                                           if(!a.empty() && a.front_unchecked() != u8'/' && b == u8".") { return true; }

                                           if(a.size() < b.size()) { return false; }
                                           if(a.size() == b.size()) { return a == b; }

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                           if(a.size() <= b.size()) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

                                           return (::uwvm2::utils::container::u8string_view{a.data(), b.size()} == b) && (a.index_unchecked(b.size()) == u8'/');
                                       }};

            if(wasidir_norm == existing_norm) [[unlikely]]
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
                                    wasidir_norm,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" already mounted.\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }

            if(starts_with(wasidir_norm, existing_norm) || starts_with(existing_norm, wasidir_norm)) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Mount conflict: disallow overlapping prefixes between ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    wasidir_norm,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" and ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    existing_norm,
                                    u8"\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
        }

        // Record into default_wasi_env (own the string, then move)
        env.mount_dir_roots.emplace_back(::uwvm2::utils::container::u8string{wasidir_norm}, ::std::move(entry));

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
                                wasidir_norm,
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

