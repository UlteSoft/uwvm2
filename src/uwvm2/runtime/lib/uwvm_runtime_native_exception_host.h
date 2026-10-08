/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

// This header is also included in global module fragments. Keep dependencies
// limited to fast_io containers and LLVM/platform/standard headers: guest types and compiler-owned
// declaration structs are dependent template parameters, never redefined here.
#include <fast_io_dsal/array.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ADT/SmallString.h>
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/IR/DerivedTypes.h>
# include <llvm/IR/Function.h>
# include <llvm/IR/GlobalVariable.h>
# include <llvm/IR/Module.h>
# include <llvm/IR/Mangler.h>
#endif

// The selected TargetMachine must additionally advertise ELF/MachO DWARF, or
// GNU COFF x64 WinEH with Itanium LSDA encoding. This host check rules out
// MSVC, SjLj, unregistered ARM EHABI, missing C++ ABI headers and builds
// without C++ exceptions. The GNU/Linux ARM opt-in below uses actual dynamic
// EHABI registration; it never substitutes a DWARF ABI for the host provider.
// ARM/Thumb compilers explicitly selecting DWARF EH remain eligible; Clang
// defines __ARM_DWARF_EH__ from its actual exception model. TargetMachine must
// still report DwarfCFI, and the native C++ ABI/unwinder must match that model.
// https://github.com/llvm/llvm-project/blob/main/clang/lib/Frontend/InitPreprocessor.cpp
// ARM64EC's x64 compatibility predefines do not qualify its native C++ EH ABI;
// reject them independently of the AArch64 GNU candidate opt-in.
// The exact-1 ARM64 GNU candidate below additionally requires MinGW/SEH and
// excludes MSVC/ARM64EC. Common GXX/C++-EH/provider-header checks still apply;
// actual WindowsGNU/COFF/AArch64/WinEH/Itanium is checked by TargetMachine.
// This gate is pending provider/native qualification, not a platform claim.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__GXX_ABI_VERSION) && \
    (defined(__cpp_exceptions) || defined(__EXCEPTIONS)) && \
    !defined(__USING_SJLJ_EXCEPTIONS__) && \
    (!(defined(__arm__) || defined(__thumb__)) || defined(__ARM_DWARF_EH__) || (defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35)))) && \
    __has_include(<cxxabi.h>) && __has_include(<unwind.h>) && \
    ((!defined(_WIN32)) || \
     (defined(_WIN64) && defined(__SEH__) && !defined(__CYGWIN__) && \
      !defined(__arm64ec__) && !defined(_M_ARM64EC) && \
      (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))) || \
     (defined(UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT) && \
      UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT == 1 && \
      defined(_WIN32) && defined(_WIN64) && defined(__MINGW32__) && defined(__SEH__) && \
      !defined(_MSC_VER) && !defined(__CYGWIN__) && \
      !defined(__arm64ec__) && !defined(_M_ARM64EC) && \
      (defined(__aarch64__) || defined(_M_ARM64))))
# include <cxxabi.h>
# include <unwind.h>
# define UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX 1
# if defined(_WIN64)
#  define UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH 1
#  pragma push_macro("NOMINMAX")
#  ifndef NOMINMAX
#   define NOMINMAX 1
#  endif
#  include <windows.h>
#  pragma pop_macro("NOMINMAX")
// The personality address is mapped into LLVM; generated code invokes it
// through Windows' registered .pdata/.xdata, never through this declaration.
extern "C" EXCEPTION_DISPOSITION __gxx_personality_seh0(
    PEXCEPTION_RECORD, void*, PCONTEXT, PDISPATCHER_CONTEXT);
# elif defined(__arm__) && !defined(__ARM_DWARF_EH__)
extern "C" _Unwind_Reason_Code __gxx_personality_v0(_Unwind_State, _Unwind_Control_Block*, _Unwind_Context*);
# else
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action,
    ::std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
# endif
#endif

namespace uwvm2::runtime::lib::details::native_exception_host
{
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX) && defined(__FreeBSD__)
    // FreeBSD libcxxrt exports these Itanium ABI symbols but its installed
    // cxxabi.h omits their declarations. Private aliases avoid extending the
    // implementation namespace or relying on an exception-object layout.
    extern "C" void* freebsd_begin_catch(void*) __asm__("__cxa_begin_catch");
    extern "C" void freebsd_end_catch() __asm__("__cxa_end_catch");
    extern "C" [[noreturn]] void freebsd_rethrow() __asm__("__cxa_rethrow");
#endif
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX) && \
    defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    // ABI-compatible native alias used only for the VM's exact noexcept guest
    // destructor. An asm label avoids contaminating this wrapper with a C++
    // assumption that an unrelated C declaration may throw.
# if defined(__APPLE__)
    extern "C++" void end_exact_guest_catch_noexcept() noexcept __asm__("___cxa_end_catch");
# else
    extern "C++" void end_exact_guest_catch_noexcept() noexcept __asm__("__cxa_end_catch");
# endif
#endif
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX)
    inline constexpr bool available{true};
#else
    inline constexpr bool available{false};
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
    namespace details
    {
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX)
        // C++ exception matching still emits the genuine typeinfo with -fno-rtti.
        // The ABI accessor is used only while our exact typed catch is active.
        // No mangled typeinfo name, exception-header layout or foreign catch-all
        // is assumed. Its immutable object belongs to the pinned native image.
        // https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#cxx-abi
        template<typename GuestException>
        [[nodiscard]] inline void const* guest_type_info(GuestException (*make_probe)())
        {
            static_assert(::std::is_final_v<GuestException> && ::std::is_nothrow_destructible_v<GuestException>);
            // Only GuestException participates in the template identity; a
            // different cold factory at another engine call site does not create
            // another cache. C++ static initialization serializes concurrent VMs.
            static void const* const type_info{[make_probe]() -> void const*
            {
                try { throw make_probe(); }
                catch(GuestException const&)
                {
                    return ::__cxxabiv1::__cxa_current_exception_type();
                }
            }()};
            return type_info;
        }
#endif

        [[nodiscard]] inline bool valid_function(::llvm::Function const* function,
            ::llvm::Module const* module, char const* name, ::llvm::FunctionType* type) noexcept
        {
            return function != nullptr && function->getParent() == module && function->getName() == name &&
                function->isDeclaration() && function->hasExternalLinkage() && function->getAddressSpace() == 0u &&
                function->getCallingConv() == ::llvm::CallingConv::C && function->getFunctionType() == type &&
                !function->hasDLLImportStorageClass() && !function->hasDLLExportStorageClass();
        }
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH)
        [[nodiscard]] inline bool valid_seh_personality_wrapper(::llvm::Function const* function,
            ::llvm::Module const* module, ::llvm::FunctionType* type) noexcept
        {
            return function != nullptr && function->getParent() == module &&
                function->getName() == "__gxx_personality_seh0" && !function->isDeclaration() &&
                function->hasLinkOnceODRLinkage() && function->getComdat() != nullptr &&
                function->getAddressSpace() == 0u && function->getCallingConv() == ::llvm::CallingConv::C &&
                function->getFunctionType() == type && !function->hasDLLImportStorageClass() &&
                !function->hasDLLExportStorageClass();
        }
#endif
    }

    // Declarations come from native_exception_symbols / native_exception_landingpad.
    // Bind before finalizeObject/addObject can resolve code or cached relocations.
    // Engine creation transfers their module's ownership but preserves every GV.
    // The engine and native runtime image must outlive all published JIT entries.
    template<typename GuestException, typename Symbols, typename CatchRuntime>
    [[nodiscard]] inline bool bind(::llvm::ExecutionEngine& engine, Symbols const& symbols,
        CatchRuntime const& runtime, GuestException (*make_probe)()
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        , void (*actual_end_catch)() noexcept = nullptr
#endif
        )
    {
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX)
        if(symbols.status != decltype(symbols.status)::ok || symbols.type_info == nullptr ||
           symbols.personality == nullptr || !runtime || make_probe == nullptr) { return false; }
        auto const module{symbols.type_info->getParent()}; // borrowed from the newly created engine
        if(module == nullptr || engine.getDataLayout() != module->getDataLayout() ||
           engine.getDataLayout().getPointerSize(0u) != sizeof(::std::uintptr_t)) { return false; }
        auto const type_info{symbols.type_info};
        if(type_info->getName() != "uwvm_guest_exception_typeinfo_v1" || !type_info->isDeclaration() ||
           !type_info->hasExternalLinkage() || !type_info->isConstant() || type_info->isThreadLocal() ||
           type_info->getAddressSpace() != 0u || !type_info->getValueType()->isIntegerTy(8u) ||
           type_info->hasDLLImportStorageClass() || type_info->hasDLLExportStorageClass()) { return false; }
        auto& context{module->getContext()};
        auto const pointer{::llvm::PointerType::getUnqual(context)};
        auto const void_function{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), false)};
# if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH)
        auto const seh_personality_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context),
            {pointer, pointer, pointer, pointer}, false)};
        constexpr char host_personality_name[]{"uwvm_guest_seh_host_personality_v1"};
        auto const personality_address{reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&::__gxx_personality_seh0))};
        if(!details::valid_seh_personality_wrapper(symbols.personality, module, seh_personality_type) ||
           !details::valid_function(symbols.host_personality, module, host_personality_name, seh_personality_type))
        { return false; }
# else
        constexpr char personality_name[]{"__gxx_personality_v0"};
        auto const personality_address{reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&::__gxx_personality_v0))};
# endif
        if(
# if !defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH)
           !details::valid_function(symbols.personality, module, personality_name,
                ::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), true)) ||
# endif
           !details::valid_function(runtime.begin, module, "__cxa_begin_catch",
                ::llvm::FunctionType::get(pointer, {pointer}, false)) ||
           !details::valid_function(runtime.end, module, "__cxa_end_catch", void_function) ||
           !details::valid_function(runtime.rethrow, module, "__cxa_rethrow", void_function)) { return false; }
        auto const actual_type_info{details::guest_type_info<GuestException>(make_probe)};
        if(actual_type_info == nullptr) { return false; }
        struct mapping { ::llvm::GlobalValue const* global; void* address; };
        // [five live module-owned declarations] [five pinned host ABI symbols]
        // [safe                             ] mappings store addresses only;
        // type_info maps the OBJECT, never the address of our cached pointer.
        // On Win64, the mapped personality is the wrapper's EXTERNAL callee;
        // the executable wrapper itself remains a module-owned definition.
        ::fast_io::array<mapping, 5uz> const mappings{{
            {type_info, const_cast<void*>(actual_type_info)},
# if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH)
            {symbols.host_personality, personality_address},
# else
            {symbols.personality, personality_address},
# endif
            {runtime.begin, reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(
#if defined(__FreeBSD__)
                &freebsd_begin_catch
#else
                &::__cxxabiv1::__cxa_begin_catch
#endif
                ))},
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
            {runtime.end, actual_end_catch == nullptr ?
                reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(
#if defined(__FreeBSD__)
                &freebsd_end_catch
#else
                &::__cxxabiv1::__cxa_end_catch
#endif
                )) :
                reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(actual_end_catch))},
#else
            {runtime.end, reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(
#if defined(__FreeBSD__)
                &freebsd_end_catch
#else
                &::__cxxabiv1::__cxa_end_catch
#endif
                ))},
#endif
            {runtime.rethrow, reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(
#if defined(__FreeBSD__)
                &freebsd_rethrow
#else
                &::__cxxabiv1::__cxa_rethrow
#endif
                ))}
        }};
        // Validate ALL five addresses under the engine's recursive lock before
        // publishing any. A conflicting helper must not leave only typeinfo or
        // personality installed. Same-symbol mappings in other engines remain
        // independent; process-global DynamicLibrary::AddSymbol is never used.
        ::std::lock_guard guard{engine.lock};
        for(auto const& item: mappings)
        {
            auto const existing{engine.getPointerToGlobalIfAvailable(item.global)};
            if(item.address == nullptr || (existing != nullptr && existing != item.address)) { return false; }
        }
        // EH lowering adds this external call after IR emission. A statically
        // linked unwinder need not export a process dynamic symbol on ELF,
        // MachO or GNU COFF. Bind the exact throwing ABI entry in this engine;
        // never install a process-global override or a noexcept wrapper.
        // String mappings already use the object symbol spelling. Derive its
        // prefix from this qualified engine's DataLayout (including MachO).
        ::llvm::SmallString<32u> resume_name{};
        ::llvm::Mangler::getNameWithPrefix(resume_name, "_Unwind_Resume", engine.getDataLayout());
        auto const resume_address{reinterpret_cast<::std::uintptr_t>(&::_Unwind_Resume)};
        auto const existing_resume{engine.getPointerToGlobalIfAvailable(resume_name)};
        if(resume_address == 0u || (existing_resume != nullptr &&
            reinterpret_cast<::std::uintptr_t>(existing_resume) != resume_address)) { return false; }
        // All conflicts, including the late codegen symbol, were checked before
        // any write. Repeated binding is idempotent even with LLVM assertions.
        for(auto const& item: mappings)
        {
            if(engine.getPointerToGlobalIfAvailable(item.global) == nullptr)
            { engine.addGlobalMapping(item.global, item.address); }
        }
        if(existing_resume == nullptr) { engine.addGlobalMapping(resume_name, resume_address); }
        return true;
#else
        (void)engine;
        (void)symbols;
        (void)runtime;
        (void)make_probe;
        return false;
#endif
    }
#endif
}

#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH)
# undef UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_WIN64_SEH
#endif
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX)
# undef UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX
#endif
