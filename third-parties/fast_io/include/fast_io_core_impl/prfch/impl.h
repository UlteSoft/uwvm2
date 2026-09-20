#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#if !defined(__arm64ec__) && !defined(_M_ARM64EC) && \
	(defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
#include <xmmintrin.h>
#endif
#endif

// GCC 16 replaced the legacy pre-GCC-16 `__builtin_aarch64_pld*` spellings (GCC 15 in fast_io's supported range) with
// the standard ACLE interface. Include the compiler-owned header only for that exact backend/ISA pair; other targets
// must not acquire an Arm header dependency.
#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
#include <arm_acle.h>
#endif

#include "platform.h"
#include "prfch.h"
#include "provenance.h"
#include "policy.h"
