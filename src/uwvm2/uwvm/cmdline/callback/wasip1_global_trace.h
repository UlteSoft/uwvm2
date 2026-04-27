/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-26
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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    namespace wasip1_global_trace_details
    {
        using trace_output_target_t = ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_output_target_t;
        using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
        using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;

        [[nodiscard]] inline parameter_return_type print_usage_error(::uwvm2::utils::container::u8string_view msg) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                msg,
                                u8" Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_global_trace),
                                u8"\n\n");
            return parameter_return_type::return_m1_imme;
        }

        [[nodiscard]] inline parameter_return_type print_open_error(::uwvm2::utils::container::u8string_view path) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Unable to open WASI trace output file \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                path,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\".\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return parameter_return_type::return_m1_imme;
        }

        [[nodiscard]] inline parameter_return_type apply_trace_target_to_default_env(
            ::uwvm2::utils::cmdline::parameter_parsing_results* target_arg,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
        {
            auto& env{::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env};
            auto const target_text{::uwvm2::utils::container::u8string_view{target_arg->str}};

            auto const disable_trace{[&]() noexcept
                                     {
                                         env.trace_wasip1_call = false;
                                         env.trace_wasip1_output_target = trace_output_target_t::none;
                                         env.trace_wasip1_output_file_path_storage.clear();
                                         env.trace_wasip1_group_kind =
                                             ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t::global;
                                         env.trace_wasip1_group_name_storage.clear();
                                     }};

            if(target_text == u8"none")
            {
                disable_trace();
                return parameter_return_type::def;
            }
            if(target_text == u8"out")
            {
                env.trace_wasip1_call = true;
                env.trace_wasip1_output_target = trace_output_target_t::out;
                env.trace_wasip1_output_file_path_storage.clear();
                return parameter_return_type::def;
            }
            if(target_text == u8"err")
            {
                env.trace_wasip1_call = true;
                env.trace_wasip1_output_target = trace_output_target_t::err;
                env.trace_wasip1_output_file_path_storage.clear();
                return parameter_return_type::def;
            }

            if(target_text != u8"file") [[unlikely]] { return print_usage_error(u8"Invalid trace output target."); }

            auto file_arg{target_arg + 1u};
            if(file_arg == para_end || file_arg->type != parameter_type::arg) [[unlikely]]
            {
                return print_usage_error(u8"Missing trace output file path.");
            }
            file_arg->type = parameter_type::occupied_arg;
            auto const file_path{::uwvm2::utils::container::u8string_view{file_arg->str}};

            if(file_path.empty()) [[unlikely]] { return print_usage_error(u8"Missing trace output file path."); }

            ::fast_io::u8native_file test_output{};
            if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::reopen_wasip1_trace_output_file(test_output, file_path)) [[unlikely]]
            {
                return print_open_error(file_path);
            }

            env.trace_wasip1_call = true;
            env.trace_wasip1_output_target = trace_output_target_t::file;
            env.trace_wasip1_output_file_path_storage = ::uwvm2::utils::container::u8string{file_path};
            env.trace_wasip1_group_kind = ::uwvm2::imported::wasi::wasip1::environment::trace_wasip1_group_kind_t::global;
            env.trace_wasip1_group_name_storage.clear();
            if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::reopen_wasip1_trace_output_file(env.trace_wasip1_output_file, file_path))
                [[unlikely]]
            {
                disable_trace();
                return print_open_error(file_path);
            }

            return parameter_return_type::def;
        }
    }  // namespace wasip1_global_trace_details

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_global_trace_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
                                                                             ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                                             ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        auto target_arg{para_curr + 1u};
        if(target_arg == para_end || target_arg->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            return wasip1_global_trace_details::print_usage_error(u8"Missing trace output target.");
        }

        target_arg->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        return wasip1_global_trace_details::apply_trace_target_to_default_env(target_arg, para_end);
    }

# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
