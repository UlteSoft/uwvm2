module;
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Target/TargetMachine.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.shared.i32_dual_emission;
import fast_io;
import uwvm2.utils.container;
import uwvm2.validation.standard.wasm3;
import uwvm2.validation.error;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.runtime.storage;
import uwvm2.runtime.compiler.uwvm_int.optable;
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "i32_dual_emission.h"
