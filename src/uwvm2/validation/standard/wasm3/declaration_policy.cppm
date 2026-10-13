module;
#include <cstddef>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.validation.standard.wasm3.declaration_policy;
import fast_io;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.reference_policy;
import uwvm2.validation.standard.wasm3.value_immediate;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "declaration_policy.h"
