/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Private source-only persistent numeric foundation.
 *************************************************************/
module;
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <utility>
export module uwvm2.uwvm.runtime.storage:compact_numeric_descriptor;
import uwvm2.object.global;
import uwvm2.runtime.gc.entry_admission;
import :compact_numeric_payload;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "descriptor.h"
