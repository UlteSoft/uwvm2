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
// global
# include "version.h"
# include "run.h"
# include "help.h"
# include "mode.h"

// debug
# include "debug_test.h"

// wasm
# include "wasm_set_main_module_name.h"
# include "wasm_preload_library.h"
# include "wasm_register_dl.h"
# include "wasm_set_preload_module_attribute.h"
# include "wasm_depend_recursion_limit.h"
# include "wasm_set_memory_limit.h"
# include "wasm_set_parser_limit.h"
# include "wasm_set_initializer_limit.h"
# include "wasm_list_weak_symbol_module.h"
# include "wasm_memory_grow_strict.h"

// runtime
# include "runtime_custom_mode.h"
# include "runtime_custom_compiler.h"
# include "runtime_compiler_log.h"
# include "runtime_compile_threads.h"
# include "runtime_scheduling_policy.h"
# include "runtime_debug_int.h"
# include "runtime_int.h"
# include "runtime_jit.h"
# include "runtime_aot.h"
# include "runtime_tiered.h"

// wasi
# include "wasi_disable_utf8_check.h"
# include "wasip1_global_trace.h"
# include "wasip1_global_expose_host_api.h"
# include "wasip1_global_disable.h"
# include "wasip1_global_set_fd_limit.h"
# include "wasip1_global_mount_dir.h"
# include "wasip1_global_set_argv0.h"
# include "wasip1_global_noinherit_system_environment.h"
# include "wasip1_global_delete_system_environment.h"
# include "wasip1_global_add_environment.h"
# include "wasip1_global_socket_tcp_listen.h"
# include "wasip1_global_socket_tcp_connect.h"
# include "wasip1_global_socket_udp_bind.h"
# include "wasip1_global_socket_udp_connect.h"
# include "wasip1_single_create.h"
# include "wasip1_single_enable.h"
# include "wasip1_single_disable.h"
# include "wasip1_single_expose_host_api.h"
# include "wasip1_single_hide_host_api.h"
# include "wasip1_single_noinherit_system_environment.h"
# include "wasip1_single_inherit_system_environment.h"
# include "wasip1_single_disable_utf8_check.h"
# include "wasip1_single_enable_utf8_check.h"
# include "wasip1_single_trace.h"
# include "wasip1_single_set_argv0.h"
# include "wasip1_single_set_fd_limit.h"
# include "wasip1_single_add_environment.h"
# include "wasip1_single_delete_system_environment.h"
# include "wasip1_single_mount_dir.h"
# include "wasip1_single_socket_tcp_listen.h"
# include "wasip1_single_socket_tcp_connect.h"
# include "wasip1_single_socket_udp_bind.h"
# include "wasip1_single_socket_udp_connect.h"
# include "wasip1_group_create.h"
# include "wasip1_group_add_module.h"
# include "wasip1_group_enable.h"
# include "wasip1_group_disable.h"
# include "wasip1_group_expose_host_api.h"
# include "wasip1_group_hide_host_api.h"
# include "wasip1_group_noinherit_system_environment.h"
# include "wasip1_group_inherit_system_environment.h"
# include "wasip1_group_disable_utf8_check.h"
# include "wasip1_group_enable_utf8_check.h"
# include "wasip1_group_trace.h"
# include "wasip1_group_set_argv0.h"
# include "wasip1_group_set_fd_limit.h"
# include "wasip1_group_add_environment.h"
# include "wasip1_group_delete_system_environment.h"
# include "wasip1_group_mount_dir.h"
# include "wasip1_group_socket_tcp_listen.h"
# include "wasip1_group_socket_tcp_connect.h"
# include "wasip1_group_socket_udp_bind.h"
# include "wasip1_group_socket_udp_connect.h"

// log
# include "log_output.h"
# include "log_enable_warning.h"
# include "log_disable_warning.h"
# include "log_convert_warn_to_fatal.h"
# include "log_verbose.h"
# include "log_win32_use_ansi.h"

#endif
