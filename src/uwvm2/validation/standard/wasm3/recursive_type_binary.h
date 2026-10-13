/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include <utility>
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class recursive_type_binary_error : unsigned
    {
        ok, truncated, invalid_integer, invalid_heap_type, invalid_value_type,
        invalid_composite_type, invalid_mutability, impossible_count, trailing_bytes
    };
    struct recursive_type_binary_result
    {
        recursive_type_binary_error error{};
        ::std::size_t error_offset{};
    };
    namespace recursive_binary_details
    {
        namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
        // All input traversal uses bounded offsets. A failed parse never commits a caller cursor or partial type section.
        struct reader
        {
            ::std::span<::std::byte const> input{};
            ::std::size_t position{};
            recursive_type_binary_result status{};
            inline constexpr bool fail(recursive_type_binary_error error) noexcept
            {
                if(status.error == recursive_type_binary_error::ok) { status = {error, position}; }
                return false;
            }
            inline constexpr bool byte(unsigned& value) noexcept
            {
                if(position == input.size()) { return fail(recursive_type_binary_error::truncated); }
                // [consumed][one available byte] ... end
                // [safe    ][safe              ] the offset check dominates the only input access.
                value = ::std::to_integer<unsigned>(input[position]);
                // [consumed][one available byte] ... end
                // [safe    ][safe              ] unsafe (could be end)
                //           ^^ position: position < input.size() above proves increment remains <= size.
                ++position;
                // [consumed including byte] ... end; position is <= input.size(), possibly exactly one-past.
                // [safe                  ] unsafe (could be end)
                //                          ^^ position: no read occurs until another bound check.
                return true;
            }
            template<typename T>
            inline constexpr bool integer(T& value) noexcept
            {
                if(position == input.size()) { return fail(recursive_type_binary_error::truncated); }
                // Both u32 and s33 occupy at most five wire bytes.
                auto const remaining{input.size() - position};
                auto const count{remaining < 5u ? remaining : 5u};
                auto const scan{[&](unsigned char const* first) constexpr noexcept
                {
                    T decoded{};
                    // [consumed][up to count bytes] ... end
                    // [safe    ][safe            ] unsafe (could be end)
                    //           ^^ first: count <= remaining and <= local buffer capacity (5).
                    auto const [next, code]{::fast_io::parse_by_scan(first, first + count, ::fast_io::mnp::leb128_get(decoded))};
                    // [consumed][at most five bounded LEB bytes] ... end
                    // [safe    ][safe                          ] unsafe (could be end)
                    //           ^^ position: scanner returns next in [first, first + count].
                    position += static_cast<::std::size_t>(next - first);
                    // [consumed including scanned bytes] ... end
                    // [safe                          ] unsafe (could be end)
                    //                                  ^^ position: at most input.size(); no read here.
                    if(code != ::fast_io::parse_code::ok)
                    {
                        return fail(code == ::fast_io::parse_code::overflow || count == 5u
                            ? recursive_type_binary_error::invalid_integer : recursive_type_binary_error::truncated);
                    }
                    value = decoded;
                    return true;
                }};
                if UWVM_IF_CONSTEVAL
                {
                    unsigned char buffer[5]{};
                    for(::std::size_t i{}; i != count; ++i)
                    { buffer[i] = ::std::to_integer<unsigned char>(input[position + i]); }
                    return scan(buffer);
                }
                else
                {
                    // [consumed][at least one available byte] ... end
                    // [safe    ][safe                       ] unsafe (could be end)
                    //           ^^ position: the earlier position != input.size() check excludes null/one-past data.
                    return scan(reinterpret_cast<unsigned char const*>(input.data() + position));
                }
            }
            inline constexpr bool u32(::std::uint_least32_t& value) noexcept
            {
                ::std::uint_least32_t decoded{};
                if(!integer(decoded)) { return false; }
                if(decoded > 0xffff'ffffu) { return fail(recursive_type_binary_error::invalid_integer); }
                value = decoded;
                return true;
            }
            inline constexpr bool s33(::std::int_least64_t& value) noexcept
            {
                ::std::int_least64_t decoded{};
                if(!integer(decoded)) { return false; }
                // fast_io decodes the signed carrier; WebAssembly narrows it to s33.
                if(decoded < -0x1'0000'0000ll || decoded > 0xffff'ffffll)
                { return fail(recursive_type_binary_error::invalid_integer); }
                value = decoded;
                return true;
            }
            [[nodiscard]] static inline constexpr bool abstract_heap(unsigned byte) noexcept
            { return byte >= 0x69u && byte <= 0x74u; }
            inline constexpr bool heap(types::heap_type& value) noexcept
            {
                if(position == input.size()) { return fail(recursive_type_binary_error::truncated); }
                // [consumed][heap prefix] ... end; the prior check proves this peek.
                auto const prefix{::std::to_integer<unsigned>(input[position])};
                if(abstract_heap(prefix))
                {
                    unsigned ignored{};
                    if(!byte(ignored)) { return false; }
                    value.code = static_cast<::std::int_least64_t>(prefix) - 128;
                    return true;
                }
                ::std::int_least64_t index{};
                if(!s33(index)) { return false; }
                // Abstract heaps are SINGLE-BYTE productions, not padded negative signed LEBs.
                if(index < 0) { return fail(recursive_type_binary_error::invalid_heap_type); }
                value.code = index;
                return true;
            }
            inline constexpr bool value(types::core_value_type& result) noexcept
            {
                unsigned prefix{};
                if(!byte(prefix)) { return false; }
                result.source_prefix = prefix;
                switch(prefix)
                {
                    case 0x7f: result.kind = types::value_kind::i32; return true;
                    case 0x7e: result.kind = types::value_kind::i64; return true;
                    case 0x7d: result.kind = types::value_kind::f32; return true;
                    case 0x7c: result.kind = types::value_kind::f64; return true;
                    case 0x7b: result.kind = types::value_kind::v128; return true;
                    case 0x63: case 0x64:
                        result.kind = types::value_kind::reference;
                        result.nullable = prefix == 0x63u;
                        return heap(result.heap);
                    default:
                        if(!abstract_heap(prefix)) { return fail(recursive_type_binary_error::invalid_value_type); }
                        result.kind = types::value_kind::reference;
                        result.nullable = true;
                        result.heap.code = static_cast<::std::int_least64_t>(prefix) - 128;
                        return true;
                }
            }
            inline constexpr bool field(types::field_type& result) noexcept
            {
                if(position == input.size()) { return fail(recursive_type_binary_error::truncated); }
                // [field storage][mutability] ... end; the preceding check proves the first-byte peek only.
                auto const prefix{::std::to_integer<unsigned>(input[position])};
                if(prefix == 0x77u || prefix == 0x78u)
                {
                    unsigned ignored{};
                    if(!byte(ignored)) { return false; }
                    result.storage.packed = prefix == 0x78u ? types::packed_kind::i8 : types::packed_kind::i16;
                }
                else if(!value(result.storage.value)) { return false; }
                unsigned mutability{};
                if(!byte(mutability)) { return false; }
                if(mutability > 1u) { return fail(recursive_type_binary_error::invalid_mutability); }
                result.mutable_ = mutability != 0u;
                return true;
            }
            template<typename T, typename Read>
            inline constexpr bool list(::uwvm2::utils::container::vector<T>& values, ::std::size_t minimum_bytes, Read&& read) noexcept
            {
                ::std::uint_least32_t count{};
                if(!u32(count)) { return false; }
                // Every element has a known nonzero minimum encoding size; reject hostile counts before allocating.
                if(count > (input.size() - position) / minimum_bytes ||
                   count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(T))
                { return fail(recursive_type_binary_error::impossible_count); }
                values.reserve(static_cast<::std::size_t>(count));
                for(::std::uint_least32_t index{}; index != count; ++index)
                {
                    T item{};
                    if(!read(item)) { return false; }
                    values.push_back_unchecked(::std::move(item));
                }
                return true;
            }
            inline constexpr bool subtype(types::sub_type& result) noexcept
            {
                result.binary_offset = position;
                unsigned prefix{};
                if(!byte(prefix)) { return false; }
                if(prefix == 0x4fu || prefix == 0x50u)
                {
                    result.final_ = prefix == 0x4fu;
                    if(!list(result.supertypes, 1uz, [&](auto& index) constexpr noexcept { return u32(index); })) { return false; }
                    if(!byte(prefix)) { return false; }
                }
                switch(prefix)
                {
                    case 0x60:
                        result.kind = types::composite_kind::function;
                        return list(result.parameters, 1uz, [&](auto& type) constexpr noexcept { return value(type); }) &&
                               list(result.results, 1uz, [&](auto& type) constexpr noexcept { return value(type); });
                    case 0x5f:
                        result.kind = types::composite_kind::struct_;
                        return list(result.fields, 2uz, [&](auto& type) constexpr noexcept { return field(type); });
                    case 0x5e:
                    {
                        result.kind = types::composite_kind::array;
                        types::field_type item{};
                        if(!field(item)) { return false; }
                        result.fields.push_back(::std::move(item));
                        return true;
                    }
                    default: return fail(recursive_type_binary_error::invalid_composite_type);
                }
            }
            inline constexpr bool group(types::recursive_group& result) noexcept
            {
                result.binary_offset = position;
                if(position == input.size()) { return fail(recursive_type_binary_error::truncated); }
                // [group prefix] ... end; peek only after proving a live byte.
                if(input[position] == ::std::byte{0x4e})
                {
                    unsigned ignored{};
                    if(!byte(ignored)) { return false; }
                    return list(result.types, 2uz, [&](auto& type) constexpr noexcept { return subtype(type); });
                }
                types::sub_type item{};
                if(!subtype(item)) { return false; }
                result.types.push_back(::std::move(item));
                return true;
            }
        };
    }
    // Syntax decoder only: reference context, declared-supertype validity and feature policy belong to type validation.
    [[nodiscard]] inline constexpr recursive_type_binary_result scan_core3_type_section(
        ::std::byte const*& cursor, ::std::byte const* end,
        ::uwvm2::parser::wasm::standard::wasm3::type::recursive_type_section& output) noexcept
    {
        namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
        // [cursor ... end) is the caller-proven section allocation; equal endpoints may both be null.
        // [safe         ] borrow the range without dereferencing either endpoint.
        recursive_binary_details::reader input{{cursor, cursor == end ? 0uz : static_cast<::std::size_t>(end - cursor)}};
        types::recursive_type_section temporary{};
        if(!input.list(temporary.groups, 2uz, [&](types::recursive_group& group) constexpr noexcept
        {
            group.first_type_index = temporary.type_count;
            if(!input.group(group)) { return false; }
            if(group.types.size() > 0x1'0000'0000ull - temporary.type_count)
            { return input.fail(recursive_type_binary_error::impossible_count); }
            temporary.type_count += group.types.size();
            return true;
        })) { return input.status; }
        if(input.position != input.input.size()) { return {recursive_type_binary_error::trailing_bytes, input.position}; }
        output = ::std::move(temporary);
        // [complete type-section payload] end
        // [safe                         ] unsafe (end is one-past, or both endpoints are null)
        // ^^ cursor still names the original section begin; all traversed offsets were bounded.
        cursor = end;
        // [complete type-section payload] end
        // [safe                         ] unsafe (one-past)
        //                                 ^^ cursor: committed end is never dereferenced here.
        return {};
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
