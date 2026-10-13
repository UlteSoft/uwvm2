/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_dwarf_types.h"
# include <cstring>
# include <memory>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    struct copied_location_piece
    {
        ::std::size_t piece_index{};
        ::std::uint64_t first_source_byte{};
        // OWNED memory copy, made while the actual authenticated source capture
        // and execution lease remain valid. This is never a borrowed host span.
        ::std::vector<::std::byte> bytes{};
    };
    enum class piece_unavailable_reason
    { none, empty_or_unsupported, local_not_captured, carrier_mismatch, source_bounds, memory_not_copied, local_unavailable };
    enum class piece_query_error { none, malformed, limit_exceeded, allocation_failure };
    struct composite_value
    {
        ::std::vector<::std::byte> bytes{}, known_bits{};
        ::std::vector<piece_unavailable_reason> piece_reasons{};
        bool fully_available{};
    };
    struct piece_query_limits
    { ::std::size_t max_pieces{64u}, max_object_bytes{65536u}, max_captured_locals{256u}, max_copied_memory_bytes{65536u}; };
    // Finite piece atoms, copied locals, and OWNED memory pieces only. The caller
    // must authenticate one coherent source/stop/generation/activation snapshot
    // FIRST. No DWARF instructions, host pointer, syscall, function or guest
    // dereference is executed here. Unknown bits remain explicitly unknown.
    [[nodiscard]] inline piece_query_error materialize_location_pieces(location_plan const& plan,
        ::std::span<copied_numeric_local const> locals, ::std::size_t total_count,
        ::std::span<copied_location_piece const> memory_pieces, ::std::size_t object_bytes,
        composite_value& out, piece_query_limits const& cap = {}) noexcept
    {
        out = {};
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            if(plan.kind != plan_kind::composite_value || plan.reason != unavailable_reason::none || plan.pieces.empty() ||
               (plan.address_bytes != 4u && plan.address_bytes != 8u) ||
               locals.size() > total_count) { return piece_query_error::malformed; }
            if(plan.pieces.size() > cap.max_pieces || memory_pieces.size() > cap.max_pieces ||
               object_bytes > cap.max_object_bytes || locals.size() > cap.max_captured_locals)
            { return piece_query_error::limit_exceeded; }
            if(::std::cmp_greater(object_bytes, (::std::numeric_limits<::std::uint64_t>::max)() / 8u)) { return piece_query_error::limit_exceeded; }
            auto const object_bits{static_cast<::std::uint64_t>(object_bytes) * 8u};
            if(plan.composite_bits > object_bits) { return piece_query_error::malformed; }
            ::std::size_t copied_bytes{};
            for(::std::size_t i{}; i != memory_pieces.size(); ++i)
            {
                auto const& copy{memory_pieces[i]};
                if(copy.piece_index >= plan.pieces.size() || plan.pieces[copy.piece_index].atom.kind != plan_kind::frame_relative_offset ||
                   copy.first_source_byte != plan.pieces[copy.piece_index].source_bit_offset / 8u)
                { return piece_query_error::malformed; }
                if(!budget::charge(copy.bytes.size(), cap.max_copied_memory_bytes, copied_bytes)) { return piece_query_error::limit_exceeded; }
                for(::std::size_t j{}; j != i; ++j)
                { if(memory_pieces[j].piece_index == copy.piece_index) { return piece_query_error::malformed; } }
            }
            composite_value pending{}; pending.bytes.resize(object_bytes); pending.known_bits.resize(object_bytes);
            pending.piece_reasons.resize(plan.pieces.size());
            ::std::uint64_t position{};
            for(::std::size_t i{}; i != plan.pieces.size(); ++i)
            {
                auto const& piece{plan.pieces[i]}; auto const& atom{piece.atom};
                if(piece.object_bit_offset != position || position > object_bits || piece.bit_size > object_bits - position ||
                   atom.kind == plan_kind::composite_value || !atom.pieces.empty() || atom.address_bytes != plan.address_bytes)
                { return piece_query_error::malformed; }
                position += piece.bit_size; // preceding subtraction checked before cumulative bit advance.
                if(piece.bit_size == 0u) { continue; }
                if(atom.reason != unavailable_reason::none ||
                   (atom.integer_transform_count != 0u && atom.kind != plan_kind::wasm_local_value))
                { pending.piece_reasons[i] = piece_unavailable_reason::empty_or_unsupported; continue; }
                ::std::array<::std::byte, 16u> local_bytes{};
                ::std::span<::std::byte const> source{}; ::std::uint64_t source_bit{piece.source_bit_offset};
                if(atom.kind == plan_kind::wasm_local_value)
                {
                    if(atom.storage != wasm_location_space::local || atom.local_index >= total_count || atom.local_index >= locals.size())
                    { pending.piece_reasons[i] = piece_unavailable_reason::local_not_captured; continue; }
                    auto const& local{locals[static_cast<::std::size_t>(atom.local_index)]};
                    if(!local.available) { pending.piece_reasons[i] = piece_unavailable_reason::local_unavailable; continue; }
                    if(local.wasm_type == 0x7fu || local.wasm_type == 0x7du)
                    {
                        // [owned native 16-byte carrier] end
                        // [safe                       ] copy one complete i32/f32
                        //  ^^ carrier, then fast_io converts to Wasm little endian.
                        ::std::uint32_t bits{}; ::std::memcpy(::std::addressof(bits), local.bytes.data(), sizeof(bits));
                        ::std::uint64_t transformed{bits};
                        if(atom.integer_transform_count != 0u && (local.wasm_type != 0x7fu ||
                           !transform_copied_integer(atom, transformed, 32u)))
                        { pending.piece_reasons[i] = piece_unavailable_reason::carrier_mismatch; continue; }
                        auto const little{::fast_io::little_endian(static_cast<::std::uint32_t>(transformed))};
                        ::std::memcpy(local_bytes.data(), ::std::addressof(little), sizeof(little)); source = {local_bytes.data(), sizeof(little)};
                    }
                    else if(local.wasm_type == 0x7eu || local.wasm_type == 0x7cu)
                    {
                        // [safe] complete i64/f64 carrier inside the same owned slot.
                        ::std::uint64_t bits{}; ::std::memcpy(::std::addressof(bits), local.bytes.data(), sizeof(bits));
                        if(atom.integer_transform_count != 0u && (local.wasm_type != 0x7eu ||
                           !transform_copied_integer(atom, bits, 64u)))
                        { pending.piece_reasons[i] = piece_unavailable_reason::carrier_mismatch; continue; }
                        auto const little{::fast_io::little_endian(bits)};
                        ::std::memcpy(local_bytes.data(), ::std::addressof(little), sizeof(little)); source = {local_bytes.data(), sizeof(little)};
                    }
                    else { pending.piece_reasons[i] = piece_unavailable_reason::carrier_mismatch; continue; }
                }
                else if(atom.kind == plan_kind::constant_value)
                {
                    if(atom.implicit_constant)
                    {
                        if(atom.byte_count == 0u || atom.byte_count > atom.implicit_bytes.size()) { return piece_query_error::malformed; }
                        // [owned implicit bytes] end; checked byte_count first.
                        source = {atom.implicit_bytes.data(), atom.byte_count};
                    }
                    else
                    {
                        // Integer DWARF constants preserve ALL 64 bits even for
                        // Wasm32, independently of the memory address width.
                        auto const little{::fast_io::little_endian(atom.constant_bits)};
                        ::std::memcpy(local_bytes.data(), ::std::addressof(little), sizeof(little)); source = {local_bytes.data(), sizeof(little)};
                    }
                }
                else if(atom.kind == plan_kind::frame_relative_offset)
                {
                    copied_location_piece const* selected{};
                    for(auto const& copy : memory_pieces)
                    { if(copy.piece_index == i) { selected = ::std::addressof(copy); break; } }
                    if(selected == nullptr) { pending.piece_reasons[i] = piece_unavailable_reason::memory_not_copied; continue; }
                    source = selected->bytes; source_bit %= 8u; // scalar within the already copied requested byte slice.
                }
                else { pending.piece_reasons[i] = piece_unavailable_reason::empty_or_unsupported; continue; }
                if(::std::cmp_greater(source.size(), (::std::numeric_limits<::std::uint64_t>::max)() / 8u)) { return piece_query_error::limit_exceeded; }
                auto const available_bits{static_cast<::std::uint64_t>(source.size()) * 8u};
                if(source_bit > available_bits || piece.bit_size > available_bits - source_bit)
                { pending.piece_reasons[i] = piece_unavailable_reason::source_bounds; continue; }
                for(::std::uint64_t bit{}; bit != piece.bit_size; ++bit)
                {
                    auto const input_bit{source_bit + bit}; auto const output_bit{piece.object_bit_offset + bit};
                    // [owned source byte span ... input_bit/8 ... end]
                    // [safe                                        ] unsafe (one-past)
                    //                              ^^ full source bit range was
                    // checked BEFORE deriving this one-byte synchronous borrow.
                    auto const first{reinterpret_cast<char const*>(source.data() + static_cast<::std::size_t>(input_bit / 8u))};
                    auto const last{first + 1u}; ::std::uint8_t byte{};
                    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<8>(byte))};
                    if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last) { return piece_query_error::malformed; }
                    auto const target{static_cast<::std::size_t>(output_bit / 8u)};
                    auto const mask{static_cast<unsigned char>(1u << (output_bit % 8u))};
                    // [owned output bytes and validity bits] end
                    // [safe                                  ] output_bit<object_bits
                    // from the checked descriptor; these are scalar bit updates.
                    if(((byte >> (input_bit % 8u)) & 1u) != 0u) { pending.bytes[target] |= ::std::byte{mask}; }
                    pending.known_bits[target] |= ::std::byte{mask};
                }
            }
            if(position != plan.composite_bits) { return piece_query_error::malformed; }
            pending.fully_available = true;
            for(auto byte : pending.known_bits) { if(byte != ::std::byte{0xffu}) { pending.fully_available = false; break; } }
            out = ::std::move(pending); return piece_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return piece_query_error::allocation_failure; }
#endif
    }
}
