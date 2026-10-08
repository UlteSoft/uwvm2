module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <array>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <memory>
# include <new>
# include <string>
# include <type_traits>
# include <utility>
# include <fast_io.h>
# include <windows.h>
# include <uwvm2/utils/control/win32_abi.h>
# include <tlhelp32.h>
#endif
export module uwvm2.uwvm.debugger:windows_control_handle;
import uwvm2.utils.control;
import :command;
import :controller;
import :console;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "windows_control_handle.h"
