module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <utility>
export module uwvm2.validation.standard.wasm3.function_signature;
import uwvm2.utils.container;
import uwvm2.parser.wasm.standard.wasm3.type.function_signature;
import uwvm2.parser.wasm.standard.wasm3.type.recursive_type;
import uwvm2.validation.standard.wasm3.recursive_type_binary;
import uwvm2.validation.standard.wasm3.value_immediate;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "function_signature.h"
