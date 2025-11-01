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
 * @date        2025-07-04
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
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/container/impl.h>
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
# include "import_section.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct export_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Export"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::export_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_export_type<Fs...>> exports{};

        inline static constexpr ::std::size_t exportdesc_count{
            static_cast<::std::size_t>(decltype(::uwvm2::parser::wasm::standard::wasm1::features::final_export_type_t<Fs...>{}.type)::external_type_end) + 1uz};
        ::uwvm2::utils::container::
            array<::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_export_type<Fs...> const*>, exportdesc_count>
                exportdesc{};
    };

    /// @brief Define function for wasm1 external_types
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_handler_export_index(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<export_section_storage_t<Fs...>> sec_adl,
        decltype(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_export_type<Fs...>{}.storage)& fwet_exports_storage,  // [adl] can be replaced
        decltype(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_export_type<Fs...>{}.type) const fwet_exports_type,   // [adl] can be replaced
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // type is what has been fetched
        auto const type{fwet_exports_type};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[0]: func
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 3uz);  // Ensure that subsequent index visits do not cross boundaries

        // Since this content is all idx, it is treated uniformly

        // [...  export_namelen ... export_name ... export_type export_idx] ... next_export
        // [                            safe                              ] unsafe (could be the section_end)
        //                                                      ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 export_idx;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [export_idx_next, export_idx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(export_idx))};

        if(export_idx_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_export_idx;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(export_idx_err);
        }

        // [...  export_namelen ... export_name ... export_type export_idx ...] next_export
        // [                            safe                                  ] unsafe (could be the section_end)
        //                                                      ^^ section_curr

        // Check if index is greater than or equal to the type size
        switch(type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                fwet_exports_storage.func_idx = export_idx;

                // function section
                auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
                auto const defined_funcsec_funcs_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};
                auto const imported_func_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};
                // Addition does not overflow, Dependency Pre-Fill Pre-Check
                auto const all_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_funcsec_funcs_size + imported_func_size)};

                if(auto const index{fwet_exports_storage.func_idx}; index >= all_func_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exported_index_exceeds_maxvul.idx = index;
                    err.err_selectable.exported_index_exceeds_maxvul.maxval = all_func_size;
                    err.err_selectable.exported_index_exceeds_maxvul.type = 0x00;  // function
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exported_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table:
            {
                fwet_exports_storage.table_idx = export_idx;

                // table section
                auto const& tablesec{
                    ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
                auto const defined_tablesec_tables_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
                auto const imported_table_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size())};
                // Addition does not overflow, Dependency Pre-Fill Pre-Check
                auto const all_table_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_tablesec_tables_size + imported_table_size)};

                if(auto const index{fwet_exports_storage.table_idx}; index >= all_table_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exported_index_exceeds_maxvul.idx = index;
                    err.err_selectable.exported_index_exceeds_maxvul.maxval = all_table_size;
                    err.err_selectable.exported_index_exceeds_maxvul.type = 0x01;  // table
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exported_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory:
            {
                fwet_exports_storage.memory_idx = export_idx;

                // memory section
                auto const& memorysec{
                    ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section_storage_t<Fs...>>(module_storage.sections)};
                auto const defined_memorysec_memories_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memorysec.memories.size())};
                auto const imported_memory_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(2uz).size())};
                // Addition does not overflow, Dependency Pre-Fill Pre-Check
                auto const all_memory_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_memorysec_memories_size + imported_memory_size)};

                if(auto const index{fwet_exports_storage.memory_idx}; index >= all_memory_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exported_index_exceeds_maxvul.idx = index;
                    err.err_selectable.exported_index_exceeds_maxvul.maxval = all_memory_size;
                    err.err_selectable.exported_index_exceeds_maxvul.type = 0x02;  // memory
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exported_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::global:
            {
                fwet_exports_storage.global_idx = export_idx;

                // global section
                auto const& globalsec{
                    ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t<Fs...>>(module_storage.sections)};
                auto const defined_globalsec_globals_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(globalsec.local_globals.size())};
                auto const imported_global_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(3uz).size())};
                // Addition does not overflow, Dependency Pre-Fill Pre-Check
                auto const all_global_size{
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_globalsec_globals_size + imported_global_size)};

                if(auto const index{fwet_exports_storage.global_idx}; index >= all_global_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exported_index_exceeds_maxvul.idx = index;
                    err.err_selectable.exported_index_exceeds_maxvul.maxval = all_global_size;
                    err.err_selectable.exported_index_exceeds_maxvul.type = 0x03;  // global
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exported_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();  // never match, checked before
            }
        }

        section_curr = reinterpret_cast<::std::byte const*>(export_idx_next);

        // [...  export_namelen ... export_name ... export_type export_idx ...] next_export
        // [                            safe                                  ] unsafe (could be the section_end)
        //                                                                      ^^ section_curr

        return section_curr;
    }

    /// @brief Define the handler function for export_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<export_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse export section (id: 7)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get export_section_storage_t from storages
        auto& exportsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<export_section_storage_t<Fs...>>(module_storage.sections)};

        // check duplicate
        if(exportsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = exportsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // There is no need to check typesec.sec_span.sec_begin (forward_dependency_missing) here,
        // it can be checked later in handle_export_func with index
        // Turning on checking can instead cause non-typesection export dependencies to error out

        // export section
        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        exportsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        exportsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};
        // [before_section ... ] | export_count ... export_namelen ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 export_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [export_count_next, export_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(export_count))};

        if(export_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_export_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(export_count_err);
        }

        // [before_section ... | export_count ...] export_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(export_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(export_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(export_count) > parser_limit.max_export_sec_exports) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"exportsec_exports";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(export_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_export_sec_exports;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        exportsec.exports.reserve(static_cast<::std::size_t>(export_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 export_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(export_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | export_count ...] export_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        constexpr ::std::size_t exportdesc_count{exportsec.exportdesc_count};
        ::uwvm2::utils::container::array<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, exportdesc_count> exportdesc_counter{};  // use for reserve
        // desc counter

        // Each export is labeled by a unique name. Exportable definitions are functions, tables, memories, and globals, which are referenced through a
        // respective descriptor.
        // @see: WebAssembly Release 1.0 (2019-07-20) § 2.5.10
        ::uwvm2::utils::container::unordered_flat_set<::uwvm2::utils::container::u8string_view> duplicate_name_checker{};  // use for check duplicate name
        duplicate_name_checker.reserve(export_count);

        // Each type is of unknown size, so it cannot be reserved.

        while(section_curr != section_end) [[likely]]
        {
            // get final utf8-checker
            using curr_final_import_export_text_format_wapper = ::uwvm2::parser::wasm::standard::wasm1::features::final_text_format_wapper<Fs...>;
#if 0
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::can_check_import_export_text_format<Fs...>);
#endif
            // Ensuring the existence of valid information

            // [...  export_namelen] ... export_namelen ... export_type export_idx ...
            // [       safe        ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check export counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(export_counter, section_curr, err) > export_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = export_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage fwet (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_export_type<Fs...> fwet{};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 export_namelen;  // No initialization necessary
            auto const [export_namelen_next, export_namelen_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(export_namelen))};

            if(export_namelen_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_export_name_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(export_namelen_err);
            }

            // [...  export_namelen ...] export_name ... export_type export_idx ...
            // [       safe            ] unsafe (could be the section_end)
            //       ^^ section_curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(export_namelen > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(export_namelen);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            // wasm1.0 standard permits empty export name; no rejection here

            constexpr bool disable_zero_length_string{::uwvm2::parser::wasm::standard::wasm1::features::disable_zero_length_string<Fs...>()};
            if constexpr(disable_zero_length_string)
            {
                if(export_namelen == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u)) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_name_length_cannot_be_zero;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            auto const export_module_name_too_length_ptr{section_curr};
            // [...  export_namelen ...] export_name ... export_type export_idx ...
            // [       safe            ] unsafe (could be the section_end)
            //       ^^ export_module_name_too_length_ptr

            section_curr = reinterpret_cast<::std::byte const*>(export_namelen_next);
            // Note that section_curr may be equal to section_end

            // [...  export_namelen ...] export_name ... export_type export_idx ...
            // [       safe            ] unsafe (could be the section_end)
            //                           ^^ section_curr

            // check modulenamelen
            if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(export_namelen)) [[unlikely]]
            {
                err.err_curr = export_module_name_too_length_ptr;
                err.err_selectable.u32 = export_namelen;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_name_too_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  export_namelen ... export_name ...] export_type export_idx ...
            // [                  safe                 ] unsafe (could be the section_end)
            //                          ^^ section_curr

            // No access, security
            fwet.export_name = ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), export_namelen};

            // For platforms with CHAR_BIT greater than 8, the view here does not need to do any non-zero checking of non-low 8 bits within a single byte,
            // because a standards-compliant UTF-8 decoder must only care about the low 8 bits of each byte when verifying or decoding a byte sequence.

            // check utf8
            check_import_export_text_format(curr_final_import_export_text_format_wapper{},
                                            reinterpret_cast<::std::byte const*>(fwet.export_name.cbegin()),
                                            reinterpret_cast<::std::byte const*>(fwet.export_name.cend()),
                                            err);

            section_curr += export_namelen;  // safe

            // [...  export_namelen ... export_name ...] export_type export_idx ...
            // [                  safe                 ] unsafe (could be the section_end)
            //                                           ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent parse_by_scan

            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_missing_export_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  export_namelen ... export_name ... export_type] export_idx ...
            // [                            safe                   ] unsafe (could be the section_end)
            //                                          ^^ section_curr

            ::std::memcpy(::std::addressof(fwet.exports.type), section_curr, sizeof(fwet.exports.type));

            static_assert(sizeof(fwet.exports.type) == 1uz);
            // Size equal to one does not need to do little-endian conversion

            // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
            fwet.exports.type = static_cast<decltype(fwet.exports.type)>(static_cast<::std::uint_least8_t>(fwet.exports.type) & 0xFFu);
#endif

            // exportdesc_count never > 256 (max=255+1), convert to unsigned
            if(static_cast<unsigned>(fwet.exports.type) >= static_cast<unsigned>(exportdesc_count)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u8 = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fwet.exports.type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_exportdesc_prefix;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // use for reserve
            // 0: func
            // 1: table
            // 2: mem
            // 3: global
            // 4: tag (wasm3)

            auto const fwet_export_type{static_cast<::std::size_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fwet.exports.type))};

            // counter
            ++exportdesc_counter.index_unchecked(fwet_export_type);

            // check duplicate name
            // Each export is labeled by a unique name. Exportable definitions are functions, tables, memories, and globals, which are referenced through a
            // respective descriptor.
            // @see: WebAssembly Release 1.0 (2019-07-20) § 2.5.10
            // Use the return value of emplace to combine the lookup and insertion operations, avoiding two hash lookups
            if(!duplicate_name_checker.emplace(fwet.export_name).second) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.duplic_exports.export_name = fwet.export_name;
                err.err_selectable.duplic_exports.type = static_cast<::std::uint_least8_t>(fwet_export_type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_exports_of_the_same_export_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            ++section_curr;

            // [...  export_namelen ... export_name ... export_type] export_idx ... next_export
            // [                            safe                   ] unsafe (could be the section_end)
            //                                                       ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent define_extern_prefix_exports_handler

            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_missing_export_idx;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  export_namelen ... export_name ... export_type export_idx] ... next_export
            // [                            safe                              ] unsafe (could be the section_end)
            //                                                      ^^ section_curr

            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_handle_export_index<Fs...>);

            section_curr =
                define_handler_export_index(sec_adl, fwet.exports.storage, fwet.exports.type, module_storage, section_curr, section_end, err, fs_para);

            // [...  export_namelen ... export_name ... export_type export_idx ...] next_export
            // [                            safe                                  ] unsafe (could be the section_end)
            //                                                                      ^^ section_curr

            exportsec.exports.push_back_unchecked(::std::move(fwet));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check export counter match
        if(export_counter != export_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = export_counter;
            err.err_selectable.u32arr[1] = export_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::export_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // reserve exportsec_exportdesc_curr, so can unchecked push_back
        auto const exportsec_exportdesc_begin{exportsec.exportdesc.begin()};
        auto exportsec_exportdesc_curr{exportsec_exportdesc_begin};
        for(auto exportdesc_curr{exportdesc_counter.begin()}; exportdesc_curr != exportdesc_counter.end(); ++exportdesc_curr)
        {
            exportsec_exportdesc_curr->reserve(*exportdesc_curr);
            ++exportsec_exportdesc_curr;
        }

        // Categorize each export type and then store the
        // Since exportsec.exports will be no subsequent expansion, the address of the it will always be fixed
        for(auto const& export_curr: exportsec.exports)
        {
            auto const export_curr_exports_type_wasm_byte{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(export_curr.exports.type)};
            // export_curr_exports_type_wasm_byte have been previously checked and never cross the line

            [[assume(static_cast<unsigned>(export_curr_exports_type_wasm_byte) < static_cast<unsigned>(exportdesc_count))]];

            exportsec_exportdesc_begin[export_curr_exports_type_wasm_byte].push_back_unchecked(::std::addressof(export_curr));
        }
    }

    /// @brief Wrapper for the export section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct export_section_storage_section_details_wrapper_t
    {
        export_section_storage_t<Fs...> const* export_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr export_section_storage_section_details_wrapper_t<Fs...> section_details(
        export_section_storage_t<Fs...> const& export_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    {
        return {::std::addressof(export_section_storage), ::std::addressof(all_sections)};
    }

    /// @brief Print the export section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, export_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       export_section_storage_section_details_wrapper_t<Fs...> const export_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(export_section_details_wrapper.export_section_storage_ptr == nullptr || export_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const export_section_span{export_section_details_wrapper.export_section_storage_ptr->sec_span};
        auto const export_section_size{static_cast<::std::size_t>(export_section_span.sec_end - export_section_span.sec_begin)};

        if(export_section_size)
        {
            auto const export_size{export_section_details_wrapper.export_section_storage_ptr->exports.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nExport[", export_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nExport[", export_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nExport[", export_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nExport[", export_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nExport[", export_size, U"]:\n");
            }

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 export_counter{};

            for(auto const& curr_export: export_section_details_wrapper.export_section_storage_ptr->exports)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    " - export[",
                                                                    export_counter,
                                                                    "] -> ",
                                                                    section_details(curr_export, *export_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    L" - export[",
                                                                    export_counter,
                                                                    L"] -> ",
                                                                    section_details(curr_export, *export_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    u8" - export[",
                                                                    export_counter,
                                                                    u8"] -> ",
                                                                    section_details(curr_export, *export_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    u" - export[",
                                                                    export_counter,
                                                                    u"] -> ",
                                                                    section_details(curr_export, *export_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    U" - export[",
                                                                    export_counter,
                                                                    U"] -> ",
                                                                    section_details(curr_export, *export_section_details_wrapper.all_sections_ptr));
                }
                ++export_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
