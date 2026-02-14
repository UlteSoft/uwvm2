/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-26
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
# include <cstring>
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <algorithm>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{

    ////////////////////////////
    /// @brief Binary Format ///
    ////////////////////////////
    struct wasm1;

    ///
    /// @brief Defining structures or concepts related to wasm1 versions
    ///

    ///////////////////////////
    /// @brief type section ///
    ///////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct type_section_storage_t;

    /// @brief Define functions for checking value_type to provide extensibility
    template <typename... Fs>
    concept has_check_typesec_value_type = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
                                                    ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> value_type) {
        { define_check_typesec_value_type(sec_adl, value_type) } -> ::std::same_as<bool>;
    };

    /// @brief Define functions to handle type prefix
    template <typename... Fs>
    concept has_type_prefix_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_type_type_t<Fs...>> type_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<Fs...> prefix,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
                 ::std::byte const* const prefix_module_ptr) {
            {
                define_type_prefix_handler(sec_adl, type_adl, prefix, module_storage, section_curr, section_end, err, fs_para, prefix_module_ptr)
            } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Define functions to handle type prefix
    template <typename... Fs>
    concept has_check_duplicate_types_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_type_type_t<Fs...>> type_adl,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            { define_check_duplicate_types(sec_adl, type_adl, module_storage, section_curr, section_end, err, fs_para) } -> ::std::same_as<void>;
        };

    /// @brief name checker
    struct type_function_checker_base_t
    {
        ::std::byte const* begin{};
        ::std::byte const* end{};
    };

    inline constexpr bool operator== (type_function_checker_base_t n1, type_function_checker_base_t n2) noexcept
    { return ::std::equal(n1.begin, n1.end, n2.begin, n2.end); }

    inline constexpr ::std::strong_ordering operator<=> (type_function_checker_base_t n1, type_function_checker_base_t n2) noexcept
    { return ::fast_io::freestanding::lexicographical_compare_three_way(n1.begin, n1.end, n2.begin, n2.end, ::std::compare_three_way{}); }

    struct type_function_checker
    {
        type_function_checker_base_t parameter{};
        type_function_checker_base_t result{};
    };

    inline constexpr bool operator== (type_function_checker const& n1, type_function_checker const& n2) noexcept
    { return n1.parameter == n2.parameter && n1.result == n2.result; }

    inline constexpr ::std::strong_ordering operator<=> (type_function_checker const& n1, type_function_checker const& n2) noexcept
    {
        ::std::strong_ordering const parameter_check{n1.parameter <=> n2.parameter};

        if(parameter_check != ::std::strong_ordering::equal) { return parameter_check; }

        return n1.result <=> n2.result;
    }

    /////////////////////////////
    /// @brief import section ///
    /////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct import_section_storage_t;

    /// @deprecated The wasm standard states that the standard sections follow a sequence, which can never happen
#if 0
    /// @brief Define functions for define_imported_and_defined_exceeding_checker
    template <typename... Fs>
    concept has_imported_and_defined_exceeding_checker = requires(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>> extern_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...> const*> const* importdesc_begin,
        ::std::byte const* section_curr,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        {
            define_imported_and_defined_exceeding_checker(sec_adl, extern_adl, module_storage, importdesc_begin, section_curr, err, fs_para)
        } -> ::std::same_as<void>;
    };
#endif

    /// @brief Define functions for handle extern_prefix
    template <typename... Fs>
    concept has_extern_prefix_imports_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>& fit,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            {
                define_extern_prefix_imports_handler(sec_adl, fit.imports, module_storage, section_curr, section_end, err, fs_para)
            } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Defining structures or concepts related to wasm versions
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_final_extern_type
    {
        union storage_t
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const* function;
            static_assert(::std::is_trivially_copyable_v<decltype(function)> && ::std::is_trivially_destructible_v<decltype(function)>);

            ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...> table;
            static_assert(::std::is_trivially_copyable_v<::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>> &&
                          ::std::is_trivially_destructible_v<::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>>);

            ::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...> memory;
            static_assert(::std::is_trivially_copyable_v<::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>> &&
                          ::std::is_trivially_destructible_v<::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>>);

            ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...> global;
            static_assert(::std::is_trivially_copyable_v<::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...>> &&
                          ::std::is_trivially_destructible_v<::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...>>);
        } storage{};

        ::uwvm2::parser::wasm::standard::wasm1::type::external_types type{};
    };

    /// @brief Wrapper for the final_import_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_final_extern_type_section_details_wrapper_t
    {
        wasm1_final_extern_type<Fs...> const* extern_type_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32* importdesc_counter_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_final_extern_type_section_details_wrapper_t<Fs...> section_details(
        wasm1_final_extern_type<Fs...> const& extern_type,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32* importdesc_counter_ptr) noexcept
    { return {::std::addressof(extern_type), ::std::addressof(all_sections), importdesc_counter_ptr}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_final_extern_type_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_final_extern_type_section_details_wrapper_t<Fs...> const extern_type_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(extern_type_details_wrapper.extern_type_ptr == nullptr || extern_type_details_wrapper.all_sections_ptr == nullptr ||
           extern_type_details_wrapper.importdesc_counter_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const& all_sections{*extern_type_details_wrapper.all_sections_ptr};

        switch(extern_type_details_wrapper.extern_type_ptr->type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                // After ADL degradation, pointer access is safe.
                auto& curr_importdesc_counter{extern_type_details_wrapper.importdesc_counter_ptr[0u]};
                auto const curr_functype_ptr{extern_type_details_wrapper.extern_type_ptr->storage.function};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(curr_functype_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                auto const& type_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(all_sections)};

                auto const type_section_cbegin{type_section.types.cbegin()};

                auto const sign{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_functype_ptr - type_section_cbegin)};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "func[",
                                                                     curr_importdesc_counter,
                                                                     "]: {sig: type[",
                                                                     sign,
                                                                     "]}");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"func[",
                                                                     curr_importdesc_counter,
                                                                     L"]: {sig: type[",
                                                                     sign,
                                                                     L"]}");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"func[",
                                                                     curr_importdesc_counter,
                                                                     u8"]: {sig: type[",
                                                                     sign,
                                                                     u8"]}");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"func[",
                                                                     curr_importdesc_counter,
                                                                     u"]: {sig: type[",
                                                                     sign,
                                                                     u"]}");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"func[",
                                                                     curr_importdesc_counter,
                                                                     U"]: {sig: type[",
                                                                     sign,
                                                                     U"]}");
                }

                ++curr_importdesc_counter;

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table:
            {
                // After ADL degradation, pointer access is safe.
                auto& curr_importdesc_counter{extern_type_details_wrapper.importdesc_counter_ptr[1u]};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "table[",
                                                                     curr_importdesc_counter,
                                                                     "]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.table),
                                                                     "}");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"table[",
                                                                     curr_importdesc_counter,
                                                                     L"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.table),
                                                                     L"}");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"table[",
                                                                     curr_importdesc_counter,
                                                                     u8"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.table),
                                                                     u8"}");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"table[",
                                                                     curr_importdesc_counter,
                                                                     u"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.table),
                                                                     u"}");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"table[",
                                                                     curr_importdesc_counter,
                                                                     U"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.table),
                                                                     U"}");
                }

                ++curr_importdesc_counter;
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory:
            {
                // After ADL degradation, pointer access is safe.
                auto& curr_importdesc_counter{extern_type_details_wrapper.importdesc_counter_ptr[2u]};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "memory[",
                                                                     curr_importdesc_counter,
                                                                     "]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.memory),
                                                                     "}");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"memory[",
                                                                     curr_importdesc_counter,
                                                                     L"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.memory),
                                                                     L"}");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"memory[",
                                                                     curr_importdesc_counter,
                                                                     u8"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.memory),
                                                                     u8"}");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"memory[",
                                                                     curr_importdesc_counter,
                                                                     u"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.memory),
                                                                     u"}");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"memory[",
                                                                     curr_importdesc_counter,
                                                                     U"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.memory),
                                                                     U"}");
                }

                ++curr_importdesc_counter;
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::global:
            {
                // After ADL degradation, pointer access is safe.
                auto& curr_importdesc_counter{extern_type_details_wrapper.importdesc_counter_ptr[3u]};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "global[",
                                                                     curr_importdesc_counter,
                                                                     "]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.global),
                                                                     "}");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"global[",
                                                                     curr_importdesc_counter,
                                                                     L"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.global),
                                                                     L"}");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"global[",
                                                                     curr_importdesc_counter,
                                                                     u8"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.global),
                                                                     u8"}");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"global[",
                                                                     curr_importdesc_counter,
                                                                     u"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.global),
                                                                     u"}");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"global[",
                                                                     curr_importdesc_counter,
                                                                     U"]: {",
                                                                     section_details(extern_type_details_wrapper.extern_type_ptr->storage.global),
                                                                     U"}");
                }

                ++curr_importdesc_counter;
                break;
            }
            [[unlikely]] default:
            {
                if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "unknown"); }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"unknown");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"unknown");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"unknown");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"unknown");
                }
                break;
            }
        }
    }

    /// @brief Define functions for handle imports table
    template <typename... Fs>
    concept has_extern_imports_table_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...>& fit_imports,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            {
                extern_imports_table_handler(sec_adl, fit_imports.storage.table, module_storage, section_curr, section_end, err, fs_para)
            } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Define functions for handle imports memory
    template <typename... Fs>
    concept has_extern_imports_memory_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...>& fit_imports,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            {
                extern_imports_memory_handler(sec_adl, fit_imports.storage.memory, module_storage, section_curr, section_end, err, fs_para)
            } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Define functions for handle imports global
    template <typename... Fs>
    concept has_extern_imports_global_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...>& fit_imports,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            {
                extern_imports_global_handler(sec_adl, fit_imports.storage.global, module_storage, section_curr, section_end, err, fs_para)
            } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Define functions for checking imports and exports name
    template <typename... Fs>
    concept can_check_import_export_text_format = requires(::uwvm2::parser::wasm::standard::wasm1::features::final_text_format_wapper<Fs...> adl,
                                                           ::std::byte const* begin,
                                                           ::std::byte const* end,
                                                           ::uwvm2::parser::wasm::base::error_impl& err) {
        { check_import_export_text_format(adl, begin, end, err) } -> ::std::same_as<void>;
    };

    /// @brief name checker
    struct name_checker
    {
        ::uwvm2::utils::container::u8string_view module_name{};
        ::uwvm2::utils::container::u8string_view extern_name{};
    };

    inline constexpr bool operator== (name_checker const& n1, name_checker const& n2) noexcept
    { return n1.module_name == n2.module_name && n1.extern_name == n2.extern_name; }

    inline constexpr ::std::strong_ordering operator<=> (name_checker const& n1, name_checker const& n2) noexcept
    {
        ::std::strong_ordering const module_name_check{n1.module_name <=> n2.module_name};

        if(module_name_check != ::std::strong_ordering::equal) { return module_name_check; }

        return n1.extern_name <=> n2.extern_name;
    }

    ///////////////////////////////
    /// @brief function section ///
    ///////////////////////////////

    struct function_section_storage_t;

    enum class vectypeidx_minimize_storage_mode : unsigned
    {
        null,
        u8_view,
        u8_vector,
        u16_vector,
        u32_vector
    };

    struct typeidx_u8_view_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 const* begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8 const* end{};
    };

    struct vectypeidx_minimize_storage_t 
    {
        inline static constexpr ::std::size_t sizeof_vectypeidx_minimize_storage_u{::uwvm2::parser::wasm::concepts::operation::get_union_size<
            typeidx_u8_view_t,
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8>,
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16>,
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>>()};

        union vectypeidx_minimize_storage_u 
        {
            typeidx_u8_view_t typeidx_u8_view;
            static_assert(::std::is_trivially_copyable_v<typeidx_u8_view_t> && ::std::is_trivially_destructible_v<typeidx_u8_view_t>);

            // Self-control of the life cycle
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u8> typeidx_u8_vector;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(typeidx_u8_vector)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(typeidx_u8_vector)>);

            // Self-control of the life cycle
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u16> typeidx_u16_vector;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(typeidx_u16_vector)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(typeidx_u16_vector)>);

            // Self-control of the life cycle
            ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> typeidx_u32_vector;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(typeidx_u32_vector)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(typeidx_u32_vector)>);

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte vectypeidx_minimize_storage_u_reserve[sizeof_vectypeidx_minimize_storage_u]{};

            // destructor of 'vectypeidx_minimize_storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~vectypeidx_minimize_storage_u() {}

            // The release of fast_io::vectors is managed by struct vectypeidx_minimize_storage_t, there is no issue of raii resources being unreleased.
        };

        static_assert(sizeof(vectypeidx_minimize_storage_u) == sizeof_vectypeidx_minimize_storage_u,
                      "sizeof_vectypeidx_minimize_storage_u not equal to sizeof_vectypeidx_minimize_storage_u");

        vectypeidx_minimize_storage_u storage{};
        vectypeidx_minimize_storage_mode mode{};

        inline constexpr vectypeidx_minimize_storage_t() noexcept = default;  // all-zero

        inline constexpr explicit vectypeidx_minimize_storage_t(vectypeidx_minimize_storage_mode new_mode) noexcept : mode{new_mode}
        {
            switch(this->mode)
            {
                case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){};
                    break;
                }
                [[unlikely]] default:
                {
                    // new_mode is external input and needs to be checked.
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr vectypeidx_minimize_storage_t(vectypeidx_minimize_storage_t const& other) noexcept : mode{other.mode}
        {
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    // trivial, placement new (regulate)
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){other.storage.typeidx_u8_view};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    // copy constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){other.storage.typeidx_u8_vector};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    // copy constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){other.storage.typeidx_u16_vector};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    // copy constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){other.storage.typeidx_u32_vector};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        inline constexpr vectypeidx_minimize_storage_t(vectypeidx_minimize_storage_t&& other) noexcept : mode{other.mode}
        {
            // Since fast_io::vector satisfies both is_trivially_copyable_or_relocatable and is_zero_default_constructible, it is possible to do this

            // There is no need to set other.mode here, as it is a move, and if you want to modify other.mode you need to destruct the type on other

            // move data
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    // trivial, placement new (regulate)
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){::std::move(other.storage.typeidx_u8_view)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    // move constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){
                        ::std::move(other.storage.typeidx_u8_vector)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    // move constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){
                        ::std::move(other.storage.typeidx_u16_vector)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    // move constructor, placement new
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){
                        ::std::move(other.storage.typeidx_u32_vector)};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        inline constexpr vectypeidx_minimize_storage_t& operator= (vectypeidx_minimize_storage_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // Prevent type puns, must destruct union
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    // The trivial destructor type does not require any destructors.
                    static_assert(::std::is_trivially_destructible_v<decltype(this->storage.typeidx_u8_view)>);
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u8_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u16_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u32_vector));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            this->mode = other.mode;

            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){other.storage.typeidx_u8_view};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){other.storage.typeidx_u8_vector};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){other.storage.typeidx_u16_vector};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){other.storage.typeidx_u32_vector};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            return *this;
        }

        inline constexpr vectypeidx_minimize_storage_t& operator= (vectypeidx_minimize_storage_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // Prevent type puns, must destruct union
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    static_assert(::std::is_trivially_destructible_v<decltype(this->storage.typeidx_u8_view)>);
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u8_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u16_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u32_vector));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            // Since fast_io::vector satisfies both is_trivially_copyable_or_relocatable and is_zero_default_constructible, it is possible to do this

            this->mode = other.mode;

            // There is no need to set other.mode here, as it is a move, and if you want to modify other.mode you need to destruct the type on other

            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    // do nothing
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){::std::move(other.storage.typeidx_u8_view)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){
                        ::std::move(other.storage.typeidx_u8_vector)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){
                        ::std::move(other.storage.typeidx_u16_vector)};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){
                        ::std::move(other.storage.typeidx_u32_vector)};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            return *this;
        }

        inline constexpr ~vectypeidx_minimize_storage_t()
        {
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    // Multiple destructuring is ub in the standard, so mundane types don't need to do any operations
                    // this->storage.typeidx_u8_view = {};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u8_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u16_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u32_vector));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            // No need to set this mode, multiple destructs ub
        }

        inline constexpr void clear_destroy() noexcept
        {
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    this->storage.typeidx_u8_view = {};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    // clear_destroy equivalent to destructuring
                    this->storage.typeidx_u8_vector.clear_destroy();
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    // clear_destroy equivalent to destructuring
                    this->storage.typeidx_u16_vector.clear_destroy();
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    // clear_destroy equivalent to destructuring
                    this->storage.typeidx_u32_vector.clear_destroy();
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        inline constexpr void clear() noexcept
        {
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    this->storage.typeidx_u8_view = {};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    this->storage.typeidx_u8_vector.clear();
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    this->storage.typeidx_u16_vector.clear();
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    this->storage.typeidx_u32_vector.clear();
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        inline constexpr void change_mode(vectypeidx_minimize_storage_mode new_mode) noexcept
        {
            // fast path: no-op when mode unchanged
            if(this->mode == new_mode) [[unlikely]] { return; }

            // 1) Destroy currently active member if needed
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    // trivial, nothing to do
                    static_assert(::std::is_trivially_copyable_v<decltype(this->storage.typeidx_u8_view)>);
                    static_assert(::std::is_trivially_destructible_v<decltype(this->storage.typeidx_u8_view)>);

                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u8_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u16_vector));
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::std::destroy_at(::std::addressof(this->storage.typeidx_u32_vector));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }

            // 2) Activate new member via placement-new
            this->mode = new_mode;

            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_view)) decltype(this->storage.typeidx_u8_view){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u8_vector)) decltype(this->storage.typeidx_u8_vector){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u16_vector)) decltype(this->storage.typeidx_u16_vector){};
                    break;
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    ::new(::std::addressof(this->storage.typeidx_u32_vector)) decltype(this->storage.typeidx_u32_vector){};
                    break;
                }
                [[unlikely]] default:
                {
                    // new_mode is external input and needs to be checked.
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr ::std::size_t size() const noexcept
        {
            switch(this->mode)
            {
                case vectypeidx_minimize_storage_mode::null:
                {
                    // There should be no error here; it should just return 0.
                    return 0uz;
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    return static_cast<::std::size_t>(this->storage.typeidx_u8_view.end - this->storage.typeidx_u8_view.begin);
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    return this->storage.typeidx_u8_vector.size();
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    return this->storage.typeidx_u16_vector.size();
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    return this->storage.typeidx_u32_vector.size();
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 index_unchecked(::std::size_t sz) const noexcept
        {
            switch(this->mode)
            {
                [[unlikely]] case vectypeidx_minimize_storage_mode::null:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
                case vectypeidx_minimize_storage_mode::u8_view:
                {
                    return static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(this->storage.typeidx_u8_view.begin[sz]);
                }
                case vectypeidx_minimize_storage_mode::u8_vector:
                {
                    return static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(this->storage.typeidx_u8_vector.index_unchecked(sz));
                }
                case vectypeidx_minimize_storage_mode::u16_vector:
                {
                    return static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(this->storage.typeidx_u16_vector.index_unchecked(sz));
                }
                case vectypeidx_minimize_storage_mode::u32_vector:
                {
                    return this->storage.typeidx_u32_vector.index_unchecked(sz);
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }
    };

    ////////////////////////////
    /// @brief table section ///
    ////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct table_section_storage_t;

    /// @brief Define functions for handle table section table
    template <typename... Fs>
    concept has_table_section_table_handler = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
                                                       ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>& table_r,
                                                       ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                                       ::std::byte const* section_curr,
                                                       ::std::byte const* const section_end,
                                                       ::uwvm2::parser::wasm::base::error_impl& err,
                                                       ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        { table_section_table_handler(sec_adl, table_r, module_storage, section_curr, section_end, err, fs_para) } -> ::std::same_as<::std::byte const*>;
    };

    /////////////////////////////
    /// @brief memory section ///
    /////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct memory_section_storage_t;

    /// @brief Define functions for handle memory section memory
    template <typename... Fs>
    concept has_memory_section_memory_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<memory_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>& memory_r,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            { memory_section_memory_handler(sec_adl, memory_r, module_storage, section_curr, section_end, err, fs_para) } -> ::std::same_as<::std::byte const*>;
        };

    /////////////////////////////
    /// @brief global section ///
    /////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct global_section_storage_t;

    /// @brief Define functions for handle global section global
    template <typename... Fs>
    concept has_global_section_global_handler =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...>& global_r,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            { global_section_global_handler(sec_adl, global_r, module_storage, section_curr, section_end, err, fs_para) } -> ::std::same_as<::std::byte const*>;
        };

    /// @brief Used to check for type errors when parsing initialization expressions.
    template <typename... Fs>
    concept has_parse_and_check_global_expr_valid =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_global_type<Fs...> const& global_r,
                 ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& global_expr,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* section_curr,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
            {
                parse_and_check_global_expr_valid(sec_adl, global_r, global_expr, module_storage, section_curr, section_end, err, fs_para)
            } -> ::std::same_as<::std::byte const*>;
        };

    /////////////////////////////
    /// @brief export section ///
    /////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct export_section_storage_t;

    union wasm1_final_export_storage_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_idx;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_idx;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_idx;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_idx;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_final_export_type
    {
        wasm1_final_export_storage_t storage{};
        ::uwvm2::parser::wasm::standard::wasm1::type::external_types type{};
    };

    /// @brief Wrapper for the final_export_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_final_export_type_section_details_wrapper_t
    {
        wasm1_final_export_type<Fs...> const* export_type_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_final_export_type_section_details_wrapper_t<Fs...> section_details(
        wasm1_final_export_type<Fs...> const& export_type,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(export_type), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_final_export_type_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_final_export_type_section_details_wrapper_t<Fs...> const export_type_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(export_type_details_wrapper.export_type_ptr == nullptr || export_type_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        switch(export_type_details_wrapper.export_type_ptr->type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                // After ADL degradation, pointer access is safe.
                auto const curr_sign{export_type_details_wrapper.export_type_ptr->storage.func_idx};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "func[", curr_sign, "]");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"func[", curr_sign, L"]");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"func[", curr_sign, u8"]");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"func[", curr_sign, u"]");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"func[", curr_sign, U"]");
                }

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table:
            {
                // After ADL degradation, pointer access is safe.
                auto const curr_sign{export_type_details_wrapper.export_type_ptr->storage.table_idx};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "table[", curr_sign, "]");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"table[", curr_sign, L"]");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"table[", curr_sign, u8"]");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"table[", curr_sign, u"]");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"table[", curr_sign, U"]");
                }

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory:
            {
                // After ADL degradation, pointer access is safe.
                auto const curr_sign{export_type_details_wrapper.export_type_ptr->storage.memory_idx};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "memory[", curr_sign, "]");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"memory[", curr_sign, L"]");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"memory[", curr_sign, u8"]");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"memory[", curr_sign, u"]");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"memory[", curr_sign, U"]");
                }

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::global:
            {
                // After ADL degradation, pointer access is safe.
                auto const curr_sign{export_type_details_wrapper.export_type_ptr->storage.global_idx};

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "global[", curr_sign, "]");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"global[", curr_sign, L"]");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"global[", curr_sign, u8"]");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"global[", curr_sign, u"]");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"global[", curr_sign, U"]");
                }

                break;
            }
            [[unlikely]] default:
            {
                if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "unknown"); }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"unknown");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"unknown");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"unknown");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"unknown");
                }

                break;
            }
        }
    }

    template <typename... Fs>
    concept has_handle_export_index = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<export_section_storage_t<Fs...>> sec_adl,
                                               ::uwvm2::parser::wasm::standard::wasm1::features::final_export_type_t<Fs...>& fwet,
                                               ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                               ::std::byte const* section_curr,
                                               ::std::byte const* const section_end,
                                               ::uwvm2::parser::wasm::base::error_impl& err,
                                               ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        {
            define_handler_export_index(sec_adl, fwet.storage, fwet.type, module_storage, section_curr, section_end, err, fs_para)
        } -> ::std::same_as<::std::byte const*>;
    };

    //////////////////////////////
    /// @brief element section ///
    //////////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct element_section_storage_t;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_elem_storage_t 
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_idx{};
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> vec_funcidx{};
    };

    /// @brief Wrapper for the final_import_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_elem_storage_t_section_details_wrapper_t
    {
        wasm1_elem_storage_t<Fs...> const* element_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_elem_storage_t_section_details_wrapper_t<Fs...> section_details(
        wasm1_elem_storage_t<Fs...> const& element_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(element_storage), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_elem_storage_t_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_elem_storage_t_section_details_wrapper_t<Fs...> const element_storage_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(element_storage_details_wrapper.element_storage_ptr == nullptr || element_storage_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                            "table_idx: ",
                                                            element_storage_details_wrapper.element_storage_ptr->table_idx,
                                                            ", count: ",
                                                            element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size());
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                            L"table_idx: ",
                                                            element_storage_details_wrapper.element_storage_ptr->table_idx,
                                                            L", count: ",
                                                            element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size());
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                            u8"table_idx: ",
                                                            element_storage_details_wrapper.element_storage_ptr->table_idx,
                                                            u8", count: ",
                                                            element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size());
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                            u"table_idx: ",
                                                            element_storage_details_wrapper.element_storage_ptr->table_idx,
                                                            u", count: ",
                                                            element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size());
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                            U"table_idx: ",
                                                            element_storage_details_wrapper.element_storage_ptr->table_idx,
                                                            U", count: ",
                                                            element_storage_details_wrapper.element_storage_ptr->vec_funcidx.size());
        }

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 curr_elem_counter{};

        for(auto const curr_func_idx: element_storage_details_wrapper.element_storage_ptr->vec_funcidx)
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 "  - ref[",
                                                                 curr_elem_counter,
                                                                 "] -> func[",
                                                                 curr_func_idx,
                                                                 "]\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 L"  - ref[",
                                                                 curr_elem_counter,
                                                                 L"] -> func[",
                                                                 curr_func_idx,
                                                                 L"]\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u8"  - ref[",
                                                                 curr_elem_counter,
                                                                 u8"] -> func[",
                                                                 curr_func_idx,
                                                                 u8"]\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 u"  - ref[",
                                                                 curr_elem_counter,
                                                                 u"] -> func[",
                                                                 curr_func_idx,
                                                                 u"]\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                 U"  - ref[",
                                                                 curr_elem_counter,
                                                                 U"] -> func[",
                                                                 curr_func_idx,
                                                                 U"]\n");
            }

            ++curr_elem_counter;
        }
    }
}

// Subsequent specifications of union must include this information, so it has to be declared here.
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    enum class wasm1_element_type_t : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
    {
        tableidx = 0u,
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>  // Fs is used as an extension to an existing type, but does not extend the type
    struct wasm1_element_t 
    {
        inline static constexpr ::std::size_t sizeof_storage_u{sizeof(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...>)};

        union storage_u 
        {
            // This type can be initialized with all zeros
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...> table_idx;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(table_idx)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(table_idx)>);

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte sizeof_storage_u_reserve[sizeof_storage_u]{};

            // destructor of 'storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~storage_u() {}

            // The release of table_idx is managed by struct wasm1_element_t, there is no issue of raii resources being unreleased.
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_storage_u, "sizeof(storage_t) not equal to sizeof_storage_u");

        // In wasm1, type stands for table index, which conceptually can be any value, but since the standard specifies only 1 table, it can only be 0. Here
        // union does not need to make any type-safe judgments since there is only one type.

        wasm1_element_type_t type{};

        inline constexpr wasm1_element_t() noexcept { ::new(::std::addressof(this->storage.table_idx)) decltype(this->storage.table_idx){}; }

        inline constexpr wasm1_element_t(wasm1_element_t const& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.table_idx)) decltype(this->storage.table_idx){other.storage.table_idx}; }

        inline constexpr wasm1_element_t(wasm1_element_t&& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.table_idx)) decltype(this->storage.table_idx){::std::move(other.storage.table_idx)}; }

        inline constexpr wasm1_element_t& operator= (wasm1_element_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The table_idx type in union is always valid regardless of the value of type.

            this->type = other.type;

            this->storage.table_idx = other.storage.table_idx;

            return *this;
        }

        inline constexpr wasm1_element_t& operator= (wasm1_element_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The table_idx type in union is always valid regardless of the value of type.

            this->type = other.type;

            this->storage.table_idx = ::std::move(other.storage.table_idx);

            return *this;
        }

        inline constexpr ~wasm1_element_t() { ::std::destroy_at(::std::addressof(this->storage.table_idx)); }
    };

    /// @brief Wrapper for the final_import_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_element_t_section_details_wrapper_t
    {
        wasm1_element_t<Fs...> const* element_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_element_t_section_details_wrapper_t<Fs...> section_details(
        wasm1_element_t<Fs...> const& element,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(element), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_element_t_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_element_t_section_details_wrapper_t<Fs...> const element_details_wrapper)
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
                "flag: 0, ",
                section_details(element_details_wrapper.element_ptr->storage.table_idx, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                L"flag: 0, ",
                section_details(element_details_wrapper.element_ptr->storage.table_idx, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u8"flag: 0, ",
                section_details(element_details_wrapper.element_ptr->storage.table_idx, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u"flag: 0, ",
                section_details(element_details_wrapper.element_ptr->storage.table_idx, *element_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                U"flag: 0, ",
                section_details(element_details_wrapper.element_ptr->storage.table_idx, *element_details_wrapper.all_sections_ptr));
        }
    }

    template <typename... Fs>
    concept has_handle_element_type = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<element_section_storage_t<Fs...>> sec_adl,
                                               ::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...>& fet,
                                               ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                               ::std::byte const* section_curr,
                                               ::std::byte const* const section_end,
                                               ::uwvm2::parser::wasm::base::error_impl& err,
                                               ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        {
            define_handler_element_type(sec_adl, fet.storage, fet.type, module_storage, section_curr, section_end, err, fs_para)
        } -> ::std::same_as<::std::byte const*>;
    };

    ///////////////////////////
    /// @brief code section ///
    ///////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct code_section_storage_t;

    struct code_body_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* code_begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* expr_begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* code_end{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_local_entry_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count{};
        ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> type{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_wasm_code_t 
    {
        code_body_t body{};
        ::uwvm2::utils::container::vector<final_local_entry_t<Fs...>> locals{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_local_count{};
    };

    /// @brief Define functions for checking value_type to provide extensibility
    template <typename... Fs>
    concept has_check_codesec_value_type = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>> sec_adl,
                                                    ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> value_type) {
        { define_check_codesec_value_type(sec_adl, value_type) } -> ::std::same_as<bool>;
    };

    ///////////////////////////
    /// @brief data section ///
    ///////////////////////////

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct data_section_storage_t;

    struct wasm1_data_init_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* begin{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* end{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_data_storage_t 
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_idx{};
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
        wasm1_data_init_t byte{};

        // trivially copyable or relocatable
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<final_wasm_const_expr<Fs...>>);
    };

    /// @brief Wrapper for the wasm1_data_storage_t
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_data_storage_t_section_details_wrapper_t
    {
        wasm1_data_storage_t<Fs...> const* data_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_data_storage_t_section_details_wrapper_t<Fs...> section_details(
        wasm1_data_storage_t<Fs...> const& data_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(data_storage), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_data_storage_t_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_data_storage_t_section_details_wrapper_t<Fs...> const data_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(data_details_wrapper.data_storage_ptr == nullptr || data_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                "memory_idx: ",
                data_details_wrapper.data_storage_ptr->memory_idx,
                ", size: ",
                static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                L"memory_idx: ",
                data_details_wrapper.data_storage_ptr->memory_idx,
                L", size: ",
                static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u8"memory_idx: ",
                data_details_wrapper.data_storage_ptr->memory_idx,
                u8", size: ",
                static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u"memory_idx: ",
                data_details_wrapper.data_storage_ptr->memory_idx,
                u", size: ",
                static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                U"memory_idx: ",
                data_details_wrapper.data_storage_ptr->memory_idx,
                U", size: ",
                static_cast<::std::size_t>(data_details_wrapper.data_storage_ptr->byte.end - data_details_wrapper.data_storage_ptr->byte.begin));
        }
    }
}

// Subsequent specifications of union must include this information, so it has to be declared here.
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    enum class wasm1_data_type_t : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
    {
        memoryidx = 0u,
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>  // Fs is used as an extension to an existing type, but does not extend the type
    struct wasm1_data_t 
    {
        inline static constexpr ::std::size_t sizeof_storage_u{sizeof(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_storage_t<Fs...>)};

        union storage_u
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_storage_t<Fs...> memory_idx;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(memory_idx)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(memory_idx)>);

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte sizeof_storage_u_reserve[sizeof_storage_u]{};

            // destructor of 'storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~storage_u() {}

            // The release of memory_idx is managed by struct wasm1_data_t, there is no issue of raii resources being unreleased.
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_storage_u, "sizeof(storage_t) not equal to sizeof_storage_u");

        // In wasm1, type stands for memory index, which conceptually can be any value, but since the standard specifies only 1 memory, it can only be 0.
        // Here union does not need to make any type-safe judgments since there is only one type.

        wasm1_data_type_t type{};

        inline constexpr wasm1_data_t() noexcept { ::new(::std::addressof(this->storage.memory_idx)) decltype(this->storage.memory_idx){}; }

        inline constexpr wasm1_data_t(wasm1_data_t const& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.memory_idx)) decltype(this->storage.memory_idx){other.storage.memory_idx}; }

        inline constexpr wasm1_data_t(wasm1_data_t&& other) noexcept : type{other.type}
        { ::new(::std::addressof(this->storage.memory_idx)) decltype(this->storage.memory_idx){::std::move(other.storage.memory_idx)}; }

        inline constexpr wasm1_data_t& operator= (wasm1_data_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The memory_idx type in union is always valid regardless of the value of type.

            this->type = other.type;

            this->storage.memory_idx = other.storage.memory_idx;

            return *this;
        }

        inline constexpr wasm1_data_t& operator= (wasm1_data_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            // The memory_idx type in union is always valid regardless of the value of type.

            this->type = other.type;

            this->storage.memory_idx = ::std::move(other.storage.memory_idx);

            return *this;
        }

        inline constexpr ~wasm1_data_t() { ::std::destroy_at(::std::addressof(this->storage.memory_idx)); }
    };

    /// @brief Wrapper for the wasm1_data_t
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm1_data_t_section_details_wrapper_t
    {
        wasm1_data_t<Fs...> const* data_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm1_data_t_section_details_wrapper_t<Fs...> section_details(
        wasm1_data_t<Fs...> const& data,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(data), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm1_data_t_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm1_data_t_section_details_wrapper_t<Fs...> const data_details_wrapper)
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
                "flag: 0, ",
                section_details(data_details_wrapper.data_ptr->storage.memory_idx, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                L"flag: 0, ",
                section_details(data_details_wrapper.data_ptr->storage.memory_idx, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u8"flag: 0, ",
                section_details(data_details_wrapper.data_ptr->storage.memory_idx, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                u"flag: 0, ",
                section_details(data_details_wrapper.data_ptr->storage.memory_idx, *data_details_wrapper.all_sections_ptr));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                U"flag: 0, ",
                section_details(data_details_wrapper.data_ptr->storage.memory_idx, *data_details_wrapper.all_sections_ptr));
        }
    }

    template <typename... Fs>
    concept has_handle_data_type = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<data_section_storage_t<Fs...>> sec_adl,
                                            ::uwvm2::parser::wasm::standard::wasm1::features::final_data_type_t<Fs...>& fet,
                                            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                            ::std::byte const* section_curr,
                                            ::std::byte const* const section_end,
                                            ::uwvm2::parser::wasm::base::error_impl& err,
                                            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        {
            define_handler_data_type(sec_adl, fet.storage, fet.type, module_storage, section_curr, section_end, err, fs_para)
        } -> ::std::same_as<::std::byte const*>;
    };

    //////////////////////////
    /// @brief Final Check ///
    //////////////////////////

    struct wasm1_final_check;

    ///////////////////////////
    /// @brief Code Version ///
    ///////////////////////////

    struct wasm1_code_version
    {
    };
}

UWVM_MODULE_EXPORT namespace std
{
    template <>
    struct hash<::uwvm2::parser::wasm::standard::wasm1::features::type_function_checker>
    {
        inline constexpr ::std::size_t operator() (::uwvm2::parser::wasm::standard::wasm1::features::type_function_checker const& checker) const noexcept
        {
#if CHAR_BIT > 8
            // use std hash
            using char_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
            ::uwvm2::utils::container::string_view const tmp_para{reinterpret_cast<char_const_may_alias_ptr>(checker.parameter.begin),
                                                                  static_cast<::std::size_t>(checker.parameter.end - checker.parameter.begin)};
            ::uwvm2::utils::container::string_view const tmp_res{reinterpret_cast<char_const_may_alias_ptr>(checker.result.begin),
                                                                 static_cast<::std::size_t>(checker.result.end - checker.result.begin)};
            ::std::size_t const h1{::std::hash<::uwvm2::utils::container::string_view>{}(tmp_para)};
            ::std::size_t const h2{::std::hash<::uwvm2::utils::container::string_view>{}(tmp_res)};
            return static_cast<::std::size_t>(h1 ^ (h2 + 0x9e3779b9u + (h1 << 6u) + (h1 >> 2u) + (h2 << 16u)));
#else
            ::std::size_t const parameter_sz{static_cast<::std::size_t>(checker.parameter.end - checker.parameter.begin)};
            ::std::size_t const result_sz{static_cast<::std::size_t>(checker.result.end - checker.result.begin)};
            auto const seed1{::uwvm2::utils::hash::xxh3_64bits(checker.parameter.begin, parameter_sz)};
            return static_cast<::std::size_t>(::uwvm2::utils::hash::xxh3_64bits(checker.result.begin, result_sz, seed1));
#endif
        }
    };

    template <>
    struct hash<::uwvm2::parser::wasm::standard::wasm1::features::name_checker>
    {
        inline constexpr ::std::size_t operator() (::uwvm2::parser::wasm::standard::wasm1::features::name_checker const& checker) const noexcept
        {
#if CHAR_BIT > 8
            // use std hash
            ::std::size_t h1{::std::hash<::uwvm2::utils::container::u8string_view>{}(checker.module_name)};
            ::std::size_t h2{::std::hash<::uwvm2::utils::container::u8string_view>{}(checker.extern_name)};
            return static_cast<::std::size_t>(h1 ^ (h2 + 0x9e3779b9u + (h1 << 6u) + (h1 >> 2u) + (h2 << 16u)));
#else
            ::std::size_t module_name_sz;
            ::std::size_t extern_name_sz;
            if constexpr(requires { checker.module_name.size_bytes(); }) { module_name_sz = checker.module_name.size_bytes(); }
            else
            {
                module_name_sz = checker.module_name.size() * sizeof(char8_t);
            }

            if constexpr(requires { checker.extern_name.size_bytes(); }) { extern_name_sz = checker.extern_name.size_bytes(); }
            else
            {
                extern_name_sz = checker.extern_name.size() * sizeof(char8_t);
            }

            auto const seed1{::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(checker.module_name.data()), module_name_sz)};

            return static_cast<::std::size_t>(
                ::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(checker.extern_name.data()), extern_name_sz, seed1));
#endif
        }
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_local_entry_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_export_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
