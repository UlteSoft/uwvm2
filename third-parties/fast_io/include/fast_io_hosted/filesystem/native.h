#pragma once

#include "apis.h"
#include "fsutils.h"

#if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(__MSDOS__) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#include "posix.h"
#include "posix_at.h"
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
#include "posix_nothrow.h"
#endif
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include "posix_timestamps_nothrow.h"
#include "posix_readlink_nothrow.h"
#include "posix_timestamp_options_nothrow.h"
#endif
#endif

#if (defined(_WIN32) || defined(__CYGWIN__))
#if defined(_WIN32_WINDOWS)
#include "win32_9xa.h"
#include "win32_9xa_at.h"
#else
#include "nt.h"
#include "nt_at.h"
#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
#include "nt_readonly_sync_nothrow.h"
#endif
#endif
#endif

#if defined(__MSDOS__) || defined(__DJGPP__)
#include "dos.h"
#include "dos_at.h"
#endif
