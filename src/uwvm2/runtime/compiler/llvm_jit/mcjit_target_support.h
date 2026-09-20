/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)           *
 * Licensed under the APL-2.0 License (see LICENSE file).     *
 *************************************************************/

#pragma once
#include <type_traits>
#include <llvm/Config/llvm-config.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>

namespace uwvm2::runtime::compiler::llvm_jit::details
{
    template<typename type>
    concept llvm_jit_mcjit_pointer = ::std::is_pointer_v<::std::remove_cvref_t<type>>;

    template<typename subtarget_type>
    [[nodiscard]] inline bool llvm_jit_mcjit_subtarget_features_supported(subtarget_type&& subtarget) noexcept
    {
        if constexpr(llvm_jit_mcjit_pointer<subtarget_type>)
        {
            if(subtarget == nullptr) { return false; }
            return !subtarget->checkFeatures("+micromips") && !subtarget->checkFeatures("+mips16");
        }
        else
        {
            return !subtarget.checkFeatures("+micromips") && !subtarget.checkFeatures("+mips16");
        }
    }

    [[nodiscard]] inline bool llvm_jit_mcjit_subtarget_supported(::llvm::TargetMachine const& machine) noexcept
    {
        if(!machine.getTargetTriple().isMIPS()) { return true; }
        // RuntimeDyld's MIPS relocator implements standard MIPS instruction
        // encodings, not R_MICROMIPS_* / R_MIPS16_* relocations. Architecture-
        // wide hasJIT() is insufficient: compressed-mode objects may abort in
        // the loader (or fail object emission) despite registering the target.
        // Inspect effective features, including CPU defaults and +/- ordering;
        // never silently generate standard MIPS for a compressed-only CPU.
        // This is a native-loader gate, NOT a cross-AOT inventory filter.
        return llvm_jit_mcjit_subtarget_features_supported(machine.getMCSubtargetInfo());
    }

    // A Target's hasJIT flag is architecture-wide, not an object-loader check.
    // RuntimeDyld (used by MCJIT, not ORC/JITLink) accepts ELF, Mach-O and COFF;
    // its Mach-O/COFF factories support only the architectures below. For
    // example PowerPC advertises JIT support, but AIX XCOFF and PowerPC Mach-O
    // reach fatal unsupported-format/CPU paths. Reject before creating/emitting
    // native code; offline AOT emission must NOT use this capability gate.
    //
    // ARM Thumb objects identify as ARM in Mach-O, and Windows ARMNT objects
    // identify as Thumb in COFF, hence accept either source-triple spelling.
    // This mirrors the LLVM 22/23 RuntimeDyld factories, not a promise that all
    // relocations, CFI or host ABIs work. Re-audit it when changing that loader.
    [[nodiscard]] inline bool llvm_jit_mcjit_object_format_supported(::llvm::Triple const& triple) noexcept
    {
        auto const arch{triple.getArch()};
        switch(triple.getObjectFormat())
        {
            case ::llvm::Triple::ELF:
                // Mirror RuntimeDyldELF's relocation dispatch (MIPS has its
                // own path). Its BPF relocator exists, but the separate native
                // selection gate rejects that bytecode execution ABI.
                // The relocation dispatch also names Thumb/PPC32, but those
                // are NOT complete loaders: ELF Thumb calls are unimplemented;
                // PPC32 lacks REL24/REL32 used by calls and .eh_frame. ARM BE
                // data relocations still use little-endian writes. Reject these
                // native paths rather than treating successful AOT as safety.
                // Keep Mach-O/COFF checks separate: their resolvers differ.
                return triple.isX86() || arch == ::llvm::Triple::arm ||
                       // ELF ILP32 uses R_AARCH64_P32_* relocations, absent
                       // from RuntimeDyldELF's AArch64 resolver. A trivial
                       // relocation-free object can still emit successfully;
                       // it is not a valid native loader capability probe.
                       // Mach-O's aarch64_32 below is a different object ABI.
                       // BE LP64 additionally relies on ROS .5's explicit LE
                       // RuntimeDyld instruction/stub writes. Ordinary UWVM
                       // cannot assume its external LLVM contains that patch
                       // and intentionally keeps a narrower gate here.
                       ((arch == ::llvm::Triple::aarch64 || arch == ::llvm::Triple::aarch64_be) &&
                        triple.getEnvironment() != ::llvm::Triple::GNUILP32) ||
                       triple.isMIPS() || arch == ::llvm::Triple::ppc64 || arch == ::llvm::Triple::ppc64le || triple.isBPF() ||
                       arch == ::llvm::Triple::loongarch64 || arch == ::llvm::Triple::systemz ||
                       arch == ::llvm::Triple::riscv32 || arch == ::llvm::Triple::riscv64;
            case ::llvm::Triple::MachO:
                return arch == ::llvm::Triple::arm || arch == ::llvm::Triple::thumb ||
                       arch == ::llvm::Triple::aarch64 || arch == ::llvm::Triple::aarch64_32 ||
                       arch == ::llvm::Triple::x86 || arch == ::llvm::Triple::x86_64;
            case ::llvm::Triple::COFF:
                return arch == ::llvm::Triple::arm || arch == ::llvm::Triple::thumb ||
                       arch == ::llvm::Triple::aarch64 ||
                       arch == ::llvm::Triple::x86 || arch == ::llvm::Triple::x86_64;
            default:
                return false;
        }
    }
}
