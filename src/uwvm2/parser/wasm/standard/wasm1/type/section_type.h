/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <bit>
# include <limits>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/parser/wasm/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include "value_type.h"
# include "value_binfmt.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::type
{
    namespace details
    {
        template <::std::integral char_type, ::std::size_t n>
        inline constexpr ::std::size_t section_details_literal_size(char_type const (&)[n]) noexcept
        {
            constexpr ::std::size_t size{n - 1uz};
            return size;
        }

        template <::std::integral char_type, ::std::size_t n>
        inline constexpr char_type* section_details_copy_literal(char_type* iter, char_type const (&literal)[n]) noexcept
        { return ::fast_io::freestanding::my_copy_n(literal, n - 1uz, iter); }

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(limits_min_prefix, "limits: {min: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(limits_max_prefix, ", max: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(right_brace, "}");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(pages_prefix, "pages: {");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(type_funcref_prefix, "type: funcref, ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(mutable_prefix, "mutable: ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(bool_true_literal, "true");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(bool_false_literal, "false");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(type_prefix, ", type: ");
    }  // namespace details

    /// @brief      Limits
    /// @details    Limits classify the size range of resizeable storage associated with memory types and table types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.4
    struct limits_type
    {
        // Provide a default maximum value guarantee that min is always less than or equal to max.
        inline static constexpr auto default_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 min{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max{default_max};
        bool present_max{};
    };

    /// @brief Wrapper for the section storage structure
    struct limits_type_section_details_wrapper_t
    { limits_type limits{}; };

    inline constexpr limits_type_section_details_wrapper_t section_details(limits_type limits) noexcept { return {limits}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, limits_type_section_details_wrapper_t>,
                                       Stm && stream,
                                       limits_type_section_details_wrapper_t const limits_section_details_wrapper)
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
        ::fast_io::io_reserve_type_t<char_type, limits_type_section_details_wrapper_t>) noexcept
    {
        constexpr auto stack_size{128u};
        return stack_size;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, limits_type_section_details_wrapper_t>,
                                                      limits_type_section_details_wrapper_t const limits_section_details_wrapper) noexcept
    {
        constexpr auto wasm_u32_size{print_reserve_size(::fast_io::io_reserve_type<char_type, wasm_u32>)};
        auto size{details::section_details_literal_size(details::limits_min_prefix<char_type>()) + wasm_u32_size};

        if(limits_section_details_wrapper.limits.present_max)
        {
            size += details::section_details_literal_size(details::limits_max_prefix<char_type>()) + wasm_u32_size;
        }

        return size + details::section_details_literal_size(details::right_brace<char_type>());
    }

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, limits_type_section_details_wrapper_t>,
                                                     char_type* iter,
                                                     limits_type_section_details_wrapper_t const limits_section_details_wrapper) noexcept
    {
        iter = details::section_details_copy_literal(iter, details::limits_min_prefix<char_type>());
        iter = print_reserve_define(::fast_io::io_reserve_type<char_type, wasm_u32>, iter, limits_section_details_wrapper.limits.min);

        if(limits_section_details_wrapper.limits.present_max)
        {
            iter = details::section_details_copy_literal(iter, details::limits_max_prefix<char_type>());
            iter = print_reserve_define(::fast_io::io_reserve_type<char_type, wasm_u32>, iter, limits_section_details_wrapper.limits.max);
        }

        return details::section_details_copy_literal(iter, details::right_brace<char_type>());
    }

    /// @brief      Function Types
    /// @details    Function types are encoded by the byte 0x60 followed by the respective vectors of parameter and result types.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 5.3.3
    struct function_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::vec_value_type parameter{};
        ::uwvm2::parser::wasm::standard::wasm1::type::vec_value_type result{};
    };

    /// @brief      Memory Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.5
    struct memory_type
    { limits_type limits{}; };

    /// @brief Wrapper for the memory_type storage structure
    struct memory_type_section_details_wrapper_t
    { memory_type memory{}; };

    inline constexpr memory_type_section_details_wrapper_t section_details(memory_type memory) noexcept { return {memory}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, memory_type_section_details_wrapper_t>,
                                       Stm && stream,
                                       memory_type_section_details_wrapper_t const memory_section_details_wrapper)
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             "pages: {",
                                                             section_details(memory_section_details_wrapper.memory.limits),
                                                             "}");
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             L"pages: {",
                                                             section_details(memory_section_details_wrapper.memory.limits),
                                                             L"}");
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u8"pages: {",
                                                             section_details(memory_section_details_wrapper.memory.limits),
                                                             u8"}");
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u"pages: {",
                                                             section_details(memory_section_details_wrapper.memory.limits),
                                                             u"}");
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             U"pages: {",
                                                             section_details(memory_section_details_wrapper.memory.limits),
                                                             U"}");
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type, memory_type_section_details_wrapper_t>) noexcept
    {
        constexpr auto stack_size{160u};
        return stack_size;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, memory_type_section_details_wrapper_t>,
                                                      memory_type_section_details_wrapper_t const memory_section_details_wrapper) noexcept
    {
        return details::section_details_literal_size(details::pages_prefix<char_type>()) +
               print_reserve_size(::fast_io::io_reserve_type<char_type, limits_type_section_details_wrapper_t>,
                                  section_details(memory_section_details_wrapper.memory.limits)) +
               details::section_details_literal_size(details::right_brace<char_type>());
    }

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, memory_type_section_details_wrapper_t>,
                                                     char_type* iter,
                                                     memory_type_section_details_wrapper_t const memory_section_details_wrapper) noexcept
    {
        iter = details::section_details_copy_literal(iter, details::pages_prefix<char_type>());
        iter = print_reserve_define(::fast_io::io_reserve_type<char_type, limits_type_section_details_wrapper_t>,
                                    iter,
                                    section_details(memory_section_details_wrapper.memory.limits));
        return details::section_details_copy_literal(iter, details::right_brace<char_type>());
    }

    /// @brief      Table Types
    /// @details    Memory types classify linear memories and their size range.
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.6
    struct table_type
    {
        limits_type limits{};

        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte reftype{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(0x70)};
    };

    struct table_type_section_details_wrapper_t
    { table_type table{}; };

    inline constexpr table_type_section_details_wrapper_t section_details(table_type table) noexcept { return {table}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, table_type_section_details_wrapper_t>,
                                       Stm && stream,
                                       table_type_section_details_wrapper_t const table_section_details_wrapper)
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             "type: funcref, ",
                                                             section_details(table_section_details_wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             L"type: funcref, ",
                                                             section_details(table_section_details_wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u8"type: funcref, ",
                                                             section_details(table_section_details_wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u"type: funcref, ",
                                                             section_details(table_section_details_wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             U"type: funcref, ",
                                                             section_details(table_section_details_wrapper.table.limits));
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type, table_type_section_details_wrapper_t>) noexcept
    {
        constexpr auto stack_size{160u};
        return stack_size;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, table_type_section_details_wrapper_t>,
                                                      table_type_section_details_wrapper_t const table_section_details_wrapper) noexcept
    {
        return details::section_details_literal_size(details::type_funcref_prefix<char_type>()) +
               print_reserve_size(::fast_io::io_reserve_type<char_type, limits_type_section_details_wrapper_t>,
                                  section_details(table_section_details_wrapper.table.limits));
    }

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, table_type_section_details_wrapper_t>,
                                                     char_type* iter,
                                                     table_type_section_details_wrapper_t const table_section_details_wrapper) noexcept
    {
        iter = details::section_details_copy_literal(iter, details::type_funcref_prefix<char_type>());
        return print_reserve_define(::fast_io::io_reserve_type<char_type, limits_type_section_details_wrapper_t>,
                                    iter,
                                    section_details(table_section_details_wrapper.table.limits));
    }

    /// @brief      Global Types
    /// @details    Global types classify global variables, which hold a value and can either be mutable or immutable
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.7
    struct global_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type type{};
        bool is_mutable{};
    };

    struct global_type_section_details_wrapper_t
    { global_type global{}; };

    inline constexpr global_type_section_details_wrapper_t section_details(global_type global) noexcept { return {global}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, global_type_section_details_wrapper_t>,
                                       Stm && stream,
                                       global_type_section_details_wrapper_t const global_section_details_wrapper)
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             "mutable: ",
                                                             ::fast_io::mnp::boolalpha(global_section_details_wrapper.global.is_mutable),
                                                             ", type: ",
                                                             section_details(global_section_details_wrapper.global.type));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             L"mutable: ",
                                                             ::fast_io::mnp::boolalpha(global_section_details_wrapper.global.is_mutable),
                                                             L", type: ",
                                                             section_details(global_section_details_wrapper.global.type));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u8"mutable: ",
                                                             ::fast_io::mnp::boolalpha(global_section_details_wrapper.global.is_mutable),
                                                             u8", type: ",
                                                             section_details(global_section_details_wrapper.global.type));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u"mutable: ",
                                                             ::fast_io::mnp::boolalpha(global_section_details_wrapper.global.is_mutable),
                                                             u", type: ",
                                                             section_details(global_section_details_wrapper.global.type));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             U"mutable: ",
                                                             ::fast_io::mnp::boolalpha(global_section_details_wrapper.global.is_mutable),
                                                             U", type: ",
                                                             section_details(global_section_details_wrapper.global.type));
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_static_stack_size(
        ::fast_io::io_reserve_type_t<char_type, global_type_section_details_wrapper_t>) noexcept
    {
        constexpr auto stack_size{96u};
        return stack_size;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, global_type_section_details_wrapper_t>,
                                                      global_type_section_details_wrapper_t const global_section_details_wrapper) noexcept
    {
        return details::section_details_literal_size(details::mutable_prefix<char_type>()) +
               (global_section_details_wrapper.global.is_mutable
                    ? details::section_details_literal_size(details::bool_true_literal<char_type>())
                    : details::section_details_literal_size(details::bool_false_literal<char_type>())) +
               details::section_details_literal_size(details::type_prefix<char_type>()) +
               print_reserve_size(::fast_io::io_reserve_type<char_type, value_type_section_details_wrapper_t>);
    }

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, global_type_section_details_wrapper_t>,
                                                     char_type* iter,
                                                     global_type_section_details_wrapper_t const global_section_details_wrapper) noexcept
    {
        iter = details::section_details_copy_literal(iter, details::mutable_prefix<char_type>());
        iter = global_section_details_wrapper.global.is_mutable
                   ? details::section_details_copy_literal(iter, details::bool_true_literal<char_type>())
                   : details::section_details_copy_literal(iter, details::bool_false_literal<char_type>());
        iter = details::section_details_copy_literal(iter, details::type_prefix<char_type>());
        return print_reserve_define(::fast_io::io_reserve_type<char_type, value_type_section_details_wrapper_t>,
                                    iter,
                                    section_details(global_section_details_wrapper.global.type));
    }

    /// @brief      External Types
    /// @details    External types classify imports and external values with their respective types
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.8
    enum class external_types : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
    {
        func = 0x00u,
        table = 0x01u,
        memory = 0x02u,
        global = 0x03u,
        // extern_type_end: for concept, not standard
        external_type_end = global
    };

    inline constexpr external_types section_details(external_types type) noexcept { return type; }

    /// @brief      Imports
    /// @details    The imports component of a module defines a set of imports that are required for instantiation
    /// @details    New feature
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.11
    struct extern_type
    {
        union storage_t
        {
            function_type const* function;
            static_assert(::std::is_trivially_copyable_v<decltype(function)> && ::std::is_trivially_destructible_v<decltype(function)>);
            table_type table;
            static_assert(::std::is_trivially_copyable_v<table_type> && ::std::is_trivially_destructible_v<table_type>);
            memory_type memory;
            static_assert(::std::is_trivially_copyable_v<memory_type> && ::std::is_trivially_destructible_v<memory_type>);
            global_type global;
            static_assert(::std::is_trivially_copyable_v<global_type> && ::std::is_trivially_destructible_v<global_type>);
        } storage{};

        external_types type{};
    };

    // print func

    template <::std::integral char_type>
    inline constexpr auto get_extern_kind_name(external_types exttype) noexcept
    {
        if constexpr(::std::same_as<char_type, char>)
        {
            switch(exttype)
            {
                case external_types::func: return ::uwvm2::utils::container::string_view{"func"};
                case external_types::table: return ::uwvm2::utils::container::string_view{"table"};
                case external_types::memory: return ::uwvm2::utils::container::string_view{"memory"};
                case external_types::global: return ::uwvm2::utils::container::string_view{"global"};
                default: return ::uwvm2::utils::container::string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::uwvm2::utils::container::wstring_view{L"func"};
                case external_types::table: return ::uwvm2::utils::container::wstring_view{L"table"};
                case external_types::memory: return ::uwvm2::utils::container::wstring_view{L"memory"};
                case external_types::global: return ::uwvm2::utils::container::wstring_view{L"global"};
                default: return ::uwvm2::utils::container::wstring_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::uwvm2::utils::container::u8string_view{u8"func"};
                case external_types::table: return ::uwvm2::utils::container::u8string_view{u8"table"};
                case external_types::memory: return ::uwvm2::utils::container::u8string_view{u8"memory"};
                case external_types::global: return ::uwvm2::utils::container::u8string_view{u8"global"};
                default: return ::uwvm2::utils::container::u8string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::uwvm2::utils::container::u16string_view{u"func"};
                case external_types::table: return ::uwvm2::utils::container::u16string_view{u"table"};
                case external_types::memory: return ::uwvm2::utils::container::u16string_view{u"memory"};
                case external_types::global: return ::uwvm2::utils::container::u16string_view{u"global"};
                default: return ::uwvm2::utils::container::u16string_view{};
            }
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            switch(exttype)
            {
                case external_types::func: return ::uwvm2::utils::container::u32string_view{U"func"};
                case external_types::table: return ::uwvm2::utils::container::u32string_view{U"table"};
                case external_types::memory: return ::uwvm2::utils::container::u32string_view{U"memory"};
                case external_types::global: return ::uwvm2::utils::container::u32string_view{U"global"};
                default: return ::uwvm2::utils::container::u32string_view{};
            }
        }
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, external_types>) noexcept
    { return 6u; }

    namespace details
    {
        template <::std::integral char_type>
        inline constexpr char_type* print_reserve_extern_kind_impl(char_type* iter, external_types exttype) noexcept
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n("func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n("table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n("memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n("global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(L"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(L"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(L"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(L"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(u8"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(u8"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(u8"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(u8"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(u"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(u"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(u"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(u"global", 6u, iter);
                    default: return iter;
                }
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                switch(exttype)
                {
                    case external_types::func: return ::fast_io::freestanding::my_copy_n(U"func", 4u, iter);
                    case external_types::table: return ::fast_io::freestanding::my_copy_n(U"table", 5u, iter);
                    case external_types::memory: return ::fast_io::freestanding::my_copy_n(U"memory", 6u, iter);
                    case external_types::global: return ::fast_io::freestanding::my_copy_n(U"global", 6u, iter);
                    default: return iter;
                }
            }
        }
    }  // namespace details

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, external_types>, char_type * iter, external_types exttype) noexcept
    { return details::print_reserve_extern_kind_impl(iter, exttype); }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::function_type>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::type::global_type>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/parser/wasm/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
