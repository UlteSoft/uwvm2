module;
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.runtime.compiler.shared.wasm_exception_control;
import fast_io;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.exception_immediate;
import uwvm2.utils.container;
import uwvm2.validation.standard.wasm3.exception_policy;
import uwvm2.validation.standard.wasm3.exception_validation;
import uwvm2.validation.standard.wasm3.reference_policy;
import uwvm2.uwvm.runtime.storage;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "wasm_exception_control.h"
