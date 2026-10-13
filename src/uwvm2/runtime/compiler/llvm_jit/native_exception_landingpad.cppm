module;
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ADT/SmallVector.h>
# include <llvm/IR/BasicBlock.h>
# include <llvm/IR/IRBuilder.h>
# include <llvm/IR/Instructions.h>
# include <llvm/IR/Intrinsics.h>
#endif
export module uwvm2.runtime.compiler.llvm_jit.native_exception_landingpad;
import uwvm2.runtime.compiler.llvm_jit.native_exception_symbols;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_exception_landingpad.h"
