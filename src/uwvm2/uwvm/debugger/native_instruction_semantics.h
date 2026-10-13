/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# if defined(UWVM_USE_LLVM_JIT)
#  include <llvm/MC/MCDisassembler/MCDisassembler.h>
#  include <llvm/MC/MCInst.h>
#  include <llvm/MC/MCInstrDesc.h>
#  include <llvm/MC/MCInstrInfo.h>
#  include <llvm/MC/MCRegisterInfo.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_instruction_semantics
{
    enum class flow : unsigned char
    {
        unknown, ordinary, call, return_instruction, branch, trap, other_control
    };
    struct classification
    {
        flow kind{flow::unknown};
        ::std::size_t size{};
        bool conditional_branch{};
        bool indirect_branch{}; // MCInstrDesc branch flag; not a guess about indirect calls.
        bool barrier{};
        bool may_change_pc{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return kind != flow::unknown; }
    };
#if defined(UWVM_USE_LLVM_JIT)
    // Cold scalar inspection of an MCInst decoded from OWNED bytes. There is
    // no native pointer, register context, target identity or mutation here.
    // A call classification alone cannot authorize ni, finish or a breakpoint:
    // those still require the real stopped frame and live code-owner lifetime.
    [[nodiscard]] inline classification classify(::llvm::MCInst const& instruction,
        ::llvm::MCInstrInfo const& instructions, ::llvm::MCRegisterInfo const& registers,
        ::llvm::MCDisassembler::DecodeStatus status, ::std::uint64_t decoded_size,
        ::std::size_t available) noexcept
    {
        if(status != ::llvm::MCDisassembler::Success || decoded_size == 0u ||
           decoded_size > available || instruction.getOpcode() >= instructions.getNumOpcodes()) { return {}; }
        // [target-owned descriptor table ... getNumOpcodes) end
        // [safe                                            ] opcode was checked;
        //  ^^ LLVM's internal descriptor indexing cannot leave its real table.
        auto const& descriptor{instructions.get(instruction.getOpcode())};
        auto const operands{instruction.getNumOperands()};
        if(descriptor.isPseudo() || descriptor.isMetaInstruction() || descriptor.getNumDefs() > operands ||
           (descriptor.variadicOpsAreDefs() &&
            (descriptor.getNumOperands() == 0u || descriptor.getNumOperands() - 1u > operands))) { return {}; }
        for(unsigned index{}; index != operands; ++index)
        {
            // [decoded MCOperand array ... operands) end
            // [safe                                  ] index < operands;
            //  ^^ each register is bounded before LLVM's subregister query.
            auto const& operand{instruction.getOperand(index)};
            if(operand.isReg() && operand.getReg() >= registers.getNumRegs()) { return {}; }
        }
        classification result{};
        result.size = static_cast<::std::size_t>(decoded_size);
        result.conditional_branch = descriptor.isConditionalBranch();
        result.indirect_branch = descriptor.isIndirectBranch();
        result.barrier = descriptor.isBarrier();
        result.may_change_pc = descriptor.mayAffectControlFlow(instruction, registers) || descriptor.isTrap();
        if(descriptor.isReturn()) { result.kind = flow::return_instruction; }
        else if(descriptor.isCall()) { result.kind = flow::call; }
        else if(descriptor.isBranch()) { result.kind = flow::branch; }
        else if(descriptor.isTrap()) { result.kind = flow::trap; }
        else if(result.may_change_pc) { result.kind = flow::other_control; }
        else { result.kind = flow::ordinary; }
        return result;
    }
#endif
}
