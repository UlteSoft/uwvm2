module;
#include <cstddef>
#include <memory>
#include <cstdint>
#include <climits>
#include <cerrno>
#include <fast_io.h>
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8
# include <csignal>
# include <linux/hw_breakpoint.h>
# include <linux/perf_event.h>
# include <sys/syscall.h>
# include "posix_abi.h"
# include "native_perf_signal_linux.h"
#endif
export module uwvm2.uwvm.debugger:native_continuation_linux;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_continuation_linux.h"
