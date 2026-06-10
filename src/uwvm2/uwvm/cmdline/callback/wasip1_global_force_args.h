/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-05
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# endif
// import
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include "wasip1_module_common.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

#  if defined(UWVM_MODULE)
    extern "C++"
#  else
    inline constexpr
#  endif
        void wasip1_global_force_args_pretreatment(char8_t const* const*& argv_curr,
                                                   char8_t const* const* argv_end,
                                                   ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>& pr) noexcept
    { wasip1_module_details::force_args_pretreatment(argv_curr, argv_end, pr, 0uz); }

#  if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_global_force_args_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                             para_begin,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;

        auto count_arg{para_curr + 1u};
        if(count_arg == para_end || !wasip1_module_details::is_argument_result(count_arg->type)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_global_force_args, u8"Missing forced argv count.");
        }

        ::std::size_t arg_count{};
        if(!wasip1_module_details::parse_size_t(::uwvm2::utils::container::u8string_view{count_arg->str}, arg_count)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_global_force_args, u8"Invalid forced argv count.");
        }

        if(::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_argv0_is_set) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_global_force_args,
                                                            u8"Cannot combine global force-args and set-argv0 for the same WASI Preview 1 target.");
        }

        ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> forced_args{};
        forced_args.reserve(arg_count);

        auto arg_curr{count_arg + 1u};
        for(::std::size_t i{}; i != arg_count; ++i)
        {
            if(arg_curr == para_end || !wasip1_module_details::is_argument_result(arg_curr->type)) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_global_force_args, u8"Missing forced argv entry.");
            }
            forced_args.emplace_back(::uwvm2::utils::container::u8string{arg_curr->str});
            ++arg_curr;
        }

        for(auto curr{count_arg}; curr != arg_curr; ++curr) { curr->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg; }
        ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_force_argument_storage = ::std::move(forced_args);
        ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_force_args_is_set = true;
        return parameter_return_type::def;
    }

# endif
#endif
}

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
