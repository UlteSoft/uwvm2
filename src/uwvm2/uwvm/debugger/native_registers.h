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
# include <string_view>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_registers
{
    // General-purpose registers from the stopped, selected native thread.
    // Values are immutable display data: no context pointer, native-memory
    // observer, or operation to write a register is part of this interface.
    enum class architecture : unsigned char { unavailable, x86_64, aarch64, i686, powerpc, mips64, riscv64, loongarch64, sparc64, s390x, arm };
    inline constexpr ::std::size_t max_registers{38u};
    inline constexpr ::std::size_t unavailable_index{max_registers};

    [[nodiscard]] inline constexpr ::std::size_t count(architecture machine) noexcept
    {
        switch(machine)
        {
            case architecture::x86_64: return 18u;
            case architecture::aarch64: return 34u;
            case architecture::i686: return 10u;
            case architecture::powerpc: return 38u;
            case architecture::mips64: return 36u;
            case architecture::riscv64: return 34u;
            case architecture::loongarch64: return 34u;
            case architecture::sparc64: return 36u;
            case architecture::s390x: return 18u;
            case architecture::arm: return 17u;
            default: return 0u;
        }
    }
    [[nodiscard]] inline constexpr ::std::size_t pc_index(architecture machine) noexcept
    {
        switch(machine)
        {
            case architecture::x86_64: return 16u;
            case architecture::i686: return 8u;
            case architecture::arm: return 15u;
            case architecture::s390x: return 16u;
            case architecture::unavailable: return max_registers;
            default: return count(machine) != 0u ? 32u : max_registers;
        }
    }
    [[nodiscard]] inline constexpr ::std::size_t sp_index(architecture machine) noexcept
    {
        switch(machine)
        {
            case architecture::x86_64: case architecture::i686: return 7u;
            case architecture::aarch64: return 31u;
            case architecture::powerpc: return 1u;
            case architecture::mips64: return 29u;
            case architecture::riscv64: return 2u;
            case architecture::loongarch64: return 3u;
            case architecture::sparc64: return 14u;
            case architecture::s390x: return 15u;
            case architecture::arm: return 13u;
            default: return max_registers;
        }
    }
    [[nodiscard]] inline constexpr ::std::size_t fp_index(architecture machine) noexcept
    {
        switch(machine)
        {
            case architecture::x86_64: case architecture::i686: return 6u;
            case architecture::aarch64: return 29u;
            case architecture::powerpc: return 31u;
            case architecture::mips64: return 30u;
            case architecture::riscv64: return 8u;
            case architecture::loongarch64: return 22u;
            case architecture::sparc64: return 30u;
            case architecture::arm: case architecture::s390x: return 11u;
            default: return max_registers;
        }
    }
    [[nodiscard]] inline constexpr char8_t const* name(architecture machine, ::std::size_t index) noexcept
    {
        constexpr ::std::array<char8_t const*, 18u> x86{
            u8"rax", u8"rbx", u8"rcx", u8"rdx", u8"rsi", u8"rdi", u8"rbp", u8"rsp",
            u8"r8", u8"r9", u8"r10", u8"r11", u8"r12", u8"r13", u8"r14", u8"r15", u8"rip", u8"rflags"};
        constexpr ::std::array<char8_t const*, 34u> aarch64{
            u8"x0", u8"x1", u8"x2", u8"x3", u8"x4", u8"x5", u8"x6", u8"x7", u8"x8", u8"x9",
            u8"x10", u8"x11", u8"x12", u8"x13", u8"x14", u8"x15", u8"x16", u8"x17", u8"x18", u8"x19",
            u8"x20", u8"x21", u8"x22", u8"x23", u8"x24", u8"x25", u8"x26", u8"x27", u8"x28", u8"fp",
            u8"lr", u8"sp", u8"pc", u8"cpsr"};
        // [fixed architecture register-name table] end
        // [safe                                 ] check the selected table extent before indexing.
        //  ^^ the returned name has static storage, never a target-supplied pointer.
        if(machine == architecture::x86_64 && index < x86.size()) { return x86[index]; }
        if(machine == architecture::aarch64 && index < aarch64.size()) { return aarch64[index]; }
        constexpr ::std::array<char8_t const*, 10u> i686{u8"eax", u8"ebx", u8"ecx", u8"edx", u8"esi", u8"edi", u8"ebp", u8"esp", u8"eip", u8"eflags"};
        constexpr ::std::array<char8_t const*, 38u> powerpc{u8"r0", u8"r1", u8"r2", u8"r3", u8"r4", u8"r5", u8"r6", u8"r7", u8"r8", u8"r9", u8"r10", u8"r11", u8"r12", u8"r13", u8"r14", u8"r15", u8"r16", u8"r17", u8"r18", u8"r19", u8"r20", u8"r21", u8"r22", u8"r23", u8"r24", u8"r25", u8"r26", u8"r27", u8"r28", u8"r29", u8"r30", u8"r31", u8"pc", u8"msr", u8"lr", u8"ctr", u8"cr", u8"xer"};
        constexpr ::std::array<char8_t const*, 36u> mips64{u8"zero", u8"at", u8"v0", u8"v1", u8"a0", u8"a1", u8"a2", u8"a3", u8"a4", u8"a5", u8"a6", u8"a7", u8"t0", u8"t1", u8"t2", u8"t3", u8"s0", u8"s1", u8"s2", u8"s3", u8"s4", u8"s5", u8"s6", u8"s7", u8"t8", u8"t9", u8"k0", u8"k1", u8"gp", u8"sp", u8"fp", u8"ra", u8"pc", u8"status", u8"hi", u8"lo"};
        constexpr ::std::array<char8_t const*, 34u> riscv64{u8"zero", u8"ra", u8"sp", u8"gp", u8"tp", u8"t0", u8"t1", u8"t2", u8"s0", u8"s1", u8"a0", u8"a1", u8"a2", u8"a3", u8"a4", u8"a5", u8"a6", u8"a7", u8"s2", u8"s3", u8"s4", u8"s5", u8"s6", u8"s7", u8"s8", u8"s9", u8"s10", u8"s11", u8"t3", u8"t4", u8"t5", u8"t6", u8"pc", u8"status"};
        constexpr ::std::array<char8_t const*, 34u> loongarch64{u8"zero", u8"ra", u8"tp", u8"sp", u8"a0", u8"a1", u8"a2", u8"a3", u8"a4", u8"a5", u8"a6", u8"a7", u8"t0", u8"t1", u8"t2", u8"t3", u8"t4", u8"t5", u8"t6", u8"t7", u8"t8", u8"r21", u8"fp", u8"s0", u8"s1", u8"s2", u8"s3", u8"s4", u8"s5", u8"s6", u8"s7", u8"s8", u8"pc", u8"status"};
        constexpr ::std::array<char8_t const*, 36u> sparc64{u8"g0", u8"g1", u8"g2", u8"g3", u8"g4", u8"g5", u8"g6", u8"g7", u8"o0", u8"o1", u8"o2", u8"o3", u8"o4", u8"o5", u8"o6", u8"o7", u8"l0", u8"l1", u8"l2", u8"l3", u8"l4", u8"l5", u8"l6", u8"l7", u8"i0", u8"i1", u8"i2", u8"i3", u8"i4", u8"i5", u8"i6", u8"i7", u8"pc", u8"npc", u8"tstate", u8"y"};
        constexpr ::std::array<char8_t const*, 18u> s390x{u8"r0", u8"r1", u8"r2", u8"r3", u8"r4", u8"r5", u8"r6", u8"r7", u8"r8", u8"r9", u8"r10", u8"r11", u8"r12", u8"r13", u8"r14", u8"r15", u8"pc", u8"psw"};
        constexpr ::std::array<char8_t const*, 17u> arm32{u8"r0", u8"r1", u8"r2", u8"r3", u8"r4", u8"r5", u8"r6", u8"r7", u8"r8", u8"r9", u8"r10", u8"r11", u8"r12", u8"sp", u8"lr", u8"pc", u8"cpsr"};
        if(machine == architecture::i686 && index < i686.size()) { return i686[index]; }
        if(machine == architecture::powerpc && index < powerpc.size()) { return powerpc[index]; }
        if(machine == architecture::mips64 && index < mips64.size()) { return mips64[index]; }
        if(machine == architecture::riscv64 && index < riscv64.size()) { return riscv64[index]; }
        if(machine == architecture::loongarch64 && index < loongarch64.size()) { return loongarch64[index]; }
        if(machine == architecture::sparc64 && index < sparc64.size()) { return sparc64[index]; }
        if(machine == architecture::s390x && index < s390x.size()) { return s390x[index]; }
        if(machine == architecture::arm && index < arm32.size()) { return arm32[index]; }
        return nullptr;
    }
    [[nodiscard]] inline constexpr ::std::size_t index_of(architecture machine, ::std::u8string_view requested) noexcept
    {
        // The optional '$' follows GDB's register-expression spelling. This
        // is a bounded token comparison, not parsing a number or host address.
        if(!requested.empty() && requested.front() == u8'$')
        {
            // [borrowed request token ... end] size >= 1
            // [safe                          ] remove one character only after nonempty check.
            //                          ^^ the view moves within its caller-owned token.
            requested.remove_prefix(1u);
        }
        if(machine == architecture::x86_64)
        {
            if(requested == u8"pc") { return 16u; }
            if(requested == u8"sp") { return 7u; }
            if(requested == u8"fp") { return 6u; }
            if(requested == u8"eflags" || requested == u8"flags" || requested == u8"ps") { return 17u; }
        }
        else if(machine == architecture::aarch64)
        {
            if(requested == u8"x29") { return 29u; }
            if(requested == u8"x30") { return 30u; }
            if(requested == u8"flags" || requested == u8"ps") { return 33u; }
        }
        if(requested == u8"pc") { return pc_index(machine); }
        if(requested == u8"sp") { return sp_index(machine); }
        if(requested == u8"fp") { return fp_index(machine); }
        for(::std::size_t index{}; index < count(machine); ++index)
        {
            // [architecture names ... count <= max_registers] end
            // [safe                                        ] count selects a valid static table above.
            //  ^^ scalar index advances only within that bounded register set.
            if(requested == name(machine, index)) { return index; }
        }
        return unavailable_index;
    }

    // Owned FXSAVE register VALUES only. No signal-frame/native-memory
    // pointer, x87 instruction/data address or kernel-reserved byte escapes.
    // Baseline XMM and x87 state is independent of optional XSAVE extensions;
    // YMM/ZMM are unavailable until their complete kernel layout is qualified.
    inline constexpr ::std::size_t max_fp_registers{64u};
    struct fp_value
    {
        ::std::array<::std::uint8_t, 16u> bytes{};
        ::std::uint8_t width{}; // little-endian architectural register bits
        friend constexpr bool operator==(fp_value const&, fp_value const&) noexcept = default;
    };
    struct fp_snapshot
    {
        bool available{};
        ::std::array<fp_value, max_fp_registers> values{};
        friend constexpr bool operator==(fp_snapshot const&, fp_snapshot const&) noexcept = default;
    };
    [[nodiscard]] inline constexpr char8_t const* fp_name(architecture machine, ::std::size_t index) noexcept
    {
        constexpr ::std::array<char8_t const*, 30u> names{
            u8"xmm0", u8"xmm1", u8"xmm2", u8"xmm3", u8"xmm4", u8"xmm5", u8"xmm6", u8"xmm7",
            u8"xmm8", u8"xmm9", u8"xmm10", u8"xmm11", u8"xmm12", u8"xmm13", u8"xmm14", u8"xmm15",
            u8"st0", u8"st1", u8"st2", u8"st3", u8"st4", u8"st5", u8"st6", u8"st7",
            u8"fcw", u8"fsw", u8"ftw", u8"fop", u8"mxcsr", u8"mxcsr_mask"};
        // [fixed register names ... index<30] end
        // [safe                             ] architecture and extent BEFORE indexing.
        //  ^^ returned text has static storage and grants no target access.
        if(machine == architecture::x86_64 && index < names.size()) { return names[index]; }
        if(machine == architecture::i686 && index < 8u) { return names[index]; }
        constexpr ::std::array<char8_t const*, 32u> f{u8"f0", u8"f1", u8"f2", u8"f3", u8"f4", u8"f5", u8"f6", u8"f7", u8"f8", u8"f9", u8"f10", u8"f11", u8"f12", u8"f13", u8"f14", u8"f15", u8"f16", u8"f17", u8"f18", u8"f19", u8"f20", u8"f21", u8"f22", u8"f23", u8"f24", u8"f25", u8"f26", u8"f27", u8"f28", u8"f29", u8"f30", u8"f31" };
        constexpr ::std::array<char8_t const*, 32u> v{u8"v0", u8"v1", u8"v2", u8"v3", u8"v4", u8"v5", u8"v6", u8"v7", u8"v8", u8"v9", u8"v10", u8"v11", u8"v12", u8"v13", u8"v14", u8"v15", u8"v16", u8"v17", u8"v18", u8"v19", u8"v20", u8"v21", u8"v22", u8"v23", u8"v24", u8"v25", u8"v26", u8"v27", u8"v28", u8"v29", u8"v30", u8"v31" };
        constexpr ::std::array<char8_t const*, 32u> d{u8"d0", u8"d1", u8"d2", u8"d3", u8"d4", u8"d5", u8"d6", u8"d7", u8"d8", u8"d9", u8"d10", u8"d11", u8"d12", u8"d13", u8"d14", u8"d15", u8"d16", u8"d17", u8"d18", u8"d19", u8"d20", u8"d21", u8"d22", u8"d23", u8"d24", u8"d25", u8"d26", u8"d27", u8"d28", u8"d29", u8"d30", u8"d31" };
        if(machine == architecture::aarch64 && index < 32u) { return v[index]; }
        if((machine == architecture::arm || machine == architecture::sparc64) && index < 32u) { return d[index]; }
        if(machine == architecture::sparc64 && index >= 32u && index < 64u) { return f[index-32u]; }
        if(machine == architecture::powerpc && index >= 32u && index < 64u) { return v[index-32u]; }
        constexpr ::std::array<char8_t const*, 32u> w{u8"w0", u8"w1", u8"w2", u8"w3", u8"w4", u8"w5", u8"w6", u8"w7", u8"w8", u8"w9", u8"w10", u8"w11", u8"w12", u8"w13", u8"w14", u8"w15", u8"w16", u8"w17", u8"w18", u8"w19", u8"w20", u8"w21", u8"w22", u8"w23", u8"w24", u8"w25", u8"w26", u8"w27", u8"w28", u8"w29", u8"w30", u8"w31"};
        if(machine == architecture::mips64 && index >= 32u && index < 64u) { return w[index-32u]; }
        if((machine == architecture::powerpc || machine == architecture::mips64 || machine == architecture::riscv64 ||
            machine == architecture::loongarch64 ||
            (machine == architecture::s390x && index < 16u)) && index < 32u) { return f[index]; }
        return nullptr;
    }
    [[nodiscard]] inline constexpr ::std::size_t fp_width(::std::size_t index) noexcept
    {
        // Intel FXSAVE abridged FTW is ONE byte at offset4; byte5 is
        // reserved. FOP keeps its separate two-byte field at offset6.
        return index < 16u ? 16u : index < 24u ? 10u : index == 26u ? 1u :
            index < 28u ? 2u : index < 30u ? 4u : 0u;
    }
    [[nodiscard]] inline constexpr ::std::size_t fp_index_of(architecture machine, ::std::u8string_view requested) noexcept
    {
        if(count(machine) == 0u) { return max_fp_registers; }
        if(!requested.empty() && requested.front() == u8'$')
        {
            // [owned request token ... size>=1] end
            // [safe                           ] remove one character after nonempty check.
            //                           ^^ view stays within this synchronous token borrow.
            requested.remove_prefix(1u);
        }
        constexpr ::std::array<char8_t const*, 8u> stack_names{
            u8"st(0)", u8"st(1)", u8"st(2)", u8"st(3)", u8"st(4)", u8"st(5)", u8"st(6)", u8"st(7)"};
        for(::std::size_t index{}; index != max_fp_registers; ++index)
        {
            // [static bounded fp-name table] end
            // [safe                        ] index<30; stack index is formed only for [16,24).
            //  ^^ scalar iteration never reads a captured native address.
            auto const* register_name{fp_name(machine, index)};
            if(register_name != nullptr && (requested == register_name ||
               (machine == architecture::x86_64 && index >= 16u && index < 24u && requested == stack_names[index - 16u]))) { return index; }
        }
        return max_fp_registers;
    }

    [[nodiscard]] inline constexpr ::std::size_t fp_width(architecture machine, ::std::size_t index) noexcept
    {
        if(fp_name(machine, index) == nullptr) { return 0u; }
        if(machine == architecture::x86_64 || machine == architecture::i686) { return fp_width(index); }
        if(machine == architecture::sparc64 && index >= 32u) { return 4u; }
        return machine == architecture::aarch64 || machine == architecture::loongarch64 ||
            ((machine == architecture::powerpc || machine == architecture::mips64) && index >= 32u) ? 16u : 8u;
    }
    struct snapshot
    {
        architecture machine{architecture::unavailable};
        ::std::array<::std::uint64_t, max_registers> values{};
        fp_snapshot floating{};
        unsigned char word_bits{64u};
        // A kernel ABI may omit registers (notably SPARC register windows).
        // Capture NEVER walks an integer SP to fabricate those values.
        ::std::array<bool, max_registers> unavailable_registers{};
        [[nodiscard]] constexpr ::std::size_t size() const noexcept { return count(machine); }
        [[nodiscard]] constexpr ::std::uint64_t pc() const noexcept
        { auto const index{pc_index(machine)}; return index < size() ? values[index] : 0u; }
        [[nodiscard]] constexpr ::std::uint64_t sp() const noexcept
        { auto const index{sp_index(machine)}; return index < size() ? values[index] : 0u; }
        [[nodiscard]] constexpr ::std::uint64_t fp() const noexcept
        { auto const index{fp_index(machine)}; return index < size() && !unavailable_registers[index] ? values[index] : 0u; }
    };
    // External values are projected from compiler-qualified numeric locations.
    // The authentic kernel snapshot remains private for continuation checks.
    struct display_snapshot : snapshot
    {
        ::std::array<unsigned char, max_registers> known_bits{};
    };
    struct numeric_location { unsigned dwarf_register{}, bits{}; };
    // Classic PPC FPRs hold scalar single values in double encoding. Convert
    // only the proved f32 value; do not publish the other physical bits or use
    // host FP arithmetic/rounding/status. Reject non-representable encodings.
    [[nodiscard]] inline bool ppc_single_value(fp_value const& source, ::std::uint32_t& out) noexcept
    {
        if(source.width != 8u) { return false; }
        ::std::uint64_t bits{};
        for(unsigned i{}; i != 8u; ++i) { bits |= ::std::uint64_t{source.bytes[i]} << (8u*i); }
        auto const exponent{static_cast<unsigned>((bits >> 52u) & 0x7ffu)};
        auto const fraction{bits & 0x000fffffffffffffull};
        auto const sign{static_cast<::std::uint32_t>(bits >> 63u) << 31u};
        if(exponent == 0x7ffu)
        {
            auto const payload{static_cast<::std::uint32_t>(fraction >> 29u)};
            if(fraction != 0u && payload == 0u) { return false; }
            out = sign | 0x7f800000u | payload; return true;
        }
        if(exponent == 0u && fraction == 0u) { out = sign; return true; }
        if(exponent >= 897u && exponent <= 1150u)
        {
            if((fraction & ((::std::uint64_t{1u} << 29u)-1u)) != 0u) { return false; }
            out = sign | ((exponent-896u) << 23u) | static_cast<::std::uint32_t>(fraction >> 29u); return true;
        }
        if(exponent >= 874u && exponent <= 896u)
        {
            unsigned const shift{926u-exponent}; auto const significand{fraction | (::std::uint64_t{1u} << 52u)};
            if((significand & ((::std::uint64_t{1u} << shift)-1u)) != 0u) { return false; }
            auto const subnormal{significand >> shift};
            if(subnormal == 0u || subnormal >= 0x800000u) { return false; }
            out = sign | static_cast<::std::uint32_t>(subnormal); return true;
        }
        return false;
    }
    [[nodiscard]] inline display_snapshot project(snapshot const& raw,
        numeric_location const* locations, ::std::size_t size) noexcept
    {
        display_snapshot result{}; result.machine = raw.machine; result.word_bits = raw.word_bits;
        auto const pc_index{native_registers::pc_index(raw.machine)};
        if(raw.size() != 0u) { result.values[pc_index] = raw.pc(); result.known_bits[pc_index] = raw.word_bits == 32u ? 32u : 64u; }
        if(locations == nullptr || size > 64u) { return result; }
        result.floating.available = raw.floating.available;
        for(::std::size_t i{}; i != size; ++i)
        {
            auto const location{locations[i]};
            ::std::size_t index{max_registers};
            if(raw.machine == architecture::x86_64 && location.dwarf_register < 16u)
            {
                constexpr ::std::array<unsigned char, 16u> mapping{0u,3u,2u,1u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u};
                index = mapping[location.dwarf_register];
                if(index == 6u || index == 7u) { continue; } // frame/stack pointers are never numeric display authority
            }
            else if(raw.machine == architecture::aarch64 && location.dwarf_register < 29u && location.dwarf_register != 18u)
            { index = location.dwarf_register; }
            else if(raw.machine == architecture::i686 && location.dwarf_register < 8u)
            {
                constexpr unsigned char mapping[]{0u,2u,3u,1u,7u,6u,4u,5u};
                index = mapping[location.dwarf_register];
            }
            else if((raw.machine == architecture::powerpc || raw.machine == architecture::mips64 ||
                     raw.machine == architecture::riscv64 || raw.machine == architecture::loongarch64 ||
                     raw.machine == architecture::sparc64) && location.dwarf_register < 32u)
            { index = location.dwarf_register; }
            else if((raw.machine == architecture::s390x && location.dwarf_register < 16u) ||
                    (raw.machine == architecture::arm && location.dwarf_register < 13u))
            { index = location.dwarf_register; }
            if(index < raw.size() && (index == sp_index(raw.machine) || index == fp_index(raw.machine))) { continue; }
            // ABI infrastructure, zero/return-link registers and TLS/GOT bases
            // remain hidden even when a malformed location calls them numeric.
            if(index < raw.size() && ((raw.machine == architecture::powerpc && (index == 2u || index == 13u)) ||
               (raw.machine == architecture::mips64 && (index == 0u || (index >= 26u && index <= 28u) || index == 31u)) ||
               (raw.machine == architecture::riscv64 && index <= 4u) ||
               (raw.machine == architecture::loongarch64 && (index <= 3u || index == 21u)) ||
               (raw.machine == architecture::sparc64 && (index == 0u || index == 6u || index == 7u || index == 15u || index >= 30u)) ||
               (raw.machine == architecture::s390x && index == 14u)))
            { continue; }
            if(index < raw.size() && !raw.unavailable_registers[index] && location.bits <= raw.word_bits && (location.bits == 32u || location.bits == 64u))
            {
                // Only the proved low bits are copied; an i32 never releases
                // a residual upper half of a physical 64-bit register.
                auto const bits{result.known_bits[index] == 0u ? location.bits :
                    (::std::min)(static_cast<unsigned>(result.known_bits[index]), location.bits)};
                result.values[index] = bits == 32u ? raw.values[index] & 0xffffffffu : raw.values[index];
                result.known_bits[index] = static_cast<unsigned char>(bits);
            }
            else if(raw.machine == architecture::x86_64 && location.dwarf_register >= 17u && location.dwarf_register < 33u &&
                    raw.floating.available && (location.bits == 32u || location.bits == 64u || location.bits == 128u))
            {
                auto const fp_index{location.dwarf_register - 17u};
                auto& destination{result.floating.values[fp_index]}; auto const& source{raw.floating.values[fp_index]};
                auto width{location.bits / 8u};
                if(source.width != 16u) { continue; }
                if(destination.width != 0u) { width = (::std::min)(width, static_cast<unsigned>(destination.width)); }
                destination = {}; destination.width = static_cast<::std::uint8_t>(width);
                for(::std::size_t byte{}; byte != width; ++byte) { destination.bytes[byte] = source.bytes[byte]; }
            }
            else if(raw.floating.available)
            {
                ::std::size_t fp{max_fp_registers};
                if(raw.machine == architecture::aarch64 && location.dwarf_register >= 64u && location.dwarf_register < 96u)
                { fp = location.dwarf_register - 64u; }
                else if((raw.machine == architecture::powerpc || raw.machine == architecture::mips64 ||
                         raw.machine == architecture::riscv64 || raw.machine == architecture::loongarch64) &&
                        location.dwarf_register >= 32u && location.dwarf_register < 64u)
                {
                    fp = location.dwarf_register - 32u;
                    // LLVM MIPS Dn and Wn share DWARF32+n. Only a v128
                    // role selects the separately saved, complete MSA slot;
                    // an f32/f64 role never grants its residual upper half.
                    if(raw.machine == architecture::mips64 && location.bits == 128u)
                    {
                        bool scalar_alias{};
                        for(::std::size_t other{}; other != size; ++other)
                        {
                            auto const alias{locations[other]};
                            if(alias.dwarf_register == location.dwarf_register && (alias.bits == 32u || alias.bits == 64u))
                            { scalar_alias = true; break; }
                        }
                        // Conflicting scalar/vector roles for the same
                        // physical register intersect at the scalar width.
                        // A second spelling must not bypass that bound.
                        if(scalar_alias) { continue; }
                        fp += 32u;
                    }
                }
                else if(raw.machine == architecture::sparc64 && location.dwarf_register >= 32u && location.dwarf_register < 64u && location.bits == 32u)
                { fp = location.dwarf_register; }
                else if(raw.machine == architecture::sparc64 && location.dwarf_register >= 72u && location.dwarf_register < 88u && location.bits == 64u)
                { fp = location.dwarf_register - 72u; }
                else if(raw.machine == architecture::i686 && location.dwarf_register >= 21u && location.dwarf_register < 29u)
                { fp = location.dwarf_register - 21u; }
                else if(raw.machine == architecture::arm && location.dwarf_register >= 256u && location.dwarf_register < 288u)
                { fp = location.dwarf_register - 256u; }
                else if(raw.machine == architecture::s390x && location.dwarf_register >= 16u && location.dwarf_register < 32u)
                {
                    constexpr unsigned char mapping[]{0u,2u,4u,6u,1u,3u,5u,7u,8u,10u,12u,14u,9u,11u,13u,15u};
                    fp = mapping[location.dwarf_register-16u];
                }
                // LLVM's PPC DWARF register table uses 77..108 for VMX;
                // GNU producer metadata also uses the 1124..1155 spelling.
                // Both identify only these saved physical vector registers.
                else if(raw.machine == architecture::powerpc && location.dwarf_register >= 77u && location.dwarf_register < 109u)
                { fp = 32u + location.dwarf_register - 77u; }
                else if(raw.machine == architecture::powerpc && location.dwarf_register >= 1124u && location.dwarf_register < 1156u)
                { fp = 32u + location.dwarf_register - 1124u; }
                if(fp >= max_fp_registers || fp_name(raw.machine, fp) == nullptr ||
                   (location.bits != 32u && location.bits != 64u && location.bits != 128u)) { continue; }
                auto const& source{raw.floating.values[fp]}; auto& destination{result.floating.values[fp]};
                auto width{location.bits / 8u};
                if(source.width < width || source.width > source.bytes.size()) { continue; }
                if(destination.width != 0u) { width = (::std::min)(width, static_cast<unsigned>(destination.width)); }
                destination = {}; destination.width = static_cast<::std::uint8_t>(width);
                // SystemZ F32 is the HIGH half of its F64 physical register.
                // The other half may be inherited host state, and must never
                // be interpreted as the Wasm f32 location's low bits.
                auto const source_offset{raw.machine == architecture::s390x && width == 4u ? 4u : 0u};
                if(raw.machine == architecture::powerpc && fp < 32u && width == 4u)
                {
                    ::std::uint32_t single{};
                    if(!ppc_single_value(source,single)) { destination = {}; continue; }
                    for(unsigned byte{}; byte != 4u; ++byte) { destination.bytes[byte] = static_cast<::std::uint8_t>(single >> (8u*byte)); }
                    continue;
                }
                if(source_offset + width > source.width) { destination = {}; continue; }
                for(::std::size_t byte{}; byte != width; ++byte) { destination.bytes[byte] = source.bytes[source_offset + byte]; }
            }
        }
        return result;
    }
    static_assert(count(architecture::x86_64) <= max_registers);
    static_assert(count(architecture::aarch64) <= max_registers);
}
