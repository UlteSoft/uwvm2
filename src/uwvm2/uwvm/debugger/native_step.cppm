module;
#include <uwvm2/utils/macro/push_macros.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "native_linux_platform.h"
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# include <array>
# if defined(__i386__)
#  include <cpuid.h>
# endif
# include <bit>
# include <csignal>
# include <cstring>
# include <limits>
# include <linux/futex.h>
# include <sys/mman.h>
# include <sys/syscall.h>
# include <ucontext.h>
# if defined(__powerpc__)
#  include <sys/auxv.h>
# endif
# include <unistd.h>
# include <fast_io.h>
# include "posix_abi.h"
#endif
#if defined(__APPLE__)
# include <TargetConditionals.h>
#endif
#if defined(__linux__) && defined(__x86_64__)
# include <csignal>
# include <linux/futex.h>
# include <linux/perf_event.h>
# include "native_perf_signal_linux.h"
# include <sys/syscall.h>
# include <ucontext.h>
# include <unistd.h>
# include "posix_abi.h"
#elif defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
      defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1
# include <fast_io.h>
# include <limits>
# include <windows.h>
#elif defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
# include <cstring>
# include <limits>
# include <mach/mach.h>
# if defined(__aarch64__)
#  include <mach/arm/thread_status.h>
#  include <mach/arm/exception.h>
# else
#  include <mach/i386/thread_status.h>
#  include <mach/i386/exception.h>
#  include <cerrno>
# endif
# include <mach/exc.h>
# include <Security/SecTask.h>
# include <CoreFoundation/CoreFoundation.h>
# include <pthread.h>
# include <unistd.h>
#endif
export module uwvm2.uwvm.debugger:native_step;
import :native_registers;
#if defined(__linux__) && (defined(__x86_64__) || UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
import uwvm2.runtime;
#endif
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
      defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1
import :native_step_windows;
#elif defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
import :native_step_macos;
#endif
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#if UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# include "native_linux_context.h"
#endif
#include "native_step.h"
