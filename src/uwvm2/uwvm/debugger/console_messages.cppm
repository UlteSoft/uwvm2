module;
#include <uwvm2/utils/macro/push_macros.h>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
export module uwvm2.uwvm.debugger:console_messages;
import fast_io;
import uwvm2.utils.control;
import :command;
import :controller;
import :console_help;
import :console_tui;
import :console_aliases;
import :native_registers;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console_messages.h"
