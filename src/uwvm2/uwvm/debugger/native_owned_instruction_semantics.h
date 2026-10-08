/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <algorithm>
# include <array>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <span>
# include <string>
# include <type_traits>
# include "native_disassembly.h"
# include "native_target_metadata.h"
# include "native_instruction_semantics.h"
# include "native_branch_destination.h"
# if defined(UWVM_USE_LLVM_JIT)
#  include <llvm/ADT/ArrayRef.h>
#  include <llvm/ADT/StringRef.h>
#  include <llvm/MC/MCAsmInfo.h>
#  include <llvm/MC/MCContext.h>
#  include <llvm/MC/MCInstrAnalysis.h>
#  include <llvm/MC/MCSubtargetInfo.h>
#  include <llvm/MC/MCTargetOptions.h>
#  include <llvm/MC/TargetRegistry.h>
#  include <llvm/Support/raw_ostream.h>
#  include <llvm/TargetParser/Triple.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_owned_instruction_semantics
{
    using classification = native_instruction_semantics::classification;
    struct call_memory_operand
    {
        ::std::size_t base{SIZE_MAX},index{SIZE_MAX};
        ::std::int64_t displacement{};
        ::std::uint8_t scale{};
        bool rip_relative{},valid{};
        // MC operand DATA only. No address/value, read capability or public row.
    };
    class decoder;
    class decoded_instruction final
    {
        classification semantics_{};
        native_branch_destination::result destination_{};
        bool direct_branch_step_safe_{};
        bool call_continuation_safe_{};
        unsigned call_delay_bytes_{};
        bool public_display_safe_{};
        bool delayed_branch_{}, annulled_branch_{}, delayed_branch_pair_{};
        // Scalar MC operand DATA, never a native register read capability.
        // SIZE_MAX means this instruction supplies no exact near-call GPR.
        ::std::size_t call_register_index_{SIZE_MAX};
        call_memory_operand call_memory_{};
        ::std::size_t near_return_pop_{SIZE_MAX};
        explicit constexpr decoded_instruction(classification semantics, native_branch_destination::result destination,
            bool direct_branch_step_safe, bool call_continuation_safe,
            ::std::size_t call_register_index, bool public_display_safe, bool delayed_branch = false, bool annulled_branch = false, call_memory_operand memory = {}, bool delayed_branch_pair = false) noexcept
            : semantics_{semantics}, destination_{destination}, direct_branch_step_safe_{direct_branch_step_safe},
              call_continuation_safe_{call_continuation_safe}, public_display_safe_{public_display_safe},
              delayed_branch_{delayed_branch}, annulled_branch_{annulled_branch}, delayed_branch_pair_{delayed_branch_pair},
              call_register_index_{call_register_index}, call_memory_{memory} {}
        friend class decoder;
    public:
        constexpr decoded_instruction() noexcept = default;
        [[nodiscard]] constexpr classification const& semantics() const noexcept { return semantics_; }
        // Confidentiality is separate from execution safety. Memory/address
        // forms, implicit transfers and unresolved operands have no public row.
        // A direct branch still needs the caller's exact guest target proof.
        [[nodiscard]] constexpr bool safe_for_public_display() const noexcept { return public_display_safe_; }
        [[nodiscard]] constexpr native_branch_destination::result const& destination() const noexcept { return destination_; }
        [[nodiscard]] constexpr bool delayed_direct_branch() const noexcept { return delayed_branch_; }
        [[nodiscard]] constexpr bool annulled_direct_branch() const noexcept { return annulled_branch_; }
        [[nodiscard]] constexpr bool delayed_branch_pair() const noexcept { return delayed_branch_pair_; }
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return static_cast<bool>(semantics_); }
        // This distinguishes this conservative decoder's result from a bare
        // descriptor classification. It is DATA, not a secret credential or
        // permission to operate on code, a frame, or any requested address.
        [[nodiscard]] constexpr bool safe_for_single_instruction() const noexcept
        {
            return semantics_.kind == native_instruction_semantics::flow::ordinary && semantics_.size != 0u &&
                   !semantics_.may_change_pc && !semantics_.conditional_branch && !semantics_.indirect_branch && !semantics_.barrier;
        }
        // Returning ABI call DATA only. The caller still requires the same real trap,
        // canonical immutable Wasm owner and a verified native continuation.
        [[nodiscard]] constexpr bool safe_for_call_continuation() const noexcept
        { return call_continuation_safe_ && semantics_.kind == native_instruction_semantics::flow::call; }
        [[nodiscard]] constexpr unsigned call_delay_bytes() const noexcept { return call_delay_bytes_; }
        // Exact CALL64r operand index in native_registers::snapshot's fixed
        // RAX,RBX,RCX,RDX,RSI,RDI,RBP,RSP,R8..R15 order. Only a runtime-owned
        // SAME kernel snapshot can supply its value. A caller-populated snapshot,
        // decoded byte buffer or integer target grants no execution authority.
        [[nodiscard]] constexpr ::std::size_t call_register_index() const noexcept
        { return call_register_index_; }
        [[nodiscard]] constexpr call_memory_operand const& call_memory() const noexcept { return call_memory_; }
        // Exact MC-decoded near RET32/RETI32 or RET64/RETI64 DATA. SIZE_MAX denotes
        // unsupported return semantics. This grants no stack/frame permission.
        [[nodiscard]] constexpr ::std::size_t near_return_pop_bytes() const noexcept { return near_return_pop_; }


        // Target-opcode/flags evidence from this exact MC decode only. Still
        // DATA: the runtime must separately authenticate the complete current
        // Wasm image and every possible successor before opening a trap gate.
        [[nodiscard]] constexpr bool safe_for_same_owner_direct_branch() const noexcept
        {
            return direct_branch_step_safe_ && semantics_.kind == native_instruction_semantics::flow::branch &&
                   !semantics_.indirect_branch && static_cast<bool>(destination_);
        }
    };

#if defined(UWVM_USE_LLVM_JIT)
    namespace details
    {
        // LLVM 23 development SDKs changed these public constructors to Triple
        // and references. Probe the actual SDK signature, not its major number.
        template<typename Target>
        [[nodiscard]] inline auto make_registers(Target const& target, ::llvm::Triple const& triple)
        {
            if constexpr(requires { target.createMCRegInfo(triple); })
            { return ::std::unique_ptr<::llvm::MCRegisterInfo>{target.createMCRegInfo(triple)}; }
            else { return ::std::unique_ptr<::llvm::MCRegisterInfo>{target.createMCRegInfo(triple.str())}; }
        }
        template<typename Target>
        [[nodiscard]] inline auto make_assembly(Target const& target, ::llvm::MCRegisterInfo const& registers, ::llvm::Triple const& triple,
            ::llvm::MCTargetOptions const& options)
        {
            if constexpr(requires { target.createMCAsmInfo(registers, triple, options); })
            { return ::std::unique_ptr<::llvm::MCAsmInfo>{target.createMCAsmInfo(registers, triple, options)}; }
            else { return ::std::unique_ptr<::llvm::MCAsmInfo>{target.createMCAsmInfo(registers, triple.str(), options)}; }
        }
        template<typename Target>
        [[nodiscard]] inline auto make_subtarget(Target const& target, ::llvm::Triple const& triple,
            ::llvm::StringRef cpu = {}, ::llvm::StringRef features = {})
        {
            if constexpr(requires { target.createMCSubtargetInfo(triple, cpu, features); })
            { return ::std::unique_ptr<::llvm::MCSubtargetInfo>{target.createMCSubtargetInfo(triple, cpu, features)}; }
            else { return ::std::unique_ptr<::llvm::MCSubtargetInfo>{target.createMCSubtargetInfo(triple.str(), cpu, features)}; }
        }
        template<typename Assembly, typename Subtarget, typename Triple>
        [[nodiscard]] inline unsigned maximum_instruction_length(Assembly const& assembly, Subtarget const& subtarget,
            Triple const& actual_triple) noexcept
        {
            // MCAsmInfo estimates inline assembly; LLVM23 X86 inherits four.
            // Match the real runtime target's architectural decoder bound.
            if(actual_triple.isX86()) { return 15u; }
            if constexpr(requires { assembly.getMaxInstLength(::std::addressof(subtarget)); })
            { return assembly.getMaxInstLength(::std::addressof(subtarget)); }
            else { return assembly.getMaxInstLength(); }
        }
        template<typename Context = ::llvm::MCContext>
        [[nodiscard]] inline auto make_context(::llvm::Triple const& triple, ::llvm::MCAsmInfo const& assembly,
            ::llvm::MCRegisterInfo const& registers, ::llvm::MCSubtargetInfo const& subtarget)
        {
            if constexpr(::std::is_constructible_v<Context, ::llvm::Triple const&, ::llvm::MCAsmInfo const&,
                ::llvm::MCRegisterInfo const&, ::llvm::MCSubtargetInfo const&>)
            { return ::std::make_unique<Context>(triple, assembly, registers, subtarget); }
            else { return ::std::make_unique<Context>(triple, ::std::addressof(assembly), ::std::addressof(registers), ::std::addressof(subtarget)); }
        }
        template<typename Registry = ::llvm::TargetRegistry>
        [[nodiscard]] inline auto lookup_target(::llvm::Triple const& triple, ::std::string& error)
        {
            if constexpr(requires { Registry::lookupTarget(triple, error); })
            { return Registry::lookupTarget(triple, error); }
            else { return Registry::lookupTarget(triple.str(), error); }
        }
        // Public LLVM opcode-table identities, NOT disassembly text or a byte
        // prefix scanner. MC side-effect/control-flow flags describe codegen,
        // not the x86 TF/interrupt-shadow protocol. In LLVM 23 POPF explicitly
        // hasSideEffects=0; segment loads may omit implicit SS definitions.
        // Refuse whole decoded segment-write families conservatively. This
        // cold DATA predicate does not grant any address or stepping authority.
        [[nodiscard]] inline bool changes_native_debug_trap_state(::llvm::StringRef actual_opcode) noexcept
        {
            return actual_opcode == "POPF16" || actual_opcode == "POPF32" || actual_opcode == "POPF64" ||
                   actual_opcode == "IRET16" || actual_opcode == "IRET32" || actual_opcode == "IRET64" ||
                   actual_opcode == "MOV16sr" || actual_opcode == "MOV32sr" || actual_opcode == "MOV64sr" ||
                   actual_opcode == "MOV16sm" || actual_opcode == "LSS16rm" || actual_opcode == "LSS32rm" ||
                   actual_opcode == "LSS64rm" || actual_opcode == "POPSS16" || actual_opcode == "POPSS32" ||
                   actual_opcode == "STI";
        }
        // AArch64 expands a shifted-register Operand into a fixed GPR and
        // packed shift immediate, both marked with a target OperandType by TableGen.
        // Qualify that immediate ONLY for these exact numeric ALU table forms;
        // arbitrary unknown operands, memory and address forms stay hidden.
        [[nodiscard]] inline bool numeric_shift_operand(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, unsigned index, ::std::int64_t value) noexcept
        {
            if(!triple.isAArch64() || index != 3u || value < 0 || value > 255) { return false; }
            bool word{}, logical{};
            if(opcode == "ADDWrs" || opcode == "SUBWrs" || opcode == "ADDSWrs" || opcode == "SUBSWrs") { word = true; }
            else if(opcode == "ADDXrs" || opcode == "SUBXrs" || opcode == "ADDSXrs" || opcode == "SUBSXrs") {}
            else if(opcode == "ANDWrs" || opcode == "ANDSWrs" || opcode == "ORRWrs" || opcode == "EORWrs" ||
                    opcode == "BICWrs" || opcode == "BICSWrs" || opcode == "ORNWrs" || opcode == "EONWrs") { word = logical = true; }
            else if(opcode == "ANDXrs" || opcode == "ANDSXrs" || opcode == "ORRXrs" || opcode == "EORXrs" ||
                    opcode == "BICXrs" || opcode == "BICSXrs" || opcode == "ORNXrs" || opcode == "EONXrs") { logical = true; }
            else { return false; }
            return (!word || (value & 63) < 32) && (logical || (value >> 6) < 3);
        }
        // LoongArch ALU immediates use generic OtherVT operand metadata.
        // Only these exact numeric forms/ranges are printable; memory, address
        // construction, control and unknown target forms never enter this set.
        [[nodiscard]] inline bool numeric_loongarch_immediate(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, unsigned index, ::std::int64_t value) noexcept
        {
            if((triple.getArch() != ::llvm::Triple::loongarch64 && triple.getArch() != ::llvm::Triple::loongarch32) ||
               index != 2u) { return false; }
            if(opcode == "ADDI_W" || opcode == "ADDI_D") { return value >= -2048 && value <= 2047; }
            if(opcode == "ANDI" || opcode == "ORI" || opcode == "XORI") { return value >= 0 && value <= 4095; }
            if(opcode == "SLLI_W" || opcode == "SRLI_W" || opcode == "SRAI_W") { return value >= 0 && value < 32; }
            if(opcode == "SLLI_D" || opcode == "SRLI_D" || opcode == "SRAI_D") { return value >= 0 && value < 64; }
            return false;
        }
        // RISCV uses target-specific simm12_lo/shamt operand kinds even
        // for numeric ALU operations. Keep exact opcode, operand role and
        // architectural bounds; the same simm12_lo in loads/stores is hidden.
        [[nodiscard]] inline bool numeric_riscv_immediate(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, unsigned index, ::std::int64_t value) noexcept
        {
            if(!triple.isRISCV() || index != 2u) { return false; }
            if(opcode == "ADDI" || opcode == "ADDIW" || opcode == "ANDI" || opcode == "ORI" ||
               opcode == "XORI" || opcode == "SLTI" || opcode == "SLTIU") { return value >= -2048 && value <= 2047; }
            if(opcode == "SLLI" || opcode == "SRLI" || opcode == "SRAI")
            { return value >= 0 && value < (triple.isArch64Bit() ? 64 : 32); }
            if(opcode == "SLLIW" || opcode == "SRLIW" || opcode == "SRAIW") { return value >= 0 && value < 32; }
            return false;
        }
        // MIPS simm16/uimm16/uimm5 operands have UNKNOWN table metadata.
        // Qualify exact numeric opcode/role/range only. Loads, addresses,
        // control, HI/LO/ABI registers and arbitrary unknown operands stay hidden.
        [[nodiscard]] inline bool numeric_mips_immediate(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, unsigned index, ::std::int64_t value) noexcept
        {
            if(!triple.isMIPS()) { return false; }
            if(opcode == "LUi" || opcode == "LUi64")
            { return index == 1u && value >= 0 && value <= 65535; }
            if(index != 2u) { return false; }
            if(opcode == "ADDiu" || opcode == "DADDiu" || opcode == "SLTi" || opcode == "SLTiu" ||
               opcode == "SLTi64" || opcode == "SLTiu64") { return value >= -32768 && value <= 32767; }
            if(opcode == "ANDi" || opcode == "ORi" || opcode == "XORi" ||
               opcode == "ANDi64" || opcode == "ORi64" || opcode == "XORi64")
            { return value >= 0 && value <= 65535; }
            if(opcode == "SLL" || opcode == "SRL" || opcode == "SRA" ||
               opcode == "DSLL32" || opcode == "DSRL32" || opcode == "DSRA32")
            { return value >= 0 && value < 32; }
            if(opcode == "DSLL" || opcode == "DSRL" || opcode == "DSRA")
            { return value >= 0 && value < 64; }
            return false;
        }
        // These three MIPS high-half shifts have empty TD patterns and
        // conservatively inherit hasSideEffects. Their actual complete MC
        // shape is one GPR definition, one GPR input, and an unsigned shift.
        // This bounded numeric exception grants no address/register read.
        [[nodiscard]] inline bool bounded_mips_high_shift(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, ::llvm::MCInst const& instruction,
            ::llvm::MCInstrDesc const& descriptor, ::llvm::MCRegisterInfo const& registers) noexcept
        {
            if(!triple.isMIPS() || (opcode != "DSLL32" && opcode != "DSRL32" && opcode != "DSRA32") ||
               instruction.getFlags()!=0u || instruction.getNumOperands()!=3u || descriptor.getNumOperands()!=3u ||
               descriptor.getNumDefs()!=1u || descriptor.isVariadic() || descriptor.isTerminator() ||
               descriptor.isBarrier() || descriptor.hasDelaySlot() || descriptor.isCall() ||
               descriptor.isReturn() || descriptor.isBranch() || descriptor.isTrap() ||
               descriptor.mayLoad() || descriptor.mayStore() ||
               !descriptor.implicit_defs().empty() || !descriptor.implicit_uses().empty()) { return false; }
            for(unsigned i{};i<2u;++i)
            {
                auto const& value{instruction.getOperand(i)};auto const& info{*(descriptor.operands().begin()+i)};
                if(!value.isReg() || value.getReg()==0u || value.getReg()>=registers.getNumRegs() ||
                   info.OperandType!=::llvm::MCOI::OPERAND_REGISTER || info.isLookupRegClassByHwMode() ||
                   info.RegClass<0 || static_cast<unsigned>(info.RegClass)>=registers.getNumRegClasses() ||
                   !registers.getRegClass(static_cast<unsigned>(info.RegClass)).contains(value.getReg())) { return false; }
            }
            auto const& shift{instruction.getOperand(2u)};
            return (*(descriptor.operands().begin()+2u)).OperandType==::llvm::MCOI::OPERAND_UNKNOWN &&
                shift.isImm() && shift.getImm()>=0 && shift.getImm()<32;
        }
        // ARM mod_imm remains its encoded 12-bit rotation/immediate in MC.
        // No address, shifted-register, memory or unknown opcode is admitted.
        [[nodiscard]] inline bool numeric_arm_immediate(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, unsigned index, ::std::int64_t value) noexcept
        {
            if(!triple.isARM() || triple.isThumb() || value < 0 || value > 4095) { return false; }
            if(opcode == "HINT") { return index == 0u && value == 0; } // exact NOP, never another hint
            if(opcode == "MOVi") { return index == 1u; }
            return index == 2u && (opcode == "ADDri" || opcode == "SUBri" || opcode == "RSBri" ||
                opcode == "ANDri" || opcode == "ORRri" || opcode == "EORri" || opcode == "BICri");
        }
        // DecodePredicateOperand emits AL(14),NoRegister; DecodeCCOutOperand
        // emits NoRegister when this instruction does not write CPSR. These
        // complete table-marked neutral pairs/tails are syntax, not a register
        // value. Conditional CPSR operands and every other zero register fail.
        [[nodiscard]] inline bool neutral_arm_operand(::llvm::Triple const& triple,
            ::llvm::MCInst const& instruction, ::llvm::MCInstrDesc const& descriptor, unsigned index) noexcept
        {
            if(!triple.isARM() || triple.isThumb() || index >= instruction.getNumOperands() ||
               instruction.getNumOperands() != descriptor.getNumOperands()) { return false; }
            auto const& operand{instruction.getOperand(index)};
            auto const& info{*(descriptor.operands().begin() + index)};
            if(info.OperandType != ::llvm::MCOI::OPERAND_UNKNOWN) { return false; }
            if(info.isPredicate())
            {
                if(operand.isImm() && operand.getImm() == 14 && index + 1u < instruction.getNumOperands())
                {
                    auto const& next{instruction.getOperand(index + 1u)};
                    return (*(descriptor.operands().begin() + index + 1u)).isPredicate() && next.isReg() && next.getReg() == 0u;
                }
                if(operand.isReg() && operand.getReg() == 0u && index != 0u)
                {
                    auto const& previous{instruction.getOperand(index - 1u)};
                    return (*(descriptor.operands().begin() + index - 1u)).isPredicate() &&
                        previous.isImm() && previous.getImm() == 14;
                }
            }
            if(info.isOptionalDef() && operand.isReg() && operand.getReg() == 0u &&
               index + 1u == instruction.getNumOperands() && index >= 2u)
            {
                auto const& condition{instruction.getOperand(index - 2u)};
                auto const& predicate{instruction.getOperand(index - 1u)};
                return (*(descriptor.operands().begin() + index - 2u)).isPredicate() &&
                    (*(descriptor.operands().begin() + index - 1u)).isPredicate() &&
                    condition.isImm() && condition.getImm() == 14 && predicate.isReg() && predicate.getReg() == 0u;
            }
            return false;
        }
        // MC table identifiers need not use their assembly aliases. The
        // independent public projection must hide ABI infrastructure in either
        // spelling, including subregisters. This grants no register read.
        [[nodiscard]] inline bool private_register_operand(::llvm::Triple const& triple, ::llvm::StringRef name) noexcept
        {
            if(name == "SP" || name == "FP" || name == "BP" || name == "PC" || name == "LR") { return true; }
            if(triple.isX86())
            {
                return name == "RSP" || name == "ESP" || name == "SPL" || name == "RBP" || name == "EBP" || name == "BPL" ||
                    name == "RIP" || name == "EIP" || name == "IP" || name == "FS" || name == "GS" ||
                    name == "CS" || name == "DS" || name == "ES" || name == "SS" || name == "EFLAGS" || name == "RFLAGS";
            }
            if(triple.isAArch64())
            {
                return name == "WSP" || name == "X18" || name == "W18" || name == "X29" || name == "W29" ||
                    name == "X30" || name == "W30" || name == "NZCV";
            }
            if(triple.isPPC())
            {
                return name == "R1" || name == "X1" || name == "R2" || name == "X2" || name == "R13" || name == "X13" ||
                    name == "R31" || name == "X31" || name == "LR8" || name == "CTR" || name == "CTR8" ||
                    name == "XER" || name.starts_with("CR");
            }
            if(triple.isRISCV())
            {
                return name == "X1" || name == "X2" || name == "X3" || name == "X4" || name == "X8" ||
                    name == "RA" || name == "GP" || name == "TP" || name == "S0" || name == "FCSR";
            }
            if(triple.isMIPS())
            {
                if(name.ends_with("_64")) { name = name.drop_back(3u); }
                return name == "SP" || name == "FP" || name == "RA" || name == "GP" || name == "K0" || name == "K1" ||
                    name == "S8" || name == "HI" || name == "LO" || name == "FCR31";
            }
            if(triple.getArch() == ::llvm::Triple::loongarch64 || triple.getArch() == ::llvm::Triple::loongarch32)
            {
                return name == "R1" || name == "R2" || name == "R3" || name == "R21" || name == "R22" ||
                    name == "RA" || name == "TP" || name == "FCSR0";
            }
            if(triple.getArch() == ::llvm::Triple::sparc || triple.getArch() == ::llvm::Triple::sparcel || triple.getArch() == ::llvm::Triple::sparcv9)
            {
                return name == "G6" || name == "G7" || name == "O6" || name == "O7" || name == "I6" || name == "I7" ||
                    name == "ICC" || name == "XCC" || name == "Y" || name == "FSR";
            }
            if(triple.getArch() == ::llvm::Triple::systemz)
            {
                return name == "R11D" || name == "R11L" || name == "R11H" || name == "R11" ||
                    name == "R14D" || name == "R14L" || name == "R14H" || name == "R14" ||
                    name == "R15D" || name == "R15L" || name == "R15H" || name == "R15" ||
                    name.starts_with("A") || name == "CC" || name == "FPC";
            }
            if(triple.isARM() || triple.isThumb())
            {
                return name == "R11" || name == "R13" || name == "R14" || name == "R15" || name == "CPSR" || name == "FPSCR";
            }
            return false;
        }
        // Vector configuration is a bounded fallthrough operation, not a
        // transfer or a trap-control instruction. The real RV64 MC tables must
        // identify exactly one GPR definition and precisely VL/VTYPE as the
        // implicit definitions. vstart is reset architecturally; no memory or
        // vector-bank access is performed. These effects remain private: this
        // exception never clears the independent public-disassembly veto.
        [[nodiscard]] inline bool bounded_riscv_vector_configuration(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, ::llvm::MCInst const& instruction,
            ::llvm::MCInstrDesc const& descriptor, ::llvm::MCRegisterInfo const& registers) noexcept
        {
            if(triple.getArch()!=::llvm::Triple::riscv64 ||
               (opcode!="VSETIVLI" && opcode!="VSETVLI" && opcode!="VSETVL") ||
               instruction.getFlags()!=0u || instruction.getNumOperands()!=3u ||
               descriptor.getNumOperands()!=3u || descriptor.getNumDefs()!=1u ||
               descriptor.isVariadic() || descriptor.isTerminator() || descriptor.isBarrier() ||
               descriptor.hasDelaySlot() || descriptor.isCall() || descriptor.isReturn() ||
               descriptor.isBranch() || descriptor.isTrap() || descriptor.mayLoad() || descriptor.mayStore() ||
               !descriptor.implicit_uses().empty() || descriptor.implicit_defs().size()!=2u) { return false; }
            bool vl{},vtype{};
            for(auto reg:descriptor.implicit_defs())
            {
                if(reg==0u || reg>=registers.getNumRegs()) { return false; }
                auto const name{::llvm::StringRef{registers.getName(reg)}};
                if(name=="VL" && !vl) { vl=true; }
                else if(name=="VTYPE" && !vtype) { vtype=true; }
                else { return false; }
            }
            if(!vl || !vtype) { return false; }
            for(unsigned i{};i!=3u;++i)
            {
                if((i==1u && opcode=="VSETIVLI") || (i==2u && opcode!="VSETVL")) { continue; }
                auto const& value{instruction.getOperand(i)};
                auto const& info{*(descriptor.operands().begin()+i)};
                if(!value.isReg() || value.getReg()==0u || value.getReg()>=registers.getNumRegs() ||
                   info.OperandType!=::llvm::MCOI::OPERAND_REGISTER || info.isLookupRegClassByHwMode() ||
                   info.RegClass<0 || static_cast<unsigned>(info.RegClass)>=registers.getNumRegClasses() ||
                   !registers.getRegClass(static_cast<unsigned>(info.RegClass)).contains(value.getReg())) { return false; }
            }
            if(opcode=="VSETIVLI")
            {
                auto const& avl{instruction.getOperand(1u)};
                if(!avl.isImm() || avl.getImm()<0 || avl.getImm()>31) { return false; }
            }
            if(opcode!="VSETVL")
            {
                auto const& value{instruction.getOperand(2u)};
                // Standard VTYPE: only E8/E16/E32/E64, nonreserved LMUL,
                // tail/mask policy bits. No high/reserved immediate bits.
                if(!value.isImm() || value.getImm()<0 || value.getImm()>255 ||
                   (value.getImm() & 7)==4 || ((value.getImm()>>3) & 7)>3) { return false; }
            }
            // VSETVL's register-sourced VTYPE stays private. Unsupported
            // settings may produce vill or a guest fault; neither grants a
            // successor outside the immutable owner or a public register read.
            return true;
        }
        // These real MC forms order memory without changing the PC, trap
        // protocol or exclusive monitor. LLVM's empty-pattern descriptors may
        // conservatively mark them unmodelled. Admit only their exact table
        // identities AND complete operand shapes for execution; memory forms
        // and side-effect descriptors remain hidden by public_display_safe.
        [[nodiscard]] inline bool bounded_memory_order_instruction(::llvm::Triple const& triple,
            ::llvm::StringRef opcode, ::llvm::MCInst const& instruction,
            ::llvm::MCInstrDesc const& descriptor) noexcept
        {
            if(instruction.getFlags() != 0u || descriptor.isVariadic() || descriptor.isTerminator() ||
               descriptor.isBarrier() || descriptor.hasDelaySlot() || descriptor.isCall() ||
               descriptor.isReturn() || descriptor.isBranch() || descriptor.isTrap() ||
               !descriptor.implicit_defs().empty() || !descriptor.implicit_uses().empty()) { return false; }
            if(triple.getArch()==::llvm::Triple::sparcv9 && opcode=="MEMBARi")
            {
                // Only the four ordinary memory-order bits used by LLVM's
                // acquire/release fences. Lookaside/MemIssue/Sync and reserved
                // simm13 bits never acquire this single-instruction permission.
                // The complete Success-decoded opcode fixes rd/rs1 and the
                // immediate form; no address, register or memory operand exists.
                return descriptor.getNumDefs()==0u && descriptor.getNumOperands()==1u &&
                    instruction.getNumOperands()==1u && instruction.getOperand(0u).isImm() &&
                    instruction.getOperand(0u).getImm()>=0 && instruction.getOperand(0u).getImm()<=15;
            }
            if(triple.getArch()==::llvm::Triple::loongarch64 && opcode=="DBAR")
            {
                // LLVM emits these exact completion/acquire/release/LL-SC
                // ordering hints. The ordering-only effect has no
                // address, register definition or control transfer. Empty TD
                // patterns may conservatively report loads/stores/side effects;
                // no other miscellaneous/system opcode gets this exception.
                if(descriptor.getNumDefs()!=0u || descriptor.getNumOperands()!=1u ||
                   instruction.getNumOperands()!=1u || !instruction.getOperand(0u).isImm()) { return false; }
                auto const hint{instruction.getOperand(0u).getImm()};
                return hint==0 || hint==0x10 || hint==0x12 || hint==0x14 || hint==0x700;
            }
            if((triple.getArch()==::llvm::Triple::mips64 || triple.getArch()==::llvm::Triple::mips64el) && opcode=="SYNC")
            {
                // Standard N64 code uses SYNC 0 after an acquire load of the
                // actual typed-entry slot. Its sole memory-order effect has no
                // address, register definition or control transfer. Other SYNC
                // types and SYNCI/system instructions receive no exception.
                return descriptor.getNumDefs()==0u && descriptor.getNumOperands()==1u &&
                    instruction.getNumOperands()==1u && instruction.getOperand(0u).isImm() &&
                    instruction.getOperand(0u).getImm()==0;
            }
            if(instruction.getNumOperands()!=2u || descriptor.getNumOperands()!=2u) { return false; }
            if(triple.isAArch64() && (opcode == "LDARB" || opcode == "LDARH" || opcode == "LDARW" || opcode == "LDARX"))
            {
                return descriptor.getNumDefs() == 1u && descriptor.mayLoad() && !descriptor.mayStore() &&
                    instruction.getOperand(0u).isReg() && instruction.getOperand(1u).isReg();
            }
            if(triple.isRISCV() && opcode == "FENCE" && descriptor.getNumDefs() == 0u &&
               !descriptor.mayLoad() && !descriptor.mayStore())
            {
                return instruction.getOperand(0u).isImm() && instruction.getOperand(1u).isImm() &&
                    instruction.getOperand(0u).getImm() >= 0 && instruction.getOperand(0u).getImm() <= 15 &&
                    instruction.getOperand(1u).getImm() >= 0 && instruction.getOperand(1u).getImm() <= 15;
            }
            return false;
        }
# if defined(__APPLE__) && defined(__aarch64__)
        inline constexpr char const* native_triple{"aarch64-apple-darwin"};
# elif defined(__APPLE__) && defined(__x86_64__)
        inline constexpr char const* native_triple{"x86_64-apple-darwin"};
# elif defined(_WIN32)
        inline constexpr char const* native_triple{"x86_64-pc-windows-msvc"};
# elif defined(__linux__) && defined(__aarch64__)
        inline constexpr char const* native_triple{"aarch64-linux-gnu"};
# elif defined(__linux__) && defined(__loongarch64)
        inline constexpr char const* native_triple{"loongarch64-linux-gnu"};
# elif defined(__linux__) && defined(__s390x__)
        inline constexpr char const* native_triple{"s390x-linux-gnu"};
# elif defined(__linux__) && defined(__mips__) && __SIZEOF_POINTER__ == 8
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        inline constexpr char const* native_triple{"mips64el-linux-gnuabi64"};
#  else
        inline constexpr char const* native_triple{"mips64-linux-gnuabi64"};
#  endif
# elif defined(__linux__) && defined(__powerpc64__)
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        inline constexpr char const* native_triple{"powerpc64le-linux-gnu"};
#  else
        inline constexpr char const* native_triple{"powerpc64-linux-gnu"};
#  endif
# elif defined(__linux__) && defined(__powerpc__)
        inline constexpr char const* native_triple{"powerpc-linux-gnu"};
# elif defined(__linux__) && defined(__riscv) && __riscv_xlen == 64
        inline constexpr char const* native_triple{"riscv64-linux-gnu"};
# elif defined(__linux__) && defined(__sparc__) && defined(__arch64__)
        inline constexpr char const* native_triple{"sparc64-linux-gnu"};
# elif defined(__linux__) && defined(__arm__)
        inline constexpr char const* native_triple{"arm-linux-gnueabihf"};
# elif defined(__linux__) && defined(__i386__)
        inline constexpr char const* native_triple{"i686-pc-linux-gnu"};
# else
        inline constexpr char const* native_triple{"x86_64-pc-linux-gnu"};
# endif
    }

    // Cold management-thread decoder only. No requested target, external
    // symbolizer, live code pointer or register context enters this owner.
    // Member destruction is intentionally reverse dependency order: decoder,
    // context, subtarget, assembly and register/descriptor tables. Native
    // execution still requires the caller's separate real stop/owner proof.
    class decoder final
    {
        ::llvm::Triple triple_{};
        ::std::size_t maximum_{15u}, alignment_{1u};
        bool standard_mips_{};
        ::std::unique_ptr<::llvm::MCInstrInfo> instructions_{};
        ::std::unique_ptr<::llvm::MCInstrAnalysis> analysis_{};
        ::std::unique_ptr<::llvm::MCRegisterInfo> registers_{};
        // MCAsmInfo retains a reference to these options. Keep them alive
        // until the disassembler/context/assembly have all been destroyed.
        ::llvm::MCTargetOptions assembly_options_{};
        ::std::unique_ptr<::llvm::MCAsmInfo> assembly_{};
        ::std::unique_ptr<::llvm::MCSubtargetInfo> subtarget_{};
        ::std::unique_ptr<::llvm::MCContext> context_{};
        ::std::unique_ptr<::llvm::MCDisassembler> disassembler_{};
        // All possibly allocating initialization stays inside these helpers.
        // In particular Triple/native strings and std::make_unique<MCContext>
        // must not run in a noexcept initializer before the constructor catch.
        void initialize_default()
        {
            triple_ = ::llvm::Triple{details::native_triple};
            // Share the existing native registry's once gate. This leaf does
            // not register a second competing LLVM target initializer.
            if(!native_disassembly::details::initialize_native()) { return; }
# if defined(__APPLE__) && defined(__aarch64__)
            maximum_ = 4u; alignment_ = 4u;
# endif
            ::std::string error{};
            auto const* target{details::lookup_target(triple_, error)};
            if(target == nullptr) { return; }
            instructions_.reset(target->createMCInstrInfo());
            registers_ = details::make_registers(*target, triple_);
            if(!instructions_ || !registers_) { return; }
            // Use the actual target control-flow analysis where registered.
            analysis_.reset(target->createMCInstrAnalysis(instructions_.get()));
            assembly_ = details::make_assembly(*target, *registers_, triple_, assembly_options_);
            subtarget_ = details::make_subtarget(*target, triple_);
            if(!assembly_ || !subtarget_) { return; }
            standard_mips_ = triple_.isMIPS() && !subtarget_->checkFeatures("+micromips") && !subtarget_->checkFeatures("+mips16");
            maximum_ = details::maximum_instruction_length(*assembly_, *subtarget_, triple_);
            alignment_ = assembly_->getMinInstAlignment();
            if(maximum_ == 0u || maximum_ > native_target_metadata::max_instruction_bytes || alignment_ == 0u ||
               alignment_ > maximum_ || (alignment_ & (alignment_ - 1u)) != 0u) { return; }
            context_ = details::make_context(triple_, *assembly_, *registers_, *subtarget_);
            disassembler_.reset(target->createMCDisassembler(*subtarget_, *context_));
        }
        template<typename Description>
        void initialize_target(Description const& description)
        {
            if(!native_target_metadata::valid(description) || !native_disassembly::details::initialize_native()) { return; }
            // Both inputs are OWNED descriptions, not a requested target capability.
            triple_ = ::llvm::Triple{::llvm::StringRef{description.triple.data(), description.triple_size}};
            ::std::string error{};
            auto const* target{details::lookup_target(triple_, error)};
            if(target == nullptr) { return; }
            instructions_.reset(target->createMCInstrInfo());
            registers_ = details::make_registers(*target, triple_);
            if(!instructions_ || !registers_) { return; }
            analysis_.reset(target->createMCInstrAnalysis(instructions_.get()));
            assembly_ = details::make_assembly(*target, *registers_, triple_, assembly_options_);
            subtarget_ = details::make_subtarget(*target, triple_,
                {description.cpu.data(), description.cpu_size}, {description.features.data(), description.features_size});
            if(!assembly_ || !subtarget_ || assembly_->isLittleEndian() != description.little_endian) { return; }
            standard_mips_ = triple_.isMIPS() && !subtarget_->checkFeatures("+micromips") && !subtarget_->checkFeatures("+mips16");
            auto const actual_maximum{details::maximum_instruction_length(*assembly_, *subtarget_, triple_)};
            if(actual_maximum != description.maximum_instruction_bytes ||
               assembly_->getMinInstAlignment() != description.minimum_instruction_alignment) { return; }
            maximum_ = actual_maximum; alignment_ = description.minimum_instruction_alignment;
            context_ = details::make_context(triple_, *assembly_, *registers_, *subtarget_);
            disassembler_.reset(target->createMCDisassembler(*subtarget_, *context_));
        }
    public:
        decoder() noexcept
        {
# if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            try { initialize_default(); }
            catch(...)
            {
                // Partially constructed MC tables remain RAII-owned. Allocation
                // failure supplies NO decoder/provider rather than terminating
                // at a noexcept std::make_unique call before runtime can refuse.
                disassembler_.reset();
            }
# else
            initialize_default(); // A no-exception LLVM/allocator retains its own OOM contract.
# endif
        }
        template<typename Description>
        explicit decoder(Description const& description) noexcept
        {
# if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
            try { initialize_target(description); }
            catch(...) { disassembler_.reset(); }
# else
            initialize_target(description);
# endif
        }
        decoder(decoder const&) = delete;
        decoder& operator=(decoder const&) = delete;
        decoder(decoder&&) = delete;
        decoder& operator=(decoder&&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return disassembler_ != nullptr; }

        // display_pc affects relative decoding only. It is NEVER converted to
        // a pointer. The bounded owned copy must come from the caller's genuine
        // private code query; this DATA API cannot authenticate that caller.
        [[nodiscard]] decoded_instruction decode(::std::uintptr_t display_pc,
            ::std::span<::std::uint8_t const> owned) noexcept
        {
            if(!disassembler_ || (!analysis_ && triple_.getArch() != ::llvm::Triple::sparcv9) ||
               display_pc == 0u || owned.empty()) { return {}; }
            // RISCV MCAsmInfo inherits an assembler byte alignment of one.
            // Every actual RISCV instruction starts on a halfword boundary,
            // including compressed instructions. Do not decode an odd PC even
            // when that SDK estimate is present in an owned target description.
            if((display_pc & (alignment_ - 1u)) != 0u || (triple_.isRISCV() && (display_pc & 1u) != 0u) ||
               ((triple_.isARM() || triple_.getArch() == ::llvm::Triple::loongarch64 ||
                 triple_.getArch() == ::llvm::Triple::loongarch32 || standard_mips_) && (display_pc & 3u) != 0u)) { return {}; }
            auto const available{::std::min<::std::size_t>(maximum_, owned.size())};
            if(available > UINTPTR_MAX - display_pc) { return {}; }
            ::std::array<::std::uint8_t, native_target_metadata::max_instruction_bytes> bytes{};
            for(::std::size_t index{}; index != available; ++index)
            {
                // [owned ... owned.size) [local bytes ... capacity=32] end
                // [safe                                                     ] index < available <= both extents;
                //  ^^ indexed byte copy cannot read a display PC or retain a borrow.
                bytes[index] = owned[index];
            }
            ::llvm::MCInst instruction{};
            ::std::uint64_t size{};
            // [local OWNED bytes ... available] end
            // [safe                          ] available <= actual bounded target maximum;
            //  ^^ LLVM receives only this bounded synchronous local borrow.
            auto const status{disassembler_->getInstruction(instruction, size,
                ::llvm::ArrayRef<::std::uint8_t>{bytes.data(), available},
                static_cast<::std::uint64_t>(display_pc), ::llvm::nulls())};
            // LLVM's standard N64 LDL/LDR decoder omits the fourth operand:
            // the incoming value tied to the actual output register. Restore
            // only that descriptor-proved identity, never a guessed register
            // or memory address. The normal complete-operand checks below
            // still apply; memory forms have no public disassembly row.
            if(status == ::llvm::MCDisassembler::Success && standard_mips_ && size == 4u &&
               (triple_.getArch() == ::llvm::Triple::mips64 || triple_.getArch() == ::llvm::Triple::mips64el) &&
               instruction.getFlags() == 0u && instruction.getNumOperands() == 3u &&
               instruction.getOpcode() < instructions_->getNumOpcodes())
            {
                auto const& desc{instructions_->get(instruction.getOpcode())};
                auto const name{instructions_->getName(instruction.getOpcode())};
                if((name == "LDL" || name == "LDR") && desc.getNumOperands() == 4u && desc.getNumDefs() == 1u &&
                   !desc.isPseudo() && !desc.isMetaInstruction() && !desc.isVariadic() && desc.mayLoad() && !desc.mayStore() &&
                   !desc.isCall() && !desc.isReturn() && !desc.isBranch() && !desc.isTrap() && !desc.isTerminator() &&
                   !desc.isBarrier() && !desc.hasDelaySlot() && desc.implicit_defs().empty() && desc.implicit_uses().empty() &&
                   desc.getOperandConstraint(3u, ::llvm::MCOI::TIED_TO) == 0 &&
                   instruction.getOperand(0u).isReg() && instruction.getOperand(1u).isReg() && instruction.getOperand(2u).isImm())
                {
                    auto const& output{*(desc.operands().begin())};
                    auto const& input{*(desc.operands().begin() + 3u)};
                    auto const reg{instruction.getOperand(0u).getReg()};
                    if(output.OperandType == ::llvm::MCOI::OPERAND_REGISTER && input.OperandType == ::llvm::MCOI::OPERAND_REGISTER &&
                       !output.isLookupRegClassByHwMode() && !input.isLookupRegClassByHwMode() &&
                       output.RegClass >= 0 && output.RegClass == input.RegClass &&
                       static_cast<unsigned>(output.RegClass) < registers_->getNumRegClasses() &&
                       reg != 0u && reg < registers_->getNumRegs())
                    {
                        // DecodeMem also reports the GPR32 alias for these
                        // 64-bit loads. Resolve only a unique, table-proved
                        // super-register with the identical hardware encoding.
                        unsigned canonical{};
                        for(auto candidate : registers_->getRegClass(static_cast<unsigned>(output.RegClass)))
                        {
                            if(candidate == 0u || candidate >= registers_->getNumRegs() ||
                               registers_->getEncodingValue(candidate) != registers_->getEncodingValue(reg) ||
                               !registers_->isSuperRegisterEq(reg,candidate)) { continue; }
                            if(canonical != 0u) { canonical = 0u; break; }
                            canonical = candidate;
                        }
                        if(canonical != 0u)
                        {
                            instruction.getOperand(0u).setReg(canonical);
                            instruction.addOperand(::llvm::MCOperand::createReg(canonical));
                        }
                    }
                }
            }
            // SoftFail describes disassemblable but architecturally invalid
            // code. Only Success plus checked actual MC tables is usable DATA.
            auto result{native_instruction_semantics::classify(instruction, *instructions_, *registers_, status, size, available)};
            if(!result) { return {}; }
            // A target descriptor may omit a PC definition for system/interrupt
            // instructions. A repeat or lock prefix is also target-specific
            // MCInst state, not evidence of one unambiguous fallthrough step.
            // Refuse these conservatively without opcode/text guessing or an
            // installed-SDK dependency on private X86 flag constants.
            // [actual MC descriptor table ... getNumOpcodes) end
            // [safe                                           ] classify bounded opcode before this indexing;
            //  ^^ neither target-specific flags nor unmodelled effects grant execution.
            auto const& descriptor{instructions_->get(instruction.getOpcode())};
            auto const declared_operands{descriptor.getNumOperands()};
            auto const minimum_operands{descriptor.isVariadic() && declared_operands != 0u ?
                declared_operands - 1u : declared_operands};
            // SystemZ's BRAS/BRASL TD includes an optional assembler TLS
            // marker. Actual machine bytes carry only link register and target.
            // This exact absent third marker cannot admit any ordinary opcode.
            auto const early_name{instructions_->getName(instruction.getOpcode())};
            bool const missing_systemz_tls_marker{triple_.getArch()==::llvm::Triple::systemz &&
                ((early_name=="BRAS" && size==4u) || (early_name=="BRASL" && size==6u)) &&
                descriptor.isCall() && !descriptor.isReturn() && !descriptor.isTrap() &&
                descriptor.getNumDefs()==0u && declared_operands==3u && instruction.getNumOperands()==2u &&
                instruction.getFlags()==0u && instruction.getOperand(0u).isReg() && instruction.getOperand(1u).isImm()};
            if(instruction.getNumOperands() < minimum_operands && !missing_systemz_tls_marker) { return {}; }
            // This is the actual Success-decoded MCInst, with its opcode,
            // operand extent and register IDs checked above. Target overrides
            // may classify control flow from operands rather than TD flags:
            // LoongArch JIRL and RISC-V JALR are examples. Merge only vetoes;
            // false analysis results never clear descriptor restrictions.
            // SPARC has no registered MCInstrAnalysis. Preserve only its
            // existing descriptor and exact bounded branch-table contract;
            // absent analysis for any other supported target still refuses.
            if(analysis_)
            {
                auto const analyzed_return{analysis_->isReturn(instruction)};
                auto const analyzed_call{analysis_->isCall(instruction)};
                auto const analyzed_branch{analysis_->isBranch(instruction)};
                auto const analyzed_conditional{analysis_->isConditionalBranch(instruction)};
                auto const analyzed_indirect{analysis_->isIndirectBranch(instruction)};
                auto const analyzed_barrier{analysis_->isBarrier(instruction)};
                auto const analyzed_terminator{analysis_->isTerminator(instruction)};
                result.conditional_branch = result.conditional_branch || analyzed_conditional;
                result.indirect_branch = result.indirect_branch || analyzed_indirect;
                result.barrier = result.barrier || analyzed_barrier;
                bool const analyzed_control{analyzed_return || analyzed_call || analyzed_branch ||
                    analyzed_conditional || analyzed_indirect || analyzed_barrier || analyzed_terminator};
                result.may_change_pc = result.may_change_pc || analyzed_control;
                if(result.kind == native_instruction_semantics::flow::ordinary ||
                   result.kind == native_instruction_semantics::flow::other_control)
                {
                    if(analyzed_return) { result.kind = native_instruction_semantics::flow::return_instruction; }
                    else if(analyzed_call) { result.kind = native_instruction_semantics::flow::call; }
                    else if(analyzed_branch) { result.kind = native_instruction_semantics::flow::branch; }
                    else if(analyzed_control) { result.kind = native_instruction_semantics::flow::other_control; }
                }
            }
            // MIPS lowers PseudoReturn64 to the ordinary JR encoding; its
            // MC descriptor describes all indirect branches. Recognize only
            // the complete standard N64 JR $ra form as return DATA. Every
            // control/barrier flag survives, so this cannot grant fallthrough
            // stepping, call continuation or a public ABI-register row.
            if(standard_mips_ && size == 4u &&
               (triple_.getArch() == ::llvm::Triple::mips64 || triple_.getArch() == ::llvm::Triple::mips64el) &&
               (early_name == "JR" || early_name == "JR64") &&
               instruction.getFlags() == 0u && instruction.getNumOperands() == 1u &&
               descriptor.getNumOperands() == 1u && descriptor.getNumDefs() == 0u && descriptor.hasDelaySlot() &&
               descriptor.isIndirectBranch() && !descriptor.isCall() && !descriptor.isTrap() &&
               instruction.getOperand(0u).isReg())
            {
                auto const reg{instruction.getOperand(0u).getReg()};
                if(reg != 0u && reg < registers_->getNumRegs())
                {
                    auto const name{::llvm::StringRef{registers_->getName(reg)}};
                    if((name == "RA" || name == "RA_64") && registers_->getEncodingValue(reg) == 31u)
                    { result.kind = native_instruction_semantics::flow::return_instruction; result.may_change_pc = true; }
                }
            }
            bool trap_state_change{};
            // [actual opcode/name table ... getNumOpcodes) names_end
            // [safe                                                ] classify
            //  ^^ proved this opcode before BOTH descriptor and name lookup;
            // LLVM owns the name for the table lifetime. No guest text/PC read.
            trap_state_change = triple_.isX86() && details::changes_native_debug_trap_state(instructions_->getName(instruction.getOpcode()));
            auto const ordered_memory{details::bounded_memory_order_instruction(triple_,
                instructions_->getName(instruction.getOpcode()), instruction, descriptor)};
            auto const bounded_numeric{details::bounded_mips_high_shift(triple_,
                instructions_->getName(instruction.getOpcode()), instruction, descriptor, *registers_)};
            auto const vector_configuration{size==4u && details::bounded_riscv_vector_configuration(triple_,
                instructions_->getName(instruction.getOpcode()), instruction, descriptor, *registers_)};
            // Empty TD patterns conservatively mark PPC NOP as loading,
            // storing and having unknown effects. Its complete fixed encoding
            // plus the Success-decoded operand-free NOP identity proves the
            // architectural no-op; no other opcode/encoding gets this exception.
            bool const ppc_nop_encoding{triple_.isPPC() && size==4u &&
                (triple_.isLittleEndian() ? bytes[0u]==0u && bytes[1u]==0u && bytes[2u]==0u && bytes[3u]==0x60u :
                    bytes[0u]==0x60u && bytes[1u]==0u && bytes[2u]==0u && bytes[3u]==0u)};
            // ARM's compiler-created consuming NOP is HINT #0. Admit
            // only its exact unconditional ARM-state encoding and complete
            // operand shape. YIELD/WFE/WFI/SEV, conditional predicates and
            // every control, implicit-register or memory effect stay refused.
            bool const arm_nop{triple_.isARM() && !triple_.isThumb() && size==4u &&
                bytes[0u]==0u && bytes[1u]==0xf0u && bytes[2u]==0x20u && bytes[3u]==0xe3u &&
                instructions_->getName(instruction.getOpcode())=="HINT" && instruction.getFlags()==0u &&
                instruction.getNumOperands()==3u && descriptor.getNumOperands()==3u && descriptor.getNumDefs()==0u &&
                instruction.getOperand(0u).isImm() && instruction.getOperand(0u).getImm()==0 &&
                instruction.getOperand(1u).isImm() && instruction.getOperand(1u).getImm()==14 &&
                instruction.getOperand(2u).isReg() && instruction.getOperand(2u).getReg()==0u &&
                descriptor.implicit_defs().empty() && descriptor.implicit_uses().empty() &&
                // LLVM marks the entire HINT family as loading/storing and
                // unmodelled. Exact HINT #0 performs no memory/state access;
                // this fixed encoding gets the same bounded NOP exception.
                !descriptor.isTerminator() &&
                !descriptor.isCall() && !descriptor.isBranch() && !descriptor.isReturn() && !descriptor.isTrap() &&
                !descriptor.isBarrier() && !descriptor.hasDelaySlot() && !descriptor.isVariadic() &&
                details::neutral_arm_operand(triple_,instruction,descriptor,1u) &&
                details::neutral_arm_operand(triple_,instruction,descriptor,2u)};
            bool const harmless_nop{arm_nop || ((triple_.getArch() == ::llvm::Triple::sparcv9 || ppc_nop_encoding) &&
                instructions_->getName(instruction.getOpcode()) == "NOP" && result.size == 4u &&
                instruction.getFlags() == 0u && instruction.getNumOperands() == 0u && descriptor.getNumOperands() == 0u &&
                descriptor.implicit_defs().empty() && descriptor.implicit_uses().empty() &&
                (ppc_nop_encoding || (!descriptor.mayLoad() && !descriptor.mayStore())) && !descriptor.isTerminator() &&
                !descriptor.isCall() && !descriptor.isBranch() && !descriptor.isReturn() && !descriptor.isTrap() &&
                !descriptor.isBarrier() && !descriptor.hasDelaySlot())};
            // LLVM X86 IP_HAS_OP_SIZE (bit 0) selects the already decoded
            // operand width. VEX integer/f64 moves also report that bit on
            // i686. It is not REP/LOCK or an interrupt-shadow operation.
            // Admit only this one flag for ordinary instructions; every other
            // prefix bit, control descriptor and trap-state veto remains in
            // force. Actual MC fixtures cover width-only and mixed prefixes.
            bool const benign_width_prefix{triple_.isX86() && instruction.getFlags() == 1u};
            if(result.kind == native_instruction_semantics::flow::ordinary &&
               ((instruction.getFlags() != 0u && !benign_width_prefix) || descriptor.isTerminator() ||
                (triple_.isARM() && instructions_->getName(instruction.getOpcode())=="HINT" && !arm_nop) ||
                (descriptor.hasUnmodeledSideEffects() && !ordered_memory && !harmless_nop && !bounded_numeric && !vector_configuration) || trap_state_change))
            {
                result.kind = native_instruction_semantics::flow::other_control;
                result.may_change_pc = true; // conservative uncertainty, not a resolved branch target
            }
            auto const destination{native_branch_destination::evaluate(triple_, instruction, *instructions_,
                analysis_.get(), result, display_pc)};
            auto const branch_name{instructions_->getName(instruction.getOpcode())};
            bool const delayed_branch{triple_.getArch() == ::llvm::Triple::sparcv9 &&
                result.kind == native_instruction_semantics::flow::branch && descriptor.hasDelaySlot() &&
                // The exact accepted SPARC opcode/operand forms were checked
                // by destination(). Annulled TD forms have empty patterns and
                // inherit LLVM's conservative unmodelled flag. They still have
                // no load/store/call/trap/implicit register definition here.
                instruction.getFlags() == 0u && !descriptor.mayLoad() && !descriptor.mayStore() &&
                !descriptor.isCall() && !descriptor.isReturn() && !descriptor.isTrap() &&
                descriptor.implicit_defs().empty() && !result.indirect_branch && static_cast<bool>(destination)};
            bool const annulled_branch{delayed_branch && (branch_name == "BCONDA" || branch_name == "FBCONDA" || branch_name == "FBCONDA_V9" ||
                branch_name == "BPFCCA" || branch_name == "BPFCCANT" || branch_name == "BPICCA" || branch_name == "BPICCANT" ||
                branch_name == "BPXCCA" || branch_name == "BPXCCANT" || branch_name == "BPRA" || branch_name == "BPRANT")};
            bool const delayed_branch_pair{triple_.isMIPS() &&
                result.kind == native_instruction_semantics::flow::branch && descriptor.hasDelaySlot() &&
                instruction.getFlags() == 0u && !descriptor.hasUnmodeledSideEffects() &&
                !descriptor.mayLoad() && !descriptor.mayStore() && !descriptor.isCall() && !descriptor.isReturn() &&
                !descriptor.isTrap() && !result.indirect_branch && static_cast<bool>(destination)};
            bool const direct_branch_step_safe{result.kind == native_instruction_semantics::flow::branch &&
                instruction.getFlags() == 0u && (!descriptor.hasUnmodeledSideEffects() || delayed_branch) && !trap_state_change &&
                (!descriptor.hasDelaySlot() || delayed_branch || delayed_branch_pair) && !result.indirect_branch && static_cast<bool>(destination)};
            bool call_continuation_safe{};
            ::std::size_t call_register_index{SIZE_MAX};
            call_memory_operand call_memory{};
            // [actual MC opcode/name table ... getNumOpcodes) names_end
            // [safe                                                 ] classify
            //  ^^ checked the opcode. Only these decoded near CALL64 forms keep
            // the LP64 continuation ABI; no far/system/tail call or text guess.
            auto const actual_name{instructions_->getName(instruction.getOpcode())};
            call_continuation_safe = triple_.getArch() == ::llvm::Triple::x86_64 &&
                result.kind == native_instruction_semantics::flow::call &&
                instruction.getFlags() == 0u && !trap_state_change && !descriptor.isReturn() &&
                !descriptor.isBranch() && !descriptor.isTerminator() && !descriptor.isBarrier() &&
                (actual_name == "CALL64pcrel32" || actual_name == "CALL64r" || actual_name == "CALL64m");
            if(call_continuation_safe && actual_name == "CALL64r" &&
               instruction.getNumOperands() == 1u && descriptor.getNumOperands() == 1u)
            {
                // [actual decoded MC operands: exactly one] operands_end
                // [safe] complete owned decode and descriptor count agree BEFORE
                //  ^^ selecting operand0; no implicit or memory operand is read.
                auto const& operand{instruction.getOperand(0u)};
                if(operand.isReg())
                {
                    auto const reg{operand.getReg()};
                    if(reg != 0u && reg < registers_->getNumRegs())
                    {
                        // [actual native MC register-name table 0 ... N) end
                        // [safe] reg<N BEFORE this name-table lookup. LLVM owns
                        //  ^^ the immutable name for this cold decoder's lifetime.
                        auto const name{::llvm::StringRef{registers_->getName(reg)}};
                        constexpr ::llvm::StringLiteral names[]{"RAX","RBX","RCX","RDX","RSI","RDI","RBP","RSP",
                            "R8","R9","R10","R11","R12","R13","R14","R15"};
                        for(::std::size_t index{}; index != 16u; ++index)
                        {
                            // [fixed complete GPR-name DATA table 0 ... 16) end
                            // [safe] index<16 BEFORE selecting this DATA identity;
                            //  ^^ this records an index, NEVER a native value/read.
                            if(name == names[index]) { call_register_index = index; break; }
                        }
                    }
                }
            }
            if(call_continuation_safe && actual_name=="CALL64m" &&
               instruction.getNumOperands()==5u && descriptor.getNumOperands()==5u)
            {
                auto const& base{instruction.getOperand(0u)};auto const& scale{instruction.getOperand(1u)};
                auto const& index{instruction.getOperand(2u)};auto const& displacement{instruction.getOperand(3u)};
                auto const& segment{instruction.getOperand(4u)};
                auto const map_register{[&](unsigned reg,::std::size_t& out,bool permit_rip) noexcept -> bool
                {
                    if(reg==0u) { out=SIZE_MAX;return true; }
                    if(reg>=registers_->getNumRegs()) { return false; }
                    auto const name{::llvm::StringRef{registers_->getName(reg)}};
                    if(permit_rip && name=="RIP") { call_memory.rip_relative=true;out=SIZE_MAX;return true; }
                    constexpr ::llvm::StringLiteral names[]{"RAX","RBX","RCX","RDX","RSI","RDI","RBP","RSP",
                        "R8","R9","R10","R11","R12","R13","R14","R15"};
                    for(::std::size_t n{};n!=16u;++n) { if(name==names[n]) { out=n;return true; } }
                    return false; // 32-bit/address overrides, TLS/segments and unknown register classes
                }};
                if(base.isReg() && scale.isImm() && index.isReg() && displacement.isImm() &&
                   segment.isReg() && segment.getReg()==0u &&
                   (scale.getImm()==1 || scale.getImm()==2 || scale.getImm()==4 || scale.getImm()==8) &&
                   map_register(base.getReg(),call_memory.base,true) && map_register(index.getReg(),call_memory.index,false))
                {
                    call_memory.displacement=displacement.getImm();
                    call_memory.scale=static_cast<::std::uint8_t>(scale.getImm());call_memory.valid=true;
                }
            }
            // SPARC decodes indirect calls as general JMPL. LLVM's exact
            // rd=%o7 encoding is the returning ABI call form; rd=%g0 stays a
            // transfer/return. No target register value is inspected here.
            auto const sparc_name{instructions_->getName(instruction.getOpcode())};
            if(triple_.getArch()==::llvm::Triple::sparcv9 && size==4u && instruction.getFlags()==0u &&
               (sparc_name=="JMPLrr" || sparc_name=="JMPLri") && descriptor.hasDelaySlot() &&
               descriptor.getNumDefs()==1u && !descriptor.isReturn() && !descriptor.isTrap() &&
               instruction.getNumOperands()==3u && instruction.getOperand(0u).isReg())
            {
                auto const rd{instruction.getOperand(0u).getReg()};
                if(rd!=0u && rd<registers_->getNumRegs() && ::llvm::StringRef{registers_->getName(rd)}=="O7")
                { result.kind=native_instruction_semantics::flow::call; }
            }
            // Returning ABI calls only. This identifies an owned caller's
            // fallthrough; it never grants authority to the opaque callee.
            if(!triple_.isX86() && instruction.getFlags()==0u && !trap_state_change &&
               result.kind==native_instruction_semantics::flow::call &&
               !descriptor.isReturn() && !descriptor.isTrap())
            {
                auto const name{instructions_->getName(instruction.getOpcode())};
                auto const first_is{[&](::llvm::StringRef expected) noexcept
                {
                    if(instruction.getNumOperands()==0u || !instruction.getOperand(0u).isReg()) { return false; }
                    auto const reg{instruction.getOperand(0u).getReg()};
                    return reg!=0u && reg<registers_->getNumRegs() &&
                        ::llvm::StringRef{registers_->getName(reg)}==expected;
                }};
                switch(triple_.getArch())
                {
                    case ::llvm::Triple::aarch64:
                    case ::llvm::Triple::aarch64_be:
                        call_continuation_safe=(name=="BL" || name=="BLR") && size==4u; break;
                    case ::llvm::Triple::arm:
                    case ::llvm::Triple::armeb:
                        call_continuation_safe=(name=="BL" || name=="BL_pred" || name=="BLX" || name=="BLX_pred") && size==4u; break;
                    case ::llvm::Triple::ppc:
                    case ::llvm::Triple::ppcle:
                    case ::llvm::Triple::ppc64:
                    case ::llvm::Triple::ppc64le:
                        call_continuation_safe=(name=="BL" || name=="BL8" || name=="BLA" || name=="BLA8" ||
                            name=="BCTRL" || name=="BCTRL8" || name=="BLRL" || name=="BLRL8") && size==4u; break;
                    case ::llvm::Triple::mips64:
                    case ::llvm::Triple::mips64el:
                        call_continuation_safe=(name=="JAL" || name=="JAL64" ||
                            ((name=="JALR" || name=="JALR64") && (first_is("RA") || first_is("RA_64")))) &&
                            size==4u && descriptor.hasDelaySlot(); break;
                    case ::llvm::Triple::riscv64:
                        call_continuation_safe=((name=="JAL" || name=="JALR") && first_is("X1")) || name=="C_JALR"; break;
                    case ::llvm::Triple::loongarch64:
                        call_continuation_safe=name=="BL" || (name=="JIRL" && first_is("R1")); break;
                    case ::llvm::Triple::sparcv9:
                        call_continuation_safe=(name=="CALL" || name=="CALLrr" || name=="CALLri" ||
                            ((name=="JMPLrr" || name=="JMPLri") && first_is("O7"))) &&
                            size==4u && descriptor.hasDelaySlot(); break;
                    case ::llvm::Triple::systemz:
                        call_continuation_safe=(name=="BRAS" || name=="BRASL" || name=="BASR") && first_is("R14D"); break;
                    default: break;
                }
            }
            if(triple_.getArch()==::llvm::Triple::x86 && instruction.getFlags()==0u &&
               result.kind==native_instruction_semantics::flow::call && !trap_state_change &&
               !descriptor.isReturn() && !descriptor.isBranch() && !descriptor.isTerminator() && !descriptor.isBarrier())
            {
                auto const name{instructions_->getName(instruction.getOpcode())};
                call_continuation_safe=name=="CALLpcrel32" || name=="CALL32r" || name=="CALL32m";
            }
            if(call_continuation_safe && triple_.isX86())
            {
                // MC can discard an ignored segment prefix on register CALL.
                // Preserve the same conservative prefix policy for every near
                // call, including opaque NI; an MC flag of zero is insufficient.
                for(::std::size_t prefix{};prefix<size;++prefix)
                {
                    auto const byte{bytes[prefix]};
                    if(byte==0x26u || byte==0x2eu || byte==0x36u || byte==0x3eu || byte==0x64u || byte==0x65u ||
                       byte==0x66u || byte==0x67u || byte==0xf0u || byte==0xf2u || byte==0xf3u)
                    { call_continuation_safe=false;break; }
                    if(triple_.getArch()==::llvm::Triple::x86_64 && byte>=0x40u && byte<=0x4fu) { continue; }
                    break; // actual CALL opcode, never scan its operand bytes
                }
            }
            bool public_display_safe{(result.kind == native_instruction_semantics::flow::ordinary || direct_branch_step_safe) &&
                !(triple_.getArch()==::llvm::Triple::loongarch64 && ordered_memory) &&
                (!triple_.isARM() || instructions_->getName(instruction.getOpcode())!="HINT" || arm_nop) &&
                (instruction.getFlags() == 0u || benign_width_prefix) &&
                ((!descriptor.mayLoad() && !descriptor.mayStore()) || harmless_nop) &&
                (!descriptor.hasUnmodeledSideEffects() || harmless_nop || bounded_numeric) && !descriptor.hasDelaySlot() && !trap_state_change &&
                !descriptor.isVariadic() && instruction.getNumOperands() == descriptor.getNumOperands()};
            if(public_display_safe && triple_.isPPC())
            {
                // Native ABI call setup writes CTR/LR implicitly. An explicit
                // numeric-looking source operand cannot turn that scaffolding
                // into public Wasm arithmetic, nor release the hidden link state.
                for(auto reg:descriptor.implicit_defs())
                {
                    if(reg==0u || reg>=registers_->getNumRegs()) { public_display_safe=false;break; }
                    auto const name{::llvm::StringRef{registers_->getName(reg)}};
                    if(name=="CTR" || name=="CTR8" || name=="LR" || name=="LR8") { public_display_safe=false;break; }
                }
            }
            for(unsigned index{}; public_display_safe && index != instruction.getNumOperands(); ++index)
            {
                auto const& operand{instruction.getOperand(index)};
                auto const& info{*(descriptor.operands().begin() + index)};
                auto const type{info.OperandType};
                bool const shifted_register{operand.isReg() && type >= ::llvm::MCOI::OPERAND_FIRST_TARGET && index == 2u &&
                    details::numeric_shift_operand(triple_, instructions_->getName(instruction.getOpcode()), 3u, 0) &&
                    !info.isLookupRegClassByHwMode() && info.RegClass >= 0 &&
                    static_cast<unsigned>(info.RegClass) < registers_->getNumRegClasses() &&
                    registers_->getRegClass(static_cast<unsigned>(info.RegClass)).contains(operand.getReg())};
                bool const numeric_shift{operand.isImm() && type >= ::llvm::MCOI::OPERAND_FIRST_TARGET &&
                    details::numeric_shift_operand(triple_, instructions_->getName(instruction.getOpcode()), index, operand.getImm())};
                bool const neutral_arm{details::neutral_arm_operand(triple_, instruction, descriptor, index)};
                public_display_safe = (operand.isReg() || operand.isImm()) &&
                    (type == ::llvm::MCOI::OPERAND_REGISTER || type == ::llvm::MCOI::OPERAND_IMMEDIATE ||
                     shifted_register || numeric_shift || neutral_arm ||
                     (operand.isImm() && type >= ::llvm::MCOI::OPERAND_FIRST_TARGET &&
                      details::numeric_riscv_immediate(triple_, instructions_->getName(instruction.getOpcode()), index, operand.getImm())) ||
                     (operand.isImm() && type == ::llvm::MCOI::OPERAND_UNKNOWN &&
                      details::numeric_mips_immediate(triple_, instructions_->getName(instruction.getOpcode()), index, operand.getImm())) ||
                     (operand.isImm() && type == ::llvm::MCOI::OPERAND_UNKNOWN &&
                      details::numeric_arm_immediate(triple_, instructions_->getName(instruction.getOpcode()), index, operand.getImm())) ||
                     (operand.isImm() && type == ::llvm::MCOI::OPERAND_UNKNOWN &&
                      details::numeric_loongarch_immediate(triple_, instructions_->getName(instruction.getOpcode()), index, operand.getImm())) ||
                     (direct_branch_step_safe && operand.isImm() && type != ::llvm::MCOI::OPERAND_MEMORY));
                if(operand.isImm() && !direct_branch_step_safe &&
                   (operand.getImm() < INT32_MIN || operand.getImm() > UINT32_MAX)) { public_display_safe = false; }
                if(operand.isReg())
                {
                    auto const reg{operand.getReg()};
                    if(reg == 0u && neutral_arm) { continue; }
                    if(reg == 0u || reg >= registers_->getNumRegs()) { public_display_safe = false; continue; }
                    auto const name{::llvm::StringRef{registers_->getName(reg)}};
                    if(details::private_register_operand(triple_, name))
                    { public_display_safe = false; }
                }
            }
            auto decoded{decoded_instruction{result, destination, direct_branch_step_safe, call_continuation_safe,
                call_register_index, public_display_safe, delayed_branch, annulled_branch, call_memory, delayed_branch_pair}};
            if(call_continuation_safe && descriptor.hasDelaySlot()) { decoded.call_delay_bytes_=4u; }
            if((triple_.getArch() == ::llvm::Triple::x86_64 || triple_.getArch() == ::llvm::Triple::x86) &&
               instruction.getFlags() == 0u && result.kind == native_instruction_semantics::flow::return_instruction)
            {
                auto const opcode{instructions_->getName(instruction.getOpcode())};
                bool const x86{triple_.getArch() == ::llvm::Triple::x86};
                if(opcode == (x86 ? "RET32" : "RET64") && instruction.getNumOperands() == 0u) { decoded.near_return_pop_ = 0u; }
                else if(opcode == (x86 ? "RETI32" : "RETI64") && instruction.getNumOperands() == 1u && instruction.getOperand(0u).isImm())
                {
                    auto const amount{instruction.getOperand(0u).getImm()};
                    // X86 MC sign-extends its encoded imm16. RET interprets the
                    // actual 16-bit field as an unsigned stack-pop amount.
                    if(amount >= INT16_MIN && amount <= UINT16_MAX)
                    { decoded.near_return_pop_ = static_cast<::std::uint16_t>(amount); }
                }
            }
            return decoded;
        }
    };
#else
    // A no-LLVM build has no MC provider. This fallback never acquires an
    // instruction-stepping capability by including a DATA leaf.
    class decoder final
    {
    public:
        constexpr decoder() noexcept = default;
        template<typename Description> explicit constexpr decoder(Description const&) noexcept {}
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return false; }
        [[nodiscard]] constexpr decoded_instruction decode(::std::uintptr_t,
            ::std::span<::std::uint8_t const>) noexcept { return {}; }
    };
#endif
}
