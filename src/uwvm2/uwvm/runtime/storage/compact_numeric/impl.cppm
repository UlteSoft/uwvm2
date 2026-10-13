/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Private source-only persistent numeric foundation.
 *************************************************************/
module;
export module uwvm2.uwvm.runtime.storage:compact_numeric;
export import :compact_numeric_payload;
export import :compact_numeric_descriptor;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "impl.h"
