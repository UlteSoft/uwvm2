module;
#include <cstddef>
#include <cstdint>
export module uwvm2.validation.standard.wasm3.atomic_semantics;
import uwvm2.validation.standard.wasm3.threads;
import uwvm2.validation.standard.wasm3.typed_stack_semantics;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.validation.standard.wasm3.address_limits;
import uwvm2.validation.standard.wasm3.atomic_immediate;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "atomic_semantics.h"
