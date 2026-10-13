/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <uwvm2/utils/macro/push_macros.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#if defined(__APPLE__)
# include <TargetConditionals.h>
#endif
#if defined(UWVM_USE_LLVM_JIT)
# include <llvm/ADT/ArrayRef.h>
# include <llvm/ADT/StringRef.h>
# include <llvm/MC/MCAsmInfo.h>
# include <llvm/MC/MCContext.h>
# include <llvm/MC/MCDisassembler/MCDisassembler.h>
# include <llvm/MC/MCInst.h>
# include <llvm/MC/MCInstrInfo.h>
# include <llvm/MC/MCInstrAnalysis.h>
# include <llvm/MC/MCRegisterInfo.h>
# include <llvm/MC/MCSubtargetInfo.h>
# include <llvm/MC/MCTargetOptions.h>
# include <llvm/MC/TargetRegistry.h>
# include <llvm/Support/raw_ostream.h>
# include <llvm/TargetParser/Triple.h>
#endif
export module uwvm2.uwvm.debugger:native_owned_instruction_semantics;
import :native_disassembly;
import :native_target_metadata;
import :native_instruction_semantics;
import :native_branch_destination;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_owned_instruction_semantics.h"
