module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#include <fast_io.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
# include <vector>
#include <fast_io_unit/string.h>
# if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#  include <fcntl.h>
#  if defined(__linux__) && defined(__x86_64__)
#   include <csignal>
#   include <linux/perf_event.h>
#   include "native_perf_signal_linux.h"
#  endif
#  include <sys/stat.h>
#  include <unistd.h>
#  include "posix_abi.h"
# endif

#endif
export module uwvm2.uwvm.debugger:controller;
import fast_io;
import uwvm2.utils.control;
import uwvm2.utils.thread;
import uwvm2.runtime;
import uwvm2.runtime.exception;
import :command;
import :management_wait_interrupt;
import :source_map;
import :source_dwarf_query;
import :source_dwarf_values;
import :source_dwarf_objects;
import :source_dwarf_selectors;
import :source_dwarf_expression;
import :source_language_expression;
import :source_scalar_expression;
import :source_scope_path;
import :source_frames;
import :source_step_policy;
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
import :source_dwarf_index;
#endif
import :native_step;
import :native_continuation_linux;
import :native_wasm_call_continuation;
import :native_registers;
import :wasm_events;
import uwvm2.uwvm.debugger.wasm_state;
import uwvm2.uwvm.debugger.wasm_mutation;
import uwvm2.uwvm.debugger.wasm_path;
import uwvm2.uwvm.debugger.wasip1_state;
import uwvm2.uwvm.debugger.wasip1_portable_checkpoint;
import uwvm2.uwvm.debugger.wasip1_calls;
import :native_disassembly;
import :native_disassembly_window;
import :native_owned_instruction_semantics;
import :native_next_policy;
import :native_wasm_step_boundary;
import :native_branch_display;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "controller.h"
