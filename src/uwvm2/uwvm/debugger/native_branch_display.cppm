module;
#include <cstddef>
#include <cstdint>
#include <span>
#include <iterator>
export module uwvm2.uwvm.debugger:native_branch_display;
import fast_io;
import :native_disassembly;
import :native_disassembly_window;
import :native_branch_destination;
import :native_owned_instruction_semantics;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_branch_display.h"
