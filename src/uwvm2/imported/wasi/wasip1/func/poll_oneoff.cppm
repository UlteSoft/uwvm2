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
#include <cstddef>
#include <cstdint>
#include <climits>
#include <cstring>
#include <limits>
#include <concepts>
#include <bit>
#include <memory>
#include <type_traits>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
// platform
#if !defined(_WIN32)
# include <errno.h>
# if __has_include(<poll.h>)
#  include <poll.h>
# endif
# if defined(__linux__)
#  if __has_include(<sys/epoll.h>)
#   include <sys/epoll.h>
#  endif
#  if __has_include(<sys/timerfd.h>)
#   include <sys/timerfd.h>
#  endif
# endif
# if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(BSD) || defined(_SYSTYPE_BSD) ||          \
     (defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
#  if __has_include(<sys/event.h>)
#   include <sys/event.h>
#  endif
#  if __has_include(<sys/time.h>)
#   include <sys/time.h>
#  endif
# endif
#endif

export module uwvm2.imported.wasi.wasip1.func:poll_oneoff;

import fast_io;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.uwvm_predefine.io;
import uwvm2.utils.mutex;
import uwvm2.utils.debug;
import uwvm2.utils.container;
import uwvm2.object.memory.linear;
import uwvm2.imported.wasi.wasip1.abi;
import uwvm2.imported.wasi.wasip1.fd_manager;
import uwvm2.imported.wasi.wasip1.memory;
import uwvm2.imported.wasi.wasip1.environment;
import :base;
import :posix;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "poll_oneoff.h"

