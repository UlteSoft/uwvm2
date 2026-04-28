/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-27
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
# include "wasip1_group_common.h"
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
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_group_create_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
                                                            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                            ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        auto group_arg{para_curr + 1u};
        ::uwvm2::utils::container::u8string_view group_name{};
        if(auto const ret{wasip1_group_details::validate_group_arg(::uwvm2::uwvm::cmdline::params::wasip1_group_create, group_arg, para_end, group_name)};
           ret != wasip1_group_details::parameter_return_type::def) [[unlikely]]
        {
            return ret;
        }

        auto const group_index{::uwvm2::uwvm::imported::wasi::wasip1::storage::try_create_named_wasip1_group_index(group_name)};
        if(group_index == ::uwvm2::uwvm::imported::wasi::wasip1::storage::invalid_wasip1_group_index) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_group_create, u8"Duplicate WASI Preview 1 module group.");
        }
        group_arg->type = wasip1_group_details::parameter_type::occupied_arg;
        return wasip1_group_details::parameter_return_type::def;
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
