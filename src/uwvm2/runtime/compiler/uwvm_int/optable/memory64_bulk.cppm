module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:memory64_bulk;
import uwvm2.utils.container;
import uwvm2.runtime.compiler.shared.wasm_memory64;
import uwvm2.uwvm.runtime.storage;
import :define;
import :memory;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "memory64_bulk.h"
