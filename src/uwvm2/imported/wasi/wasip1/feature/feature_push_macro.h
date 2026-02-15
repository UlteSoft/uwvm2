/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-01
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

// #pragma once

/// @brief This option automatically excludes unsupported systems.
/// @note  UWVM_DISABLE_LOCAL_IMPORTED_WASIP1 controls imports within UWVM rather than this implementation.
#pragma push_macro("UWVM_IMPORT_WASI_WASIP1")
#undef UWVM_IMPORT_WASI_WASIP1
#if !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(__freestanding__)
# define UWVM_IMPORT_WASI_WASIP1
#endif

#pragma push_macro("UWVM_IMPORT_WASI_WASIP1_WASM64")
#undef UWVM_IMPORT_WASI_WASIP1_WASM64
#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64)
# define UWVM_IMPORT_WASI_WASIP1_WASM64
#endif

#pragma push_macro("UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET")
#undef UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET
#if defined(UWVM_IMPORT_WASI_WASIP1) && (defined(UWVM_ENABLE_WASIP1_SOCKET) || defined(UWVM_ENABLE_WASIX_VER_WASIP1_SOCKET)) &&                                \
    (defined(_WIN32) || (!defined(__wasi__) && __has_include(<sys/socket.h>) && __has_include(<netinet/in.h>)))
# define UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET
#endif

#pragma push_macro("UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET")
#undef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET) && defined(UWVM_ENABLE_WASIX_VER_WASIP1_SOCKET)
# define UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
#endif

/// @todo add more features here
