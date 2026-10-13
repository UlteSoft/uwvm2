module;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
// Bootstrap in this module's own global fragment; imports cannot export macros.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <cstdint>
#include <memory>
#include <utility>
#endif
export module uwvm2.runtime.gc.managed_exception_cohort;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
import uwvm2.runtime.exception.external_handle;
import uwvm2.runtime.exception.native_roots;
import uwvm2.utils.thread;
import uwvm2.uwvm.runtime.storage;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "managed_exception_cohort.h"
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
#endif
