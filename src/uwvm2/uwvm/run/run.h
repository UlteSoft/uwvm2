/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
#  include <uwvm2/runtime/lib/uwvm_runtime.h>
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
# include <uwvm2/uwvm/runtime/impl.h>
# include "retval.h"
# include "loader.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    inline ::std::size_t resolve_first_entry_function_index(::uwvm2::utils::container::u8string_view main_module_name) noexcept
    {
        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
        using start_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t;
        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

        // Prefer start section when present.
        if(auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(main_module_name)}; it != ::uwvm2::uwvm::wasm::storage::all_module.end())
        {
            auto const& am{it->second};
            if(am.type == module_type_t::exec_wasm || am.type == module_type_t::preloaded_wasm)
            {
                auto const* wf{am.module_storage_ptr.wf};
                if(wf != nullptr && wf->binfmt_ver == 1u)
                {
                    auto const& mod{wf->wasm_module_storage.wasm_binfmt_ver1_storage};
                    auto const& startsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_t>(mod.sections)};

                    auto const span{startsec.sec_span};
                    auto const size{static_cast<::std::size_t>(span.sec_end - span.sec_begin)};
                    if(size != 0uz) { return static_cast<::std::size_t>(startsec.start_idx); }
                }
            }
        }

        // Otherwise, fall back to exported entrypoints.
        ::std::size_t idx{};
        auto const mit{::uwvm2::uwvm::wasm::storage::all_module_export.find(main_module_name)};
        if(mit != ::uwvm2::uwvm::wasm::storage::all_module_export.end())
        {
            auto const try_export{[&](::uwvm2::utils::container::u8string_view export_name) noexcept -> bool
                                  {
                                      auto const eit{mit->second.find(export_name)};
                                      if(eit == mit->second.end()) { return false; }

                                      auto const& ex{eit->second};
                                      if(ex.type != module_type_t::exec_wasm && ex.type != module_type_t::preloaded_wasm) { return false; }

                                      auto const* exp{ex.storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                                      if(exp == nullptr || exp->type != external_types::func) { return false; }

                                      idx = static_cast<::std::size_t>(exp->storage.func_idx);
                                      return true;
                                  }};

            if(try_export(::uwvm2::utils::container::u8string_view{u8"_start"})) { return idx; }
            if(try_export(::uwvm2::utils::container::u8string_view{u8"main"})) { return idx; }
        }

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
        ::fast_io::fast_terminate();
    }
#endif

    inline int run() noexcept
    {
        // already preload wasm module

        // already bind dl

        // load main module
        if(auto const ret{::uwvm2::uwvm::run::load_exec_wasm_module()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // load local modules
        if(auto const ret{::uwvm2::uwvm::run::load_local_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // load weak symbol modules
        if(auto const ret{::uwvm2::uwvm::run::load_weak_symbol_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // check duplicate module and construct ::uwvm2::uwvm::wasm::storage::all_module
        if(auto const ret{::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        // section details occurs before dependency checks
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details:
            {
                // All modules loaded
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Start printing section details. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }

                ::uwvm2::uwvm::wasm::section_detail::print_section_details();

                // Return directly
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                // Validate all wasm code

                // Runtime initialization is not performed; only validity checks are conducted using the parser's built-in validation, not the runtime
                // validation with compilation and partitioning capabilities.

                // validate_all_wasm_code has verbose message, no necessary to print again.
                if(!::uwvm2::uwvm::runtime::validator::validate_all_wasm_code()) [[unlikely]]
                {
                    return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
                }

                // Return directly
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            default:
            {
                break;
            }
        }

        // run vm

        // check import exist and detect cycles
        if(auto const ret{::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        // initialize runtime
        ::uwvm2::uwvm::runtime::initializer::initialize_runtime();

#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter &&
           ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode != ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile) [[unlikely]]
        {
            if(::uwvm2::uwvm::io::show_runtime_warning)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Debug interpreter requires full compile; forcing full compile.",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (runtime)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(runtime)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile;
        }
#endif

        // run vm
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details: [[fallthrough]];
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                /// this is an vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
            case ::uwvm2::uwvm::wasm::base::mode::run:
            {
                switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode)
                {
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile:
                    {
                        /// @todo run interpreter

                        // not supported yet
                        ::fast_io::fast_terminate();

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile_with_full_code_verification:
                    {
                        /// @todo run interpreter

                        // not supported yet
                        ::fast_io::fast_terminate();

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile:
                    {
                        switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
                        {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only:
                            {
                                // full compile + uwvm_int interpreter backend
                                ::uwvm2::runtime::uwvm_int::full_compile_run_config cfg{};
                                cfg.entry_function_index = resolve_first_entry_function_index(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name);
                                ::uwvm2::runtime::uwvm_int::full_compile_and_run_main_module(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name, cfg);

                                break;
                            }
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter:
                            {
                                // not supported yet
                                ::fast_io::fast_terminate();

                                break;
                            }
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                            {
                                // not supported yet
                                ::fast_io::fast_terminate();

                                break;
                            }
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                            {
                                // not supported yet
                                ::fast_io::fast_terminate();

                                break;
                            }
#endif
                            [[unlikely]] default:
                            {
/// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::std::unreachable();
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }
                break;
            }
            /// @todo add more modes here
            [[unlikely]] default:
            {
                /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
        }

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
    }
}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
