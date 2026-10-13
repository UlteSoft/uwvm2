module;
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
# include <fast_io.h>
# include <windows.h>
# include <uwvm2/utils/control/win32_abi.h>
# if defined(_MSC_VER) && !defined(__clang__)
#  include <intrin.h>
# endif
#endif
export module uwvm2.uwvm.debugger:native_step_windows;
import :native_registers;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_step_windows.h"
