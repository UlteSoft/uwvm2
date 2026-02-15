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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/feature/feature_push_macro.h>  // wasi
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline
{
    namespace details
    {
        inline constexpr ::uwvm2::utils::cmdline::parameter const* parameter_unsort[]{
            // global
            ::std::addressof(::uwvm2::uwvm::cmdline::params::version),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::run),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::help),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::mode),

        // debug
#ifdef _DEBUG
            ::std::addressof(::uwvm2::uwvm::cmdline::params::debug_test),
#endif

            // wasm
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_set_main_module_name),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_preload_library),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_depend_recursion_limit),
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_register_dl),
#endif
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_set_parser_limit),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_set_initializer_limit),
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_list_weak_symbol_module),
#endif
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasm_memory_grow_strict),

            // runtime
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_custom_mode),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_custom_compiler),
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_debug),
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_int),
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_jit),
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_tiered),
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_aot),
#endif
            ::std::addressof(::uwvm2::uwvm::cmdline::params::runtime_compiler_log),

        // wasi
#if defined(UWVM_IMPORT_WASI)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasi_disable_utf8_check),
#endif
        // wasip1
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_set_fd_limit),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_mount_dir),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_noinherit_system_environment),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_delete_system_environment),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_add_environment),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_disable),
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_socket_tcp_listen),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_socket_tcp_connect),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_socket_udp_bind),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::wasip1_socket_udp_connect),
#  endif
# endif
#endif

            // log
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_output),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_enable_warning),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_disable_warning),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_convert_warn_to_fatal),
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_verbose),
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            ::std::addressof(::uwvm2::uwvm::cmdline::params::log_win32_use_ansi),
#endif
        };
    }  // namespace details

    inline constexpr auto parameters{::uwvm2::utils::cmdline::parameter_sort(details::parameter_unsort)};

    inline constexpr ::std::size_t parameter_lookup_table_size{::uwvm2::utils::cmdline::calculate_alias_parameters_size(parameters)};
    inline constexpr auto parameter_lookup_table{::uwvm2::utils::cmdline::expand_alias_parameters_and_check<parameter_lookup_table_size>(parameters)};

    inline constexpr ::std::size_t parameter_max_principal_name_size{::uwvm2::utils::cmdline::calculate_max_principal_name_size(parameters)};
    inline constexpr ::std::size_t parameter_max_name_size{::uwvm2::utils::cmdline::calculate_max_para_size(parameter_lookup_table)};

    inline constexpr auto hash_table_size{::uwvm2::utils::cmdline::calculate_hash_table_size(parameter_lookup_table)};
    inline constexpr auto hash_table{
        ::uwvm2::utils::cmdline::generate_hash_table<hash_table_size.hash_table_size, hash_table_size.extra_size, hash_table_size.real_max_conflict_size>(
            parameter_lookup_table)};

    inline constexpr ::std::size_t hash_table_byte_sz{sizeof(hash_table)};
    // inline constexpr auto sizeof_hash_table{sizeof(hash_table)};
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/imported/wasi/feature/feature_pop_macro.h>  // wasi
# include <uwvm2/utils/macro/pop_macros.h>
#endif
