/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/// @brief      codecvt parameters on winnt
/// @details    In Windows NT, process parameters are stored in the Process Environment Block (PEB) as UTF-16 encoded strings.
///             When a new process is created, the CreateProcess function passes these UTF-16 encoded command-line arguments
///             to the new process. However, in environments where the system's Active Code Page (ACP) is set to a non-UTF-8 encoding,
///             such as GBK, the C runtime (crt) of the new process converts these UTF-16 arguments into the current ACP encoding when
///             initializing the char** argv array for the main function. This means that even if UTF-16 encoded arguments are passed
///             from the parent process, they are converted to the system's ACP (e.g., GBK) in the child process. To handle this,
///             applications can retrieve the original UTF-16 command-line arguments using the CommandLineToArgvW function and perform
///             necessary conversions to UTF-8 or other desired encodings as needed.​
///
///             In contrast, Windows 9x systems do not exhibit this behavior. Internally, command-line arguments are stored as char* strings,
///             and the system assumes a consistent encoding based on the system's code page. When using CreateProcess, the arguments passed
///             to the new process are received in the same encoding without any automatic conversion, maintaining the original byte representation.
///
///             This file introduces a global variable ::fast_io::u8string
///
/// @author     MacroModel
/// @version    2.0.0
/// @date       2025-03-25
/// @copyright  APL-2.0 License

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

module;

// std
#include <cstdint>
#include <cstddef>
#include <memory>
#include <bit>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

export module uwvm2.uwvm.cmdline:winnt_code_cvt;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.ansies;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "winnt_code_cvt.h"
