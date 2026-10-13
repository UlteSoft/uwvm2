module;
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#if defined(__APPLE__)
# include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
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
# include "posix_abi.h"
#endif
export module uwvm2.uwvm.debugger:native_step_macos;
import fast_io;
import :native_registers;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_step_macos.h"
