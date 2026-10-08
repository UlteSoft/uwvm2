module;
#include <cstddef>
#include <cstdint>
#include <limits>
export module uwvm2.uwvm.debugger:command;
import fast_io;
import uwvm2.utils.control;
import :wasm_events;
import uwvm2.uwvm.debugger.wasm_state;
import uwvm2.uwvm.debugger.wasm_mutation;
import uwvm2.uwvm.debugger.wasm_path;
import uwvm2.uwvm.debugger.wasip1_state;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "command.h"
