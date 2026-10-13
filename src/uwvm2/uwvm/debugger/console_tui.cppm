module;
#include <uwvm2/uwvm/debugger/console_stdio.h>
#include <uwvm2/uwvm/debugger/console_memory.h>
#include <uwvm2/uwvm/debugger/console_layout.h>
#include <vector>
#include <span>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/ioctl.h>
#endif
export module uwvm2.uwvm.debugger:console_tui;
import fast_io;
import uwvm2.uwvm.utils.ansies;
import :console_line_editor;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console_tui.h"
