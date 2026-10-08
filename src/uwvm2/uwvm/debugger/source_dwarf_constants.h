/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_variants.h"
#endif
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
# ifndef UWVM_MODULE
#  include <llvm/BinaryFormat/Dwarf.h>
#  include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
# endif
# ifndef UWVM_MODULE_EXPORT
#  define UWVM_MODULE_EXPORT
# endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf_constants
{
    namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
    enum class error { none, unavailable, malformed, ambiguous, value_too_large };
    // No expression evaluation or pointers. Copies bounded target scalar bits;
    // metadata never becomes guest read authority. The index creates its DWARF
    // context with WebAssembly's little-endian byte order on every host.
    // Floating blocks must exactly match binary32/binary64 width. Fixed forms
    // carry exact-width target bits. LLVM emits floating bits in unsigned LEB
    // too (DwarfUnit::addConstantFPValue); preserve those bits without conversion.
    // Strings, aggregates and other floating formats remain unavailable.
    [[nodiscard]] inline error decode(::llvm::DWARFFormValue const& value, dwarf::type_record const& type,
        ::std::uint8_t address_bytes, dwarf::location_plan& out) noexcept
    {
        out = {};
        if((address_bytes != 4u && address_bytes != 8u) || type.kind != dwarf::type_kind::scalar ||
           !type.size_known || !dwarf::variant_integer_width(type.byte_count) || type.byte_size != type.byte_count)
        { return error::unavailable; }
        if(type.encoding == ::llvm::dwarf::DW_ATE_float)
        {
            if(type.byte_count != 4u && type.byte_count != 8u) { return error::unavailable; }
            dwarf::location_plan pending{};
            pending.kind = dwarf::plan_kind::constant_value;
            pending.reason = dwarf::unavailable_reason::none;
            pending.byte_count = type.byte_count; pending.address_bytes = address_bytes;
            pending.implicit_constant = true; pending.direct_constant_attribute = true;
            auto const form{value.getForm()};
            if(form == ::llvm::dwarf::DW_FORM_block || form == ::llvm::dwarf::DW_FORM_block1 ||
               form == ::llvm::dwarf::DW_FORM_block2 || form == ::llvm::dwarf::DW_FORM_block4)
            {
                auto const block{value.getAsBlock()};
                if(!block) { return error::malformed; }
                if(block->size() != type.byte_count) { return error::unavailable; }
                // [borrowed exact-width block ... end] [owned 16-byte array ... end]
                // [safe                              ] byte_count is 4/8 and both
                // spans are proven before copying; no host-endian reinterpretation.
                for(::std::size_t i{}; i != type.byte_count; ++i)
                { pending.implicit_bytes[i] = static_cast<::std::byte>((*block)[i]); }
            }
            else
            {
                if((form != ::llvm::dwarf::DW_FORM_data4 || type.byte_count != 4u) &&
                   (form != ::llvm::dwarf::DW_FORM_data8 || type.byte_count != 8u) &&
                   form != ::llvm::dwarf::DW_FORM_udata)
                { return error::unavailable; }
                auto const bits{value.getAsUnsignedConstant()};
                if(!bits) { return error::malformed; }
                if(type.byte_count == 4u && (*bits >> 32u) != 0u) { return error::value_too_large; }
                for(::std::size_t i{}; i != type.byte_count; ++i)
                { pending.implicit_bytes[i] = static_cast<::std::byte>((*bits >> (i * 8u)) & 0xffu); }
            }
            out = ::std::move(pending);
            return error::none;
        }
        bool const signed_type{type.encoding == ::llvm::dwarf::DW_ATE_signed || type.encoding == ::llvm::dwarf::DW_ATE_signed_char};
        // UTF scalars expose an unsigned target code unit, consistent with
        // source_dwarf_values. Do not infer glyphs or validate Unicode strings.
        // DWARF character widths are 1/2/4; wider encodings remain unavailable.
        bool const utf_type{type.encoding == ::llvm::dwarf::DW_ATE_UTF};
        if(utf_type && type.byte_count == 8u) { return error::unavailable; }
        bool const unsigned_type{type.encoding == ::llvm::dwarf::DW_ATE_unsigned ||
                                 type.encoding == ::llvm::dwarf::DW_ATE_unsigned_char || utf_type};
        bool const boolean_type{type.encoding == ::llvm::dwarf::DW_ATE_boolean};
        if(!signed_type && !unsigned_type && !boolean_type) { return error::unavailable; }
        if(!value.isFormClass(::llvm::DWARFFormValue::FC_Constant)) { return error::unavailable; }
        auto const form{value.getForm()};
        ::std::uint8_t fixed_bytes{};
        switch(form)
        {
            case ::llvm::dwarf::DW_FORM_data1: fixed_bytes = 1u; break;
            case ::llvm::dwarf::DW_FORM_data2: fixed_bytes = 2u; break;
            case ::llvm::dwarf::DW_FORM_data4: fixed_bytes = 4u; break;
            case ::llvm::dwarf::DW_FORM_data8: fixed_bytes = 8u; break;
            case ::llvm::dwarf::DW_FORM_sdata:
            case ::llvm::dwarf::DW_FORM_udata:
            case ::llvm::dwarf::DW_FORM_implicit_const: break;
            default: return error::unavailable;
        }
        ::std::uint64_t bits{};
        bool const signed_form{form == ::llvm::dwarf::DW_FORM_sdata || form == ::llvm::dwarf::DW_FORM_implicit_const};
        if(signed_form)
        {
            auto const number{value.getAsSignedConstant()};
            if(!number) { return error::malformed; }
            if(!signed_type && *number < 0) { return error::value_too_large; }
            bits = static_cast<::std::uint64_t>(*number);
        }
        else
        {
            auto const number{value.getAsUnsignedConstant()};
            if(!number) { return error::malformed; }
            bits = *number;
            if(signed_type && fixed_bytes != 0u)
            {
                // DWARF5 7.5.4 leaves fixed-width constants context dependent.
                // Exact type-width bytes are unambiguous target integer bits.
                // A shorter signed representation with its top bit set admits
                // both zero/sign extension (DWARF issue 260921.1 remains open).
                // Never guess a negative constant from that ambiguous input.
                if(fixed_bytes < type.byte_count)
                {
                    if((bits & (::std::uint64_t{1u} << (fixed_bytes * 8u - 1u))) != 0u) { return error::ambiguous; }
                }
                else if(fixed_bytes == type.byte_count)
                { bits = dwarf::canonical_variant_bits(bits, type.byte_count, true); }
                else
                {
                    auto const signed_number{value.getAsSignedConstant()};
                    if(!signed_number) { return error::malformed; }
                    bits = static_cast<::std::uint64_t>(*signed_number);
                }
            }
            // An unsigned LEB constant denotes a nonnegative number; a signed
            // destination must represent it exactly, not reinterpret its sign.
            if(signed_type && form == ::llvm::dwarf::DW_FORM_udata &&
               bits > dwarf::variant_integer_mask(type.byte_count) / 2u) { return error::value_too_large; }
        }
        // Clang's concrete UTF DW_AT_const_value can carry a narrow APInt
        // sign-extended to 64 bits in DW_FORM_udata (e.g. char8_t(0x80)).
        // Accept only that exact all-ones extension of the declared target
        // code unit. Other overflow and signed negative forms stay rejected.
        // This is bit preservation for UTF, never general integer wrapping.
        if(utf_type && form == ::llvm::dwarf::DW_FORM_udata &&
           dwarf::canonical_variant_bits(bits, type.byte_count, true) == bits)
        { bits &= dwarf::variant_integer_mask(type.byte_count); }
        if(dwarf::canonical_variant_bits(bits, type.byte_count, signed_type) != bits || (boolean_type && bits > 1u))
        { return error::value_too_large; }
        dwarf::location_plan pending{};
        pending.kind = dwarf::plan_kind::constant_value; pending.reason = dwarf::unavailable_reason::none;
        pending.constant_bits = bits; pending.signed_constant = signed_type;
        pending.byte_count = type.byte_count; pending.address_bytes = address_bytes;
        pending.direct_constant_attribute = true;
        out = ::std::move(pending);
        return error::none;
    }
}

#endif
