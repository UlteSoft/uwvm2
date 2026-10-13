/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <utility>
# include <fast_io.h>
# include "recursive_type.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
    // Print the retained Core 3 declarations, never their execution ABI carrier.
    // Text spelling follows https://webassembly.github.io/spec/core/text/types.html.
    // All wrappers borrow immutable, parser-owned vectors for the duration of a
    // synchronous fast_io print. They do not decode or advance module pointers.
    struct heap_type_details_t { heap_type value{}; };
    struct value_type_details_t { core_value_type value{}; };
    struct storage_type_details_t { storage_type value{}; };
    struct field_type_details_t { field_type value{}; };
    struct sub_type_details_t { sub_type const* value{}; };
    struct recursive_type_details_t { recursive_type_section const* value{}; };
    struct function_type_details_t
    {
        ::uwvm2::utils::container::vector<core_value_type> const* parameters{};
        ::uwvm2::utils::container::vector<core_value_type> const* results{};
    };

    [[nodiscard]] inline constexpr auto section_details(heap_type value) noexcept { return heap_type_details_t{value}; }
    [[nodiscard]] inline constexpr auto section_details(core_value_type value) noexcept { return value_type_details_t{value}; }
    [[nodiscard]] inline constexpr auto section_details(storage_type value) noexcept { return storage_type_details_t{value}; }
    [[nodiscard]] inline constexpr auto section_details(field_type value) noexcept { return field_type_details_t{value}; }
    [[nodiscard]] inline constexpr auto section_details(sub_type const& value) noexcept
    { return sub_type_details_t{::std::addressof(value)}; }
    [[nodiscard]] inline constexpr auto section_details(recursive_type_section const& value) noexcept
    { return recursive_type_details_t{::std::addressof(value)}; }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, heap_type_details_t>, Stream&& stream,
        heap_type_details_t details)
    {
        if(details.value.is_defined())
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::dec(details.value.code));
            return;
        }
        ::uwvm2::utils::container::u8string_view name{};
        switch(static_cast<abstract_heap_type>(details.value.code))
        {
            case abstract_heap_type::noexn: name = u8"noexn"; break;
            case abstract_heap_type::nofunc: name = u8"nofunc"; break;
            case abstract_heap_type::noextern: name = u8"noextern"; break;
            case abstract_heap_type::none: name = u8"none"; break;
            case abstract_heap_type::func: name = u8"func"; break;
            case abstract_heap_type::extern_: name = u8"extern"; break;
            case abstract_heap_type::any: name = u8"any"; break;
            case abstract_heap_type::eq: name = u8"eq"; break;
            case abstract_heap_type::i31: name = u8"i31"; break;
            case abstract_heap_type::struct_: name = u8"struct"; break;
            case abstract_heap_type::array: name = u8"array"; break;
            case abstract_heap_type::exn: name = u8"exn"; break;
            default: name = u8"<invalid-heap-type>"; break; // validation-only bottom has no text encoding
        }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(name));
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, value_type_details_t>, Stream&& stream,
        value_type_details_t details)
    {
        auto const value{details.value};
        if(value.kind == value_kind::reference)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                ::fast_io::mnp::code_cvt(value.nullable ? ::uwvm2::utils::container::u8string_view{u8"(ref null "} : ::uwvm2::utils::container::u8string_view{u8"(ref "}),
                section_details(value.heap), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
            return;
        }
        ::uwvm2::utils::container::u8string_view name{};
        switch(value.kind)
        {
            case value_kind::i32: name = u8"i32"; break;
            case value_kind::i64: name = u8"i64"; break;
            case value_kind::f32: name = u8"f32"; break;
            case value_kind::f64: name = u8"f64"; break;
            case value_kind::v128: name = u8"v128"; break;
            default: name = u8"<invalid-value-type>"; break;
        }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(name));
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, storage_type_details_t>, Stream&& stream,
        storage_type_details_t details)
    {
        switch(details.value.packed)
        {
            case packed_kind::none:
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(details.value.value)); break;
            case packed_kind::i8:
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"i8")); break;
            case packed_kind::i16:
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"i16")); break;
        }
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, field_type_details_t>, Stream&& stream,
        field_type_details_t details)
    {
        if(details.value.mutable_)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"(mut ")); }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(details.value.storage));
        if(details.value.mutable_)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"})); }
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, function_type_details_t>, Stream&& stream,
        function_type_details_t details)
    {
        if(details.parameters == nullptr || details.results == nullptr) { ::fast_io::fast_terminate(); }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"(func"));
        if(!details.parameters->empty())
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8" (param"));
            for(auto const value : *details.parameters)
            { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8" "}), section_details(value)); }
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
        }
        if(!details.results->empty())
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8" (result"));
            for(auto const value : *details.results)
            { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8" "}), section_details(value)); }
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
        }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, sub_type_details_t>, Stream&& stream, sub_type_details_t details)
    {
        if(details.value == nullptr) { ::fast_io::fast_terminate(); }
        auto const& value{*details.value};
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"(sub "));
        if(value.final_)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"final ")); }
        for(auto const index : value.supertypes)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::dec(index), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8" "})); }
        if(value.kind == composite_kind::function)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                function_type_details_t{::std::addressof(value.parameters), ::std::addressof(value.results)});
        }
        else
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                ::fast_io::mnp::code_cvt(value.kind == composite_kind::struct_ ? ::uwvm2::utils::container::u8string_view{u8"(struct"} : ::uwvm2::utils::container::u8string_view{u8"(array"}));
            for(auto const field : value.fields)
            {
                // A struct contains `(field fieldtype)` declarations; an array
                // contains one fieldtype directly, without the `field` keyword.
                if(value.kind == composite_kind::struct_)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                        ::fast_io::mnp::code_cvt(u8" (field "), section_details(field), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
                }
                else
                { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8" "}), section_details(field)); }
            }
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
        }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8")"}));
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, recursive_type_details_t>, Stream&& stream,
        recursive_type_details_t details)
    {
        if(details.value == nullptr) { ::fast_io::fast_terminate(); }
        auto const& section{*details.value};
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"\nType["),
            ::fast_io::mnp::dec(section.type_count), ::fast_io::mnp::code_cvt(u8"]:\n"));
        ::std::size_t group_index{};
        for(auto const& group : section.groups)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8" - rec["),
                ::fast_io::mnp::dec(group_index), ::fast_io::mnp::code_cvt(u8"]: first-type="), ::fast_io::mnp::dec(group.first_type_index),
                ::fast_io::mnp::code_cvt(u8", count="), ::fast_io::mnp::dec(group.types.size()),
                ::fast_io::mnp::code_cvt(u8", binary-offset="), ::fast_io::mnp::dec(group.binary_offset), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8"\n"}));
            auto type_index{group.first_type_index};
            for(auto const& type : group.types)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8" - type["),
                    ::fast_io::mnp::dec(type_index), ::fast_io::mnp::code_cvt(u8"]: "), section_details(type), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8"\n"}));
                ++type_index; // Validated Core 3 type indices occupy at most the u32 index space.
            }
            ++group_index;
        }
    }
}
