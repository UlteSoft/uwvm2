/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <uwvm2/utils/container/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
    // Core 3 binary/types: abstract heaps have single-byte negative encodings;
    // defined heaps have nonnegative signed-33 type indices. Never narrow them to an opcode enum.
    enum class abstract_heap_type : ::std::int_least64_t
    {
        noexn = -12, nofunc = -13, noextern = -14, none = -15,
        func = -16, extern_ = -17, any = -18, eq = -19,
        i31 = -20, struct_ = -21, array = -22, exn = -23
    };
    struct heap_type
    {
        // Validation-only heap bottom, outside the entire binary signed-33 domain. A reference to heap bottom
        // is still a REFERENCE value, not the polymorphic bottom value that can match numeric operands.
        inline static constexpr ::std::int_least64_t bottom_code{-0x1'0000'0001ll};
        ::std::int_least64_t code{static_cast<::std::int_least64_t>(abstract_heap_type::func)};
        [[nodiscard]] inline constexpr bool is_defined() const noexcept { return code >= 0; }
        friend inline constexpr bool operator==(heap_type, heap_type) noexcept = default;
    };
    enum class value_kind : unsigned { i32, i64, f32, f64, v128, reference };
    struct core_value_type
    {
        value_kind kind{value_kind::i32};
        heap_type heap{};
        bool nullable{};
        // Validation/feature-policy provenance only. Semantic equality intentionally ignores
        // whether an equivalent reference used shorthand or explicit `(ref ...)` syntax.
        unsigned source_prefix{};
        friend inline constexpr bool operator==(core_value_type a, core_value_type b) noexcept
        { return a.kind == b.kind && (a.kind != value_kind::reference || (a.heap == b.heap && a.nullable == b.nullable)); }
    };
    enum class packed_kind : unsigned { none, i8, i16 };
    struct storage_type
    {
        core_value_type value{};
        packed_kind packed{};
        friend inline constexpr bool operator==(storage_type a, storage_type b) noexcept
        { return a.packed == b.packed && (a.packed != packed_kind::none || a.value == b.value); }
    };
    struct field_type
    {
        storage_type storage{};
        bool mutable_{};
        friend inline constexpr bool operator==(field_type, field_type) noexcept = default;
    };
    enum class composite_kind : unsigned { function, struct_, array };
    struct sub_type
    {
        composite_kind kind{};
        bool final_{true};
        // Offsets, not borrowed input pointers: records survive source-buffer relocation.
        ::std::size_t binary_offset{};
        ::uwvm2::utils::container::vector<::std::uint_least32_t> supertypes{};
        ::uwvm2::utils::container::vector<core_value_type> parameters{};
        ::uwvm2::utils::container::vector<core_value_type> results{};
        ::uwvm2::utils::container::vector<field_type> fields{};
    };
    struct recursive_group
    {
        ::std::size_t binary_offset{};
        ::std::uint_least64_t first_type_index{};
        ::uwvm2::utils::container::vector<sub_type> types{};
    };
    struct recursive_type_section
    {
        ::std::uint_least64_t type_count{};
        ::uwvm2::utils::container::vector<recursive_group> groups{};
    };
}
