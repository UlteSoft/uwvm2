module;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#endif
export module uwvm2.runtime.exception.activation;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
import uwvm2.runtime.exception.value;
import uwvm2.runtime.exception.native_roots;
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
import uwvm2.runtime.exception.external_handle;
#endif
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "activation.h"
#endif
