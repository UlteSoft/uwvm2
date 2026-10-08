module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:memory64_simd;
import fast_io;
import uwvm2.utils.container;
import uwvm2.runtime.compiler.shared.wasm1p1_simd;
import :define;
import :memory64;
import :register_ring;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "memory64_simd.h"
