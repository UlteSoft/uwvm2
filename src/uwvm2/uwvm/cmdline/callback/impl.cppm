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

module;

export module uwvm2.uwvm.cmdline.callback;
// global
export import :version;
export import :help;
export import :mode;

// debug
export import :debug_test;

// wasm
export import :wasm_set_main_module_name;
export import :wasm_preload_library;
export import :wasm_register_dl;
export import :wasm_depend_recursion_limit;
export import :wasm_set_parser_limit;
export import :wasm_set_initializer_limit;
export import :wasm_list_weak_symbol_module;

// runtime
export import :runtime_custom_mode;
export import :runtime_custom_compiler;
export import :runtime_debug;
export import :runtime_int;
export import :runtime_jit;
export import :runtime_tiered;
export import :runtime_aot;
export import :runtime_compiler_log;

// wasi
export import :wasi_disable_utf8_check;
export import :wasip1_set_fd_limit;
export import :wasip1_mount_dir;
export import :wasip1_disable;
export import :wasip1_socket_tcp_listen;
export import :wasip1_socket_tcp_connect;
export import :wasip1_socket_udp_bind;
export import :wasip1_socket_udp_connect;

// log
export import :log_output;
export import :log_enable_warning;
export import :log_disable_warning;
export import :log_convert_warn_to_fatal;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"
