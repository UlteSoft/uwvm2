/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/impl.h>
# include "retval.h"
# include "weak_symbol.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
    namespace details
    {
        [[nodiscard]] inline bool validate_wasip1_group_binding_for_loaded_module(
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t target_kind,
            ::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            using namespace ::uwvm2::uwvm::imported::wasi::wasip1::storage;

            auto const anonymous_group_index{find_wasip1_anonymous_module_group_index(target_kind, module_name)};
            auto const named_group_index{find_named_wasip1_module_group_index(module_name)};
            if(anonymous_group_index == invalid_wasip1_group_index || named_group_index == invalid_wasip1_group_index ||
               anonymous_group_index == named_group_index)
                [[likely]]
            {
                return true;
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Module \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" is bound to more than one WASI Preview 1 group.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return false;
        }

        [[nodiscard]] inline bool validate_loaded_wasip1_group_bindings() noexcept
        {
            using target_kind = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t;

            if(!::uwvm2::uwvm::wasm::storage::execute_wasm.module_name.empty())
            {
                if(!validate_wasip1_group_binding_for_loaded_module(target_kind::main_wasm, ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name))
                    [[unlikely]]
                {
                    return false;
                }
            }

            for(auto const& preload_module: ::uwvm2::uwvm::wasm::storage::preloaded_wasm)
            {
                if(!validate_wasip1_group_binding_for_loaded_module(target_kind::preload_wasm, preload_module.module_name)) [[unlikely]] { return false; }
            }

#  if defined(UWVM_SUPPORT_PRELOAD_DL)
            for(auto const& preload_dl: ::uwvm2::uwvm::wasm::storage::preloaded_dl)
            {
                if(!validate_wasip1_group_binding_for_loaded_module(target_kind::preloaded_dl, preload_dl.module_name)) [[unlikely]] { return false; }
            }
#  endif

#  if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            for(auto const& weak_module: ::uwvm2::uwvm::wasm::storage::weak_symbol)
            {
                if(!validate_wasip1_group_binding_for_loaded_module(target_kind::weak_symbol, weak_module.module_name)) [[unlikely]] { return false; }
            }
#  endif

            return true;
        }
    }  // namespace details
# endif
#endif

    inline int load_exec_wasm_module() noexcept
    {
        // The wasm preload has been fully parsed
        // The dl preload has been fully registered

        if(!::uwvm2::uwvm::cmdline::wasm_file_ppos) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                // 1
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"No execution of WASM files.\n"
                                // 2
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Try \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"uwvm ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"--help",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" for more information.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return static_cast<int>(::uwvm2::uwvm::run::retval::load_main_module_error);
        }

        // get main module path
        auto const module_file_name{::uwvm2::uwvm::cmdline::wasm_file_ppos->str};

        // load main module
        auto const load_wasm_file_rtl{::uwvm2::uwvm::wasm::loader::load_wasm_file(::uwvm2::uwvm::wasm::storage::execute_wasm,
                                                                                  module_file_name,
                                                                                  ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name,
                                                                                  ::uwvm2::uwvm::wasm::storage::wasm_parameter)};

        if(load_wasm_file_rtl != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::load_main_module_error);
        }

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
    }

    inline int load_local_modules() noexcept
    {
        // preload abi

#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
        auto const need_wasip1_environment{
            ::uwvm2::uwvm::wasm::storage::local_preload_wasip1 || ::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api ||
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::has_any_configured_wasip1_module_override()};

        auto const wasip1_import_visible_for_loaded_wasm{
            [](::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t target_kind,
               ::uwvm2::utils::container::u8string_view module_name) noexcept -> bool
            {
                if(auto const state{::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override_const(target_kind, module_name)};
                   state != nullptr && state->enabled_is_set) [[unlikely]]
                {
                    return state->enabled;
                }
                return ::uwvm2::uwvm::wasm::storage::local_preload_wasip1;
            }};

        bool need_wasip1_local_imported_module{::uwvm2::uwvm::wasm::storage::local_preload_wasip1};
        if(!need_wasip1_local_imported_module)
        {
            using target_kind = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t;

            if(!::uwvm2::uwvm::wasm::storage::execute_wasm.module_name.empty())
            {
                need_wasip1_local_imported_module =
                    wasip1_import_visible_for_loaded_wasm(target_kind::main_wasm, ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name);
            }

            if(!need_wasip1_local_imported_module)
            {
                for(auto const& preload_module: ::uwvm2::uwvm::wasm::storage::preloaded_wasm)
                {
                    if(wasip1_import_visible_for_loaded_wasm(target_kind::preload_wasm, preload_module.module_name)) [[unlikely]]
                    {
                        need_wasip1_local_imported_module = true;
                        break;
                    }
                }
            }
        }

        if(need_wasip1_environment)
        {
            // verbose
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Initialize WASI-Preview1 environment. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            if(!details::validate_loaded_wasip1_group_bindings()) [[unlikely]]
            {
                return static_cast<int>(::uwvm2::uwvm::run::retval::load_local_modules_error);
            }

            if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::init_wasip1_environment(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env))
                [[unlikely]]
            {
                return static_cast<int>(::uwvm2::uwvm::run::retval::load_local_modules_error);
            }

            for(auto& group_state: ::uwvm2::uwvm::imported::wasi::wasip1::storage::configured_wasip1_groups)
            {
                if(!group_state.has_override()) [[unlikely]] { continue; }
                if(!::uwvm2::uwvm::imported::wasi::wasip1::storage::init_wasip1_environment(group_state)) [[unlikely]]
                {
                    return static_cast<int>(::uwvm2::uwvm::run::retval::load_local_modules_error);
                }
            }
        }

        if(need_wasip1_local_imported_module)
        {
            // verbose
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Loading local module \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"WASI-Preview1",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            ::uwvm2::uwvm::wasm::storage::preload_local_imported.emplace_back(
                ::uwvm2::uwvm::imported::wasi::wasip1::local_imported::wasip1_local_imported_module);
        }
# endif
#endif

#if 0  /// @todo
        if(::uwvm2::uwvm::wasm::storage::local_preload_wasip2)
        {
            // verbose
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Loading local module \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"WASI-Preview2",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". ",                                
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }
            /// @todo
        }

        if(::uwvm2::uwvm::wasm::storage::local_preload_wasix)
        {
            // verbose
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Loading local module \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"WASI-X",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". ",                                
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }
            /// @todo
        }
#endif

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
    }

    inline int load_weak_symbol_modules() noexcept
    { return ::uwvm2::uwvm::run::load_weak_symbol_modules_details(::uwvm2::uwvm::wasm::storage::wasm_parameter); }

}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
