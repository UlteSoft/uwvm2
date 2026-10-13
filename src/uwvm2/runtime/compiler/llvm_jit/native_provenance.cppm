module;
#include <cstddef>
#include <limits>
#if defined(__linux__) && (defined(__powerpc__) || defined(__loongarch__) || defined(__arm__))
# include <sys/auxv.h>
#endif
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <llvm/IR/DebugProgramInstruction.h>
# include <llvm/ADT/SmallVector.h>
# include <llvm/ADT/DenseMap.h>
# include <llvm/ADT/ArrayRef.h>
# include <llvm/BinaryFormat/Dwarf.h>
# include <llvm/IR/Intrinsics.h>
# include <llvm/IR/Instructions.h>
# include <llvm/IR/InlineAsm.h>
# include <llvm/IR/Constants.h>
# include <llvm/IR/DIBuilder.h>
# include <llvm/IR/DebugInfoMetadata.h>
# include <llvm/IR/Function.h>
# include <llvm/IR/IRBuilder.h>
# include <llvm/IR/Dominators.h>
# include <llvm/IR/ValueHandle.h>
# include <llvm/ADT/SmallPtrSet.h>
# if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
#  include <llvm/ADT/StringMap.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/TargetParser/X86TargetParser.h>
# endif
# if defined(__linux__) && (defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4))
#  include <llvm/TargetParser/Host.h>
# endif
# include <llvm/IR/Module.h>
#endif
export module uwvm2.runtime.compiler.llvm_jit.native_provenance;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_provenance.h"
