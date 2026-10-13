/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Feature definition storage and printable details
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <memory>
# include <limits>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/section_details.h>
# include "def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    // Core 3 memory limits retain all 64 bits even on a 32-bit host. This is
    // declared module metadata; native allocation limits are checked separately.
    struct memory_limits_type
    {
        // Provide a default maximum value guarantee that min is always less than or equal to max.
        inline static constexpr auto default_max{::std::numeric_limits<::std::uint_least64_t>::max()};

        ::std::uint_least64_t min{};
        ::std::uint_least64_t max{default_max};
        bool present_max{};
    };

    /// @brief Wrapper for the section storage structure
    struct memory_limits_type_section_details_wrapper_t
    { memory_limits_type limits{}; };

    inline constexpr memory_limits_type_section_details_wrapper_t section_details(memory_limits_type limits) noexcept { return {limits}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, memory_limits_type_section_details_wrapper_t>,
                                       Stm && stream,
                                       memory_limits_type_section_details_wrapper_t const limits_section_details_wrapper)
    {
        if(limits_section_details_wrapper.limits.present_max)
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 "limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 ", max: ",
                                                                 limits_section_details_wrapper.limits.max,
                                                                 "}");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 L"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 L", max: ",
                                                                 limits_section_details_wrapper.limits.max,
                                                                 L"}");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u8"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 u8", max: ",
                                                                 limits_section_details_wrapper.limits.max,
                                                                 u8"}");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 u", max: ",
                                                                 limits_section_details_wrapper.limits.max,
                                                                 u"}");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 U"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 U", max: ",
                                                                 limits_section_details_wrapper.limits.max,
                                                                 U"}");
            }
        }
        else
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "limits: {min: ", limits_section_details_wrapper.limits.min, "}");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 L"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 L"}");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u8"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 u8"}");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 u"}");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 U"limits: {min: ",
                                                                 limits_section_details_wrapper.limits.min,
                                                                 U"}");
            }
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type, memory_limits_type_section_details_wrapper_t>) noexcept
    {
        constexpr auto stack_size{128u};
        return stack_size;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, memory_limits_type_section_details_wrapper_t>,
                                                      memory_limits_type_section_details_wrapper_t const limits_section_details_wrapper) noexcept
    {
        constexpr auto u64_size{print_reserve_size(::fast_io::io_reserve_type<char_type, ::std::uint_least64_t>)};
        auto size{::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_literal_size(::uwvm2::parser::wasm::standard::wasm1::type::details::limits_min_prefix<char_type>()) + u64_size};

        if(limits_section_details_wrapper.limits.present_max)
        {
            size += ::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_literal_size(::uwvm2::parser::wasm::standard::wasm1::type::details::limits_max_prefix<char_type>()) + u64_size;
        }

        return size + ::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_literal_size(::uwvm2::parser::wasm::standard::wasm1::type::details::right_brace<char_type>());
    }

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, memory_limits_type_section_details_wrapper_t>,
                                                     char_type* iter,
                                                     memory_limits_type_section_details_wrapper_t const limits_section_details_wrapper) noexcept
    {
        // [written prefix] [remaining u64/literal reservation] end
        // [safe                                                ]
        //                  ^^ iter; the writer consumes only its reserved suffix.
        iter = ::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_copy_literal(iter, ::uwvm2::parser::wasm::standard::wasm1::type::details::limits_min_prefix<char_type>());
        // [written prefix] [remaining u64/literal reservation] end
        // [safe                                                ]
        //                  ^^ iter; the writer consumes only its reserved suffix.
        iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::uint_least64_t>, iter, limits_section_details_wrapper.limits.min);

        if(limits_section_details_wrapper.limits.present_max)
        {
        // [written prefix] [remaining u64/literal reservation] end
        // [safe                                                ]
        //                  ^^ iter; the writer consumes only its reserved suffix.
            iter = ::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_copy_literal(iter, ::uwvm2::parser::wasm::standard::wasm1::type::details::limits_max_prefix<char_type>());
        // [written prefix] [remaining u64/literal reservation] end
        // [safe                                                ]
        //                  ^^ iter; the writer consumes only its reserved suffix.
            iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::uint_least64_t>, iter, limits_section_details_wrapper.limits.max);
        }

        return ::uwvm2::parser::wasm::standard::wasm1::type::details::section_details_copy_literal(iter, ::uwvm2::parser::wasm::standard::wasm1::type::details::right_brace<char_type>());
    }

    // The MVP root type remains unchanged. The layered type carries sharedness
    // and Core 3 address width without narrowing the declared limits.
    struct memory_type
    {
        memory_limits_type limits{};
        bool shared{};
        // Core 3 validation uses the declared address type, independently of the
        // memory's limits or sharedness. The current binary parser still rejects
        // address64 declarations until all runtime translation paths are ready.
        bool address64{};
    };
    struct memory_type_section_details_wrapper_t { memory_type memory{}; };
    inline constexpr memory_type_section_details_wrapper_t section_details(memory_type memory) noexcept { return {memory}; }
    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, memory_type_section_details_wrapper_t>,
                                      Stream&& stream, memory_type_section_details_wrapper_t wrapper)
    {
        if constexpr(::std::same_as<Char, char>)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(wrapper.memory.limits),
             ", shared: ", wrapper.memory.shared, ", address64: ", wrapper.memory.address64); }
        else if constexpr(::std::same_as<Char, wchar_t>)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(wrapper.memory.limits),
             L", shared: ", wrapper.memory.shared, ", address64: ", wrapper.memory.address64); }
        else if constexpr(::std::same_as<Char, char8_t>)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(wrapper.memory.limits),
             u8", shared: ", wrapper.memory.shared, ", address64: ", wrapper.memory.address64); }
        else if constexpr(::std::same_as<Char, char16_t>)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(wrapper.memory.limits),
             u", shared: ", wrapper.memory.shared, ", address64: ", wrapper.memory.address64); }
        else if constexpr(::std::same_as<Char, char32_t>)
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), section_details(wrapper.memory.limits),
             U", shared: ", wrapper.memory.shared, ", address64: ", wrapper.memory.address64); }
    }

    /// @brief wasm1.1 table type with funcref/externref reference type support.
    /// @warning Extension point: new table reference types must be reflected in parser gates, element segments, runtime table storage, and diagnostics.
    struct table_type
    {
        // Preserve declaration precision on ISA32. Absent table32 maxima retain
        // their legacy 2^32-1 allocation cap; the parser sets table64's u64 cap.
        memory_limits_type limits{.max = 0xffff'ffffull};
        ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reftype{::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref};
        bool address64{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};
    };

    /// @brief Section-details wrapper for wasm1.1 table types.
    struct table_type_section_details_wrapper_t
    { ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type table{}; };

    /// @brief Return a printable details view for a wasm1.1 table type.
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type_section_details_wrapper_t section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type table) noexcept
    { return {table}; }

    /// @brief Print a wasm1.1 table type summary.
    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type_section_details_wrapper_t>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type_section_details_wrapper_t const wrapper)
    {
        namespace w3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8"type: "));
        if(wrapper.table.has_core_type &&
           (!wrapper.table.core_type.nullable || (wrapper.table.core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::func) &&
              wrapper.table.core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::extern_))))
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), w3::section_details(wrapper.table.core_type)); }
        else
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), to_value_type(wrapper.table.reftype)); }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8", "),
            section_details(wrapper.table.limits), ::fast_io::mnp::code_cvt(u8", address64: "), wrapper.table.address64);
    }

    /// @brief wasm1.1 global type with extended value-type support.
    /// @warning Extension point: new global value types must be reflected in const_expr storage, runtime global storage, initializer, local_imported, and ECO
    /// output.
    struct global_type
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type type{};
        bool is_mutable{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};
    };

    /// @brief Section-details wrapper for wasm1.1 global types.
    struct global_type_section_details_wrapper_t
    { ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type global{}; };

    /// @brief Return a printable details view for a wasm1.1 global type.
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type_section_details_wrapper_t section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type global) noexcept
    { return {global}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type_section_details_wrapper_t>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type_section_details_wrapper_t const wrapper)
    {
        namespace w3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8"type: "));
        if(wrapper.global.has_core_type && wrapper.global.core_type.kind == w3::value_kind::reference &&
           (!wrapper.global.core_type.nullable || (wrapper.global.core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::func) &&
              wrapper.global.core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::extern_))))
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), w3::section_details(wrapper.global.core_type)); }
        else
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), wrapper.global.type); }
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8", mutable: "),
            wrapper.global.is_mutable);
    }

    namespace details::wasm1p1_type_section_details_print
    {
        template <::std::integral char_type, ::std::size_t n>
        inline constexpr ::std::size_t literal_size(char_type const (&)[n]) noexcept
        {
            constexpr ::std::size_t size{n - 1uz};
            return size;
        }

        template <::std::integral char_type, ::std::size_t n>
        inline constexpr char_type* copy_literal(char_type* iter, char_type const (&literal)[n]) noexcept
        { return ::fast_io::freestanding::my_copy_n(literal, n - 1uz, iter); }

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(type_prefix, "type: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(comma_space, ", ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(mutable_prefix, ", mutable: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(shared_prefix, ", shared: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(address64_prefix, ", address64: ");
    }  // namespace details::wasm1p1_type_section_details_print

    template<::std::integral Char>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<Char, memory_type_section_details_wrapper_t>) noexcept { return 192uz; }

    template<::std::integral Char>
    inline constexpr ::std::size_t print_reserve_size(
        ::fast_io::io_reserve_type_t<Char, memory_type_section_details_wrapper_t>, memory_type_section_details_wrapper_t wrapper) noexcept
    {
        return print_reserve_size(::fast_io::io_reserve_type<Char, memory_limits_type_section_details_wrapper_t>,
                   section_details(wrapper.memory.limits)) +
               details::wasm1p1_type_section_details_print::literal_size(details::wasm1p1_type_section_details_print::shared_prefix<Char>()) +
               details::wasm1p1_type_section_details_print::literal_size(details::wasm1p1_type_section_details_print::address64_prefix<Char>()) +
               2uz * print_reserve_size(::fast_io::io_reserve_type<Char, bool>);
    }

    template<::std::integral Char>
    inline constexpr Char* print_reserve_define(
        ::fast_io::io_reserve_type_t<Char, memory_type_section_details_wrapper_t>, Char* iter, memory_type_section_details_wrapper_t wrapper) noexcept
    {
        // [output reservation: limits + shared label + bool] end
        // [safe                                           ]
        // ^^ iter; caller reserved print_reserve_size characters.
        iter = print_reserve_define(::fast_io::io_reserve_type<Char, memory_limits_type_section_details_wrapper_t>,
                                    iter, section_details(wrapper.memory.limits));
        // [limits] [shared label + bool] end
        //          ^^ iter; limits writer stays within its reserved size.
        iter = details::wasm1p1_type_section_details_print::copy_literal(iter, details::wasm1p1_type_section_details_print::shared_prefix<Char>());
        // [limits + shared label] [bool] end
        //                         ^^ iter; complete bool reservation remains.
        iter = print_reserve_define(::fast_io::io_reserve_type<Char, bool>, iter, wrapper.memory.shared);
        // [limits/shared] [address64 label + bool] end
        //                 ^^ iter; reservation includes the final address type.
        iter = details::wasm1p1_type_section_details_print::copy_literal(iter, details::wasm1p1_type_section_details_print::address64_prefix<Char>());
        // [limits/shared/address64 label] [bool] end
        //                                  ^^ iter; complete bool reservation remains.
        return print_reserve_define(::fast_io::io_reserve_type<Char, bool>, iter, wrapper.memory.address64);
    }

    // Tables/globals retain richer Core 3 references than their execution
    // carriers. Use the public print_define above for these cold diagnostics;
    // the former reserve/context path silently erased heap indices/nullability
    // and table64. Memory's bounded reserve formatter remains unchanged.

    /// @brief Fixed-width SIMD payload for the wasm1.1 `v128.const` constant-expression instruction.
    using wasm1p1_const_expr_v128_storage_t = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

    // A 0xFB constant-expression instruction keeps the prefix in opcode and its bounded,
    // already-decoded unsigned immediates here. count is used only by array.new_fixed.
    struct wasm3_const_expr_gc_immediate_t
    {
        ::std::uint_least32_t subopcode{};
        ::std::uint_least32_t typeidx{};
        ::std::uint_least32_t count{};
    };

    static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t) == 16uz);

    /// @brief wasm1.1 constant-expression opcode payload.
    /// @details Keeps wasm1 MVP payloads in the wasm1.1 replacement type, while adding reference and SIMD initializer payloads.
    /// @warning Extension point: new const-expression opcodes need storage here plus parser, validator, initializer, and diagnostics updates.
    union wasm1p1_const_expr_opcode_storage_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
        // The legacy name also holds local indices when extended-const is enabled. All global.get contexts use
        // this one active union member; alternate scalar aliases would cause inactive-union-member reads.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 imported_global_idx;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 ref_func_idx;
        // Abstract heaps are negative; defined function heaps retain every signed-33 index bit.
        ::std::int_least64_t ref_null_heap;
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t v128;
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm3_const_expr_gc_immediate_t gc_immediate;
    };

    struct wasm1p1_const_expr_opcode_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u storage{};
        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic opcode{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(
        ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_t>);
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief wasm1.1 constant-expression storage.
    struct wasm1p1_const_expr_storage_t
    {
        ::std::byte const* begin{};
        ::std::byte const* end{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_t> opcodes{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_storage_t>
    {
        inline static constexpr bool value = true;
    };
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{

    /// @brief Storage for wasm1.1 data count section (section id 12).
    /// @details The count is checked against the parsed data section during the final module check.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct data_count_section_storage_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Data Count"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{12u};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count{};
        bool present{};
    };

    /// @brief Wire-format data segment flags introduced by bulk memory.
    /// @warning Extension point: new data segment flags must be mirrored in data_section parsing, final checks, runtime storage, and instantiation.
    enum class wasm1p1_data_type_t : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
    {
        active_implicit = 0u,
        passive = 1u,
        active_explicit = 2u
    };

    /// @brief Parsed payload shared by active and passive wasm1.1 data segments.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_data_storage_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_idx{};
        bool active{};
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_init_t byte{};

        static_assert(
            ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>>);
    };

    /// @brief Section-details wrapper for a parsed wasm1.1 data segment payload.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_data_storage_t_section_details_wrapper_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...> const* data_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    /// @brief Return a printableelemkind、reftype、segment flag details view for a wasm1.1 data segment payload.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...> section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...> const& data_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(data_storage), ::std::addressof(all_sections)}; }

    namespace details::wasm1p1_data_section_details_print
    {
        template <::std::integral char_type, ::std::size_t n>
        inline constexpr ::std::size_t literal_size(char_type const (&)[n]) noexcept
        {
            constexpr ::std::size_t size{n - 1uz};
            return size;
        }

        template <::std::integral char_type, ::std::size_t n>
        inline constexpr char_type* copy_literal(char_type* iter, char_type const (&literal)[n]) noexcept
        { return ::fast_io::freestanding::my_copy_n(literal, n - 1uz, iter); }

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(memory_idx_prefix, "memory_idx: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(size_prefix, ", size: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(passive_prefix, "passive, size: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(flag_prefix, "flag: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(comma_space, ", ");
    }  // namespace details::wasm1p1_data_section_details_print

    /// @brief Print a wasm1.1 data segment payload summary.
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...> const data_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(data_details_wrapper.data_storage_ptr == nullptr || data_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const size{static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin)};
        if(data_details_wrapper.data_storage_ptr->active)
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 "memory_idx: ",
                                                                 data_details_wrapper.data_storage_ptr->memory_idx,
                                                                 ", size: ",
                                                                 size);
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 L"memory_idx: ",
                                                                 data_details_wrapper.data_storage_ptr->memory_idx,
                                                                 L", size: ",
                                                                 size);
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u8"memory_idx: ",
                                                                 data_details_wrapper.data_storage_ptr->memory_idx,
                                                                 u8", size: ",
                                                                 size);
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u"memory_idx: ",
                                                                 data_details_wrapper.data_storage_ptr->memory_idx,
                                                                 u", size: ",
                                                                 size);
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 U"memory_idx: ",
                                                                 data_details_wrapper.data_storage_ptr->memory_idx,
                                                                 U", size: ",
                                                                 size);
            }
        }
        else
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "passive, size: ", size);
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"passive, size: ", size);
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"passive, size: ", size);
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"passive, size: ", size);
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"passive, size: ", size);
            }
        }
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>) noexcept
    {
        constexpr auto stack_size{128u};
        return stack_size;
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::size_t print_reserve_size(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...> const data_details_wrapper) noexcept
    {
        if(data_details_wrapper.data_storage_ptr->active)
        {
            return details::wasm1p1_data_section_details_print::literal_size(details::wasm1p1_data_section_details_print::memory_idx_prefix<char_type>()) +
                   print_reserve_size(::fast_io::io_reserve_type<char_type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>) +
                   details::wasm1p1_data_section_details_print::literal_size(details::wasm1p1_data_section_details_print::size_prefix<char_type>()) +
                   print_reserve_size(::fast_io::io_reserve_type<char_type, ::std::size_t>);
        }

        return details::wasm1p1_data_section_details_print::literal_size(details::wasm1p1_data_section_details_print::passive_prefix<char_type>()) +
               print_reserve_size(::fast_io::io_reserve_type<char_type, ::std::size_t>);
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr char_type* print_reserve_define(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>,
        char_type* iter,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...> const data_details_wrapper) noexcept
    {
        auto const size{static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin)};
        if(data_details_wrapper.data_storage_ptr->active)
        {
            iter =
                details::wasm1p1_data_section_details_print::copy_literal(iter, details::wasm1p1_data_section_details_print::memory_idx_prefix<char_type>());
            iter = print_reserve_define(::fast_io::io_reserve_type<char_type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>,
                                        iter,
                                        data_details_wrapper.data_storage_ptr->memory_idx);
            iter = details::wasm1p1_data_section_details_print::copy_literal(iter, details::wasm1p1_data_section_details_print::size_prefix<char_type>());
            return print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::size_t>, iter, size);
        }

        iter = details::wasm1p1_data_section_details_print::copy_literal(iter, details::wasm1p1_data_section_details_print::passive_prefix<char_type>());
        return print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::size_t>, iter, size);
    }

    /// @brief Final data-section element storage carrying the parsed flag and payload.
    /// @details The explicit union mirrors existing wasm1 storage traits while keeping the segment object lifetime controlled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_data_t
    {
        inline static constexpr ::std::size_t sizeof_storage_u{sizeof(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...>)};

        union storage_u
        {
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...> segment;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(segment)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(segment)>);

            [[maybe_unused]] ::std::byte sizeof_storage_u_reserve[sizeof_storage_u]{};

            inline constexpr ~storage_u() {}
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_storage_u, "sizeof(storage_t) not equal to sizeof_storage_u");

        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_type_t type{};

        inline constexpr wasm1p1_data_t() noexcept { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){}; }

        inline constexpr wasm1p1_data_t(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...> const& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){other.storage.segment}; }

        inline constexpr wasm1p1_data_t(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>&& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){::std::move(other.storage.segment)}; }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>&
            operator= (::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...> const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->type = other.type;
            this->storage.segment = other.storage.segment;
            return *this;
        }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>&
            operator= (::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->type = other.type;
            this->storage.segment = ::std::move(other.storage.segment);
            return *this;
        }

        inline constexpr ~wasm1p1_data_t() { ::std::destroy_at(::std::addressof(this->storage.segment)); }
    };

    /// @brief Section-details wrapper for a full wasm1.1 data segment entry.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_data_t_section_details_wrapper_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...> const* data_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    /// @brief Return a printable details view for a full wasm1.1 data segment entry.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...> section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...> const& data,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(data), ::std::addressof(all_sections)}; }

    /// @brief Print the wasm1.1 data segment flag and payload summary.
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...>>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...> const data_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(data_details_wrapper.data_ptr == nullptr || data_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                "flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type),
                ", ",
                section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                L"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type),
                L", ",
                section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u8"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type),
                u8", ",
                section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type),
                u", ",
                section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                U"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type),
                U", ",
                section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
        }
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...>>) noexcept
    {
        constexpr auto stack_size{160u};
        return stack_size;
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::size_t print_reserve_size(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...>>,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...> const data_details_wrapper) noexcept
    {
        return details::wasm1p1_data_section_details_print::literal_size(details::wasm1p1_data_section_details_print::flag_prefix<char_type>()) +
               print_reserve_size(::fast_io::io_reserve_type<char_type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>) +
               details::wasm1p1_data_section_details_print::literal_size(details::wasm1p1_data_section_details_print::comma_space<char_type>()) +
               print_reserve_size(
                   ::fast_io::io_reserve_type<char_type,
                                              ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>,
                   section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr char_type* print_reserve_define(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...>>,
        char_type* iter,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t_section_details_wrapper_t<Fs...> const data_details_wrapper) noexcept
    {
        iter = details::wasm1p1_data_section_details_print::copy_literal(iter, details::wasm1p1_data_section_details_print::flag_prefix<char_type>());
        iter = print_reserve_define(
            ::fast_io::io_reserve_type<char_type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>,
            iter,
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(data_details_wrapper.data_ptr->type));
        iter = details::wasm1p1_data_section_details_print::copy_literal(iter, details::wasm1p1_data_section_details_print::comma_space<char_type>());
        return print_reserve_define(
            ::fast_io::io_reserve_type<char_type,
                                       ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t_section_details_wrapper_t<Fs...>>,
            iter,
            section_details(data_details_wrapper.data_ptr->storage.segment, *data_details_wrapper.all_sections_ptr));
    }

    /// @brief Wire-format element segment flags introduced by reference types and bulk memory.
    /// @warning Extension point: new element segment flags must be mirrored in element_section parsing, final checks, runtime storage, and instantiation.
    enum class wasm1p1_element_type_t : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
    {
        active_implicit_funcidx = 0u,
        passive_funcidx = 1u,
        active_explicit_funcidx = 2u,
        declarative_funcidx = 3u,
        active_implicit_expr = 4u,
        passive_expr = 5u,
        active_explicit_expr = 6u,
        declarative_expr = 7u
    };

    /// @brief Parsed payload shared by active, passive, and declarative wasm1.1 element segments.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_element_storage_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_idx{};
        bool active{};
        bool declarative{};
        ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reftype{::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref};
        // Retain the declared Core 3 element heap/nullability for active initialization and table.init validation.
        // This is parser/validator metadata; runtime table elements keep their existing carrier ABI.
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type core_type{};
        bool has_core_type{};
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> vec_funcidx{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>> vec_expr{};
    };

    /// @brief Section-details wrapper for a parsed wasm1.1 element segment payload.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_element_storage_t_section_details_wrapper_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...> const* element_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    /// @brief Return a printable details view for a wasm1.1 element segment payload.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t_section_details_wrapper_t<Fs...> section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...> const& element_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(element_storage), ::std::addressof(all_sections)}; }

    namespace details::wasm1p1_element_section_details_print
    {
        template <::std::integral char_type, ::std::size_t n>
        inline constexpr ::std::size_t literal_size(char_type const (&)[n]) noexcept
        {
            constexpr ::std::size_t size{n - 1uz};
            return size;
        }

        template <::std::integral char_type, ::std::size_t n>
        inline constexpr char_type* copy_literal(char_type* iter, char_type const (&literal)[n]) noexcept
        { return ::fast_io::freestanding::my_copy_n(literal, n - 1uz, iter); }

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(table_idx_prefix, "table_idx: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(type_prefix, ", type: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(count_prefix, ", count: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(newline, "\n");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(flag_prefix, "flag: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(comma_space, ", ");
    }  // namespace details::wasm1p1_element_section_details_print

    /// @brief Print a wasm1.1 element segment payload summary.
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t_section_details_wrapper_t<Fs...>>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t_section_details_wrapper_t<Fs...> const element_storage_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(element_storage_details_wrapper.element_storage_ptr == nullptr || element_storage_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const* const element{element_storage_details_wrapper.element_storage_ptr};
        if(element == nullptr) { ::fast_io::fast_terminate(); }
        auto const func_count{element->vec_funcidx.size()};
        auto const expr_count{element->vec_expr.size()};
        namespace w3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8"table_idx: "),
            ::fast_io::mnp::dec(element->table_idx), ::fast_io::mnp::code_cvt(u8", type: "));
        if(element->has_core_type && (!element->core_type.nullable ||
            (element->core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::func) &&
             element->core_type.heap.code != static_cast<::std::int_least64_t>(w3::abstract_heap_type::extern_))))
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), w3::section_details(element->core_type)); }
        else
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), to_value_type(element->reftype)); }
        ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream), ::fast_io::mnp::code_cvt(u8", count: "),
            ::fast_io::mnp::dec(func_count + expr_count));
    }

    // As with tables and globals, a carrier-only reserve formatter cannot
    // represent the declared element heap/nullability. The outer diagnostic
    // printer selects print_define when dynamic reserve support is absent.

    /// @brief Final element-section entry storage carrying the parsed flag and payload.
    /// @details The explicit union mirrors existing wasm1 storage traits while keeping the segment object lifetime controlled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_element_t
    {
        inline static constexpr ::std::size_t sizeof_storage_u{sizeof(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...>)};

        union storage_u
        {
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...> segment;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(segment)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(segment)>);

            [[maybe_unused]] ::std::byte sizeof_storage_u_reserve[sizeof_storage_u]{};

            inline constexpr ~storage_u() {}
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_storage_u, "sizeof(storage_t) not equal to sizeof_storage_u");

        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t type{};

        inline constexpr wasm1p1_element_t() noexcept { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){}; }

        inline constexpr wasm1p1_element_t(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...> const& other) noexcept :
            type{other.type}
        { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){other.storage.segment}; }

        inline constexpr wasm1p1_element_t(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>&& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.segment)) decltype(this->storage.segment){::std::move(other.storage.segment)}; }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>&
            operator= (::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...> const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->type = other.type;
            this->storage.segment = other.storage.segment;
            return *this;
        }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>&
            operator= (::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->type = other.type;
            this->storage.segment = ::std::move(other.storage.segment);
            return *this;
        }

        inline constexpr ~wasm1p1_element_t() { ::std::destroy_at(::std::addressof(this->storage.segment)); }
    };

    /// @brief Section-details wrapper for a full wasm1.1 element segment entry.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1p1_element_t_section_details_wrapper_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...> const* element_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    /// @brief Return a printable details view for a full wasm1.1 element segment entry.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t_section_details_wrapper_t<Fs...> section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...> const& element,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(element), ::std::addressof(all_sections)}; }

    /// @brief Print the wasm1.1 element segment flag and payload summary.
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type, ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t_section_details_wrapper_t<Fs...>>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t_section_details_wrapper_t<Fs...> const element_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(element_details_wrapper.element_ptr == nullptr || element_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                "flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(element_details_wrapper.element_ptr->type),
                ", ",
                section_details(element_details_wrapper.element_ptr->storage.segment, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                L"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(element_details_wrapper.element_ptr->type),
                L", ",
                section_details(element_details_wrapper.element_ptr->storage.segment, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u8"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(element_details_wrapper.element_ptr->type),
                u8", ",
                section_details(element_details_wrapper.element_ptr->storage.segment, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(element_details_wrapper.element_ptr->type),
                u", ",
                section_details(element_details_wrapper.element_ptr->storage.segment, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                U"flag: ",
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(element_details_wrapper.element_ptr->type),
                U", ",
                section_details(element_details_wrapper.element_ptr->storage.segment, *element_details_wrapper.all_sections_ptr));
        }
    }

    /// @brief Wrapper for the data count section storage structure.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct data_count_section_storage_section_details_wrapper_t
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...> const* data_count_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...> section_details(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...> const& data_count_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(data_count_section_storage), ::std::addressof(all_sections)}; }

#ifndef UWVM_MODULE
    // This optional context-print fast path depends on non-exported fast_io protocol internals.
    namespace details::data_count_section_print
    {
        template <::std::integral char_type, ::std::size_t n>
        inline constexpr bool emit_literal(char_type*& curr, char_type* end, char_type const (&literal)[n], ::std::size_t& offset) noexcept
        {
            constexpr ::std::size_t literal_size{n - 1uz};
            auto const remain{literal_size - offset};
            auto const space{static_cast<::std::size_t>(end - curr)};
            auto const count{remain < space ? remain : space};

            curr = ::fast_io::freestanding::my_copy_n(literal + offset, count, curr);
            offset += count;

            if(offset == literal_size)
            {
                offset = 0uz;
                return true;
            }

            return false;
        }

        template <::std::integral char_type, typename T>
        inline constexpr bool emit_reserve(char_type*& curr, char_type* end, T value, ::std::size_t& offset) noexcept
        {
            using value_type = ::std::remove_cvref_t<T>;
            constexpr ::std::size_t reserve_size{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};

            if(offset == 0uz && static_cast<::std::size_t>(end - curr) >= reserve_size)
            {
                curr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, value);
                return true;
            }

            char_type buffer[reserve_size];
            auto const buffer_end{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, value)};
            auto const literal_size{static_cast<::std::size_t>(buffer_end - buffer)};
            auto const remain{literal_size - offset};
            auto const space{static_cast<::std::size_t>(end - curr)};
            auto const count{remain < space ? remain : space};

            curr = ::fast_io::freestanding::my_copy_n(buffer + offset, count, curr);
            offset += count;

            if(offset == literal_size)
            {
                offset = 0uz;
                return true;
            }

            return false;
        }

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(prefix, "\nData Count:\n| count: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(suffix, "\n");

        enum class stage : unsigned char
        {
            prefix,
            count,
            suffix,
            done
        };

        struct context
        {
            stage curr_stage{};
            ::std::size_t offset{};

            template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
            inline constexpr ::fast_io::context_print_result<char_type*> print_context_define(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...> const
                    data_count_section_details_wrapper,
                char_type* curr,
                char_type* end) noexcept
            {
                auto const* const datacountsec{data_count_section_details_wrapper.data_count_section_storage_ptr};
                if(datacountsec == nullptr || !datacountsec->present || this->curr_stage == stage::done) { return {curr, true}; }
                if(curr == end) [[unlikely]] { return {curr, false}; }

                for(;;)
                {
                    switch(this->curr_stage)
                    {
                        case stage::prefix:
                        {
                            if(!emit_literal(curr, end, prefix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::count;
                            break;
                        }
                        case stage::count:
                        {
                            if(!emit_reserve(curr, end, datacountsec->count, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::suffix;
                            break;
                        }
                        case stage::suffix:
                        {
                            if(!emit_literal(curr, end, suffix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::done;
                            break;
                        }
                        case stage::done: return {curr, true};
                    }

                    if(curr == end) { return {curr, false}; }
                }
            }
        };
    }  // namespace details::data_count_section_print
#endif

    /// @brief Print the data count section details.
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...>>,
        Stm && stream,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...> const
            data_count_section_details_wrapper)
    {
        auto const* const datacountsec{data_count_section_details_wrapper.data_count_section_storage_ptr};
        if(datacountsec == nullptr || !datacountsec->present) { return; }

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nData Count:\n| count: ", datacountsec->count, "\n");
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nData Count:\n| count: ", datacountsec->count, L"\n");
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nData Count:\n| count: ", datacountsec->count, u8"\n");
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nData Count:\n| count: ", datacountsec->count, u"\n");
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nData Count:\n| count: ", datacountsec->count, U"\n");
        }
    }

#ifndef UWVM_MODULE
    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto print_context_type(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...>>)
        noexcept
    { return ::fast_io::io_type_t<::uwvm2::parser::wasm::standard::wasm1p1::features::details::data_count_section_print::context>{}; }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::size_t print_context_static_buffer_size(
        ::fast_io::io_reserve_type_t<char_type,
                                     ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_section_details_wrapper_t<Fs...>>) noexcept
    {
        constexpr auto buffer_size{::fast_io::details::dynamic_reserve_default_static_stack_size<char_type>()};
        return buffer_size;
    }
#endif
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::table_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::table_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::global_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::global_type>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
