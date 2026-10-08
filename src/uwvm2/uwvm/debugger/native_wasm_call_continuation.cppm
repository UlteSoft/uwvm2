module;
#include <cstddef>
#include <cstdint>
#include <span>
export module uwvm2.uwvm.debugger:native_wasm_call_continuation;
import :native_wasm_step_boundary;
import :native_disassembly;
import :native_disassembly_window;
import :native_owned_instruction_semantics;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_wasm_call_continuation.h"
