/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Element segment flags 0 through 7
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <climits>
# include <concepts>
# include <limits>
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
# include "feature_def.h"
# include "data_section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    namespace wasm1p1_element_details
    {
        /// @brief Cached function and table counts needed while validating element segments.
        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        struct module_counts_t
        {
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_func_size{};
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 imported_table_size{};
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_table_size{};
        };

        /// @brief Collect imported and defined function/table counts for element index checks.
        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr module_counts_t<Fs...>
            get_module_counts(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage) noexcept
        {
            auto const& importsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
            constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
            static_assert(importdesc_count > 1uz);

            auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
            // importdesc has at least two buckets by static_assert; bucket 0 is functions and bucket 1 is tables.
            auto const imported_func_size{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};
            auto const defined_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};

            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
            auto const imported_table_size{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size())};
            auto const defined_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};

            return {static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_func_size + defined_func_size),
                    imported_table_size,
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table_size + defined_table_size)};
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type
            get_table_reftype(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                              ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_idx) noexcept
        {
            auto const& importsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
            // get_module_counts/check_active_table verify the table index before this helper is called; bucket 1 stores table imports.
            auto const& imported_table{importsec.importdesc.index_unchecked(1uz)};
            auto const imported_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_table.size())};
            if(table_idx < imported_table_size)
            {
                // table_idx < imported_table_size proves the imported table unchecked access is in bounds.
                auto const curr_imported_table_ptr{imported_table.index_unchecked(table_idx)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(curr_imported_table_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                return curr_imported_table_ptr->imports.storage.table.reftype;
            }

            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
            // check_active_table has already proven table_idx < imported_table_size + tablesec.tables.size(), so the adjusted defined-table index is in bounds.
            return tablesec.tables.index_unchecked(static_cast<::std::size_t>(table_idx - imported_table_size)).reftype;
        }

        inline constexpr ::std::byte const* parse_elemkind_funcref(::std::byte const* section_curr,
                                                                   ::std::byte const* const section_end,
                                                                   ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
        {
            // [before_elemkind ...] elemkind ... tail ... (section_end)
            // [       safe       ] unsafe (could be the section_end)
            //                     ^^ section_curr
            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_kind;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [before_elemkind ...] elemkind ... tail ... (section_end)
            // [       safe       ] [safe] unsafe
            //                     ^^ section_curr
            //
            // section_curr != section_end proves that the one-byte elemkind read is in bounds.
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte elemkind;
            ::std::memcpy(::std::addressof(elemkind), section_curr, sizeof(elemkind));
#if CHAR_BIT > 8
            elemkind = static_cast<decltype(elemkind)>(static_cast<::std::uint_least8_t>(elemkind) & 0xFFu);
#endif

            if(elemkind != 0u) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u8 = elemkind;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_element_kind_byte;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // section_curr points at the one-byte elemkind already proven safe by the equality check above.
            // Pointer move: advance to the first byte after elemkind.
            ++section_curr;

            // [before_elemkind ... elemkind] tail ... (section_end)
            // [          safe             ] unsafe (could be the section_end)
            //                               ^^ section_curr
            return section_curr;
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const* parse_reftype(::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type& reftype,
                                                          ::std::byte const* section_curr,
                                                          ::std::byte const* const section_end,
                                                          ::uwvm2::parser::wasm::base::error_impl& err,
                                                          ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            // [before_reftype ...] reftype ... tail ... (section_end)
            // [       safe      ] unsafe (could be the section_end)
            //                    ^^ section_curr
            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_kind;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [before_reftype ...] reftype ... tail ... (section_end)
            // [       safe      ] [safe] unsafe
            //                    ^^ section_curr
            //
            // section_curr != section_end proves that the one-byte reference type read is in bounds.
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte raw_ref;
            ::std::memcpy(::std::addressof(raw_ref), section_curr, sizeof(raw_ref));
#if CHAR_BIT > 8
            raw_ref = static_cast<decltype(raw_ref)>(static_cast<::std::uint_least8_t>(raw_ref) & 0xFFu);
#endif

            auto const parsed{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(raw_ref)};
            auto const parsed_value{::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(parsed)};
            if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_reference_type(parsed_value) ||
               !::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(parsed, fs_para)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.wasm1p1_reference_type.value = raw_ref;
                err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            reftype = parsed;
            // section_curr points at the one-byte reference type already proven safe by the validation above.
            // Pointer move: advance to the first byte after reftype.
            ++section_curr;

            // [before_reftype ... reftype] tail ... (section_end)
            // [          safe           ] unsafe (could be the section_end)
            //                             ^^ section_curr
            return section_curr;
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_active_table(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...> const& element_storage,
                                                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                                 module_counts_t<Fs...> const counts,
                                                 ::std::byte const* const err_curr,
                                                 ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
        {
            if(element_storage.table_idx >= counts.all_table_size) [[unlikely]]
            {
                err.err_curr = err_curr;
                err.err_selectable.elem_table_index_exceeds_maxvul.idx = element_storage.table_idx;
                err.err_selectable.elem_table_index_exceeds_maxvul.maxval = counts.all_table_size;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_table_index_exceeds_maxvul;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // element_storage.table_idx is now proven to be within the imported+defined table range used by get_table_reftype.
            auto const table_reftype{get_table_reftype(module_storage, element_storage.table_idx)};
            if(table_reftype != element_storage.reftype) [[unlikely]]
            {
                err.err_curr = err_curr;
                err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx = element_storage.table_idx;
                err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element_storage.reftype));
                err.err_selectable.wasm1p1_element_table_type_mismatch.table_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(table_reftype));
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_element_table_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        inline constexpr void check_elem_vector_size_t(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count,
                                                       ::std::byte const* const section_curr,
                                                       ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
        {
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                if(count > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(count);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_elem_funcidx_vector_limit(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count,
                                                              ::std::byte const* const section_curr,
                                                              ::uwvm2::parser::wasm::base::error_impl& err,
                                                              ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            check_elem_vector_size_t(count, section_curr, err);
            if constexpr((::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::wasm1, Fs> || ...))
            {
                auto const& wasm1_feapara_r{
                    ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(fs_para)};
                auto const& parser_limit{wasm1_feapara_r.parser_limit};
                if(static_cast<::std::size_t>(count) > parser_limit.max_elem_sec_funcidx) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exceed_the_max_parser_limit.name = u8"elemsec_funcidx";
                    err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(count);
                    err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_elem_sec_funcidx;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_elem_expr_vector_limit(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count,
                                                           ::std::byte const* const section_curr,
                                                           ::uwvm2::parser::wasm::base::error_impl& err,
                                                           ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            check_elem_vector_size_t(count, section_curr, err);
            auto const& wasm1p1_feapara_r{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
            auto const& parser_limit{wasm1p1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(count) > parser_limit.max_elem_sec_expr) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"elemsec_expr";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_elem_sec_expr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const*
            parse_funcidx_vector(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...>& element_storage,
                                 module_counts_t<Fs...> const counts,
                                 ::std::byte const* section_curr,
                                 ::std::byte const* const section_end,
                                 ::uwvm2::parser::wasm::base::error_impl& err,
                                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            // [before_funcidx_vec ...] funcidx_count ... funcidx0 ... tail ... (section_end)
            // [          safe       ] unsafe (could be the section_end)
            //                        ^^ section_curr
            //
            // parse_by_scan bounds-checks the vector length LEB128 against section_end.
            wasm_u32 funcidx_count;
            auto const [funcidx_count_next, funcidx_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                        ::fast_io::mnp::leb128_get(funcidx_count))};
            if(funcidx_count_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_funcidx_count;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(funcidx_count_err);
            }

            check_elem_funcidx_vector_limit(funcidx_count, section_curr, err, fs_para);
            element_storage.vec_funcidx.reserve(static_cast<::std::size_t>(funcidx_count));
            // parse_by_scan succeeded, so [section_curr, funcidx_count_next) is now proven safe and funcidx_count_next is inside [section_curr, section_end].
            // Proof view before moving section_curr: section_curr still points at the funcidx count, and funcidx_count_next marks the first vector element.
            // Pointer move: advance section_curr to the first funcidx byte, or to the tail when the vector is empty.
            section_curr = reinterpret_cast<::std::byte const*>(funcidx_count_next);

            // [before_funcidx_vec ... funcidx_count ...] funcidx0 ... tail ... (section_end)
            // [                 safe                  ] unsafe (could be the section_end)
            //                                          ^^ section_curr
            //
            // reserve(funcidx_count) makes the following push_back_unchecked calls capacity-safe.
            for(::std::size_t funcidx_counter{}; funcidx_counter != funcidx_count; ++funcidx_counter)
            {
                // [before_funcidx_vec ... previous_funcidx ...] funcidx ... tail ... (section_end)
                // [                   safe                    ] unsafe (could be the section_end)
                //                                             ^^ section_curr
                //
                // parse_by_scan bounds-checks each LEB128 funcidx before the unchecked vector write.
                wasm_u32 funcidx;
                auto const [funcidx_next, funcidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                ::fast_io::mnp::leb128_get(funcidx))};
                if(funcidx_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_funcidx;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(funcidx_err);
                }

                if(funcidx >= counts.all_func_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.elem_func_index_exceeds_maxvul.idx = funcidx;
                    err.err_selectable.elem_func_index_exceeds_maxvul.maxval = counts.all_func_size;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_func_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // parse_by_scan succeeded, so [section_curr, funcidx_next) is now proven safe and funcidx_next is inside [section_curr, section_end].
                // Proof view before moving section_curr: section_curr still points at this funcidx LEB128, and funcidx_next marks the next element or tail.
                element_storage.vec_funcidx.push_back_unchecked(funcidx);
                // Pointer move: advance section_curr to the next funcidx byte, or to the tail after the last checked funcidx.
                section_curr = reinterpret_cast<::std::byte const*>(funcidx_next);

                // [before_funcidx_vec ... funcidx ...] next_funcidx_or_tail ... (section_end)
                // [              safe                ] unsafe (could be the section_end)
                //                                      ^^ section_curr
            }

            return section_curr;
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const*
            parse_and_check_ref_const_expr_valid(::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& expr,
                                                 ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type expected_reftype,
                                                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                                 module_counts_t<Fs...> const counts,
                                                 ::std::byte const* section_curr,
                                                 ::std::byte const* const section_end,
                                                 ::uwvm2::parser::wasm::base::error_impl& err,
                                                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            auto const& importsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
            // Bucket 3 stores imported globals; wasm1 importdesc layout provides it for the feature set used here.
            auto const& imported_global{importsec.importdesc.index_unchecked(3uz)};
            auto const imported_global_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global.size())};
            auto const expected_value_type{::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(expected_reftype)};
            auto const expected_value_type_byte{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(expected_value_type)};

            expr.begin = section_curr;
            bool has_data_on_type_stack{};
            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            for(;;)
            {
                // [before_expr ...] opcode ... expr_tail ... end ... (section_end)
                // [      safe     ] unsafe (could be the section_end)
                //                  ^^ section_curr
                if(section_curr == section_end) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_terminator_not_found;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // [before_expr ...] opcode ... expr_tail ... end ... (section_end)
                // [      safe     ] [safe] unsafe
                //                  ^^ section_curr
                //
                // section_curr != section_end proves that reading the one-byte opcode is in bounds.
                ::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type opcode;
                ::std::memcpy(::std::addressof(opcode), section_curr, sizeof(opcode));
#if CHAR_BIT > 8
                opcode &= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFFu);
#endif

                if(opcode ==
                   static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::end))
                {
                    // section_curr points at the one-byte end opcode already proven safe by the opcode read.
                    // Pointer move: advance to the first byte after the checked terminator.
                    ++section_curr;
                    break;
                }

                if(has_data_on_type_stack) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                has_data_on_type_stack = true;

                switch(opcode)
                {
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD0u):
                    {
                        // section_curr points at the one-byte ref.null opcode already proven safe by the opcode read.
                        // Pointer move: advance to the reference-type immediate.
                        ++section_curr;

                        // [before_expr ... ref.null] reftype ... expr_tail ... end ... (section_end)
                        // [          safe          ] unsafe (could be the section_end)
                        //                           ^^ section_curr
                        if(section_curr == section_end) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [before_expr ... ref.null reftype] ... expr_tail ... end ... (section_end)
                        // [          safe                  ] unsafe
                        //                           ^^ section_curr
                        // section_curr != section_end proves that the one-byte reference type read is in bounds.
                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte raw_ref;
                        ::std::memcpy(::std::addressof(raw_ref), section_curr, sizeof(raw_ref));
#if CHAR_BIT > 8
                        raw_ref = static_cast<decltype(raw_ref)>(static_cast<::std::uint_least8_t>(raw_ref) & 0xFFu);
#endif
                        auto const ref_type{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(raw_ref)};
                        auto const ref_value_type{::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(ref_type)};
                        if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_reference_type(ref_value_type) ||
                           !::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(ref_type, fs_para)) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.wasm1p1_reference_type.value = raw_ref;
                            err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(ref_type != expected_reftype) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.wasm1p1_reference_type_mismatch.expected = expected_value_type_byte;
                            err.err_selectable.wasm1p1_reference_type_mismatch.actual =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(ref_value_type);
                            err.err_selectable.wasm1p1_reference_type_mismatch.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_reference_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // section_curr points at the one-byte reference type already proven safe by section_curr != section_end and validated above.
                        // Pointer move: advance to the next expression byte.
                        ++section_curr;

                        // [before_expr ... ref.null reftype] expr_tail ... end ... (section_end)
                        // [              safe              ] unsafe (could be the section_end)
                        //                                    ^^ section_curr
                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD2u):
                    {
                        if(expected_reftype != ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.wasm1p1_reference_type_mismatch.expected = expected_value_type_byte;
                            err.err_selectable.wasm1p1_reference_type_mismatch.actual = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref);
                            err.err_selectable.wasm1p1_reference_type_mismatch.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_reference_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // section_curr points at the one-byte ref.func opcode already proven safe by the opcode read.
                        // Pointer move: advance to the funcidx LEB128 immediate.
                        ++section_curr;

                        // [before_expr ... ref.func] funcidx ... expr_tail ... end ... (section_end)
                        // [         safe           ] unsafe (could be the section_end)
                        //                            ^^ section_curr
                        //
                        // parse_by_scan bounds-checks the LEB128 funcidx before index validation.
                        wasm_u32 func_idx;
                        auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                         reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                         ::fast_io::mnp::leb128_get(func_idx))};
                        if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                        }

                        // [before_expr ... ref.func funcidx ...] expr_tail ... end ... (section_end)
                        // [                safe                ] unsafe (could be the section_end)
                        //                           ^^ section_curr

                        if(func_idx >= counts.all_func_size) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.elem_func_index_exceeds_maxvul.idx = func_idx;
                            err.err_selectable.elem_func_index_exceeds_maxvul.maxval = counts.all_func_size;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_func_index_exceeds_maxvul;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                        // Proof view before moving section_curr: section_curr still points at the funcidx LEB128, and next marks the next expression byte.
                        // Pointer move: advance section_curr to the next expression byte after the checked funcidx.
                        section_curr = reinterpret_cast<::std::byte const*>(next);

                        // [before_expr ... ref.func funcidx ...] expr_tail ... end ... (section_end)
                        // [                safe                ] unsafe (could be the section_end)
                        //                                      ^^ section_curr
                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                    {
                        // section_curr points at the one-byte global.get opcode already proven safe by the opcode read.
                        // Pointer move: advance to the globalidx LEB128 immediate.
                        ++section_curr;

                        // [before_expr ... global.get] globalidx ... expr_tail ... end ... (section_end)
                        // [          safe            ] unsafe (could be the section_end)
                        //                             ^^ section_curr

                        // parse_by_scan bounds-checks the LEB128 global index before imported-global lookup.
                        wasm_u32 global_idx;
                        auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                         reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                         ::fast_io::mnp::leb128_get(global_idx))};
                        if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                        }

                        // [before_expr ... global.get globalidx ...] expr_tail ... end ... (section_end)
                        // [                  safe                  ] unsafe (could be the section_end)
                        //                             ^^ section_curr

                        if(global_idx >= imported_global_size) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32arr[0] = imported_global_size;
                            err.err_selectable.u32arr[1] = global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_illegal_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // global_idx was checked against imported_global_size above, so this unchecked access is in bounds.
                        auto const curr_imported_global_ptr{imported_global.index_unchecked(global_idx)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(curr_imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                        auto const& curr_imported_global{curr_imported_global_ptr->imports.storage.global};
                        auto const imported_global_type_byte{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_imported_global.type)};
                        if(imported_global_type_byte != expected_value_type_byte) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.wasm1p1_reference_type_mismatch.expected = expected_value_type_byte;
                            err.err_selectable.wasm1p1_reference_type_mismatch.actual = imported_global_type_byte;
                            err.err_selectable.wasm1p1_reference_type_mismatch.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_reference_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                        if(curr_imported_global.is_mutable) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_mutable_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                        // Proof view before moving section_curr: section_curr still points at the globalidx LEB128, and next marks the next expression byte.
                        // Pointer move: advance section_curr to the next expression byte after the checked globalidx.
                        section_curr = reinterpret_cast<::std::byte const*>(next);

                        // [before_expr ... global.get globalidx ...] expr_tail ... end ... (section_end)
                        // [                  safe                  ] unsafe (could be the section_end)
                        //                                           ^^ section_curr
                        expr.opcodes.reserve(1uz);
                        expr.opcodes.emplace_back_unchecked(
                            ::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.imported_global_idx = global_idx},
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get);
                        break;
                    }
                    [[unlikely]] default:
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }
            }

            if(!has_data_on_type_stack) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_empty;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [before_expr ... expr ... end] tail ... (section_end)
            // [            safe           ] unsafe (could be the section_end)
            //                              ^^ section_curr
            expr.end = section_curr;
            return section_curr;
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const*
            parse_expr_vector(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_storage_t<Fs...>& element_storage,
                              ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                              module_counts_t<Fs...> const counts,
                              ::std::byte const* section_curr,
                              ::std::byte const* const section_end,
                              ::uwvm2::parser::wasm::base::error_impl& err,
                              ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
        {
            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            // [before_expr_vec ...] expr_count ... expr0 ... tail ... (section_end)
            // [        safe      ] unsafe (could be the section_end)
            //                      ^^ section_curr
            //
            // parse_by_scan bounds-checks the vector length LEB128 against section_end.
            wasm_u32 expr_count;
            auto const [expr_count_next, expr_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(expr_count))};
            if(expr_count_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_elem_expr_count;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(expr_count_err);
            }

            check_elem_expr_vector_limit(expr_count, section_curr, err, fs_para);
            element_storage.vec_expr.reserve(static_cast<::std::size_t>(expr_count));
            // parse_by_scan succeeded, so [section_curr, expr_count_next) is now proven safe and expr_count_next is inside [section_curr, section_end].
            // Proof view before moving section_curr: section_curr still points at the expression count, and expr_count_next marks the first expression.
            // Pointer move: advance section_curr to the first expression byte, or to the tail when the vector is empty.
            section_curr = reinterpret_cast<::std::byte const*>(expr_count_next);

            // [before_expr_vec ... expr_count ...] expr0 ... tail ... (section_end)
            // [              safe                ] unsafe (could be the section_end)
            //                                     ^^ section_curr
            //
            // reserve(expr_count) makes the following push_back_unchecked calls capacity-safe.
            for(::std::size_t expr_counter{}; expr_counter != expr_count; ++expr_counter)
            {
                // [before_expr_vec ... previous_expr ...] expr ... tail ... (section_end)
                // [                safe                 ] unsafe (could be the section_end)
                //                                       ^^ section_curr
                ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> expr{};
                section_curr =
                    parse_and_check_ref_const_expr_valid(expr, element_storage.reftype, module_storage, counts, section_curr, section_end, err, fs_para);
                // parse_and_check_ref_const_expr_valid returns the first byte after a fully checked constant expression.
                // The preceding reserve(expr_count) makes this unchecked append capacity-safe.
                element_storage.vec_expr.push_back_unchecked(::std::move(expr));

                // [before_expr_vec ... expr ...] next_expr_or_tail ... (section_end)
                // [          safe              ] unsafe (could be the section_end)
                //                                ^^ section_curr
            }

            return section_curr;
        }
    }  // namespace wasm1p1_element_details

    /// @brief Parse one wasm1.1 element segment according to its leading flag.
    /// @details Bulk-memory and reference-type dependent segment forms are gated by runtime feature flags before their payload is read.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_handler_element_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<element_section_storage_t<Fs...>> sec_adl,
        decltype(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>{}.storage)& fet_storage,
        decltype(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>{}.type) const fet_type,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto& element_storage{fet_storage.segment};
        auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        auto const counts{wasm1p1_element_details::get_module_counts(module_storage)};

        auto const require_bulk_memory = [&]() UWVM_THROWS
        {
            if(!para.enable_bulk_memory) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.wasm1p1_feature_required.value = static_cast<wasm_u32>(fet_type);
                err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory;
                err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        };

        auto const require_reference_types = [&]() UWVM_THROWS
        {
            if(!para.enable_reference_types) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.wasm1p1_feature_required.value = static_cast<wasm_u32>(fet_type);
                err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types;
                err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        };

        auto const parse_tableidx = [&]() UWVM_THROWS -> void
        {
            // [before_element_payload ...] tableidx ... expr ... elem_payload ... (section_end)
            // [           safe           ] unsafe (could be the section_end)
            //                              ^^ section_curr
            //
            // parse_by_scan bounds-checks the tableidx LEB128 against section_end.
            wasm_u32 table_idx;
            auto const [table_idx_next, table_idx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                ::fast_io::mnp::leb128_get(table_idx))};
            if(table_idx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_table_idx;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_idx_err);
            }

            // [before_element_payload ... tableidx ...] expr ... elem_payload ... (section_end)
            // [                  safe                 ] unsafe (could be the section_end)
            //                             ^^ section_curr

            // parse_by_scan succeeded, so [section_curr, table_idx_next) is now proven safe and table_idx_next is inside [section_curr, section_end].
            // Proof view before moving section_curr: section_curr still points at the tableidx LEB128, and table_idx_next marks the offset expression.
            element_storage.table_idx = table_idx;

            // Pointer move: advance section_curr to the active offset expression.
            section_curr = reinterpret_cast<::std::byte const*>(table_idx_next);

            // [before_element_payload ... tableidx ...] expr ... elem_payload ... (section_end)
            // [                  safe                 ] unsafe (could be the section_end)
            //                                          ^^ section_curr
        };

        auto const parse_active_offset = [&]() UWVM_THROWS -> void
        {
            element_storage.active = true;
            // [before_element_payload ...] offset_expr ... elem_payload ... (section_end)
            // [           safe          ] unsafe (could be the section_end)
            //                            ^^ section_curr
            // parse_and_check_i32_const_expr_valid checks the whole i32 constant expression against section_end and returns its end pointer.
            // Pointer move: replace section_curr with the helper result after checking the whole offset expression.
            section_curr = wasm1p1_data_details::parse_and_check_i32_const_expr_valid(element_storage.expr, module_storage, section_curr, section_end, err);

            // [before_element_payload ... offset_expr ...] elem_payload ... (section_end)
            // [                  safe                    ] unsafe (could be the section_end)
            //                                             ^^ section_curr
        };

        switch(fet_type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::active_implicit_funcidx:
            {
                element_storage.active = true;
                element_storage.table_idx = 0u;
                element_storage.reftype = reference_type::funcref;
                parse_active_offset();
                wasm1p1_element_details::check_active_table(element_storage, module_storage, counts, section_curr, err);
                // parse_funcidx_vector checks the vector count and every funcidx against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked funcidx vector.
                section_curr = wasm1p1_element_details::parse_funcidx_vector(element_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::passive_funcidx:
            {
                require_bulk_memory();
                element_storage.reftype = reference_type::funcref;
                // parse_elemkind_funcref checks one elemkind byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked elemkind.
                section_curr = wasm1p1_element_details::parse_elemkind_funcref(section_curr, section_end, err);
                // parse_funcidx_vector checks the vector count and every funcidx against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked funcidx vector.
                section_curr = wasm1p1_element_details::parse_funcidx_vector(element_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::active_explicit_funcidx:
            {
                require_reference_types();
                element_storage.active = true;
                element_storage.reftype = reference_type::funcref;
                parse_tableidx();
                parse_active_offset();
                // parse_elemkind_funcref checks one elemkind byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked elemkind.
                section_curr = wasm1p1_element_details::parse_elemkind_funcref(section_curr, section_end, err);
                wasm1p1_element_details::check_active_table(element_storage, module_storage, counts, section_curr, err);
                // parse_funcidx_vector checks the vector count and every funcidx against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked funcidx vector.
                section_curr = wasm1p1_element_details::parse_funcidx_vector(element_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::declarative_funcidx:
            {
                require_bulk_memory();
                element_storage.declarative = true;
                element_storage.reftype = reference_type::funcref;
                // parse_elemkind_funcref checks one elemkind byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked elemkind.
                section_curr = wasm1p1_element_details::parse_elemkind_funcref(section_curr, section_end, err);
                // parse_funcidx_vector checks the vector count and every funcidx against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked funcidx vector.
                section_curr = wasm1p1_element_details::parse_funcidx_vector(element_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::active_implicit_expr:
            {
                require_reference_types();
                element_storage.active = true;
                element_storage.table_idx = 0u;
                element_storage.reftype = reference_type::funcref;
                parse_active_offset();
                wasm1p1_element_details::check_active_table(element_storage, module_storage, counts, section_curr, err);
                // parse_expr_vector checks the expression count and every constant expression against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked expression vector.
                section_curr = wasm1p1_element_details::parse_expr_vector(element_storage, module_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::passive_expr:
            {
                require_bulk_memory();
                require_reference_types();
                // parse_reftype checks one reference-type byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked reftype.
                section_curr = wasm1p1_element_details::parse_reftype(element_storage.reftype, section_curr, section_end, err, fs_para);
                // parse_expr_vector checks the expression count and every constant expression against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked expression vector.
                section_curr = wasm1p1_element_details::parse_expr_vector(element_storage, module_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::active_explicit_expr:
            {
                require_reference_types();
                element_storage.active = true;
                parse_tableidx();
                parse_active_offset();
                // parse_reftype checks one reference-type byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked reftype.
                section_curr = wasm1p1_element_details::parse_reftype(element_storage.reftype, section_curr, section_end, err, fs_para);
                wasm1p1_element_details::check_active_table(element_storage, module_storage, counts, section_curr, err);
                // parse_expr_vector checks the expression count and every constant expression against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked expression vector.
                section_curr = wasm1p1_element_details::parse_expr_vector(element_storage, module_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t::declarative_expr:
            {
                require_bulk_memory();
                require_reference_types();
                element_storage.declarative = true;
                // parse_reftype checks one reference-type byte and returns the first byte after it.
                // Pointer move: replace section_curr with the first byte after the checked reftype.
                section_curr = wasm1p1_element_details::parse_reftype(element_storage.reftype, section_curr, section_end, err, fs_para);
                // parse_expr_vector checks the expression count and every constant expression against section_end before returning the tail pointer.
                // Pointer move: replace section_curr with the first byte after the checked expression vector.
                section_curr = wasm1p1_element_details::parse_expr_vector(element_storage, module_storage, counts, section_curr, section_end, err, fs_para);
                break;
            }
            [[unlikely]] default:
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = static_cast<wasm_u32>(fet_type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_element_segment_flag;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        return section_curr;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
