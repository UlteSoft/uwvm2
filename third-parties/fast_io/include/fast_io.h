#pragma once

/*
 * Primary hosted fast_io entry point.
 *
 * `fast_io_hosted.h` supplies concrete platform/device implementations, and
 * `fast_io_legacy_impl/io.h` adds the lightweight `fast_io::io` scenario
 * facade (`print`, `println`, `perr`, `panic`, and `scan`). Generic explicit-
 * output operations and their CPO/decay machinery are already provided by the
 * core package. Compile-time format-language syntax is intentionally optional
 * and enters through `fast_io_format.h`.
 */

#if !defined(__cplusplus)
#error "You are not using a C++ compiler"
#endif

#if !defined(__cpp_concepts)
#error "fast_io requires at least C++20 standard compiler."
#else

#include "fast_io_hosted.h"

#include "fast_io_dsal/impl/misc/push_warnings.h"
#include "fast_io_dsal/impl/misc/push_macros.h"

#include "fast_io_legacy_impl/io.h"

#include "fast_io_dsal/impl/misc/pop_macros.h"
#include "fast_io_dsal/impl/misc/pop_warnings.h"

#endif
