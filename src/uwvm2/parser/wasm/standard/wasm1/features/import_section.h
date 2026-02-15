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
 * @date        2025-04-27
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
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "parser_limit.h"
# include "types.h"
# include "type_section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct import_section_storage_t 
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Import"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::import_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>> imports{};

        inline static constexpr ::std::size_t importdesc_count{
            static_cast<::std::size_t>(decltype(::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>{}.type)::external_type_end) + 1uz};
        ::uwvm2::utils::container::array<::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...> const*>,
                                         importdesc_count>
            importdesc{};
    };

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const*
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_func_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const*& funcptr_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [... import_func_type] type_index ... module_namelen (section_end)
        // [       safe         ] unsafe (could be the section_end)
        //                        ^^ section_curr

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        // get type index

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
        auto const [type_index_next, type_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(type_index))};
        if(type_index_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_index_err);
        }

        // [... import_func_type type_index ...] module_namelen (section_end)
        // [              safe                 ] unsafe (could be the section_end)
        //                       ^^ section_curr

        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        ::std::size_t const type_section_count{typesec.types.size()};

        // check: type_index should less than type_section_count
        if(type_index >= type_section_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32 = type_index;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // Storing Temporary Variables into Modules
        funcptr_r = typesec.types.cbegin() + type_index;

        // set curr
        section_curr = reinterpret_cast<::std::byte const*>(type_index_next);
        // [... import_func_type type_index ...] module_namelen (section_end)
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        return section_curr;
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::table_type & table_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_table_type(table_r, section_curr, section_end, err);
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_memory_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::memory_type & memory_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked

        // Keep a stable pointer to report page-limit violations at the beginning of the limits encoding.
        auto const check_max_page_ptr{section_curr};

        // [ ... ] | memory ...
        // [ safe] | unsafe (could be the section_end)
        //           ^^ section_curr
        //           ^^ check_max_page_ptr

        section_curr = ::uwvm2::parser::wasm::standard::wasm1::features::scan_memory_type(memory_r, section_curr, section_end, err);

        // [ ...  | memory] ...
        // [safe  |       ] unsafe (could be the section_end)
        //                  ^^ section_curr

        // In MVP, the memory page count (min and optional max) cannot exceed 65,536 (u32max / default_page_size).
        constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 wasm1_max_pages{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(65536u)};

        if(memory_r.limits.min > wasm1_max_pages) [[unlikely]]
        {
            err.err_curr = check_max_page_ptr;
            err.err_selectable.u32 = memory_r.limits.min;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::memory_section_resolved_exceeded_the_maximum_value;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if(memory_r.limits.present_max)
        {
            if(memory_r.limits.max > wasm1_max_pages) [[unlikely]]
            {
                err.err_curr = check_max_page_ptr;
                err.err_selectable.u32 = memory_r.limits.max;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::memory_section_resolved_exceeded_the_maximum_value;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        return section_curr;
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::global_type & global_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_global_type(global_r, section_curr, section_end, err);
    }

    /// @brief Define function for wasm1 external_types
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_extern_prefix_imports_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...> & fit_imports,  // [adl] can be replaced
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        switch(fit_imports.type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                // fit_imports.storage.function won't be replaced.
                return extern_imports_func_handler(sec_adl, fit_imports.storage.function, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_table_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_table_handler(sec_adl, fit_imports.storage.table, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_memory_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_memory_handler(sec_adl, fit_imports.storage.memory, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::global:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_global_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_global_handler(sec_adl, fit_imports.storage.global, module_storage, section_curr, section_end, err, fs_para);
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();  // never match, checked before
            }
        }
        return section_curr;
    }

    /// @brief      Define a function for wasm1 to check for utf8 sequences.
    /// @details    wasm1.0 specifies that name is a byte sequence with no encoding format requirements.
    /// @note       ADL for distribution to the correct handler function
    inline constexpr void check_import_export_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::null>,  // [adl]
        [[maybe_unused]] ::std::byte const* begin,
        [[maybe_unused]] ::std::byte const* end,
        [[maybe_unused]] ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        // wasm1.0 specifies that name is a byte sequence with no encoding format requirements.
        // do nothing
    }

    inline constexpr void check_import_export_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::zero_illegal>,  // [adl]
        ::std::byte const* begin,
        ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [utf8pos, utf8err]{::uwvm2::utils::utf::check_has_zero_illegal_unchecked(reinterpret_cast<char8_t_const_may_alias_ptr>(begin),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(end))};

        if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            err.err_curr = reinterpret_cast<::std::byte const*>(utf8pos);
            err.err_selectable.u32 = static_cast<::std::uint_least32_t>(utf8err);
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_char_sequence;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    inline constexpr void check_import_export_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629>,  // [adl]
        ::std::byte const* begin,
        ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [utf8pos, utf8err]{::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(
            reinterpret_cast<char8_t_const_may_alias_ptr>(begin),
            reinterpret_cast<char8_t_const_may_alias_ptr>(end))};

        if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            err.err_curr = reinterpret_cast<::std::byte const*>(utf8pos);
            err.err_selectable.u32 = static_cast<::std::uint_least32_t>(utf8err);
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_char_sequence;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    inline constexpr void check_import_export_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629_with_zero_illegal>,  // [adl]
        ::std::byte const* begin,
        ::std::byte const* end,
        ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [utf8pos, utf8err]{::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(
            reinterpret_cast<char8_t_const_may_alias_ptr>(begin),
            reinterpret_cast<char8_t_const_may_alias_ptr>(end))};

        if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            err.err_curr = reinterpret_cast<::std::byte const*>(utf8pos);
            err.err_selectable.u32 = static_cast<::std::uint_least32_t>(utf8err);
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_char_sequence;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Define the handler function for type_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse import section (id: 2)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get import_section_storage_t from storages
        auto& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};

        // check duplicate
        if(importsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = importsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // There is no need to check typesec.sec_span.sec_begin (forward_dependency_missing) here,
        // it can be checked later in handle_import_func with index
        // Turning on checking can instead cause non-typesection import dependencies to error out

        // import section
        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        importsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        importsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};
        // [before_section ... ] | import_count ... module_namelen ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 import_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [import_count_next, import_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(import_count))};

        if(import_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(import_count_err);
        }

        // [before_section ... | import_count ...] module_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(import_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(import_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(import_count) > parser_limit.max_import_sec_imports) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"importsec_imports";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(import_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_import_sec_imports;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        importsec.imports.reserve(static_cast<::std::size_t>(import_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 import_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(import_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | import_count ...] module_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        ::uwvm2::utils::container::array<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, importdesc_count> importdesc_counter{};  // use for reserve
        // desc counter

        /// Unlike export names, import names are not necessarily unique. It is possible to import the same module/name pair multiple times; such
        /// imports may even have different type descriptions, including different kinds of entities. A module with such imports can still be
        /// instantiated depending on the specifics of how an embedder allows resolving and supplying imports. However, embedders are not required
        /// to support such overloading, and a WebAssembly module itself cannot implement an overloaded name
        constexpr bool curr_wasm_check_duplicate_imports{::uwvm2::parser::wasm::standard::wasm1::features::check_duplicate_imports<Fs...>()};
        ::std::conditional_t<curr_wasm_check_duplicate_imports,
                             ::uwvm2::utils::container::unordered_flat_set<::uwvm2::parser::wasm::standard::wasm1::features::name_checker>,
                             ::uwvm2::parser::wasm::concepts::empty_t>
            duplicate_name_checker{};  // use for check duplicate name

        if constexpr(curr_wasm_check_duplicate_imports) { duplicate_name_checker.reserve(import_count); }

        // Each type is of unknown size, so it cannot be reserved.

        while(section_curr != section_end) [[likely]]
        {
            // get final utf8-checker
            using curr_final_import_export_text_format_wapper = ::uwvm2::parser::wasm::standard::wasm1::features::final_text_format_wapper<Fs...>;
#if 0
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::can_check_import_export_text_format<Fs...>);
#endif
            // Ensuring the existence of valid information

            // [...  module_namelen] ... module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [       safe        ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check import counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(import_counter, section_curr, err) > import_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = import_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage fit (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...> fit{};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 module_namelen;  // No initialization necessary
            auto const [module_namelen_next, module_namelen_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(module_namelen))};

            if(module_namelen_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_module_name_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(module_namelen_err);
            }

            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //       ^^ section_curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(module_namelen > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(module_namelen);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            // wasm1.0 standard permits empty module name; no rejection here

            constexpr bool disable_zero_length_string{::uwvm2::parser::wasm::standard::wasm1::features::disable_zero_length_string<Fs...>()};
            if constexpr(disable_zero_length_string)
            {
                if(module_namelen == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u)) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_length_cannot_be_zero;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            auto const import_module_name_too_length_ptr{section_curr};
            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //       ^^ import_module_name_too_length_ptr

            section_curr = reinterpret_cast<::std::byte const*>(module_namelen_next);
            // Note that section_curr may be equal to section_end

            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //                           ^^ section_curr

            // check modulenamelen
            if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(module_namelen)) [[unlikely]]
            {
                err.err_curr = import_module_name_too_length_ptr;
                err.err_selectable.u32 = module_namelen;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_too_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ...] extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe                          ] unsafe (could be the section_end)
            //                          ^^ section_curr

            // No access, security
            // Storing Temporary Variables into Modules
            fit.module_name = ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), module_namelen};

            // For platforms with CHAR_BIT greater than 8, the view here does not need to do any non-zero checking of non-low 8 bits within a single byte,
            // because a standards-compliant UTF-8 decoder must only care about the low 8 bits of each byte when verifying or decoding a byte sequence.

            // check utf8
            check_import_export_text_format(curr_final_import_export_text_format_wapper{},
                                            reinterpret_cast<::std::byte const*>(fit.module_name.cbegin()),
                                            reinterpret_cast<::std::byte const*>(fit.module_name.cend()),
                                            err);

            section_curr += module_namelen;  // safe

            // [...  module_namelen ... module_name ...] extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe                          ] unsafe (could be the section_end)
            //                                           ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent parse_by_scan

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 extern_namelen;  // No initialization necessary
            auto const [extern_namelen_next, extern_namelen_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(extern_namelen))};

            if(extern_namelen_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_extern_name_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(extern_namelen_err);
            }

            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                          ^^ section_curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(extern_namelen > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(extern_namelen);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            // wasm1.0 standard permits empty extern name; no rejection here
            if constexpr(disable_zero_length_string)
            {
                if(extern_namelen == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u)) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_length_cannot_be_zero;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            auto const import_extern_name_too_length_ptr{section_curr};
            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                          ^^ import_extern_name_too_length_ptr

            section_curr = reinterpret_cast<::std::byte const*>(extern_namelen_next);
            // Note that section_curr may be equal to section_end

            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                                              ^^ section_curr

            // check externnamelen
            if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(extern_namelen)) [[unlikely]]
            {
                err.err_curr = import_extern_name_too_length_ptr;
                err.err_selectable.u32 = extern_namelen;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_too_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ...] import_type extern_func ...
            // [                                safe                                      ] unsafe (could be the section_end)
            //                                                             ^^ section_curr

            // Storing Temporary Variables into Modules
            fit.extern_name = ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), extern_namelen};

            // For platforms with CHAR_BIT greater than 8, the view here does not need to do any non-zero checking of non-low 8 bits within a single byte,
            // because a standards-compliant UTF-8 decoder must only care about the low 8 bits of each byte when verifying or decoding a byte sequence.

            // check utf8
            check_import_export_text_format(curr_final_import_export_text_format_wapper{},
                                            reinterpret_cast<::std::byte const*>(fit.extern_name.cbegin()),
                                            reinterpret_cast<::std::byte const*>(fit.extern_name.cend()),
                                            err);

            section_curr += extern_namelen;  // safe
            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ...] import_type extern_func ...
            // [                                safe                                      ] unsafe (could be the section_end)
            //                                                                              ^^ section_curr

            // Note that section_curr may be equal to section_end

            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_missing_import_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ... import_type] extern_func ...
            // [                                   safe                                               ] unsafe (could be the section_end)
            //                                                                             ^^ section_curr

            ::std::memcpy(::std::addressof(fit.imports.type), section_curr, sizeof(fit.imports.type));

            static_assert(sizeof(fit.imports.type) == 1uz);
            // Size equal to one does not need to do little-endian conversion

            // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
            fit.imports.type = static_cast<decltype(fit.imports.type)>(static_cast<::std::uint_least8_t>(fit.imports.type) & 0xFFu);
#endif

            // importdesc_count never > 256 (max=255+1), convert to unsigned
            if(static_cast<unsigned>(fit.imports.type) >= static_cast<unsigned>(importdesc_count)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u8 = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fit.imports.type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_importdesc_prefix;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // use for reserve
            // 0: func
            // 1: table
            // 2: mem
            // 3: global
            // 4: tag (wasm3)

            auto const fit_import_type{static_cast<::std::size_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fit.imports.type))};

            // counter
            ++importdesc_counter.index_unchecked(fit_import_type);

            // Unlike export names, import names are not necessarily unique. It is possible to import the same module/name pair multiple times;
            // such imports may even have different type descriptions, including different kinds of entities. A module with such imports can still
            // be instantiated depending on the specifics of how an embedder allows resolving and supplying imports. However, embedders are not
            // required to support such overloading, and a WebAssembly module itself cannot implement an overloaded name
            if constexpr(curr_wasm_check_duplicate_imports)
            {
                constexpr bool has_controllable_check_duplicate_imports{
                    ::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_check_duplicate_imports_from_paras_c<Fs...>};
                if constexpr(has_controllable_check_duplicate_imports)
                {
                    // Provides a version that can be controlled by features parameters (fs_para).
                    if(get_feature_parameter_controllable_check_duplicate_imports_from_paras(fs_para))
                    {
                        if(!duplicate_name_checker.emplace(fit.module_name, fit.extern_name).second) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.duplic_imports.module_name = fit.module_name;
                            err.err_selectable.duplic_imports.extern_name = fit.extern_name;
                            err.err_selectable.duplic_imports.type = static_cast<::std::uint_least8_t>(fit_import_type);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_imports_of_the_same_import_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }
                }
                else
                {
                    if(!duplicate_name_checker.emplace(fit.module_name, fit.extern_name).second) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.duplic_imports.module_name = fit.module_name;
                        err.err_selectable.duplic_imports.extern_name = fit.extern_name;
                        err.err_selectable.duplic_imports.type = static_cast<::std::uint_least8_t>(fit_import_type);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_imports_of_the_same_import_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }
            }

            ++section_curr;

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ... import_type] extern_func ...
            // [                                   safe                                               ] unsafe (could be the section_end)
            //                                                                                          ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent define_extern_prefix_imports_handler

            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_prefix_imports_handler<Fs...>,
                          "define_extern_prefix_imports_handler(...) not found");
            // handle it, fit.imports.type is always valid
            section_curr = define_extern_prefix_imports_handler(sec_adl, fit.imports, module_storage, section_curr, section_end, err, fs_para);

            importsec.imports.push_back_unchecked(::std::move(fit));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check import counter match
        if(import_counter != import_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = import_counter;
            err.err_selectable.u32arr[1] = import_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // reserve importsec_importdesc_curr, so can unchecked push_back
        auto const importsec_importdesc_begin{importsec.importdesc.begin()};
        auto importsec_importdesc_curr{importsec_importdesc_begin};
        for(auto importdesc_curr{importdesc_counter.begin()}; importdesc_curr != importdesc_counter.end(); ++importdesc_curr)
        {
            importsec_importdesc_curr->reserve(static_cast<::std::size_t>(*importdesc_curr));
            ++importsec_importdesc_curr;
        }

        // Categorize each import type and then store the
        // Since importsec.imports will be no subsequent expansion, the address of the it will always be fixed
        for(auto const& imp_curr: importsec.imports)
        {
            auto const imp_curr_imports_type_wasm_byte{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(imp_curr.imports.type)};
            // imp_curr_imports_type_wasm_byte have been previously checked and never cross the line

            [[assume(static_cast<unsigned>(imp_curr_imports_type_wasm_byte) < static_cast<unsigned>(importdesc_count))]];

            importsec_importdesc_begin[imp_curr_imports_type_wasm_byte].push_back_unchecked(::std::addressof(imp_curr));
        }

        /// @deprecated The wasm standard states that the standard sections follow a sequence, which can never happen
#if 0
        // In the WebAssembly specification, the total number of internal functions defined in imported functions and function sections cannot exceed the
        // maximum value of u32 (4,294,967,295).

        static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_imported_and_defined_exceeding_checker<Fs...>);

        constexpr ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>>
            final_extern_type_adl{};

        define_imported_and_defined_exceeding_checker(sec_adl, final_extern_type_adl, module_storage, importsec_importdesc_begin, section_curr, err, fs_para);
#endif

        constexpr bool allow_multi_memory{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_memory<Fs...>()};
        if constexpr(!allow_multi_memory)
        {
            // When multiple memory allocations are not permitted, ensure no more than one is checked.
            constexpr auto mem_pos{
                static_cast<::std::size_t>(decltype(::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>{}.type)::memory)};
            static_assert(mem_pos < importdesc_count);

            if(importsec_importdesc_begin[mem_pos].size() > 1uz) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_memory;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        constexpr bool allow_multi_table{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<Fs...>()};
        if constexpr(!allow_multi_table)
        {
            // When multiple table allocations are not permitted, ensure no more than one is checked.
            constexpr auto table_pos{
                static_cast<::std::size_t>(decltype(::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>{}.type)::table)};
            static_assert(table_pos < importdesc_count);

            if(importsec_importdesc_begin[table_pos].size() > 1uz) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_table;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    /// @brief Wrapper for the import section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct import_section_storage_section_details_wrapper_t
    {
        import_section_storage_t<Fs...> const* import_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr import_section_storage_section_details_wrapper_t<Fs...> section_details(
        import_section_storage_t<Fs...> const& import_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(import_section_storage), ::std::addressof(all_sections)}; }

    /// @brief Print the import section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, import_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       import_section_storage_section_details_wrapper_t<Fs...> const import_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(import_section_details_wrapper.import_section_storage_ptr == nullptr || import_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
#endif

        auto const& import_section_span{import_section_details_wrapper.import_section_storage_ptr->sec_span};
        auto const import_section_size{static_cast<::std::size_t>(import_section_span.sec_end - import_section_span.sec_begin)};

        if(import_section_size)
        {
            auto const import_size{import_section_details_wrapper.import_section_storage_ptr->imports.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nImport[", import_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nImport[", import_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nImport[", import_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nImport[", import_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nImport[", import_size, U"]:\n");
            }

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 import_counter{};
            constexpr auto importdesc_count{::std::remove_cvref_t<decltype(*import_section_details_wrapper.import_section_storage_ptr)>::importdesc_count};
            ::uwvm2::utils::container::array<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, importdesc_count> importdesc_counter{};

            for(auto const& curr_import: import_section_details_wrapper.import_section_storage_ptr->imports)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<true>(
                        ::std::forward<Stm>(stream),
                        " - import[",
                        import_counter,
                        "] -> ",
                        section_details(curr_import, *import_section_details_wrapper.all_sections_ptr, importdesc_counter.begin()));
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(
                        ::std::forward<Stm>(stream),
                        L" - import[",
                        import_counter,
                        L"] -> ",
                        section_details(curr_import, *import_section_details_wrapper.all_sections_ptr, importdesc_counter.begin()));
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(
                        ::std::forward<Stm>(stream),
                        u8" - import[",
                        import_counter,
                        u8"] -> ",
                        section_details(curr_import, *import_section_details_wrapper.all_sections_ptr, importdesc_counter.begin()));
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(
                        ::std::forward<Stm>(stream),
                        u" - import[",
                        import_counter,
                        u"] -> ",
                        section_details(curr_import, *import_section_details_wrapper.all_sections_ptr, importdesc_counter.begin()));
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(
                        ::std::forward<Stm>(stream),
                        U" - import[",
                        import_counter,
                        U"] -> ",
                        section_details(curr_import, *import_section_details_wrapper.all_sections_ptr, importdesc_counter.begin()));
                }
                ++import_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
