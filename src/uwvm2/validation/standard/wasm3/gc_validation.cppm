module;
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
export module uwvm2.validation.standard.wasm3.gc_validation;
import uwvm2.utils.container;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
import uwvm2.validation.standard.wasm3.reference_validation;
import uwvm2.validation.standard.wasm3.gc_i31_semantics;
import uwvm2.validation.standard.wasm3.local_declarations;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "gc_validation.h"
