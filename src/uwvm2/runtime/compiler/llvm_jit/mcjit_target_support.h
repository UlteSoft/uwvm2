/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)           *
 * Licensed under the APL-2.0 License (see LICENSE file).     *
 *************************************************************/

#pragma once
#include <type_traits>
#if defined(__linux__) && defined(__powerpc__)
# include <sys/auxv.h>
#endif
#include <llvm/Config/llvm-config.h>
#if defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT) || defined(LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT)
# include <llvm/ExecutionEngine/RuntimeDyld.h>
#endif
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/TargetParser/Triple.h>

namespace uwvm2::runtime::compiler::llvm_jit::details
{
    // Non-default ARM DWARF builds must ask LLVM for the host unwind ABI before
    // TargetMachine constructs MCAsmInfo. Otherwise Linux's default EHABI can
    // produce .ARM.exidx in a process expecting registered DWARF frames.
    template<typename engine_builder_type>
    inline void llvm_jit_mcjit_configure_host_unwind_abi(engine_builder_type& builder)
    {
#if (defined(__arm__) || defined(__thumb__)) && defined(__ARM_DWARF_EH__)
        ::llvm::TargetOptions options{};
        options.ExceptionModel = ::llvm::ExceptionHandling::DwarfCFI;
        builder.setTargetOptions(options);
#elif defined(__linux__) && __SIZEOF_POINTER__ == 8 && LLVM_VERSION_MAJOR >= 23 && \
    ((defined(__loongarch64) && defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) && \
        UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT == 1) || \
     (defined(__mips__) && defined(__mips64) && defined(__mips_isa_rev) && __mips_isa_rev == 2 && \
        !defined(__mips16) && !defined(__mips_micromips) && defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && \
        UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1))
        // MCJIT defaults this pass off. Ordinary N64 R2 needs the producer's
        // async-CFI repair too. Its owner-table opt-in also works while the
        // physical continuation gate is closed; this grants no caller authority.
        // Async epilogue rows must not leak
        // their restored CFA/register state into a later laid-out body or
        // landing pad. LLVM inserts genuine remember/restore-state rows;
        // the private debugger still rejects unknown or unowned callers.
        ::llvm::TargetOptions options{};
        options.EnableCFIFixup = true;
        builder.setTargetOptions(options);
#else
        static_cast<void>(builder);
#endif
    }

    // A full PPC64 JIT module uses separate retained function sections. The
    // small code model permits distinct TOCs for these sections and therefore
    // cannot prove a direct musttail shares a TOC. Medium explicitly promises
    // one module TOC and keeps LLVM's strong-definition/DSO-local guards intact.
    // This grants no indirect-call or foreign-TOC exception.
    template<typename engine_builder_type>
    inline void llvm_jit_mcjit_configure_code_model(engine_builder_type& builder,
        ::llvm::Triple const& triple)
    {
        if(triple.isOSBinFormatELF() &&
           (triple.getArch() == ::llvm::Triple::ppc64 || triple.getArch() == ::llvm::Triple::ppc64le))
        {
            // Big-endian PPC64 defaults to PIC in LLVM. The qualified native
            // TOC-tail ABI requires static global-entry calls, as exercised by
            // its real direct/indirect/cross-engine musttail oracle. Explicitly
            // match that ABI here; this helper is only for in-process MCJIT.
            builder.setRelocationModel(::llvm::Reloc::Static);
            builder.setCodeModel(::llvm::CodeModel::Medium);
        }
    }

    // getHostCPUName() may report generic under user-mode emulation or an
    // unrecognized Linux cpuinfo. LLVM's explicit SPARC generic CPU selects
    // V8; the SPARC V9 process ABI requires the baseline V9 instructions.
    // Normalize only auto-detected generic/empty host names, never explicit
    // cross-AOT CPU requests or other architectures' feature policy.
    [[nodiscard]] inline ::llvm::StringRef llvm_jit_abi_host_cpu_name(
        ::llvm::Triple const& triple, ::llvm::StringRef detected) noexcept
    {
        if(triple.getArch() == ::llvm::Triple::sparcv9 && (detected.empty() || detected == "generic")) { return "v9"; }
#if defined(__linux__) && defined(__mips__) && defined(__mips64) && defined(__MIPSEL__) && __SIZEOF_POINTER__ == 8 && \
    defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)
        // This native process was compiled for N64EL R2. A generic cpuinfo
        // result must not lower its JIT subtarget to pre-R2 MIPS64, which also
        // prevents the qualified TailCC backend from selecting its ABI.
        if(triple.getArch() == ::llvm::Triple::mips64el && (detected.empty() || detected == "generic")) { return "mips64r2"; }
#endif
        return detected;
    }

    // PPC CPU-name detection reads procfs, which user-mode emulators may
    // expose from a different host ISA. LLVM's PPC host feature map is empty.
    // For an auto-detected generic CPU, use the process's actual Linux HWCAP
    // vector capabilities rather than scalarizing live Wasm v128 values.
    // This native-only helper grants no capability from compiler macros or
    // requested cross targets, and leaves recognized CPU defaults intact.
    template<typename attribute_storage>
    inline void llvm_jit_append_native_host_vector_features(::llvm::Triple const& triple,
        ::llvm::StringRef detected_cpu, attribute_storage& attributes)
    {
#if defined(__linux__) && defined(__powerpc__)
        auto const arch{triple.getArch()};
        if(triple.isOSLinux() && (arch == ::llvm::Triple::ppc || arch == ::llvm::Triple::ppc64 ||
            arch == ::llvm::Triple::ppc64le) && (detected_cpu.empty() || detected_cpu == "generic"))
        {
            auto const hwcap{::getauxval(AT_HWCAP)};
            attributes.emplace_back((hwcap & 0x10000000ul) != 0u ? "+altivec" : "-altivec");
            attributes.emplace_back((hwcap & 0x00000080ul) != 0u ? "+vsx" : "-vsx");
        }
#else
        static_cast<void>(triple);static_cast<void>(detected_cpu);static_cast<void>(attributes);
#endif
    }

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

    template<typename subtarget_type>
    [[nodiscard]] inline bool llvm_jit_mcjit_sparcv9_features_supported(subtarget_type&& subtarget) noexcept
    {
        if constexpr(llvm_jit_mcjit_pointer<subtarget_type>)
        { return subtarget != nullptr && subtarget->checkFeatures("+v9"); }
        else { return subtarget.checkFeatures("+v9"); }
    }

    [[nodiscard]] inline bool llvm_jit_mcjit_subtarget_supported(::llvm::TargetMachine const& machine) noexcept
    {
        if(machine.getTargetTriple().getArch() == ::llvm::Triple::sparcv9)
        { return llvm_jit_mcjit_sparcv9_features_supported(machine.getMCSubtargetInfo()); }
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

    [[nodiscard]] inline bool llvm_jit_mcjit_needs_unique_temp_labels(::llvm::Triple const& triple) noexcept
    {
        // External LLVM 22/23 RuntimeDyld resolves ELF locals through a name
        // map. RISC-V/LoongArch object writers may emit repeated ".L0 " names,
        // so the first function's FDE incorrectly names the last function.
        // Retain uniquely named temporaries on these targets until the external
        // loader can be assumed fixed. Only object metadata grows; no logical
        // stack tracking or extra hot-path instructions are introduced.
        // ROS instead fixes exact symbol-entry resolution in its pinned loader.
        auto const arch{triple.getArch()};
        return triple.isOSBinFormatELF() &&
               (arch == ::llvm::Triple::riscv32 || arch == ::llvm::Triple::riscv64 || arch == ::llvm::Triple::loongarch64);
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
                // Stock ELF Thumb calls and PPC32 calls/EH frames are
                // incomplete. PPC32/SPARC V9 require the revision-11 producer
                // capability exported by the patched SDK, not a version guess.
                // ARM BE data relocations still use LE writes. Reject these
                // native paths rather than treating successful AOT as safety.
                // Keep Mach-O/COFF checks separate: their resolvers differ.
#if defined(__powerpc64__)
                if(arch == ::llvm::Triple::ppc64 || arch == ::llvm::Triple::ppc64le)
                {
# if defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT)
                    return ::llvm::RuntimeDyld::getUWVMPPC64TOCTailABI() == 4u &&
                           ::llvm::RuntimeDyld::getUWVMPPC64TOCRelativeRelocations() == 1u;
# else
                    return false;
# endif
                }
#endif
                return triple.isX86() || arch == ::llvm::Triple::arm ||
                       // ELF ILP32 uses R_AARCH64_P32_* relocations, absent
                       // from RuntimeDyldELF's AArch64 resolver. A trivial
                       // relocation-free object can still emit successfully;
                       // it is not a valid native loader capability probe.
                       // Mach-O's aarch64_32 below is a different object ABI.
                       // Unpatched LLVM 22/23 RuntimeDyld writes AArch64 BE
                       // far-call stubs using data endianness, corrupting the
                       // always-LE instructions. Ordinary UWVM uses external
                       // LLVM, unlike ROS's documented downstream loader fix.
                       // Do not silently assume that patch is installed.
                       (arch == ::llvm::Triple::aarch64 &&
                        triple.getEnvironment() != ::llvm::Triple::GNUILP32) ||
                       triple.isMIPS() || arch == ::llvm::Triple::ppc64 || arch == ::llvm::Triple::ppc64le || triple.isBPF() ||
                       arch == ::llvm::Triple::loongarch64 || arch == ::llvm::Triple::systemz ||
                       arch == ::llvm::Triple::riscv32 || arch == ::llvm::Triple::riscv64
#if defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT) && LLVM_UWVM_ROS_ELF_PPC32_MCJIT == 1
                       || (arch == ::llvm::Triple::ppc &&
                           (::llvm::RuntimeDyld::getUWVMELFLoaderCapabilities() & 1u) != 0u)
#endif
#if defined(LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT) && LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT == 1
                       || (arch == ::llvm::Triple::sparcv9 &&
                           (::llvm::RuntimeDyld::getUWVMELFLoaderCapabilities() & 2u) != 0u)
#endif
                       ;
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
