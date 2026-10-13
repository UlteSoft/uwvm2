module;
# include <uwvm2/utils/macro/push_macros.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(__linux__)
# include <fcntl.h>
# include <poll.h>
# include <sys/random.h>
# include <sys/socket.h>
# include <sys/syscall.h>
# include <sys/types.h>
# include <unistd.h>
# include "posix_abi.h"
#endif
export module uwvm2.utils.control:linux_launch_channel;
import fast_io;
import :session;
import :protocol;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "linux_launch_channel.h"
