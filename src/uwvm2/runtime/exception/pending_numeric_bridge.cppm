// PRIVATE source-only dual-mode adapter. No compiler/native qualification is inherited.
module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

export module uwvm2.runtime.exception.pending_numeric_bridge;
import uwvm2.runtime.exception.value;
import uwvm2.runtime.exception.pending_island;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "pending_numeric_bridge.h"
