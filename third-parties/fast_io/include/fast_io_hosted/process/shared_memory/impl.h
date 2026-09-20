#pragma once

#if !defined(__MSDOS__) && !defined(__NEWLIB__) && !defined(__wasi__) && !defined(_PICOLIBC__)
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
#include "win32.h"
#if !defined(_WIN32_WINDOWS)
#include "nt.h"
#endif
#else
#include "posix.h"
#endif

namespace fast_io
{

#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
#if !defined(_WIN32_WINDOWS)
using native_shared_memory = nt_shared_memory;
#else
using native_shared_memory = win32_shared_memory;
#endif
#else
using native_shared_memory = posix_shared_memory;
#endif

} // namespace fast_io
#endif
