module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/ioctl.h>
#endif
export module uwvm2.uwvm.debugger:console_tui;
import fast_io;
import :console_line_editor;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console_tui.h"
