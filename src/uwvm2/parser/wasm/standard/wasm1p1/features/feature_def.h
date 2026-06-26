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
# include "def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief wasm1.1 table type with funcref/externref reference type support.
    struct table_type
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::limits_type limits{};
        ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reftype{::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref};
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
        auto const valtype{::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(wrapper.table.reftype)};
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "type: ", valtype, ", ", section_details(wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"type: ", valtype, L", ", section_details(wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"type: ", valtype, u8", ", section_details(wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"type: ", valtype, u", ", section_details(wrapper.table.limits));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"type: ", valtype, U", ", section_details(wrapper.table.limits));
        }
    }

    /// @brief wasm1.1 global type with extended value-type support.
    struct global_type
    {
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type type{};
        bool is_mutable{};
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
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             "type: ",
                                                             wrapper.global.type,
                                                             ", mutable: ",
                                                             wrapper.global.is_mutable);
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             L"type: ",
                                                             wrapper.global.type,
                                                             L", mutable: ",
                                                             wrapper.global.is_mutable);
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u8"type: ",
                                                             wrapper.global.type,
                                                             u8", mutable: ",
                                                             wrapper.global.is_mutable);
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u"type: ",
                                                             wrapper.global.type,
                                                             u", mutable: ",
                                                             wrapper.global.is_mutable);
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             U"type: ",
                                                             wrapper.global.type,
                                                             U", mutable: ",
                                                             wrapper.global.is_mutable);
        }
    }

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

    /// @brief Wire-format element segment flags introduced by reference types and bulk memory.
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

        auto const func_count{element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size()};
        auto const expr_count{element_storage_details_wrapper.element_storage_ptr->vec_expr.size()};
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<true>(
                ::std::forward<Stm>(stream),
                "table_idx: ",
                element_storage_details_wrapper.element_storage_ptr->table_idx,
                ", type: ",
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage_details_wrapper.element_storage_ptr->reftype),
                ", count: ",
                func_count + expr_count);
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<true>(
                ::std::forward<Stm>(stream),
                L"table_idx: ",
                element_storage_details_wrapper.element_storage_ptr->table_idx,
                L", type: ",
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage_details_wrapper.element_storage_ptr->reftype),
                L", count: ",
                func_count + expr_count);
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<true>(
                ::std::forward<Stm>(stream),
                u8"table_idx: ",
                element_storage_details_wrapper.element_storage_ptr->table_idx,
                u8", type: ",
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage_details_wrapper.element_storage_ptr->reftype),
                u8", count: ",
                func_count + expr_count);
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<true>(
                ::std::forward<Stm>(stream),
                u"table_idx: ",
                element_storage_details_wrapper.element_storage_ptr->table_idx,
                u", type: ",
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage_details_wrapper.element_storage_ptr->reftype),
                u", count: ",
                func_count + expr_count);
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<true>(
                ::std::forward<Stm>(stream),
                U"table_idx: ",
                element_storage_details_wrapper.element_storage_ptr->table_idx,
                U", type: ",
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage_details_wrapper.element_storage_ptr->reftype),
                U", count: ",
                func_count + expr_count);
        }
    }

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
