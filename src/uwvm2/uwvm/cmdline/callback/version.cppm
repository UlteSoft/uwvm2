/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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

// std
#include <memory>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// platform
#if defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
# include <linux/version.h>
#endif
#if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
# include <TargetConditionals.h>
# include <Availability.h>
#endif
#if (defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH))
# include <sgidefs.h>
#endif

export module uwvm2.uwvm.cmdline.callback:version;

import fast_io;
import fast_io_crypto;
import uwvm2.utils.container;
import uwvm2.utils.ansies;
import uwvm2.utils.cmdline;
import uwvm2.parser.wasm.concepts;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.custom;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.utils.install_path;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "version.h"
