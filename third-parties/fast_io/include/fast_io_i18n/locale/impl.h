#pragma once

#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)
#include "win32.h"
#endif
// Cygwin defines `_WIN32` while exposing the POSIX dynamic-loader contract used by its native locale wrapper. Keep it
// as an explicit alternative to the ordinary non-Windows/Wine eligibility test: placing it only in the second
// conjunction would still let `_WIN32` suppress posix.h and leave Cygwin without the `native_l10n` alias.
#if ((!defined(_WIN32) || defined(__WINE__)) && (!defined(__wasi__) || !defined(__NEWLIB__))) || defined(__CYGWIN__)
#include "posix.h"
#endif
