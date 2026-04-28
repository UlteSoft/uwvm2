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
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
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
    extern "C++" UWVM_GNU_COLD
#  else
    UWVM_GNU_COLD inline constexpr
#  endif
        ::uwvm2::utils::cmdline::parameter_return_type wasip1_module_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
                                                                              ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                              ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;
        auto module_arg{para_curr + 1u};
        if(module_arg == para_end || module_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Missing module name.");
        }

        auto const module_name{::uwvm2::utils::container::u8string_view{module_arg->str}};
        if(module_name.empty()) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Module name cannot be empty.");
        }
        if(!wasip1_module_details::validate_wasm_utf8_name(module_name)) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module,
                                                            u8"Invalid module name. WebAssembly names must be valid UTF-8.");
        }

        auto action_arg{module_arg + 1u};
        if(action_arg == para_end || action_arg->type != parameter_type::arg) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module, u8"Missing module action.");
        }

        auto target{::uwvm2::uwvm::imported::wasi::wasip1::storage::try_create_targetless_wasip1_module_override(module_name)};
        if(target == nullptr) [[unlikely]]
        {
            return wasip1_module_details::print_usage_error(::uwvm2::uwvm::cmdline::params::wasip1_module,
                                                            u8"Duplicate or conflicting WASI Preview 1 module configuration.");
        }

        return wasip1_module_details::apply_module_action_sequence(::uwvm2::uwvm::cmdline::params::wasip1_module, module_arg, action_arg, para_end, *target);
    }

# endif
#endif
}

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
