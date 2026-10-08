/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "native_arm_ehabi_loader_abi.h"
# include <cstdint>
# include <memory>
# include <mutex>
# include <type_traits>
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ADT/SmallVector.h>
#  include <llvm/ADT/StringRef.h>
#  include <llvm/IR/CallingConv.h>
#  include <llvm/IR/Comdat.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/DerivedTypes.h>
#  include <llvm/IR/Function.h>
#  include <llvm/IR/GlobalVariable.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/Instructions.h>
#  include <llvm/IR/Module.h>
#  include <llvm/MC/MCAsmInfo.h>
#  include <llvm/Support/CodeGen.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Triple.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if defined(_WIN64) && defined(__SEH__) && defined(__GXX_ABI_VERSION) && !defined(__CYGWIN__) && \
    !defined(__arm64ec__) && !defined(_M_ARM64EC) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
#  define UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH 1
# elif defined(UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT) && \
    UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT == 1 && \
    defined(_WIN32) && defined(_WIN64) && defined(__MINGW32__) && defined(__SEH__) && \
    defined(__GXX_ABI_VERSION) && (defined(__cpp_exceptions) || defined(__EXCEPTIONS)) && \
    !defined(__USING_SJLJ_EXCEPTIONS__) && !defined(_MSC_VER) && !defined(__CYGWIN__) && \
    !defined(__arm64ec__) && !defined(_M_ARM64EC) && \
    (defined(__aarch64__) || defined(_M_ARM64)) && \
    __has_include(<cxxabi.h>) && __has_include(<unwind.h>)
// An exact-1 candidate is eligible only with the native GNU C++ ABI provider.
// The live TargetMachine below must independently match this COFF/SEH model.
#  define UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH 1
#  define UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_ARM64_SEH 1
# endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::native_exception_symbols
{
    // This infrastructure does not select a host C++ ABI or enable guest EH. The caller must have
    // independently qualified an Itanium C++ ABI runtime using DWARF or GNU Win64 SEH unwinding.
    // The supplied typeinfo is its genuine, immutable guest-exception TYPE object, not a pointer slot.
    inline constexpr char type_info_symbol[]{"uwvm_guest_exception_typeinfo_v1"};
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
    inline constexpr char personality_symbol[]{"__gxx_personality_seh0"};
    // The real host personality is outside RuntimeDyld's synthetic COFF image.
    // A JIT-local executable wrapper is required because .xdata's ADDR32NB
    // relocation otherwise creates a non-executable stub inside .xdata.
    inline constexpr char host_personality_symbol[]{"uwvm_guest_seh_host_personality_v1"};
#else
    inline constexpr char personality_symbol[]{"__gxx_personality_v0"};
#endif

    enum class error
    {
        ok, unsupported_unwind_model, incompatible_module, incompatible_declaration,
        invalid_binding, conflicting_binding
    };
    struct declarations
    {
        ::llvm::GlobalVariable* type_info{};
        ::llvm::Function* personality{};
        ::llvm::Function* host_personality{};
        error status{error::invalid_binding};
    };

    namespace details
    {
        // LLVM versions expose MCAsmInfo either by pointer or reference. Borrow the target-owned
        // object without extending its lifetime, and retain a null check for the pointer interface.
        template<typename Machine>
        [[nodiscard]] inline ::llvm::MCAsmInfo const* asm_info(Machine const& machine) noexcept
        {
            auto&& info{machine.getMCAsmInfo()};
            if constexpr(::std::is_pointer_v<::std::remove_cvref_t<decltype(info)>>) { return info; }
            else { return ::std::addressof(info); }
        }
        [[nodiscard]] inline bool valid_type_info(::llvm::GlobalVariable const* value) noexcept
        {
            return value != nullptr && value->getName() == type_info_symbol && value->isDeclaration() &&
                value->hasExternalLinkage() && value->isConstant() && !value->isThreadLocal() &&
                value->getAddressSpace() == 0u && value->getValueType()->isIntegerTy(8u) &&
                !value->hasDLLImportStorageClass() && !value->hasDLLExportStorageClass();
        }
        [[nodiscard]] inline bool valid_personality(::llvm::Function const* value) noexcept
        {
            if(value == nullptr || value->getName() != personality_symbol || value->getAddressSpace() != 0u ||
               value->getCallingConv() != ::llvm::CallingConv::C || value->hasDLLImportStorageClass() ||
               value->hasDLLExportStorageClass()) { return false; }
            auto const type{value->getFunctionType()}; // borrowed from the live LLVM context
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
            if(value->isDeclaration() || !value->hasLinkOnceODRLinkage() || value->getComdat() == nullptr ||
               !value->doesNotThrow() ||
               type->getNumParams() != 4u || type->isVarArg() || !type->getReturnType()->isIntegerTy(32u))
            { return false; }
            for(auto const* param: type->params()) { if(!param->isPointerTy()) { return false; } }
            return true;
#else
            if(!value->isDeclaration() || !value->hasExternalLinkage()) { return false; }
            return type->getReturnType()->isIntegerTy(32u) && type->isVarArg() && type->getNumParams() == 0u;
#endif
        }
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        [[nodiscard]] inline bool valid_host_personality(::llvm::Function const* value) noexcept
        {
            if(value == nullptr || value->getName() != host_personality_symbol || !value->isDeclaration() ||
               !value->hasExternalLinkage() || value->getAddressSpace() != 0u ||
               value->getCallingConv() != ::llvm::CallingConv::C || value->hasDLLImportStorageClass() ||
               value->hasDLLExportStorageClass()) { return false; }
            auto const type{value->getFunctionType()}; // borrowed from the live LLVM context
            if(type->isVarArg() || type->getNumParams() != 4u || !type->getReturnType()->isIntegerTy(32u) ||
               !value->doesNotThrow()) { return false; }
            for(auto const* param: type->params()) { if(!param->isPointerTy()) { return false; } }
            return true;
        }
#endif
    }

    [[nodiscard]] inline bool is_object_local_personality_symbol(::llvm::StringRef name) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        return name == personality_symbol;
#else
        (void)name;
        return false;
#endif
    }

    [[nodiscard]] inline bool is_object_local_personality_definition(::llvm::Function const& function) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        // [live partition-owned definition] Function metadata is borrowed only
        // [safe                           ] while its owning module is alive.
        // ^^ function: preserve this checked executable definition and its COMDAT
        // in every COFF partition, so .xdata never targets an external data stub.
        return details::valid_personality(::std::addressof(function));
#else
        (void)function;
        return false;
#endif
    }

    [[nodiscard]] inline bool supports_itanium_dwarf_object(::llvm::TargetMachine const& machine) noexcept
    {
        auto const& triple{machine.getTargetTriple()};
        auto const info{details::asm_info(machine)}; // target-owned; only inspected while machine is live
        // ARM64EC exposes x64 predefines for data layout, not the GNU x64 EH ABI.
        // Reject its actual target subarchitecture before admitting any object format.
        if(info == nullptr || triple.isWindowsArm64EC()) { return false; }
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_ARM64_SEH)
        // [live target-owned triple and MCAsmInfo] only scalar metadata is inspected.
        // [safe                                  ] machine remains owned by the caller;
        // no ELF/MachO, MSVC/ARM64EC or foreign-ISA TargetMachine is admitted.
        // GNU AArch64 COFF uses WinEH with Itanium LSDA, not MSVC funclets.
        // Existing section_memory_manager registers relocated A64 .pdata before
        // execution; diagnostics alone cannot qualify guest throw/catch support.
        return triple.isWindowsGNUEnvironment() && triple.isOSBinFormatCOFF() &&
            triple.getArch() == ::llvm::Triple::aarch64 &&
            info->getExceptionHandlingType() == ::llvm::ExceptionHandling::WinEH &&
            info->getWinEHEncodingType() == ::llvm::WinEH::EncodingType::Itanium;
#else
#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
        if(triple.isOSLinux() && triple.isOSBinFormatELF() && triple.getArch() == ::llvm::Triple::arm &&
           info->getExceptionHandlingType() == ::llvm::ExceptionHandling::ARM)
        { return ::uwvm_llvm_arm_ehabi_target2_abi_v1 != nullptr && ::uwvm_llvm_arm_ehabi_target2_abi_v1() == 1u; }
#endif
        if((triple.isOSBinFormatELF() || triple.isOSBinFormatMachO()) &&
           info->getExceptionHandlingType() == ::llvm::ExceptionHandling::DwarfCFI) { return true; }
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        // LLVM GNU COFF x64 uses landingpads with an Itanium-style LSDA in
        // .xdata. RuntimeDyld's .pdata must also be registered with Windows
        // before any generated frame can unwind. Reject MSVC and ARM funclets.
        return triple.isWindowsGNUEnvironment() && triple.isOSBinFormatCOFF() &&
            triple.isX86_64() && info->getExceptionHandlingType() == ::llvm::ExceptionHandling::WinEH &&
            info->getWinEHEncodingType() == ::llvm::WinEH::EncodingType::Itanium;
#else
        return false;
#endif
#endif
    }

    // Declare relocatable metadata references. In particular, do NOT use the ordinary host-address
    // emitter: its target-specific inttoptr/SSA fallback is not a legal substitute for a typeinfo GV.
    // LLVM MachineFunction::addLandingPad converts non-GlobalValue catches to null, and the DWARF
    // emitter can then encode catch-all. Keep the return type GlobalVariable* as part of this API.
    // https://github.com/llvm/llvm-project/blob/main/llvm/lib/CodeGen/MachineFunction.cpp
    [[nodiscard]] inline declarations declare_itanium_dwarf_symbols(::llvm::Module& module,
                                                                   ::llvm::TargetMachine const& machine)
    {
        if(!supports_itanium_dwarf_object(machine)) { return {.status = error::unsupported_unwind_model}; }
        if(::llvm::Triple{module.getTargetTriple()} != machine.getTargetTriple() ||
           module.getDataLayout() != machine.createDataLayout()) { return {.status = error::incompatible_module}; }
        auto const existing_type{module.getNamedValue(type_info_symbol)}; // nullable module-owned borrow
        auto const existing_personality{module.getNamedValue(personality_symbol)};
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        auto const existing_host_personality{module.getNamedValue(host_personality_symbol)};
#endif
        auto type_info{::llvm::dyn_cast_or_null<::llvm::GlobalVariable>(existing_type)};
        auto personality{::llvm::dyn_cast_or_null<::llvm::Function>(existing_personality)};
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        auto host_personality{::llvm::dyn_cast_or_null<::llvm::Function>(existing_host_personality)};
#endif
        // Check ALL names before mutating the module: a conflicting alias/function/global cannot
        // leave half of the EH declarations installed or be silently renamed by LLVM.
        if((existing_type != nullptr && !details::valid_type_info(type_info)) ||
           (existing_personality != nullptr && !details::valid_personality(personality))
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
           || (existing_host_personality != nullptr && !details::valid_host_personality(host_personality))
#endif
          )
        { return {.status = error::incompatible_declaration}; }
        auto& context{module.getContext()};
        if(type_info == nullptr)
        {
            // [module-owned external declaration] has no guest-readable allocation or initializer.
            // [safe                            ] only its symbol address is used in EH metadata.
            // ^^ type_info: ownership transfers immediately to module, borrowed until module teardown.
            type_info = new ::llvm::GlobalVariable(module, ::llvm::Type::getInt8Ty(context), true,
                ::llvm::GlobalValue::ExternalLinkage, nullptr, type_info_symbol);
        }
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        auto const pointer_type{::llvm::PointerType::getUnqual(context)};
        auto const seh_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context),
            {pointer_type, pointer_type, pointer_type, pointer_type}, false)};
        if(host_personality == nullptr)
        {
# if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_ARM64_SEH)
            // [module-owned external declaration] maps to the pinned GNU SEH ABI entry.
            // [safe                            ] its full native address is relocated
            // through executable .text (A64 BRANCH26/full-width branch stub), never
            // through a non-executable .xdata personality stub. This is not a guest
            // pointer borrow and does not dereference the native function address.
# else
            // [module-owned external declaration] maps to the pinned host ABI entry.
            // [safe                            ] its full native pointer is used only by
            // the executable wrapper's REL32 call stub, never by .xdata ADDR32NB.
# endif
            // ^^ host_personality: Function::Create transfers ownership to module.
            host_personality = ::llvm::Function::Create(seh_type,
                ::llvm::GlobalValue::ExternalLinkage, host_personality_symbol, module);
            host_personality->setDoesNotThrow();
        }
        if(personality == nullptr)
        {
            // [module-owned executable definition] each COFF object owns its
            // [safe                              ] local .text target for .xdata's
            // ADDR32NB personality relocation. LinkOnceODR permits IR merging.
            // ^^ personality: Function::Create transfers ownership to module.
            personality = ::llvm::Function::Create(seh_type,
                ::llvm::GlobalValue::LinkOnceODRLinkage, personality_symbol, module);
            personality->setComdat(module.getOrInsertComdat(personality_symbol));
            personality->setDoesNotThrow();
            // [module-owned wrapper] [module-owned entry block]
            // [safe                ] IRBuilder borrows entry only until emission ends.
            // ^^ entry: BasicBlock::Create transfers ownership to personality.
            auto* entry{::llvm::BasicBlock::Create(context, "entry", personality)};
            ::llvm::IRBuilder<> builder{entry};
            ::llvm::SmallVector<::llvm::Value*, 4u> args;
            for(auto& arg: personality->args()) { args.push_back(&arg); }
            // [module-owned host declaration] [module-owned call instruction]
            // [safe                         ] the call's pointer operands are
            // copied from the four live wrapper arguments, never guest memory.
            // ^^ call: CreateCall inserts the instruction into entry.
            auto* call{builder.CreateCall(seh_type, host_personality, args)};
            call->setDoesNotThrow();
            call->setTailCallKind(::llvm::CallInst::TCK_MustTail);
            builder.CreateRet(call);
        }
        return {type_info, personality, host_personality, error::ok};
#else
        if(personality == nullptr)
        {
            // [module-owned external function] C ABI signature is consumed by LLVM's EH lowering.
            // [safe                          ] no host address is embedded in the IR or object cache.
            // ^^ personality: Function::Create publishes ownership to the same live module.
            personality = ::llvm::Function::Create(::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), true),
                ::llvm::GlobalValue::ExternalLinkage, personality_symbol, module);
        }
        return {type_info, personality, nullptr, error::ok};
#endif
    }

    // Historical function names mention DWARF; the same validated declarations
    // and per-engine mappings also serve GNU Win64 SEH's Itanium-style LSDA.
    // Register only in this ExecutionEngine, before object finalization and any guest execution.
    // The caller owns/pins the actual C++ runtime image, typeinfo object and personality for the
    // engine's complete lifetime. No code here reads __cxa_exception headers, acquires RTTI through
    // a runtime-specific extension, or infers an ABI from the host processor.
    //
    // MCJIT::findExistingSymbol consults its GlobalMapping before RuntimeDyld/client symbol lookup.
    // Holding the engine's recursive lock across validation and both writes prevents a check/write
    // race. Different engines may bind the same stable symbol name independently, without touching
    // DynamicLibrary::AddSymbol or changing another VM's bindings.
    // https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/MCJIT/MCJIT.cpp
    [[nodiscard]] inline error bind_itanium_dwarf_host_symbols(::llvm::ExecutionEngine& engine,
        declarations const& symbols, void const* actual_type_info, ::std::uintptr_t personality_address)
    {
        if(symbols.status != error::ok || actual_type_info == nullptr || personality_address == 0u ||
           !details::valid_type_info(symbols.type_info) || !details::valid_personality(symbols.personality)
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
           || !details::valid_host_personality(symbols.host_personality)
#endif
          )
        { return error::invalid_binding; }
        auto const module{symbols.type_info->getParent()}; // module retains both live declarations
#if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
        auto const mapped_personality_global{symbols.host_personality}; // external host entry, not local .text wrapper
#else
        auto const mapped_personality_global{symbols.personality};
#endif
        if(module == nullptr || symbols.personality->getParent() != module ||
           mapped_personality_global->getParent() != module ||
           engine.getDataLayout() != module->getDataLayout() ||
           engine.getDataLayout().getPointerSize(0u) != sizeof(::std::uintptr_t))
        { return error::invalid_binding; }
        // The engine's public lock is recursive; getPointerToGlobalIfAvailable/addGlobalMapping
        // acquire it internally too. Caller must also serialize LLVM IR mutation while binding.
        ::std::lock_guard guard{engine.lock};
        auto const mapped_type{engine.getPointerToGlobalIfAvailable(symbols.type_info)};
        auto const mapped_personality{engine.getPointerToGlobalIfAvailable(mapped_personality_global)};
        if((mapped_type != nullptr && mapped_type != actual_type_info) ||
           (mapped_personality != nullptr && reinterpret_cast<::std::uintptr_t>(mapped_personality) != personality_address))
        { return error::conflicting_binding; }
        if(mapped_type == nullptr)
        {
            // [immutable host typeinfo] remains owned by the pinned runtime image, not by the JIT.
            // [safe                   ] LLVM's mapping API requires void* but does not mutate it.
            // ^^ mapped symbol targets the OBJECT, never the address of a type_info* variable.
            engine.addGlobalMapping(symbols.type_info, const_cast<void*>(actual_type_info));
        }
        if(mapped_personality == nullptr)
        {
            // [host personality entry] the caller supplies its full native function-pointer address.
            // [safe                  ] only a relocation mapping is recorded; it is not dereferenced.
            engine.addGlobalMapping(mapped_personality_global, reinterpret_cast<void*>(personality_address));
        }
        return error::ok;
    }
}
# if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_ARM64_SEH)
#  undef UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_ARM64_SEH
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH)
#  undef UWVM_RUNTIME_LLVM_JIT_GNU_WIN64_SEH
# endif
#endif
