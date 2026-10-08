module;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#endif
export module uwvm2.runtime.exception.native_roots;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
import uwvm2.runtime.exception.value;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_roots.h"
#endif
