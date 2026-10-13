/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <uwvm2/utils/macro/push_macros.h>
#include <cstddef>
#include <cstdint>
#if defined(UWVM_USE_LLVM_JIT)
# include <llvm/MC/MCDisassembler/MCDisassembler.h>
# include <llvm/MC/MCInst.h>
# include <llvm/MC/MCInstrDesc.h>
# include <llvm/MC/MCInstrInfo.h>
# include <llvm/MC/MCRegisterInfo.h>
#endif
export module uwvm2.uwvm.debugger:native_instruction_semantics;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_instruction_semantics.h"
