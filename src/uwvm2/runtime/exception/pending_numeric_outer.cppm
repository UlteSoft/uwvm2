// PRIVATE source-only dual-mode adapter. No compiler/native qualification is inherited.
module;
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

export module uwvm2.runtime.exception.pending_numeric_outer;
import uwvm2.runtime.exception.value;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1 && defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
import uwvm2.runtime.exception.activation;
#endif
import uwvm2.runtime.exception.pending_island;
import uwvm2.runtime.exception.pending_numeric_bridge;
import uwvm2.uwvm.runtime.storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "pending_numeric_outer.h"
