module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(__APPLE__) && defined(__MACH__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <array>
# include <cerrno>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <memory>
# include <new>
# include <string>
# include <type_traits>
# include <utility>
# include <fcntl.h>
# include <poll.h>
# include <signal.h>
# include <sys/socket.h>
# include <sys/ucred.h>
# include <sys/un.h>
# include <unistd.h>
# include <mach/message.h>
# include "posix_abi.h"
#endif
export module uwvm2.uwvm.debugger:macos_control_fd;
import fast_io;
import uwvm2.utils.control;
import :command;
import :controller;
import :console;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "macos_control_fd.h"
