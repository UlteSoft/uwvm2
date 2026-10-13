module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <array>
# include <cerrno>
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
# include <sys/socket.h>
# include <sys/syscall.h>
# include <unistd.h>
# include "posix_abi.h"
#endif
export module uwvm2.uwvm.debugger:linux_control_fd;
import fast_io;
import uwvm2.utils.control;
import :command;
import :controller;
import :console;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "linux_control_fd.h"
