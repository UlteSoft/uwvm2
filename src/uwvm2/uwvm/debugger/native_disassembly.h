/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <span>
# include "native_target_metadata.h"
# if defined(__APPLE__)
#  include <TargetConditionals.h>
# endif
# if defined(UWVM_USE_LLVM_JIT)
#  include <fast_io.h>
#  include <llvm-c/Disassembler.h>
#  include <llvm-c/Target.h>
#  include "native_disassembly_abi.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_disassembly
{
    struct instruction
    {
        ::std::uintptr_t pc{};
        ::std::array<::std::uint8_t, native_target_metadata::max_instruction_bytes> bytes{};
        ::std::array<char, 128u> text{};
        ::std::size_t size{};
        [[nodiscard]] explicit operator bool() const noexcept { return size != 0u; }
    };

#if defined(UWVM_USE_LLVM_JIT)
    namespace details
    {
        // The debugger has one management thread, but an embedder may call the
        // cold decoder concurrently. Only the host initializes LLVM's target
        // registry. Signal handlers and ordinary execution never enter here.
        inline ::std::atomic<unsigned> registry_state{};
        [[nodiscard]] inline bool initialize_native() noexcept
        {
            unsigned expected{};
            if(registry_state.compare_exchange_strong(expected, 1u,
                ::std::memory_order_acq_rel, ::std::memory_order_acquire))
            {
# if (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeX86TargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeX86Target();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeX86TargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeX86Disassembler();
# elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeAArch64TargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeAArch64Target();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeAArch64TargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeAArch64Disassembler();
# elif defined(__arm__) || defined(_M_ARM)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeARMTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeARMTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeARMTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeARMDisassembler();
# elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializePowerPCTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializePowerPCTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializePowerPCTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializePowerPCDisassembler();
# elif defined(__riscv)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeRISCVTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeRISCVTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeRISCVTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeRISCVDisassembler();
# elif defined(__s390x__)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSystemZTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSystemZTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSystemZTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSystemZDisassembler();
# elif defined(__loongarch__)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeLoongArchTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeLoongArchTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeLoongArchTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeLoongArchDisassembler();
# elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeMipsTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeMipsTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeMipsTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeMipsDisassembler();
# elif defined(__sparc__)
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSparcTargetInfo();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSparcTarget();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSparcTargetMC();
                ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMInitializeSparcDisassembler();
# else
                registry_state.store(3u, ::std::memory_order_release);
                return false;
# endif
                registry_state.store(2u, ::std::memory_order_release);
            }
            else
            {
                while(registry_state.load(::std::memory_order_acquire) == 1u) {}
            }
            return registry_state.load(::std::memory_order_acquire) == 2u;
        }
    }

    // One host-owned cold MC context per bounded page. No guest callback,
    // external symbol resolver or ordinary JIT instruction enters this class.
    class decoder final
    {
        LLVMDisasmContextRef context_{};
        ::std::size_t maximum_{15u}, alignment_{1u};
        bool four_byte_slots_{};
    public:
        decoder() noexcept
        {
#if defined(UWVM_USE_LLVM_JIT) && \
    ((defined(__linux__) && defined(__x86_64__)) || \
     (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
      defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1) || \
     (defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && defined(TARGET_OS_OSX) && TARGET_OS_OSX))
            if(!details::initialize_native()) { return; }
# if defined(__APPLE__) && defined(__aarch64__)
            maximum_ = 4u; alignment_ = 4u;
            four_byte_slots_ = true;
            context_ = ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMCreateDisasm("aarch64-apple-darwin", nullptr, 0, nullptr, nullptr);
# elif defined(__APPLE__) && defined(__x86_64__)
            context_ = ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMCreateDisasm("x86_64-apple-darwin", nullptr, 0, nullptr, nullptr);
# elif defined(_WIN32)
            context_ = ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMCreateDisasm("x86_64-pc-windows-msvc", nullptr, 0, nullptr, nullptr);
# else
            context_ = ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMCreateDisasm("x86_64-pc-linux-gnu", nullptr, 0, nullptr, nullptr);
# endif
#endif
        }
        template<typename Description>
        explicit decoder(Description const& description) noexcept
        {
            if(!native_target_metadata::valid(description) || !details::initialize_native()) { return; }
            maximum_ = description.maximum_instruction_bytes;
            alignment_ = description.minimum_instruction_alignment;
            // Derive the ISA from the exact triple consumed by LLVM, never
            // merely a caller-supplied alignment/maximum pair (X86 varies).
            constexpr char a64[]{"aarch64-"}, loong64[]{"loongarch64-"};
            // These ELF MCAsmInfo classes retain the generic assembler
            // alignment of one. Both ISAs have four-byte instruction slots;
            // derive that property from the exact LLVM triple, not a supplied
            // alignment/maximum pair that could describe variable-width X86.
            four_byte_slots_ = maximum_ == 4u &&
                ((description.triple_size >= sizeof(a64) - 1u &&
                  ::std::memcmp(description.triple.data(),a64,sizeof(a64) - 1u) == 0) ||
                 (description.triple_size >= sizeof(loong64) - 1u &&
                  ::std::memcmp(description.triple.data(),loong64,sizeof(loong64) - 1u) == 0));
            // Complete OWNED strings were bounded, NUL/embedded-NUL checked.
            // LLVM synchronously copies triple/CPU/features into this cold owner.
            // These values never authenticate a code or native-step request.
            context_ = ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMCreateDisasmCPUFeatures(
                description.triple.data(), description.cpu.data(), description.features.data(), nullptr, 0, nullptr, nullptr);
        }
        decoder(decoder const&) = delete;
        decoder& operator=(decoder const&) = delete;
        ~decoder() noexcept
        {
            if(context_ != nullptr) { ::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMDisasmDispose(context_); }
        }
        [[nodiscard]] explicit operator bool() const noexcept { return context_ != nullptr; }
        [[nodiscard]] ::std::size_t fixed_instruction_bytes() const noexcept
        { return context_ != nullptr && four_byte_slots_ ? 4u : 0u; }
        // pc is display metadata only. LLVM borrows the small OWNED copy;
        // no integer PC is converted to a live code pointer by this decoder.
        [[nodiscard]] instruction decode(::std::uintptr_t pc, ::std::span<::std::uint8_t const> bytes) noexcept
        {
            instruction result{};
            if(context_ == nullptr || pc == 0u) { return result; }
            if((pc & (alignment_ - 1u)) != 0u || (four_byte_slots_ && (pc & 3u) != 0u)) { return result; }
            auto const available{::std::min<::std::size_t>(maximum_, bytes.size())};
            if(available == 0u || available > UINTPTR_MAX - pc) { return result; }
            result.pc = pc;
            // [owned copied bytes ... bytes.end) [fixed result.bytes capacity=32]
            // [safe                           ] available <= both bounded extents;
            //  ^^ LLVM receives only this local copy, never pc-as-pointer.
            ::fast_io::freestanding::my_memcpy(result.bytes.data(), bytes.data(), available);
            auto const decoded{::uwvm2::uwvm::debugger::native_disassembly_abi::uwvm_LLVMDisasmInstruction(context_,
                result.bytes.data(), available, static_cast<::std::uint64_t>(pc), result.text.data(), result.text.size())};
            if(decoded == 0u || decoded > available ||
               ::std::memchr(result.text.data(), '\0', result.text.size()) == nullptr) { return {}; }
            ::std::size_t leading{};
            while(result.text[leading] == ' ' || result.text[leading] == '\t')
            {
                // [owned text ... verified NUL] text_end
                // [safe                      ] whitespace is before the NUL;
                //                 ^^ scalar cursor advances only after this bounded read.
                ++leading;
            }
            if(result.text[leading] == '\0') { return {}; }
            if(leading != 0u)
            {
                // [bounded NUL-terminated result.text] end
                // [safe                             ] leading stops before its NUL,
                //                                     already proved inside text.size();
                //  ^^ BEFORE forming the suffix pointer: leading < NUL index < size.
                // The same bounded NUL proves cstr_len cannot leave this array.
                auto const remaining{::fast_io::cstr_len(result.text.data() + leading) + 1u};
                ::fast_io::freestanding::my_memmove(result.text.data(), result.text.data() + leading, remaining);
            }
            for(auto& character : result.text)
            {
                if(character == '\0') { break; }
                auto const byte{static_cast<unsigned char>(character)};
                if(::fast_io::char_category::is_c_cntrl(byte)) { character = ' '; }
            }
            result.size = decoded;
            for(::std::size_t index{decoded}; index != result.bytes.size(); ++index) { result.bytes[index] = 0u; }
            auto const text_length{::fast_io::cstr_len(result.text.data())};
            for(::std::size_t index{text_length}; index != result.text.size(); ++index) { result.text[index] = '\0'; }
            return result;
        }
    };
#if (defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
     defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1) || \
    (defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && defined(TARGET_OS_OSX) && TARGET_OS_OSX)
    [[nodiscard]] inline instruction decode_copied(::std::uintptr_t pc,
        ::std::span<::std::uint8_t const> bytes) noexcept
    {
        decoder context{};
        return context.decode(pc, bytes);
    }
#else
    [[nodiscard]] inline constexpr instruction decode_copied(::std::uintptr_t, ::std::span<::std::uint8_t const>) noexcept { return {}; }
#endif
    template<typename Description>
    [[nodiscard]] inline instruction decode_copied(Description const& description, ::std::uintptr_t pc,
        ::std::span<::std::uint8_t const> bytes) noexcept
    {
        decoder context{description};
        return context.decode(pc, bytes);
    }
#if (defined(__linux__) && defined(__x86_64__)) || \
    (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
     defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1) || \
    (defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && defined(TARGET_OS_OSX) && TARGET_OS_OSX)
    // Existing native-step caller owns a live exact JIT function range and
    // every guest is stopped. Keep that bounded live-copy contract separate
    // from the DAP decoder: only decode_copied accepts display addresses.
    [[nodiscard]] inline instruction decode_owned(::std::uintptr_t pc,
        ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end) noexcept
    {
        if(owner_begin == 0u || owner_end <= owner_begin || pc < owner_begin || pc >= owner_end) { return {}; }
        auto const count{::std::min<::std::size_t>(15u, owner_end - pc)};
        ::std::array<::std::uint8_t, 15u> bytes{};
        // [authenticated live JIT owner_begin ... owner_end)
        // [safe bytes pc ... pc + count] count <= owner_end - pc;
        //             ^^ bounded live borrow ends after this owned copy.
        ::fast_io::freestanding::my_memcpy(bytes.data(), reinterpret_cast<void const*>(pc), count);
        return decode_copied(pc, {bytes.data(), count});
    }
#else
    [[nodiscard]] inline constexpr instruction decode_owned(::std::uintptr_t,
        ::std::uintptr_t, ::std::uintptr_t) noexcept { return {}; }
#endif
#else
    class decoder final
    {
    public:
        constexpr decoder() noexcept = default;
        template<typename Description> explicit constexpr decoder(Description const&) noexcept {}
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return false; }
        [[nodiscard]] constexpr ::std::size_t fixed_instruction_bytes() const noexcept { return 0u; }
        [[nodiscard]] constexpr instruction decode(::std::uintptr_t, ::std::span<::std::uint8_t const>) noexcept { return {}; }
    };
    [[nodiscard]] inline constexpr instruction decode_copied(::std::uintptr_t,
        ::std::span<::std::uint8_t const>) noexcept { return {}; }
    template<typename Description>
    [[nodiscard]] inline constexpr instruction decode_copied(Description const&, ::std::uintptr_t,
        ::std::span<::std::uint8_t const>) noexcept { return {}; }
    [[nodiscard]] inline constexpr instruction decode_owned(::std::uintptr_t,
        ::std::uintptr_t, ::std::uintptr_t) noexcept { return {}; }
#endif
}
