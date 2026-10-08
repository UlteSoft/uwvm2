module;
#include <cstddef>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.validation.standard.wasm3.memory_validation;
import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.base;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.memory_immediate;
import uwvm2.validation.standard.wasm3.address_limits;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "memory_validation.h"
