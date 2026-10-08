// PRIVATE source-only dual-mode adapter. No compiler/native qualification is inherited.
module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

export module uwvm2.runtime.compiler.shared.wasm_exception_effect;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "wasm_exception_effect.h"
