/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)            *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
#pragma once
#include "native_debug_cfi_types.h"
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <initializer_list>
# include <span>
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::details
{
    // AAPCS32 ARM-state owned DATA only. r9 is platform-specific and may
    // contain TLS: it is not assumed to be a preserved numeric variable.
    // These rules grant no live read, Thumb transition or caller authority.
    struct native_debug_cfi_arm_owned_word
    {
        ::std::uintptr_t address{};
        ::std::uint32_t value{};
    };
    struct native_debug_cfi_arm_caller
    {
        ::std::array<::std::uint32_t, 16u> registers{};
        ::std::uint32_t known{};
        ::std::uintptr_t cfa{}, return_pc{};
    };
    template<typename Reader>
    [[nodiscard]] inline bool evaluate_native_debug_cfi_arm_bounded(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 16u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end, Reader const& read_word,
        native_debug_cfi_arm_caller& out) noexcept
    {
        out = {};
        // An interrupted ARM function may have only word-aligned SP; its
        // recovered caller SP is a public AAPCS32 interface, aligned to eight.
        if(!row.usable || row.cfa_register >= 15u || !(known & (1u << row.cfa_register)) ||
           !(known & (1u << 13u)) || frame_begin == 0u || frame_end < frame_begin ||
           frame_end > UINT32_MAX || registers[13u] != frame_begin || (frame_begin & 3u) != 0u)
        { return false; }
        auto const add{[](::std::uint64_t base, ::std::int32_t offset, ::std::uintptr_t& value) noexcept
        {
            if(base > UINT32_MAX) { return false; }
            auto const sum{static_cast<::std::int64_t>(base) + static_cast<::std::int64_t>(offset)};
            if(sum < 0 || sum > UINT32_MAX) { return false; }
            value = static_cast<::std::uintptr_t>(sum); return true;
        }};
        ::std::uintptr_t cfa{};
        if(!add(registers[row.cfa_register], row.cfa_offset, cfa) || cfa < frame_begin ||
           cfa > frame_end || (cfa & 7u) != 0u) { return false; }
        native_debug_cfi_arm_caller candidate{};
        for(unsigned reg : {4u, 5u, 6u, 7u, 8u, 10u, 11u, 14u})
        {
            auto const& rule{row.registers[reg]}; using K = native_debug_cfi_rule_kind;
            ::std::uintptr_t value{}; bool available{};
            if(rule.kind == K::same && (known & (1u << reg)))
            { available = add(registers[reg], 0, value); }
            else if(rule.kind == K::cfa_value || rule.kind == K::cfa_memory)
            { available = add(cfa, rule.offset, value); }
            else if((rule.kind == K::register_value || rule.kind == K::register_memory) &&
                    rule.reg < 15u && (known & (1u << rule.reg)))
            { available = add(registers[rule.reg], rule.offset, value); }
            if(available && (rule.kind == K::cfa_memory || rule.kind == K::register_memory))
            {
                ::std::uint32_t loaded{};
                available = (value & 3u) == 0u && value >= frame_begin && value < cfa &&
                    cfa - value >= 4u && read_word(value, loaded);
                value = loaded;
            }
            if(available)
            { candidate.registers[reg] = static_cast<::std::uint32_t>(value); candidate.known |= 1u << reg; }
        }
        // Do not silently clear Thumb's state bit, round an address, or turn
        // a PC rule into an LR rule. The current executor supports ARM state.
        if(!(candidate.known & (1u << 14u)) || candidate.registers[14u] == 0u ||
           (candidate.registers[14u] & 3u) != 0u) { return false; }
        candidate.cfa = cfa; candidate.return_pc = candidate.registers[14u];
        candidate.registers[13u] = static_cast<::std::uint32_t>(cfa); candidate.known |= 1u << 13u;
        out = candidate; return true;
    }
    [[nodiscard]] inline bool evaluate_native_debug_cfi_arm_sparse(native_debug_cfi_row const& row,
        ::std::array<::std::uint64_t, 16u> const& registers, ::std::uint32_t known,
        ::std::uintptr_t frame_begin, ::std::uintptr_t frame_end,
        ::std::span<native_debug_cfi_arm_owned_word const> words, native_debug_cfi_arm_caller& out) noexcept
    {
        out = {};
        if(words.size() > 8u || frame_end < frame_begin || frame_end > UINT32_MAX) { return false; }
        for(::std::size_t n{}; n != words.size(); ++n)
        {
            auto const address{words[n].address};
            if((address & 3u) != 0u || address < frame_begin || address >= frame_end ||
               frame_end - address < 4u) { return false; }
            for(::std::size_t prior{}; prior != n; ++prior)
            {
                auto const other{words[prior].address};
                if(address > other ? address - other < 4u : other - address < 4u) { return false; }
            }
        }
        auto const read{[&](::std::uintptr_t address, ::std::uint32_t& value) noexcept
        {
            for(auto const& word: words)
            { if(word.address == address) { value = word.value; return true; } }
            return false;
        }};
        return evaluate_native_debug_cfi_arm_bounded(row, registers, known, frame_begin, frame_end, read, out);
    }

}
