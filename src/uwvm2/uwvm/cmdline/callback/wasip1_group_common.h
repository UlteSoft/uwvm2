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
# include "wasip1_module_common.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    namespace wasip1_group_details
    {
        using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
        using parameter_type = ::uwvm2::utils::cmdline::parameter_parsing_results_type;
        using override_state_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_override_t;

        [[nodiscard]] inline parameter_return_type validate_group_arg(::uwvm2::utils::cmdline::parameter const& parameter,
                                                                      ::uwvm2::utils::cmdline::parameter_parsing_results* group_arg,
                                                                      ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                                                      ::uwvm2::utils::container::u8string_view& group_name) noexcept
        {
            if(group_arg == para_end || group_arg->type != parameter_type::arg) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(parameter, u8"Missing group name.");
            }

            group_name = ::uwvm2::utils::container::u8string_view{group_arg->str};
            if(group_name.empty()) [[unlikely]] { return wasip1_module_details::print_usage_error(parameter, u8"Group name cannot be empty."); }
            return parameter_return_type::def;
        }

        [[nodiscard]] inline parameter_return_type validate_module_arg(::uwvm2::utils::cmdline::parameter const& parameter,
                                                                       ::uwvm2::utils::cmdline::parameter_parsing_results* module_arg,
                                                                       ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                                                       ::uwvm2::utils::container::u8string_view& module_name) noexcept
        {
            if(module_arg == para_end || module_arg->type != parameter_type::arg) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(parameter, u8"Missing module name.");
            }

            module_name = ::uwvm2::utils::container::u8string_view{module_arg->str};
            if(module_name.empty()) [[unlikely]] { return wasip1_module_details::print_usage_error(parameter, u8"Module name cannot be empty."); }
            if(!wasip1_module_details::validate_wasm_utf8_name(module_name)) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(parameter, u8"Invalid module name. WebAssembly names must be valid UTF-8.");
            }
            return parameter_return_type::def;
        }

        [[nodiscard]] inline parameter_return_type apply_action(::uwvm2::utils::cmdline::parameter const& parameter,
                                                                ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                                ::uwvm2::utils::cmdline::parameter_parsing_results* para_end,
                                                                ::uwvm2::utils::container::u8string_view action) noexcept
        {
            auto group_arg{para_curr + 1u};
            ::uwvm2::utils::container::u8string_view group_name{};
            if(auto const ret{validate_group_arg(parameter, group_arg, para_end, group_name)}; ret != parameter_return_type::def) [[unlikely]]
            {
                return ret;
            }

            auto* target{::uwvm2::uwvm::imported::wasi::wasip1::storage::find_named_wasip1_group(group_name)};
            if(target == nullptr) [[unlikely]]
            {
                return wasip1_module_details::print_usage_error(parameter, u8"WASI Preview 1 module group does not exist.");
            }

            return wasip1_module_details::apply_target_action(parameter, group_arg, para_end, *target, action);
        }
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
