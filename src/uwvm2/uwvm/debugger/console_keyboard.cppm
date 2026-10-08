module;
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#if defined(__unix__) || defined(__APPLE__)
# include <cerrno>
# include <poll.h>
# include <signal.h>
# include <termios.h>
# include <unistd.h>
#elif defined(_WIN32) && !defined(__CYGWIN__)
# include <windows.h>
# include <uwvm2/utils/control/win32_abi.h>
#endif
export module uwvm2.uwvm.debugger:console_keyboard;
import fast_io;
import :console_line_editor;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console_keyboard.h"
