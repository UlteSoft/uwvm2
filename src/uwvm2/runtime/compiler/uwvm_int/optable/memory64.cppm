module;
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:memory64;
import fast_io;
import uwvm2.utils.container;
import uwvm2.runtime.compiler.shared.wasm_memory64;
import uwvm2.runtime.compiler.shared.wasm_threads;
import :define;
import :register_ring;
import :memory;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "memory64.h"
