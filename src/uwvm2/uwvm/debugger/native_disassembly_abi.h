/*************************************************************
 * Private source-only LLVM C ABI candidate; not yet qualified. *
 *************************************************************/
#pragma once
#include <llvm-c/Disassembler.h>
#include <llvm-c/Target.h>
#include <type_traits>

// Only these cold LLVM C APIs are bound here. True C++ throw/rethrow/resume,
// _Unwind_RaiseException and _Unwind_Resume must retain their actual EH ABI.
// This preserves the debugger's existing terminate-on-native-failure contract;
// it is not a general claim that arbitrary user LLVM C++ callbacks cannot throw.
#if defined(__GNUC__) || defined(__clang__)
# if defined(__arm64ec__) || defined(_M_ARM64EC) || defined(_M_HYBRID)
#  define UWVM_DEBUG_LLVM_C_SYMBOL(name) __asm__("#" #name)
# elif defined(__APPLE__) || (defined(_WIN32) && (defined(__i386__) || defined(_M_IX86)))
#  define UWVM_DEBUG_LLVM_C_SYMBOL(name) __asm__("_" #name)
# else
// ELF and COFF x64 use the plain C spelling. Explicit asm labels bypass
// the normal target prefix, so COFF i386 needs the literal leading underscore.
// These SDK functions are cdecl: never append a WinAPI stdcall @N suffix
// or change either uint64_t parameter to a native word.
#  define UWVM_DEBUG_LLVM_C_SYMBOL(name) __asm__(#name)
# endif
# define UWVM_DEBUG_LLVM_C_IMPORT(result, name, params, args) \
    extern "C" LLVM_C_ABI result uwvm_##name params noexcept UWVM_DEBUG_LLVM_C_SYMBOL(name)
#else
// Pure MSVC cannot spell a GNU asm label. Preserve the real SDK import and
// calling convention through its existing nonthrowing wrapper; it remains
// a separately unqualified compiler path, not an asm-link qualification.
# include <fast_io.h>
# define UWVM_DEBUG_LLVM_C_FORWARD(...) __VA_OPT__(,) __VA_ARGS__
# define UWVM_DEBUG_LLVM_C_IMPORT(result, name, params, args) \
    inline result uwvm_##name params noexcept \
    { return ::fast_io::noexcept_call(::name UWVM_DEBUG_LLVM_C_FORWARD args); }
#endif

namespace uwvm2::uwvm::debugger::native_disassembly_abi
{
    UWVM_DEBUG_LLVM_C_IMPORT(LLVMDisasmContextRef, LLVMCreateDisasm,
        (char const* triple, void* information, int tag_type,
         LLVMOpInfoCallback operand_callback, LLVMSymbolLookupCallback symbol_callback),
        (triple, information, tag_type, operand_callback, symbol_callback));
    UWVM_DEBUG_LLVM_C_IMPORT(LLVMDisasmContextRef, LLVMCreateDisasmCPUFeatures,
        (char const* triple, char const* cpu, char const* features, void* information, int tag_type,
         LLVMOpInfoCallback operand_callback, LLVMSymbolLookupCallback symbol_callback),
        (triple, cpu, features, information, tag_type, operand_callback, symbol_callback));
    UWVM_DEBUG_LLVM_C_IMPORT(::size_t, LLVMDisasmInstruction,
        (LLVMDisasmContextRef context, ::uint8_t* bytes, ::uint64_t available,
         ::uint64_t pc, char* text, ::size_t capacity),
        (context, bytes, available, pc, text, capacity));
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMDisasmDispose,
        (LLVMDisasmContextRef context), (context));

    // The actual generated Targets/Disassemblers.def must contain the native
    // backend, exactly as the existing decoder already requires.
#if (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeX86TargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeX86TargetInfo), decltype(&::LLVMInitializeX86TargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeX86Target, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeX86Target), decltype(&::LLVMInitializeX86Target)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeX86TargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeX86TargetMC), decltype(&::LLVMInitializeX86TargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeX86Disassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeX86Disassembler), decltype(&::LLVMInitializeX86Disassembler)>);
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeAArch64TargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeAArch64TargetInfo), decltype(&::LLVMInitializeAArch64TargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeAArch64Target, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeAArch64Target), decltype(&::LLVMInitializeAArch64Target)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeAArch64TargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeAArch64TargetMC), decltype(&::LLVMInitializeAArch64TargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeAArch64Disassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeAArch64Disassembler), decltype(&::LLVMInitializeAArch64Disassembler)>);
#elif defined(__arm__) || defined(_M_ARM)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeARMTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeARMTargetInfo), decltype(&::LLVMInitializeARMTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeARMTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeARMTarget), decltype(&::LLVMInitializeARMTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeARMTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeARMTargetMC), decltype(&::LLVMInitializeARMTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeARMDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeARMDisassembler), decltype(&::LLVMInitializeARMDisassembler)>);
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializePowerPCTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializePowerPCTargetInfo), decltype(&::LLVMInitializePowerPCTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializePowerPCTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializePowerPCTarget), decltype(&::LLVMInitializePowerPCTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializePowerPCTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializePowerPCTargetMC), decltype(&::LLVMInitializePowerPCTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializePowerPCDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializePowerPCDisassembler), decltype(&::LLVMInitializePowerPCDisassembler)>);
#elif defined(__riscv)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeRISCVTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeRISCVTargetInfo), decltype(&::LLVMInitializeRISCVTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeRISCVTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeRISCVTarget), decltype(&::LLVMInitializeRISCVTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeRISCVTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeRISCVTargetMC), decltype(&::LLVMInitializeRISCVTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeRISCVDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeRISCVDisassembler), decltype(&::LLVMInitializeRISCVDisassembler)>);
#elif defined(__s390x__)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSystemZTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSystemZTargetInfo), decltype(&::LLVMInitializeSystemZTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSystemZTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSystemZTarget), decltype(&::LLVMInitializeSystemZTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSystemZTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSystemZTargetMC), decltype(&::LLVMInitializeSystemZTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSystemZDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSystemZDisassembler), decltype(&::LLVMInitializeSystemZDisassembler)>);
#elif defined(__loongarch__)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeLoongArchTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeLoongArchTargetInfo), decltype(&::LLVMInitializeLoongArchTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeLoongArchTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeLoongArchTarget), decltype(&::LLVMInitializeLoongArchTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeLoongArchTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeLoongArchTargetMC), decltype(&::LLVMInitializeLoongArchTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeLoongArchDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeLoongArchDisassembler), decltype(&::LLVMInitializeLoongArchDisassembler)>);
#elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeMipsTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeMipsTargetInfo), decltype(&::LLVMInitializeMipsTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeMipsTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeMipsTarget), decltype(&::LLVMInitializeMipsTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeMipsTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeMipsTargetMC), decltype(&::LLVMInitializeMipsTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeMipsDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeMipsDisassembler), decltype(&::LLVMInitializeMipsDisassembler)>);
#elif defined(__sparc__)
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSparcTargetInfo, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSparcTargetInfo), decltype(&::LLVMInitializeSparcTargetInfo)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSparcTarget, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSparcTarget), decltype(&::LLVMInitializeSparcTarget)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSparcTargetMC, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSparcTargetMC), decltype(&::LLVMInitializeSparcTargetMC)>);
    UWVM_DEBUG_LLVM_C_IMPORT(void, LLVMInitializeSparcDisassembler, (void), ());
    static_assert(::std::is_convertible_v<decltype(&uwvm_LLVMInitializeSparcDisassembler), decltype(&::LLVMInitializeSparcDisassembler)>);
#endif
    // A noexcept function pointer converts only to the same SDK parameter,
    // result and calling-convention type with its exception specification
    // relaxed. These check the full callback types and 32-bit uint64_t fields.
    using create_abi = LLVMDisasmContextRef (*)(char const*, void*, int, LLVMOpInfoCallback, LLVMSymbolLookupCallback) noexcept;
    using create_features_abi = LLVMDisasmContextRef (*)(char const*, char const*, char const*, void*, int, LLVMOpInfoCallback, LLVMSymbolLookupCallback) noexcept;
    using decode_abi = ::size_t (*)(LLVMDisasmContextRef, ::uint8_t*, ::uint64_t, ::uint64_t, char*, ::size_t) noexcept;
    using dispose_abi = void (*)(LLVMDisasmContextRef) noexcept;
    static_assert(::std::is_same_v<decltype(&uwvm_LLVMCreateDisasm), create_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm_LLVMCreateDisasmCPUFeatures), create_features_abi>);
    static_assert(::std::is_convertible_v<create_features_abi, decltype(&::LLVMCreateDisasmCPUFeatures)>);
    static_assert(::std::is_same_v<decltype(&uwvm_LLVMDisasmInstruction), decode_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm_LLVMDisasmDispose), dispose_abi>);
    static_assert(::std::is_convertible_v<create_abi, decltype(&::LLVMCreateDisasm)>);
    static_assert(::std::is_convertible_v<decode_abi, decltype(&::LLVMDisasmInstruction)>);
    static_assert(::std::is_convertible_v<dispose_abi, decltype(&::LLVMDisasmDispose)>);
}
#undef UWVM_DEBUG_LLVM_C_IMPORT
#if defined(UWVM_DEBUG_LLVM_C_SYMBOL)
# undef UWVM_DEBUG_LLVM_C_SYMBOL
#endif
#if defined(UWVM_DEBUG_LLVM_C_FORWARD)
# undef UWVM_DEBUG_LLVM_C_FORWARD
#endif
