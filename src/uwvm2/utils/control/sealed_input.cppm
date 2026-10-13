module;
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <utility>
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/stat.h>
# include <unistd.h>
# include "posix_abi.h"
#endif
#if defined(__APPLE__) && defined(__MACH__)
# include <libproc.h>
# include <sys/proc_info.h>
# include <sys/ttycom.h>
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
# include <fast_io.h>
# include <windows.h>
# include <uwvm2/utils/control/win32_abi.h>
#endif
export module uwvm2.utils.control:sealed_input;
import fast_io;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "sealed_input.h"
