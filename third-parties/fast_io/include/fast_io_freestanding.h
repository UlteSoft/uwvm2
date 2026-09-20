#pragma once

/*
 * Freestanding-capable environment/package umbrella.
 *
 * This header extends `fast_io_core.h` with facilities that require dynamic
 * allocation and exceptions, including buffering, decorators, serializers, and
 * floating conversion. It is not the "FMT level" or an intermediate layer
 * between IO and CPO: those architectural responsibilities already cross the
 * core package. Format syntax remains in `fast_io_format.h`, while hosted
 * devices remain in `fast_io_hosted.h`.
 */

// fast_io_freestanding.h is usable when the underlining system implements dynamic memory allocations and exceptions
#if !defined(__cplusplus)
#error "You are not using a C++ compiler"
#endif

#if !defined(__cpp_concepts)
#error "fast_io requires at least C++20 standard compiler."
#else

#include "fast_io_core.h"

#include "fast_io_dsal/impl/misc/push_warnings.h"
#include "fast_io_dsal/impl/misc/push_macros.h"

#include "fast_io_freestanding_impl/exception.h"
// #include"fast_io_freestanding_impl/posix_error.h"
// compile floating point is slow since it requires algorithms like ryu
#ifndef FAST_IO_DISABLE_FLOATING_POINT
#include "fast_io_unit/floating/impl.h"
#endif
#include "fast_io_freestanding_impl/io_buffer/impl.h"
#include "fast_io_freestanding_impl/io_deco_filter/impl.h"
#include "fast_io_freestanding_impl/decorators/impl.h"
#include "fast_io_freestanding_impl/auto_indent.h"
#include "fast_io_freestanding_impl/serializations/impl.h"
#include "fast_io_freestanding_impl/space_reserve.h"
#if 0
#include "fast_io_freestanding_impl/scanners/impl.h"
#endif

#if defined(_GLIBCXX_BITSET)
#include "fast_io_unit/bitset.h"
#endif

#include "fast_io_dsal/impl/misc/pop_macros.h"
#include "fast_io_dsal/impl/misc/pop_warnings.h"

#endif
