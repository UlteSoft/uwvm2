/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Private source-only persistent numeric foundation.
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
export module uwvm2.uwvm.runtime.storage:compact_numeric_payload;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "payload.h"
