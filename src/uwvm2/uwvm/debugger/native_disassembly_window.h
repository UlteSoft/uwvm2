/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include "native_disassembly.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_disassembly
{
    inline constexpr ::std::int64_t max_window_byte_offset{65536};
    inline constexpr ::std::size_t max_window_count{32u};
    // Bound one wire request, independently of the full function size or
    // decoded instruction count. Later pages remain relative to a real stop.
    inline constexpr ::std::int64_t max_window_instruction_offset{8704};
    struct window
    {
        bool available{};
        ::std::size_t count{};
        ::std::array<instruction, max_window_count> instructions{};
    };

    // This helper consumes OWNED copied bytes, not a publication or credential.
    // owner_begin/stop_pc are display positions; neither becomes a host pointer.
    // Only the caller's private runtime query can authenticate these arguments.
    // Decode FORWARD from the exact function entry: never guess an x86 boundary
    // by looking backwards at an arbitrary PC. Unknown boundaries fail closed.
    template<typename Decoder>
    [[nodiscard]] inline window decode_window_with(Decoder& decoder,
        ::std::span<::std::uint8_t const> owned, ::std::uintptr_t owner_begin,
        ::std::uintptr_t stop_pc, ::std::int64_t byte_offset,
        ::std::int64_t instruction_offset, ::std::size_t requested,
        ::std::span<::std::uint8_t const> instruction_code = {}) noexcept
    {
        window result{};
        // Preserve bounded requests while accepting every representable owned
        // function. The margin proves signed target/index/page arithmetic below.
        constexpr auto arithmetic_limit{INT64_MAX - max_window_instruction_offset -
            static_cast<::std::int64_t>(max_window_count) - max_window_byte_offset};
        if(!decoder || owner_begin == 0u || owned.empty() || owned.size() > PTRDIFF_MAX ||
           owned.size() > static_cast<::std::uint64_t>(arithmetic_limit) ||
           owned.size() > UINTPTR_MAX - owner_begin || stop_pc < owner_begin ||
           stop_pc - owner_begin >= owned.size() || requested == 0u || requested > max_window_count ||
           byte_offset < -max_window_byte_offset || byte_offset > max_window_byte_offset ||
           instruction_offset < -max_window_instruction_offset || instruction_offset > max_window_instruction_offset)
        { return result; }
        // Only the private runtime supplies an ARM ELF mapping mask. It is
        // not code permission. Require complete aligned 0/1 regions; do not
        // infer a restart from undecodable bytes or arbitrary offsets.
        if(!instruction_code.empty())
        {
            if(instruction_code.size()!=owned.size() || owner_begin%4u!=0u ||
               owned.size()%4u!=0u || instruction_code[0u]!=1u ||
               instruction_code[stop_pc-owner_begin]!=1u) { return {}; }
            for(::std::size_t i{};i<instruction_code.size();++i)
            {
                if(instruction_code[i]>1u ||
                   (i!=0u && instruction_code[i]!=instruction_code[i-1u] && i%4u!=0u)) { return {}; }
            }
        }
        if constexpr(requires { decoder.fixed_instruction_bytes(); })
        {
            if(decoder.fixed_instruction_bytes() == 4u && instruction_code.empty() &&
               byte_offset == 0 && instruction_offset == 0)
            {
                // A64's exact four-byte ISA proves an aligned stopped slot
                // without printing a potentially huge initialization prefix.
                // This is OWNED DATA only. Actual MC still decodes every
                // returned slot; unknown/truncated bytes stop this page and
                // never become a resynchronization or execution permission.
                if(owner_begin % 4u != 0u || stop_pc % 4u != 0u || owned.size() % 4u != 0u) { return {}; }
                auto cursor{static_cast<::std::size_t>(stop_pc - owner_begin)};
                for(::std::size_t index{}; index != requested; ++index)
                {
                    if(owned.size() - cursor < 4u) { break; }
                    auto const pc{owner_begin + cursor};
                    auto const decoded{decoder.decode(pc,{owned.data() + cursor,4u})};
                    if(!decoded || decoded.pc != pc || decoded.size != 4u)
                    { if(index == 0u) { return {}; } break; }
                    result.instructions[index] = decoded; cursor += 4u;
                    if(cursor == owned.size()) { break; }
                }
                result.available = true; result.count = requested;
                return result;
            }
        }
        auto const target{static_cast<::std::int64_t>(stop_pc - owner_begin) + byte_offset};
        auto const inside{target >= 0 && target < static_cast<::std::int64_t>(owned.size())};
        if(inside && !instruction_code.empty() && instruction_code[static_cast<::std::size_t>(target)]!=1u)
        { return {}; } // A literal-pool byte never becomes a requested instruction.
        constexpr auto absent{(::std::numeric_limits<::std::size_t>::max)()};
        ::std::size_t cursor{}, total{}, target_index{absent};
        ::std::size_t region_end{instruction_code.empty() ? owned.size() : 0u};
        auto const advance_region{[&]() noexcept
        {
            if(!instruction_code.empty() && cursor==region_end)
            {
                while(cursor<owned.size() && instruction_code[cursor]==0u) { ++cursor; }
                region_end=cursor;
                while(region_end<owned.size() && instruction_code[region_end]==1u) { ++region_end; }
            }
            return cursor<owned.size();
        }};
        bool stop_seen{};
        // First prove the stop and byte target by forward decoding. Only scalar
        // indices are retained: no 8192-entry table and no allocation in this
        // noexcept policy. Unknown bytes never become a guessed new boundary.
        while(cursor < owned.size())
        {
            if(!advance_region()) { break; }
            // [owned.data ... cursor < owned.size) end
            // [safe] authenticate scalar extent BEFORE forming the suffix.
            auto const decoded{decoder.decode(owner_begin + cursor, {owned.data() + cursor, region_end - cursor})};
            if(!decoded || decoded.pc != owner_begin + cursor || decoded.size == 0u || decoded.size > region_end - cursor)
            {
                if(!stop_seen) { return {}; }
                break;
            }
            if(owner_begin + cursor == stop_pc) { stop_seen = true; }
            if(inside && cursor == static_cast<::std::size_t>(target)) { target_index = total; }
            if(instruction_offset >= 0 && target_index != absent)
            {
                // Retain only the bounded forward page during this SAME
                // entry-to-stop proof. If the stop is still ahead, continue
                // proving it; no retained row alone makes the window available.
                auto const index{static_cast<::std::int64_t>(total) -
                    static_cast<::std::int64_t>(target_index) - instruction_offset};
                if(index >= 0 && index < static_cast<::std::int64_t>(requested))
                { result.instructions[static_cast<::std::size_t>(index)] = decoded; }
            }
            ++total;
            cursor += decoded.size; // nonzero size <= remaining proves progress.
            if(stop_seen && target_index != absent &&
               static_cast<::std::int64_t>(total) >= static_cast<::std::int64_t>(target_index) +
                   instruction_offset + static_cast<::std::int64_t>(requested)) { break; }
        }
        if(!stop_seen) { return {}; }
        result.available = true; result.count = requested;
        if(!inside) { return result; }
        if(target_index == absent)
        {
            // Inside an already decoded instruction is not a boundary; an
            // unknown suffix is only an unavailable filler, never resynchronized.
            if(static_cast<::std::size_t>(target) < cursor) { return {}; }
            return result;
        }
        auto const first{static_cast<::std::int64_t>(target_index) + instruction_offset};
        auto const last{first + static_cast<::std::int64_t>(requested) - 1};
        if(last < 0 || first >= static_cast<::std::int64_t>(total)) { return result; }
        // Nonnegative pages were copied from the exact validated first
        // walk. Negative instruction offsets still use the bounded second
        // forward pass; no reverse decode, guessed boundary or table quota.
        if(instruction_offset >= 0) { return result; }
        // A second forward walk materializes at most one bounded page from the
        // known prefix. Negative instruction offsets are never a backwards read
        // or x86 resynchronization. Memory usage is constant for large functions.
        auto const prefix_end{cursor}; cursor = 0u;
        region_end=instruction_code.empty() ? owned.size() : 0u;
        ::std::size_t ordinal{};
        while(cursor < prefix_end && static_cast<::std::int64_t>(ordinal) <= last)
        {
            if(!advance_region() || cursor>=prefix_end) { break; }
            auto const decoded{decoder.decode(owner_begin + cursor, {owned.data() + cursor, region_end - cursor})};
            if(!decoded || decoded.pc != owner_begin + cursor || decoded.size == 0u || decoded.size > prefix_end - cursor || decoded.size > region_end - cursor)
            { return {}; }
            auto const index{static_cast<::std::int64_t>(ordinal) - first};
            if(index >= 0 && index < static_cast<::std::int64_t>(requested))
            { result.instructions[static_cast<::std::size_t>(index)] = decoded; }
            ++ordinal; cursor += decoded.size;
        }
        return result;
    }
    [[nodiscard]] inline window decode_window(::std::span<::std::uint8_t const> owned,
        ::std::uintptr_t owner_begin, ::std::uintptr_t stop_pc, ::std::int64_t byte_offset,
        ::std::int64_t instruction_offset, ::std::size_t requested,
        ::std::span<::std::uint8_t const> instruction_code = {}) noexcept
    {
        decoder context{};
        return decode_window_with(context, owned, owner_begin, stop_pc, byte_offset, instruction_offset, requested, instruction_code);
    }
}
