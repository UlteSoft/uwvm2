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
# include "wasm_depend_recursion_limit.h"
# include "wasm_set_parser_limit.h"
# include "wasm_list_weak_symbol_module.h"
# include "wasm_memory_grow_strict.h"

// runtime
# include "runtime_custom_mode.h"
# include "runtime_custom_compiler.h"
# include "runtime_debug.h"
# include "runtime_int.h"
# include "runtime_jit.h"
# include "runtime_tiered.h"
# include "runtime_aot.h"

// wasi
# include "wasi_disable_utf8_check.h"
# include "wasip1_set_fd_limit.h"
# include "wasip1_mount_dir.h"
# include "wasip1_disable.h"
# include "wasip1_socket_tcp_listen.h"
# include "wasip1_socket_tcp_connect.h"
# include "wasip1_socket_udp_bind.h"
# include "wasip1_socket_udp_connect.h"

// log
# include "log_output.h"
# include "log_disable_warning.h"
# include "log_convert_warn_to_fatal.h"
# include "log_verbose.h"
# include "log_win32_use_ansi.h"

#endif
