module;
#include <cstdint>
#include <fast_io_dsal/string.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
export module uwvm2.uwvm.debugger:console_aliases;
import fast_io;
import uwvm2.utils.control;
import :command;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console_aliases.h"
