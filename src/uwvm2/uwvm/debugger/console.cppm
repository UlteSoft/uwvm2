module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#include <algorithm>
#include <cstdio>
#include <bit>
#include <cstring>
#include <string>
#include <memory>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <utility>
#include <fast_io.h>
#include <fast_io_unit/string.h>

#endif
export module uwvm2.uwvm.debugger:console;
import fast_io;
import uwvm2.utils.control;
import uwvm2.utils.thread;
import uwvm2.runtime;
import uwvm2.runtime.exception;
import :command;
import :management_wait_interrupt;
import :controller;
import :managed_cli_shutdown;
import :console_line_editor;
import :console_aliases;
import :console_execution;
import :console_completion;
import :console_displays;
import :console_tui;
import :console_keyboard;
import :native_next_policy;
import :native_branch_display;
import :wasm_events;
import uwvm2.uwvm.debugger.wasm_state;
import uwvm2.uwvm.debugger.wasm_mutation;
import uwvm2.uwvm.debugger.wasip1_state;
import uwvm2.uwvm.debugger.wasip1_calls;
import :native_registers;
import :source_dwarf_objects;
import :source_scalar_expression;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "console.h"
