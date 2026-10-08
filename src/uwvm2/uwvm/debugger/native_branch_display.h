/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <span>
# include <iterator>
# include <fast_io.h>
# include "native_disassembly.h"
# include "native_disassembly_window.h"
# include "native_branch_destination.h"
# include "native_owned_instruction_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_branch_display
{
    // Copied display DATA, not a native stop ticket, breakpoint or read grant.
    // Bind each optional destination to the exact shown owned instruction.
    struct annotation
    {
        native_branch_destination::result destination{};
        ::std::uintptr_t source_pc{};
        ::std::size_t source_size{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept
        { return static_cast<bool>(destination); }
    };
    [[nodiscard]] inline annotation decode(native_owned_instruction_semantics::decoder& decoder,
        native_disassembly::instruction const& shown) noexcept
    {
        if(!shown || shown.pc == 0u || shown.size > shown.bytes.size()) { return {}; }
        // [owned shown.bytes ... shown.size <= fixed bounded instruction capacity] end
        // [safe                                                    ] immutable local copy;
        //  ^^ MC reads only these complete shown bytes, never source_pc as an address.
        auto const actual{decoder.decode(shown.pc, {shown.bytes.data(), shown.size})};
        if(!actual || actual.semantics().size != shown.size || !actual.destination()) { return {}; }
        return {actual.destination(), shown.pc, shown.size};
    }
    [[nodiscard]] constexpr bool matches(annotation const& copied,
        native_disassembly::instruction const& shown) noexcept
    {
        // An unavailable optional destination has no numeric position to show.
        // Available DATA must belong to this exact complete displayed row.
        return !copied || (shown.pc != 0u && shown.size != 0u && shown.size <= shown.bytes.size() &&
            shown.size <= UINTPTR_MAX - shown.pc &&
            copied.source_pc == shown.pc && copied.source_size == shown.size);
    }
    enum class symbol_resolution : unsigned char
    { disabled, no_owned_function_range, outside_current_function, current_function };
    struct symbol
    {
        symbol_resolution resolution{symbol_resolution::no_owned_function_range};
        ::std::uintptr_t function_offset{};
    };
    [[nodiscard]] constexpr bool inside_owner(annotation const& copied,
        ::std::uintptr_t begin, ::std::uintptr_t end) noexcept
    { return copied && begin != 0u && end > begin && copied.destination.display_pc >= begin && copied.destination.display_pc < end; }

    // SAME authenticated private image, never a public address/read grant.
    // A source row alone cannot qualify a destination or a lowered memory operand.
    template<typename Image, typename DisplayDecoder, typename SemanticDecoder>
    [[nodiscard]] inline native_disassembly::instruction project(Image const& image,
        DisplayDecoder& display, SemanticDecoder& semantics, native_disassembly::instruction const& shown) noexcept
    {
        if(!shown || shown.pc == 0u || shown.size > shown.bytes.size() ||
           image.owner_begin == 0u || image.owner_end <= image.owner_begin ||
           ::std::size(image.bytes) < image.size || ::std::size(image.guest_code) < image.size || image.owner_end - image.owner_begin != image.size ||
           shown.pc < image.owner_begin || shown.pc >= image.owner_end || shown.size > image.owner_end - shown.pc) { return {}; }
        ::std::span<::std::uint8_t const> instruction_code{};
        if constexpr(requires { image.instruction_code; })
        { instruction_code={::std::data(image.instruction_code),::std::size(image.instruction_code)}; }
        if(!instruction_code.empty() && instruction_code.size()!=image.size) { return {}; }
        auto const offset{shown.pc - image.owner_begin};
        for(::std::size_t byte{}; byte != shown.size; ++byte)
        { if(image.guest_code[offset + byte] != 1u || image.bytes[offset + byte] != shown.bytes[byte] ||
            (!instruction_code.empty() && instruction_code[offset + byte]!=1u)) { return {}; } }
        auto const actual{semantics.decode(shown.pc, {shown.bytes.data(), shown.size})};
        if(!actual || actual.semantics().size != shown.size || !actual.safe_for_public_display()) { return {}; }
        if(actual.destination())
        {
            annotation const copied{actual.destination(), shown.pc, shown.size};
            if(!inside_owner(copied, image.owner_begin, image.owner_end)) { return {}; }
            auto const target{native_disassembly::decode_window_with(display, {::std::data(image.bytes), image.size},
                image.owner_begin, copied.destination.display_pc, 0, 0, 1u,instruction_code)};
            if(!target.available || target.count != 1u || !target.instructions[0u] ||
               target.instructions[0u].pc != copied.destination.display_pc) { return {}; }
            auto const target_offset{copied.destination.display_pc - image.owner_begin};
            if(target.instructions[0u].size > image.size - target_offset) { return {}; }
            for(::std::size_t byte{}; byte != target.instructions[0u].size; ++byte)
            { if(image.guest_code[target_offset + byte] != 1u) { return {}; } }
        }
        ::std::size_t length{};
        while(length != shown.text.size() && shown.text[length] != '\0') { ++length; }
        if(length == 0u || length == shown.text.size()) { return {}; }
        native_disassembly::instruction out{};
        out.pc = shown.pc; out.size = shown.size;
        for(::std::size_t byte{}; byte != shown.size; ++byte) { out.bytes[byte] = shown.bytes[byte]; }
        for(::std::size_t index{}; index != length; ++index) { out.text[index] = shown.text[index]; }
        return out; // Unused byte/text tails never retain a decoder lookahead.
    }

    // Renderer defense, including omitted or forged optional annotations.
    // Target provenance is enforced in project(), before the public reply.
    template<typename SemanticDecoder>
    [[nodiscard]] inline bool displayable(SemanticDecoder& decoder, native_disassembly::instruction const& shown,
        annotation const& copied, ::std::uintptr_t begin, ::std::uintptr_t end) noexcept
    {
        if(!shown) { return !copied; }
        if(shown.pc == 0u || shown.size > shown.bytes.size() || !matches(copied, shown)) { return false; }
        auto const actual{decoder.decode(shown.pc, {shown.bytes.data(), shown.size})};
        if(!actual || actual.semantics().size != shown.size || !actual.safe_for_public_display()) { return false; }
        if(!actual.destination()) { return !copied; }
        return inside_owner(copied, begin, end) && copied.destination.display_pc == actual.destination().display_pc &&
            copied.destination.conditional == actual.destination().conditional &&
            copied.destination.never_taken == actual.destination().never_taken;
    }
    [[nodiscard]] constexpr symbol resolve_current_owner(annotation const& copied,
        ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end, bool resolve_symbols) noexcept
    {
        if(!resolve_symbols) { return {symbol_resolution::disabled, 0u}; }
        if(!copied || owner_begin == 0u || owner_end <= owner_begin)
        { return {symbol_resolution::no_owned_function_range, 0u}; }
        auto const target{copied.destination.display_pc};
        if(target < owner_begin || target >= owner_end)
        { return {symbol_resolution::outside_current_function, 0u}; }
        // [actual owned function begin ... target ... owner_end) integer labels
        // [safe                                               ] bounded subtraction;
        //  ^^ no target pointer, memory access, instruction-boundary or taken-branch claim.
        return {symbol_resolution::current_function, target - owner_begin};
    }
    template<typename Output>
    inline void print(Output output, annotation const& copied,
        ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end, bool resolve_symbols,
        ::std::uint64_t module, ::std::uint64_t function, ::std::uint64_t generation)
    {
        if(!inside_owner(copied, owner_begin, owner_end)) { return; }
        ::fast_io::io::print(output, " target=0x", ::fast_io::mnp::hex<false, true>(copied.destination.display_pc),
            " target-conditional=", ::fast_io::mnp::dec(static_cast<unsigned>(copied.destination.conditional)));
        if(copied.destination.never_taken) { ::fast_io::io::print(output," target-never-taken=1"); }
        auto const found{resolve_current_owner(copied, owner_begin, owner_end, resolve_symbols)};
        switch(found.resolution)
        {
            case symbol_resolution::current_function:
                // Labels come from the controller's exact authenticated copy.
                // Metadata text is already escaped separately; no symbol lookup
                // or target read is performed, even at an unaligned position.
                ::fast_io::io::print(output, " target-symbol=current-function+0x",
                    ::fast_io::mnp::hex<false, true>(found.function_offset),
                    " target-module=", ::fast_io::mnp::dec(module),
                    " target-function=", ::fast_io::mnp::dec(function),
                    " target-function-generation=", ::fast_io::mnp::dec(generation));
                break;
            case symbol_resolution::disabled:
                ::fast_io::io::print(output, " target-symbol=disabled"); break;
            case symbol_resolution::outside_current_function:
                ::fast_io::io::print(output, " target-symbol=unavailable(outside-current-function)"); break;
            default:
                ::fast_io::io::print(output, " target-symbol=unavailable(no-owned-function-range)"); break;
        }
    }
}
