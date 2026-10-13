/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include "native_instruction_semantics.h"
# if defined(UWVM_USE_LLVM_JIT)
#  include <llvm/MC/MCInst.h>
#  include <llvm/MC/MCInstrAnalysis.h>
#  include <llvm/MC/MCInstrInfo.h>
#  include <llvm/TargetParser/Triple.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_branch_destination
{
    enum class unavailability : unsigned char
    {
        incomplete_instruction, not_direct_control, unavailable_analysis,
        unsupported_architecture, incompatible_operand_metadata,
        unresolved_operand, arithmetic_overflow, backend_disagreement, none
    };
    struct result
    {
        ::std::uintptr_t display_pc{};
        unavailability reason{unavailability::incomplete_instruction};
        bool conditional{};
        // Exact static SPARC condition DATA; never a taken-branch observation.
        // Non-SPARC and dynamic forms leave this false.
        bool never_taken{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept
        { return reason == unavailability::none; }
    };
    // Pure checked arithmetic DATA. Negative INT64_MIN is never negated.
    // No resulting display position is converted to a host pointer or used
    // for a native register, memory read, breakpoint or execution operation.
    [[nodiscard]] constexpr bool add_displacement(::std::uintptr_t base,
        ::std::int64_t displacement, ::std::uintptr_t& output) noexcept
    {
        if(displacement >= 0)
        {
            auto const magnitude{static_cast<::std::uint64_t>(displacement)};
            if(magnitude > (::std::numeric_limits<::std::uintptr_t>::max)() - base) { return false; }
            output = base + static_cast<::std::uintptr_t>(magnitude);
        }
        else
        {
            auto const magnitude{static_cast<::std::uint64_t>(-(displacement + 1)) + 1u};
            if(magnitude > base) { return false; }
            output = base - static_cast<::std::uintptr_t>(magnitude);
        }
        return true;
    }
#if defined(UWVM_USE_LLVM_JIT)
    // ONLY scalar analysis of an MCInst already decoded from bounded owned
    // bytes. Supplied values remain DATA: they authenticate neither a target,
    // frame nor code owner, and cannot authorize a request at display_pc.
    // LLVM's target override supplies the actual destination. The exact
    // metadata/imm/checked-range preflight protects the override's internal
    // descriptor indexing, getImm and (for A64) signed imm*4 operation.
    [[nodiscard]] inline result evaluate(::llvm::Triple const& triple,
        ::llvm::MCInst const& instruction, ::llvm::MCInstrInfo const& instructions,
        ::llvm::MCInstrAnalysis const* analysis,
        native_instruction_semantics::classification const& semantics,
        ::std::uintptr_t display_pc) noexcept
    {
        using flow = native_instruction_semantics::flow;
        if(!semantics || semantics.size == 0u || display_pc == 0u ||
           semantics.size > (::std::numeric_limits<::std::uintptr_t>::max)() - display_pc ||
           instruction.getOpcode() >= instructions.getNumOpcodes()) { return {}; }
        if constexpr(::std::numeric_limits<::std::uintptr_t>::digits > 64)
        {
            // Target ISA addresses remain 64-bit even for a wider native
            // pointer carrier; reject BEFORE narrowing the LLVM display PC.
            if(display_pc > (::std::numeric_limits<::std::uint64_t>::max)() ||
               semantics.size > (::std::numeric_limits<::std::uint64_t>::max)() - display_pc)
            { return {0u, unavailability::arithmetic_overflow, false}; }
        }
        if(semantics.kind != flow::call && semantics.kind != flow::branch)
        { return {0u, unavailability::not_direct_control, false}; }
        bool const sparc{triple.getArch() == ::llvm::Triple::sparcv9};
        if(analysis == nullptr && !sparc) { return {0u, unavailability::unavailable_analysis, false}; }
        // [actual LLVM opcode/descriptor table ... getNumOpcodes) table_end
        // [safe                                                    ] opcode < N;
        //  ^^ the table lookup occurs only after the owned metadata bound.
        auto const& descriptor{instructions.get(instruction.getOpcode())};
        if((semantics.kind == flow::call && !descriptor.isCall()) ||
           (semantics.kind == flow::branch && !descriptor.isBranch()) ||
           descriptor.isPseudo() || descriptor.isMetaInstruction() ||
           descriptor.isVariadic() || instruction.getNumOperands() != descriptor.getNumOperands())
        { return {0u, unavailability::incompatible_operand_metadata, false}; }
        auto const operands{descriptor.operands()};
        if(operands.size() != instruction.getNumOperands())
        { return {0u, unavailability::incompatible_operand_metadata, false}; }
        // LoongArch's branch operands are OtherVT with OPERAND_UNKNOWN,
        // unlike targets that tag their displacement OPERAND_PCREL. Admit
        // only exact architectural direct forms and their decoded signed width.
        // The real target analysis must still agree, and the caller separately
        // proves both successor boundaries in the same authenticated Wasm owner.
        bool const loongarch{triple.getArch() == ::llvm::Triple::loongarch64 ||
            triple.getArch() == ::llvm::Triple::loongarch32};
        auto const opcode{instructions.getName(instruction.getOpcode())};
        unsigned loongarch_operands{}, loongarch_bits{};
        if(loongarch && semantics.kind == flow::branch && !semantics.indirect_branch)
        {
            if(opcode == "B") { loongarch_operands = 1u; loongarch_bits = 28u; }
            else if(opcode == "BEQ" || opcode == "BNE" || opcode == "BLT" || opcode == "BGE" ||
                    opcode == "BLTU" || opcode == "BGEU") { loongarch_operands = 3u; loongarch_bits = 18u; }
            else if(opcode == "BEQZ" || opcode == "BNEZ") { loongarch_operands = 2u; loongarch_bits = 23u; }
        }
        // Classic MIPS J carries a decoded absolute byte offset inside one
        // 256 MiB region. It has UNKNOWN operand metadata, unlike PC-relative
        // branches. Exact non-linking J only; JAL/JALX, registers and memory
        // never enter this set. The owner layer still proves the delay slot
        // and destination before either SI or NI can execute this DATA.
        bool const mips_absolute{triple.isMIPS() && semantics.kind==flow::branch &&
            !semantics.indirect_branch && opcode=="J" && instruction.getNumOperands()==1u &&
            descriptor.hasDelaySlot() && descriptor.isUnconditionalBranch() &&
            !descriptor.mayLoad() && !descriptor.mayStore() && !descriptor.isCall() &&
            !descriptor.isReturn() && !descriptor.isTrap()};
        ::std::int64_t displacement{};
        bool found{};
        unsigned displacement_index{};
        for(unsigned index{}; index != instruction.getNumOperands(); ++index)
        {
            // [actual MC operands ... N][actual descriptor operands ... N]
            // [safe                                                     ] index < BOTH checked extents;
            //  ^^ LLVM target evaluateBranch can subsequently index this same
            //     bounded descriptor/MCOperand pair without a variadic tail.
            bool const loongarch_displacement{loongarch_operands != 0u &&
                instruction.getNumOperands() == loongarch_operands && index + 1u == loongarch_operands &&
                operands[index].OperandType == ::llvm::MCOI::OPERAND_UNKNOWN};
            bool const mips_jump_operand{mips_absolute && index==0u &&
                (operands[index].OperandType==::llvm::MCOI::OPERAND_UNKNOWN ||
                 operands[index].OperandType==::llvm::MCOI::OPERAND_IMMEDIATE)};
            if(operands[index].OperandType != ::llvm::MCOI::OPERAND_PCREL && !loongarch_displacement && !mips_jump_operand) { continue; }
            auto const& operand{instruction.getOperand(index)};
            if(found || !operand.isImm())
            { return {0u, unavailability::unresolved_operand, false}; }
            displacement = operand.getImm();
            displacement_index = index;
            found = true;
        }
        // Register/memory indirect calls do NOT reveal their callee merely
        // because a memory operand itself is RIP-relative. No memory is read.
        if(!found) { return {0u, unavailability::not_direct_control, false}; }
        ::std::uintptr_t origin{};
        bool conditional{descriptor.isConditionalBranch()};
        if(sparc)
        {
            auto const name{instructions.getName(instruction.getOpcode())};
            bool const always{name == "BA"};
            bool const conditional{name == "BCOND" || name == "BCONDA" || name == "FBCOND" || name == "FBCONDA" ||
                name == "FBCOND_V9" || name == "FBCONDA_V9" || name == "BPFCC" || name == "BPFCCA" ||
                name == "BPFCCNT" || name == "BPFCCANT" || name == "BPICC" || name == "BPICCA" ||
                name == "BPICCNT" || name == "BPICCANT" || name == "BPXCC" || name == "BPXCCA" ||
                name == "BPXCCNT" || name == "BPXCCANT"};
            bool const register_condition{name == "BPR" || name == "BPRA" || name == "BPRNT" || name == "BPRANT"};
            bool const explicit_fcc{name == "BPFCC" || name == "BPFCCA" || name == "BPFCCNT" || name == "BPFCCANT"};
            bool const wide_displacement{always || name == "BCOND" || name == "BCONDA" || name == "FBCOND" || name == "FBCONDA"};
            // DecodeDisp<N> already sign-extends and scales the encoded displacement.
            // Each admitted opcode has its own exact architectural signed width.
            bool statically_always{always},statically_never{};
            unsigned const displacement_bits{register_condition ? 18u : wide_displacement ? 24u : 21u};
            if(semantics.kind != flow::branch || !descriptor.hasDelaySlot() || semantics.indirect_branch ||
               (!always && !conditional && !register_condition) || semantics.size != 4u || (display_pc & 3u) != 0u || displacement_index != 0u ||
               instruction.getNumOperands() != (always ? 1u : register_condition || explicit_fcc ? 3u : 2u) || (displacement & 3) != 0 ||
               displacement < -(::std::int64_t{1} << (displacement_bits - 1u)) ||
               displacement >= (::std::int64_t{1} << (displacement_bits - 1u)))
            { return {0u, unavailability::incompatible_operand_metadata, false}; }
            if(register_condition)
            {
                auto const& condition{instruction.getOperand(1u)};
                auto const& reg{instruction.getOperand(2u)};
                // rcond 000 and 100 are reserved; no indirect PC operand exists.
                if(!condition.isImm() || condition.getImm() < 1 || condition.getImm() > 7 || condition.getImm() == 4 ||
                   !reg.isReg() || reg.getReg() == 0u)
                { return {0u, unavailability::incompatible_operand_metadata, false}; }
            }
            else if(conditional)
            {
                auto const& condition{instruction.getOperand(1u)};
                if(!condition.isImm() || condition.getImm() < 0 || condition.getImm() > 15 ||
                   (explicit_fcc && (!instruction.getOperand(2u).isReg() || instruction.getOperand(2u).getReg() == 0u)))
                { return {0u, unavailability::incompatible_operand_metadata, false}; }
                statically_always=condition.getImm()==8;
                statically_never=condition.getImm()==0;
            }
            ::std::uintptr_t target{};
            if(!add_displacement(display_pc, displacement, target))
            { return {0u, unavailability::arithmetic_overflow, false}; }
            // This exact table/operand arithmetic is DATA. The runtime must prove
            // the delay slot AND both continuation boundaries in the same owner.
            return {target, unavailability::none, !statically_always && !statically_never,statically_never};
        }
        if(triple.isX86())
        { origin = display_pc + semantics.size; } // Checked above before scalar addition.
        else if(triple.getArch() == ::llvm::Triple::aarch64)
        {
            if(semantics.size != 4u || (display_pc & 3u) != 0u ||
               displacement < (::std::numeric_limits<::std::int64_t>::min)() / 4 ||
               displacement > (::std::numeric_limits<::std::int64_t>::max)() / 4)
            { return {0u, unavailability::arithmetic_overflow, false}; }
            displacement *= 4; // Exact signed range established BEFORE scaling.
            origin = display_pc;
        }
        else if(triple.getArch() == ::llvm::Triple::ppc || triple.getArch() == ::llvm::Triple::ppcle ||
                triple.getArch() == ::llvm::Triple::ppc64 || triple.getArch() == ::llvm::Triple::ppc64le)
        {
            if(semantics.size != 4u || (display_pc & 3u) != 0u ||
               displacement_index + 1u != instruction.getNumOperands() ||
               displacement < (::std::numeric_limits<::std::int64_t>::min)() / 4 ||
               displacement > (::std::numeric_limits<::std::int64_t>::max)() / 4)
            { return {0u, unavailability::incompatible_operand_metadata, false}; }
            displacement *= 4; origin = display_pc;
        }
        else if(triple.getArch() == ::llvm::Triple::riscv64 || triple.getArch() == ::llvm::Triple::riscv32)
        {
            auto const name{instructions.getName(instruction.getOpcode())};
            unsigned expected_index{};
            if(descriptor.isConditionalBranch())
            {
                if(semantics.size != 2u && semantics.size != 4u) { return {}; }
                expected_index = semantics.size == 2u ? 1u : 2u;
            }
            else if(name == "C_J" || name == "C_JAL")
            { if(semantics.size != 2u) { return {}; } }
            else if(name == "JAL")
            { if(semantics.size != 4u) { return {}; } expected_index = 1u; }
            else { return {0u,unavailability::unsupported_architecture,false}; }
            if(displacement_index != expected_index || (display_pc & 1u) != 0u)
            { return {0u,unavailability::incompatible_operand_metadata,false}; }
            origin = display_pc;
        }
        else if(triple.isMIPS())
        {
            auto const name{instructions.getName(instruction.getOpcode())};
            bool const binary{name == "BEQ" || name == "BNE" || name == "BEQ64" || name == "BNE64"};
            bool const unary{name == "BGEZ" || name == "BGTZ" || name == "BLEZ" || name == "BLTZ" ||
                name == "BGEZ64" || name == "BGTZ64" || name == "BLEZ64" || name == "BLTZ64"};
            if(mips_absolute)
            {
                if(semantics.size!=4u || (display_pc&3u)!=0u || displacement_index!=0u ||
                   displacement<0 || displacement>0x0ffffffc || (displacement&3)!=0)
                { return {0u,unavailability::incompatible_operand_metadata,false}; }
                origin=(display_pc+semantics.size)&~::std::uintptr_t{0x0fffffffu};
                // Architecture uses PC+4. LLVM uses PC for its region. At
                // the region boundary they differ; refuse before analysis.
                if(origin!=(display_pc&~::std::uintptr_t{0x0fffffffu}))
                { return {0u,unavailability::incompatible_operand_metadata,false}; }
            }
            else
            {
            // DecodeBranchTarget returns sign_extend(imm16)*4 + 4.
            // Only classic non-annulling, non-linking PC-relative forms apply.
            if(semantics.kind != flow::branch || semantics.indirect_branch || (!binary && !unary) ||
               !descriptor.hasDelaySlot() || semantics.size != 4u || (display_pc & 3u) != 0u ||
               instruction.getNumOperands() != (binary ? 3u : 2u) || displacement_index + 1u != instruction.getNumOperands() ||
               (displacement & 3) != 0 || displacement < -131068 || displacement > 131072)
            { return {0u,unavailability::incompatible_operand_metadata,false}; }
            origin = display_pc;
            }
        }
        else if(triple.getArch() == ::llvm::Triple::loongarch64 || triple.getArch() == ::llvm::Triple::loongarch32)
        {
            if(loongarch_operands == 0u || instruction.getNumOperands() != loongarch_operands ||
               semantics.size != 4u || (display_pc & 3u) != 0u || displacement_index + 1u != instruction.getNumOperands() ||
               (displacement & 3) != 0 || displacement < -(::std::int64_t{1} << (loongarch_bits - 1u)) ||
               displacement >= (::std::int64_t{1} << (loongarch_bits - 1u)))
            { return {0u,unavailability::incompatible_operand_metadata,false}; }
            origin = display_pc;
        }
        else if(triple.getArch() == ::llvm::Triple::systemz)
        {
            // This target registers the generic MCInstrAnalysis, whose branch
            // evaluator always returns false. Decode only the real BRC/BRCL
            // table forms: SystemZ's decoder already sign-extends and doubles
            // their halfword displacement. No indirect/register form is used.
            auto const name{instructions.getName(instruction.getOpcode())};
            auto const canonical_condition{[](::llvm::StringRef suffix) noexcept
            {
                return suffix == "E" || suffix == "NE" || suffix == "H" || suffix == "NH" || suffix == "L" || suffix == "NL" ||
                    suffix == "HE" || suffix == "NHE" || suffix == "LE" || suffix == "NLE" || suffix == "Z" || suffix == "NZ" ||
                    suffix == "P" || suffix == "NP" || suffix == "M" || suffix == "NM" || suffix == "LH" || suffix == "NLH" ||
                    suffix == "O" || suffix == "NO";
            }};
            // The real generated disassembler selects fixed-mask extended
            // mnemonics before the raw two-operand forms. Their exact TD set
            // still carries the same bounded PC-relative displacement.
            bool const fixed_short{name == "J" || (name.starts_with("JAsm") && canonical_condition(name.substr(4u)))};
            bool const fixed_long{name == "JG" || (name.starts_with("JGAsm") && canonical_condition(name.substr(5u)))};
            bool const fixed{fixed_short || fixed_long};
            bool const short_branch{fixed_short || name == "BRCAsm" || name == "BRC"};
            bool const long_branch{fixed_long || name == "BRCLAsm" || name == "BRCL"};
            unsigned const bits{short_branch ? 17u : 33u};
            if(semantics.kind != flow::branch || semantics.indirect_branch || (!short_branch && !long_branch) ||
               semantics.size != (short_branch ? 4u : 6u) || (display_pc & 1u) != 0u ||
               instruction.getNumOperands() != (fixed ? 1u : 2u) || displacement_index != (fixed ? 0u : 1u) || (displacement & 1) != 0 ||
               (!fixed && (!instruction.getOperand(0u).isImm() || instruction.getOperand(0u).getImm() < 0 ||
                            instruction.getOperand(0u).getImm() > 15)) ||
               displacement < -(::std::int64_t{1} << (bits - 1u)) || displacement >= (::std::int64_t{1} << (bits - 1u)))
            { return {0u,unavailability::incompatible_operand_metadata,false}; }
            ::std::uintptr_t expected{};
            if(!add_displacement(display_pc,displacement,expected))
            { return {0u,unavailability::arithmetic_overflow,false}; }
            return {expected,unavailability::none,fixed ? name != "J" && name != "JG" : instruction.getOperand(0u).getImm() != 15};
        }
        else if(triple.getArch() == ::llvm::Triple::arm || triple.getArch() == ::llvm::Triple::armeb)
        {
            if(semantics.size != 4u || (display_pc & 3u) != 0u || display_pc > UINTPTR_MAX - 8u)
            { return {0u,unavailability::arithmetic_overflow,false}; }
            origin = display_pc + 8u;
            // ARM Bcc's TD descriptor covers conditional and AL forms.
            // Only the complete actual predicate shape plus the target's
            // independent analysis can prove that AL has no fallthrough.
            // Otherwise a literal pool after B would be mistaken for a
            // reachable successor. This never changes call/indirect policy.
            if(opcode == "Bcc")
            {
                if(semantics.kind!=flow::branch || semantics.indirect_branch ||
                   instruction.getFlags()!=0u || instruction.getNumOperands()!=3u ||
                   descriptor.getNumDefs()!=0u || displacement_index!=0u ||
                   !operands[1u].isPredicate() || !operands[2u].isPredicate() ||
                   !instruction.getOperand(1u).isImm() || !instruction.getOperand(2u).isReg() ||
                   instruction.getOperand(1u).getImm()<0 || instruction.getOperand(1u).getImm()>14 ||
                   descriptor.mayLoad() || descriptor.mayStore() || descriptor.isCall() ||
                   descriptor.isReturn() || descriptor.isTrap() || descriptor.hasDelaySlot() ||
                   descriptor.hasUnmodeledSideEffects() ||
                   displacement<-(::std::int64_t{1}<<25u) || displacement>=(::std::int64_t{1}<<25u) ||
                   (displacement&3)!=0)
                { return {0u,unavailability::incompatible_operand_metadata,false}; }
                conditional=instruction.getOperand(1u).getImm()!=14;
                if((instruction.getOperand(2u).getReg()!=0u)!=conditional)
                { return {0u,unavailability::incompatible_operand_metadata,false}; }
                if(analysis->isConditionalBranch(instruction)!=conditional ||
                   analysis->isUnconditionalBranch(instruction)==conditional)
                { return {0u,unavailability::backend_disagreement,false}; }
            }
        }
        else { return {0u, unavailability::unsupported_architecture, false}; }
        ::std::uintptr_t expected{};
        if(!add_displacement(origin, displacement, expected))
        { return {0u, unavailability::arithmetic_overflow, false}; }
        if constexpr(::std::numeric_limits<::std::uintptr_t>::digits > 64)
        {
            if(expected > (::std::numeric_limits<::std::uint64_t>::max)())
            { return {0u, unavailability::arithmetic_overflow, false}; }
        }
        ::std::uint64_t actual{};
        if(!analysis->evaluateBranch(instruction, static_cast<::std::uint64_t>(display_pc),
                                    static_cast<::std::uint64_t>(semantics.size), actual))
        { return {0u, unavailability::unavailable_analysis, false}; }
        if(actual != static_cast<::std::uint64_t>(expected))
        { return {0u, unavailability::backend_disagreement, false}; }
        return {expected, unavailability::none, conditional};
    }
#endif
}
