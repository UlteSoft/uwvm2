module;
#include <type_traits>
export module uwvm2.uwvm.debugger:checkpoint_binding_dependency_contract;
import uwvm2.runtime.checkpoint.materialization;
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
import uwvm2.runtime.llvm_jit_cache;
#endif
import :checkpoint_binding;
import :checkpoint_codec;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "checkpoint_binding_dependency_contract.h"
