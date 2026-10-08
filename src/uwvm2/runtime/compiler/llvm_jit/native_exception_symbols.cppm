module;
#include <cstdint>
#include "native_arm_ehabi_loader_abi.h"
#include <memory>
#include <mutex>
#include <type_traits>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Comdat.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>
#endif
export module uwvm2.runtime.compiler.llvm_jit.native_exception_symbols;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_exception_symbols.h"
#include "uwvm2/uwvm/runtime/macro/pop_macros.h"
