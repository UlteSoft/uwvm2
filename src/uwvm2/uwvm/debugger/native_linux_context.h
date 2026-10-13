/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <bit>
# include <cstddef>
# include <csignal>
# include <cstdint>
# include <cstring>
# include <ucontext.h>
# if defined(__i386__)
#  include <cpuid.h>
# endif
# if defined(__powerpc__)
#  include <sys/auxv.h>
# endif
# include "native_registers.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_linux_context
{
    // Private adapters of actual SA_SIGINFO argument three. Never accept a
    // debugger-supplied address and never follow a saved guest SP/FP/register.
#if defined(__sparc__) && defined(__arch64__)
    struct sparc_saved_registers
    { unsigned long registers[16], status, pc, next_pc; unsigned int y, magic; };
    static_assert(sizeof(sparc_saved_registers) == 160u);
    // Linux SPARC64 SA_SIGINFO argument three starts at the 128-byte info
    // member, followed by pt_regs. QEMU's rt frame implements the same ABI.
    struct kernel_context
    {
        ::siginfo_t info;
        sparc_saved_registers saved;
        ::std::uintptr_t fp_save;
        ::stack_t stack;
        ::std::uint64_t mask;
        ::std::uintptr_t window_save;
    };
    static_assert(sizeof(::siginfo_t) == 128u && sizeof(kernel_context) == 336u);
    struct sparc_signal_window { ::std::uint64_t locals[8], inputs[8], arguments[8]; };
    static_assert(sizeof(sparc_signal_window) == 192u);
    struct sparc_signal_fpu
    { ::std::uint32_t registers[64]; ::std::uint64_t fsr, gsr, fprs; };
    static_assert(sizeof(sparc_signal_fpu) == 280u && offsetof(sparc_signal_fpu, fprs) == 272u);
#else
    using kernel_context = ::ucontext_t;
#endif
#if defined(__powerpc__)
    inline bool kernel_has_altivec{}; // Set cold, before SIGTRAP installation.
#endif
#if defined(__i386__)
    inline bool kernel_has_fxsr{}; // Cold CPUID contract, never a guest claim.
#endif
    inline void fp_value(native_registers::fp_snapshot& out, ::std::size_t index,
        void const* value, ::std::size_t width) noexcept
    {
        if(index >= out.values.size() || width == 0u || width > 16u) { return; }
        auto const* bytes{static_cast<unsigned char const*>(value)};
        for(::std::size_t i{}; i != width; ++i)
        { out.values[index].bytes[i] = bytes[::std::endian::native == ::std::endian::little ? i : width - i - 1u]; }
        out.values[index].width = static_cast<::std::uint8_t>(width); out.available = true;
    }
    // A Wasm v128 witness uses LLVM <16 x i8>; these ordered bytes
    // retain their lane order on a big-endian host. Scalar FP values still
    // use fp_value's integer-bit-order conversion above.
    inline void fp_vector_value(native_registers::fp_snapshot& out, ::std::size_t index,
        void const* value) noexcept
    {
        if(index >= out.values.size()) { return; }
        auto const* bytes{static_cast<unsigned char const*>(value)};
        for(::std::size_t byte{}; byte != 16u; ++byte) { out.values[index].bytes[byte]=bytes[byte]; }
        out.values[index].width=16u;out.available=true;
    }
    // A record walker is confined to a complete, fixed kernel ABI array.
    // Extended pointers and unknown/oversized records grant no additional read.
    inline void fixed_fp_records(native_registers::fp_snapshot& out, void const* storage,
        ::std::size_t size, ::std::uint32_t magic, ::std::size_t record_size,
        ::std::size_t values_offset, ::std::size_t width) noexcept
    {
        auto const* bytes{static_cast<unsigned char const*>(storage)};
        for(::std::size_t offset{}; size - offset >= 8u;)
        {
            ::std::uint32_t tag{}, length{};
            ::std::memcpy(&tag, bytes + offset, 4u); ::std::memcpy(&length, bytes + offset + 4u, 4u);
            if(tag == 0u || length < 8u || length > size - offset || length % 8u != 0u) { return; }
            if(tag == magic)
            {
                if(length != record_size || values_offset > length || 32u * width > length - values_offset) { return; }
                for(::std::size_t i{}; i != 32u; ++i) { fp_value(out, i, bytes + offset + values_offset + i * width, width); }
                return;
            }
            offset += length;
        }
    }
    [[nodiscard]] inline native_registers::snapshot capture(kernel_context const& context) noexcept
    {
        native_registers::snapshot out{};
        using architecture = native_registers::architecture;
        out.word_bits = sizeof(void*) * 8u;
#if defined(__aarch64__)
        out.machine = architecture::aarch64;
        for(::std::size_t i{}; i != 31u; ++i) { out.values[i] = context.uc_mcontext.regs[i]; }
        out.values[31u] = context.uc_mcontext.sp; out.values[32u] = context.uc_mcontext.pc;
        out.values[33u] = context.uc_mcontext.pstate;
        fixed_fp_records(out.floating, context.uc_mcontext.__reserved, sizeof(context.uc_mcontext.__reserved), 0x46508001u, 528u, 16u, 16u);
#elif defined(__i386__)
        out.machine = architecture::i686;
        int const indices[]{REG_EAX, REG_EBX, REG_ECX, REG_EDX, REG_ESI, REG_EDI, REG_EBP, REG_ESP, REG_EIP, REG_EFL};
        for(::std::size_t i{}; i != 10u; ++i) { out.values[i] = static_cast<::std::uint32_t>(context.uc_mcontext.gregs[indices[i]]); }
        if(auto const* prefix{context.uc_mcontext.fpregs}; prefix != nullptr && kernel_has_fxsr &&
           (reinterpret_cast<::_fpstate const*>(prefix)->magic == 0u || (context.uc_flags & 1u) != 0u))
        {
            auto const* saved{reinterpret_cast<::_fpstate const*>(prefix)};
            for(::std::size_t i{}; i != 8u; ++i) { fp_value(out.floating, i, &saved->_xmm[i], 16u); }
        }
#elif defined(__powerpc__)
        out.machine = architecture::powerpc;
        // HWCAP permits using the VMX ABI; saved MSR_VEC (bit 25) proves
        // that Linux actually wrote this frame's vector register slots.
        // VRSAVE and an inline v_regs pointer can exist without saved VRs.
# if defined(__powerpc64__)
        auto const& saved{context.uc_mcontext}; auto const& gpr{saved.gp_regs};
        for(::std::size_t i{}; i != 32u; ++i) { fp_value(out.floating, i, &saved.fp_regs[i], 8u); }
        if(kernel_has_altivec && (gpr[33u] & 0x02000000u) != 0u && saved.v_regs != nullptr)
        {
            auto const begin{reinterpret_cast<::std::uintptr_t>(saved.vmx_reserve)};
            auto const address{reinterpret_cast<::std::uintptr_t>(saved.v_regs)};
            if(address >= begin && address-begin <= sizeof(saved.vmx_reserve)-sizeof(*saved.v_regs))
            { for(::std::size_t i{}; i != 32u; ++i) { fp_vector_value(out.floating,32u+i,&saved.v_regs->vrregs[i]); } }
        }
# else
        auto const* pointer{context.uc_mcontext.uc_regs};
        auto const begin{reinterpret_cast<::std::uintptr_t>(context.uc_reg_space)};
        auto const address{reinterpret_cast<::std::uintptr_t>(pointer)};
        if(pointer == nullptr || address < begin || address - begin > sizeof(context.uc_reg_space) - sizeof(*pointer)) { return {}; }
        auto const& saved{*pointer}; auto const& gpr{saved.gregs};
        for(::std::size_t i{}; i != 32u; ++i) { fp_value(out.floating, i, &saved.fpregs.fpregs[i], 8u); }
        if(kernel_has_altivec && (gpr[33u] & 0x02000000u) != 0u)
        { for(::std::size_t i{}; i != 32u; ++i) { fp_vector_value(out.floating,32u+i,&saved.vrregs.vrregs[i]); } }
# endif
        for(::std::size_t i{}; i != 32u; ++i) { out.values[i] = gpr[i]; }
        out.values[32u] = gpr[32u]; out.values[33u] = gpr[33u];
        out.values[34u] = gpr[36u]; out.values[35u] = gpr[35u]; out.values[36u] = gpr[38u]; out.values[37u] = gpr[37u];
#elif defined(__mips__) && __SIZEOF_POINTER__ == 8
        out.machine = architecture::mips64;
        for(::std::size_t i{}; i != 32u; ++i) { out.values[i] = context.uc_mcontext.gregs[i]; }
        // USED_FP (1) alone proves that Linux wrote the scalar FP slots.
        // Other used_math flags do not. This eight-byte-pointer n64 ABI
        // always uses the complete FR=1 layout; QEMU saves all slots but
        // omits USED_FR1, so that flag must not suppress legitimate values.
        if((context.uc_mcontext.used_math & 1u) != 0u)
        { for(::std::size_t i{}; i != 32u; ++i) { fp_value(out.floating, i, &context.uc_mcontext.fpregs.fp_r.fp_dregs[i], 8u); } }
        // N64 kernel sigset_t contains 128 SIGNAL BITS (16 bytes), while
        // libc reserves 128 bytes. The inline MSA record starts after the
        // kernel mask, not after sizeof(ucontext_t). Never follow uc_link,
        // a saved SP, an unknown record length or an extension pointer.
        // Require saved FP + FR1 + EXTCONTEXT and refuse hybrid FPR layout.
        if((context.uc_mcontext.used_math & 15u) == 11u)
        {
            static_assert(sizeof(::mcontext_t) == 600u && offsetof(kernel_context, uc_sigmask) == 640u);
            auto const* tail{reinterpret_cast<unsigned char const*>(&context) + offsetof(kernel_context, uc_sigmask) + 16u};
            ::std::uint32_t magic{}, length{};
            ::std::memcpy(&magic, tail, 4u); ::std::memcpy(&length, tail + 4u, 4u);
            // Linux msa_extcontext: header8, upper halves256, CSR4,
            // padding4, then the four-byte END marker. No variable walk.
            if(magic == 0x784d5341u && length == 272u)
            {
                ::std::uint32_t end{}; ::std::memcpy(&end, tail + 272u, 4u);
                if(end == 0x78454e44u)
                {
                    for(::std::size_t i{}; i != 32u; ++i)
                    {
                        auto& vector{out.floating.values[32u + i]};
                        auto const& scalar{out.floating.values[i]};
                        // MSA ld.b puts lane0 in the low architectural byte
                        // on both endians. Normalize EACH u64 separately;
                        // reversing all 16 bytes would exchange the halves.
                        ::std::uint64_t upper{}; ::std::memcpy(&upper, tail + 8u + i * 8u, 8u);
                        for(unsigned byte{}; byte != 8u; ++byte)
                        {
                            vector.bytes[byte] = scalar.bytes[byte];
                            vector.bytes[8u + byte] = static_cast<unsigned char>(upper >> (byte * 8u));
                        }
                        vector.width = 16u;
                    }
                }
            }
        }
        out.values[32u] = context.uc_mcontext.pc; out.values[34u] = context.uc_mcontext.mdhi; out.values[35u] = context.uc_mcontext.mdlo;
#elif defined(__riscv) && __riscv_xlen == 64
        out.machine = architecture::riscv64;
        for(::std::size_t i{1u}; i != 32u; ++i) { out.values[i] = context.uc_mcontext.__gregs[i]; }
        out.values[32u] = context.uc_mcontext.__gregs[0u];
# if defined(__riscv_flen) && __riscv_flen >= 64
        for(::std::size_t i{}; i != 32u; ++i) { fp_value(out.floating, i, &context.uc_mcontext.__fpregs.__d.__f[i], 8u); }
# endif
#elif defined(__loongarch64)
        out.machine = architecture::loongarch64;
        for(::std::size_t i{}; i != 32u; ++i) { out.values[i] = context.uc_mcontext.__gregs[i]; }
        out.values[32u] = context.uc_mcontext.__pc;
        // A genuine kernel frame owns this extensible ABI tail. Read only
        // recognized complete records: never a supplied size or pointer chain.
        if((context.uc_mcontext.__flags & 1u) != 0u)
        {
            auto const* tail{reinterpret_cast<unsigned char const*>(context.uc_mcontext.__extcontext)};
            for(unsigned record{}; record != 2u; ++record)
            {
                ::std::uint32_t magic{},length{};
                ::std::memcpy(&magic,tail,4u); ::std::memcpy(&length,tail+4u,4u);
                if(magic == 0x42540001u && length == 64u && record == 0u) { tail += 64u; continue; }
                unsigned stride{};
                if(magic == 0x46505501u && length == 288u) { stride = 8u; }
                else if(magic == 0x53580001u && length == 544u) { stride = 16u; }
                // Linux's LASX payload is 1040 bytes; QEMU rounds it to
                // 1056. Tail alignment yields these three complete sizes;
                // register data starts at +16 in every case.
                else if(magic == 0x41535801u && (length == 1056u || length == 1072u || length == 1088u)) { stride = 32u; }
                else { break; }
                for(unsigned i{}; i != 32u; ++i) { fp_value(out.floating,i,tail+16u+i*stride,stride < 16u ? 8u : 16u); }
                break;
            }
        }
#elif defined(__sparc__) && defined(__arch64__)
        out.machine = architecture::sparc64;
        auto const& saved{context.saved};
        for(::std::size_t i{}; i != 16u; ++i) { out.values[i] = saved.registers[i]; }
        // These are the kernel's fixed RT frame window VALUES, preceding info
        // by exactly one complete sparc_stackf. No saved SP/FP is dereferenced.
        auto const* window{reinterpret_cast<sparc_signal_window const*>(
            reinterpret_cast<unsigned char const*>(&context)-sizeof(sparc_signal_window))};
        for(::std::size_t i{}; i != 8u; ++i) { out.values[16u+i] = window->locals[i]; out.values[24u+i] = window->inputs[i]; }
        out.values[32u] = saved.pc; out.values[33u] = saved.next_pc; out.values[34u] = saved.status; out.values[35u] = saved.y;
        if(context.fp_save == reinterpret_cast<::std::uintptr_t>(&context)+sizeof(context))
        {
            auto const* fpu{reinterpret_cast<unsigned char const*>(&context)+sizeof(context)};
            ::std::uint64_t saved_banks{};
            ::std::memcpy(&saved_banks, fpu + offsetof(sparc_signal_fpu, fprs), sizeof(saved_banks));
            // Linux writes each 128-byte bank only for FPRS_DL (1) or
            // FPRS_DU (2). FEF alone grants no saved register values. QEMU
            // writes both banks even when these flags are clear, so accepting
            // all bytes would expose unwritten real-kernel frame storage.
            for(::std::size_t i{}; i != 32u; ++i)
            { if((saved_banks & (i < 16u ? 1u : 2u)) != 0u) { fp_value(out.floating,i,fpu+i*8u,8u); } }
            // Every f0..f31 single alias belongs to the LOWER bank.
            if((saved_banks & 1u) != 0u)
            { for(::std::size_t i{}; i != 32u; ++i) { fp_value(out.floating,32u+i,fpu+i*4u,4u); } }
        }
#elif defined(__s390x__)
        out.machine = architecture::s390x;
        for(::std::size_t i{}; i != 16u; ++i)
        { out.values[i] = context.uc_mcontext.gregs[i]; fp_value(out.floating, i, &context.uc_mcontext.fpregs.fprs[i], 8u); }
        out.values[16u] = context.uc_mcontext.psw.addr; out.values[17u] = context.uc_mcontext.psw.mask;
#elif defined(__arm__)
        out.machine = architecture::arm;
        auto const& c{context.uc_mcontext};
        // The host handler may be Thumb code. Only the actual saved Wasm
        // instruction state determines this private ARM-state capability.
        if((c.arm_cpsr & (1u << 5u)) != 0u || (c.arm_pc & 3u) != 0u) { return {}; }
        out.values = {c.arm_r0,c.arm_r1,c.arm_r2,c.arm_r3,c.arm_r4,c.arm_r5,c.arm_r6,c.arm_r7,c.arm_r8,c.arm_r9,c.arm_r10,c.arm_fp,c.arm_ip,c.arm_sp,c.arm_lr,c.arm_pc,c.arm_cpsr};
        fixed_fp_records(out.floating, context.uc_regspace, sizeof(context.uc_regspace), 0x56465001u, 288u, 8u, 8u);
#endif
        return out;
    }
    inline void set_pc(kernel_context& context, ::std::uintptr_t pc) noexcept
    {
#if defined(__aarch64__)
        context.uc_mcontext.pc = pc;
#elif defined(__i386__)
        context.uc_mcontext.gregs[REG_EIP] = pc;
#elif defined(__powerpc64__)
        context.uc_mcontext.gp_regs[32u] = pc;
#elif defined(__powerpc__)
        context.uc_mcontext.uc_regs->gregs[32u] = pc;
#elif defined(__mips__) && __SIZEOF_POINTER__ == 8
        context.uc_mcontext.pc = pc;
#elif defined(__riscv) && __riscv_xlen == 64
        context.uc_mcontext.__gregs[0u] = pc;
#elif defined(__loongarch64)
        context.uc_mcontext.__pc = pc;
#elif defined(__sparc__) && defined(__arch64__)
        context.saved.pc = pc; // Preserve the real delayed-control NPC.
#elif defined(__s390x__)
        context.uc_mcontext.psw.addr = pc;
#elif defined(__arm__)
        // Never switch the saved ISA state or install an unaligned ARM PC.
        if((context.uc_mcontext.arm_cpsr & (1u << 5u)) == 0u && (pc & 3u) == 0u)
        { context.uc_mcontext.arm_pc = pc; }
#else
        (void)context; (void)pc;
#endif
    }
}
