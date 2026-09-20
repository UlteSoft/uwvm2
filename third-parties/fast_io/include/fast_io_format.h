#pragma once

/*
 * Public umbrella for the format frontend (FMT level).
 *
 * The FMT level is a syntax adapter: it validates a compile-time format
 * program, lowers each replacement field to typed fast_io printable objects,
 * and then hands that ordered object record to the ordinary IO level. It does
 * not own stream selection, locking, buffering, allocation policy, primitive
 * writes, or printable CPO dispatch; those remain responsibilities of the IO
 * and protocol levels included below.
 */

#if !defined(__cplusplus)
#error "You are not using a C++ compiler"
#endif

#if !defined(__cpp_concepts)
#error "fast_io requires at least C++20 standard compiler."
#else

#include "fast_io_format/format.h"

#endif
