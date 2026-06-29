/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-03
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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct table_section_storage_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Table"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::table_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...>> tables{};
    };

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* table_section_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::table_type & table_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [... table_curr] ...
        // [   safe       ] unsafe (could be the section_end)
        //      ^^ section_curr

        // Handling of scan_table_type is completely memory-safe

        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_table_type(table_r, section_curr, section_end, err);
    }

    /// @brief Define the handler function for table_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse table section (id: 4)"};
#endif
        // section_begin may equal section_end; parse_by_scan below bounds-checks the table count against section_end.

        // get table_section_storage_t from storages
        auto& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[1]: table
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 1uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const imported_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size())};

        // check duplicate
        if(tablesec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = tablesec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        tablesec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        tablesec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | table_count ... table1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [table_count_next, table_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                ::fast_io::mnp::leb128_get(table_count))};

        if(table_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_table_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_count_err);
        }

        // [before_section ... | table_count ...] table1 ...
        // [             safe                   ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(table_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(table_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        constexpr bool check_single_table{::uwvm2::parser::wasm::standard::wasm1::features::need_check_single_table_by_feature<Fs...>()};
        if constexpr(check_single_table)
        {
            /// @brief Check table counts with the WebAssembly 1.0 single-table restriction selected by the feature set.
            auto const check_single_table_section_limit{
                [&]() constexpr UWVM_THROWS
                {
                    /// @details    In the current version of WebAssembly, at most one table may be defined or imported in a single module,
                    ///             and all constructs implicitly reference this table 0. This restriction may be lifted in future versions.
                    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.4

                    // Since it has already been checked and an exception thrown in the import_section, it can never be greater than 1.
                    UWVM_ASSERT(imported_table_size <= 1u);
                    [[assume(imported_table_size <= 1u)]];

                    // table_count is not incremental and requires overflow checking
                    constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

                    // imported_table_size comes from importsec.importdesc[table] and has already been checked by import_section when the single-table
                    // restriction is active. The arithmetic is guarded before addition, so no u32 overflow can occur.
                    if(table_count > wasm_u32_max - imported_table_size ||
                       table_count + imported_table_size > static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_table;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }};

            constexpr bool allow_multi_table{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<Fs...>()};

            if constexpr(!allow_multi_table) { check_single_table_section_limit(); }
            else
            {
                /// @brief Check table-count overflow after a runtime feature flag permits multiple tables.
                auto const check_multi_table_section_limit{
                    [&]() constexpr UWVM_THROWS
                    {
                        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

                        // Multi-table mode keeps only the total imported+defined table count overflow check. The subtraction is safe because it is
                        // performed before any addition involving table_count.
                        if(table_count > wasm_u32_max - imported_table_size) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.imp_def_num_exceed_u32max.type = 0x01;  // table
                            err.err_selectable.imp_def_num_exceed_u32max.defined = table_count;
                            err.err_selectable.imp_def_num_exceed_u32max.imported = imported_table_size;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }
                    }};

                /// @brief Keep wasm1 table-section validation strict until a runtime extension flag enables multi-table.
                if(::uwvm2::parser::wasm::standard::wasm1::features::get_feature_parameter_controllable_allow_multi_table_from_paras(fs_para))
                {
                    check_single_table_section_limit();
                }
                else
                {
                    check_multi_table_section_limit();
                }
            }
        }
        else
        {
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

            // This branch is compiled only for feature sets that never need the wasm1 single-table check. Keep the total table count overflow check before
            // any imported+defined addition.
            if(table_count > wasm_u32_max - imported_table_size) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.imp_def_num_exceed_u32max.type = 0x01;  // table
                err.err_selectable.imp_def_num_exceed_u32max.defined = table_count;
                err.err_selectable.imp_def_num_exceed_u32max.imported = imported_table_size;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(table_count) > parser_limit.max_table_sec_tables) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"tablesec_tables";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(table_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_table_sec_tables;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        tablesec.tables.reserve(static_cast<::std::size_t>(table_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_counter{};  // use for check

        // parse_by_scan succeeded, so [section_curr, table_count_next) is now proven safe and table_count_next is inside [section_curr, section_end].
        // Proof view before moving section_curr: section_curr still points at the table count, and table_count_next marks the first table type.
        // Pointer move: advance section_curr to the first table type, or to section_end when the table vector is empty.
        section_curr = reinterpret_cast<::std::byte const*>(table_count_next);

        // [before_section ... | table_count ...] table1 ...
        // [              safe                  ] unsafe (could be the section_end)
        //                                        ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [... table_curr] ...
            // [   safe       ] unsafe (could be the section_end)
            //      ^^ section_curr

            // check table counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(table_counter, section_curr, err) > table_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = table_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage table (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Fs...> table{};

            // [... table_curr] ...
            // [   safe       ] unsafe (could be the section_end)
            //      ^^ section_curr

            // Function call matching via adl
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_table_section_table_handler<Fs...>);

            // table_section_table_handler checks the table type against section_end and returns the first byte after it.
            // Pointer move: replace section_curr with the first byte after the checked table type.
            section_curr = table_section_table_handler(sec_adl, table, module_storage, section_curr, section_end, err, fs_para);

            // [... table_curr ...] table_next
            // [   safe           ] unsafe (could be the section_end)
            //                      ^^ section_curr

            tablesec.tables.push_back_unchecked(::std::move(table));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check table counter match
        if(table_counter != table_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = table_counter;
            err.err_selectable.u32arr[1] = table_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the table section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct table_section_storage_section_details_wrapper_t
    {
        table_section_storage_t<Fs...> const* table_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr table_section_storage_section_details_wrapper_t<Fs...> section_details(
        table_section_storage_t<Fs...> const& table_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(table_section_storage), ::std::addressof(all_sections)}; }

    namespace details::table_section_print
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

        template <::std::integral char_type, typename T>
        inline constexpr bool emit_dynamic_reserve(char_type*& curr, char_type* end, T value, ::std::size_t& offset) noexcept
        {
            using value_type = ::std::remove_cvref_t<T>;
            constexpr ::std::size_t reserve_size{print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, value_type>)};
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

        template <typename char_type, typename... Fs>
        concept context_body_supported = ::std::integral<char_type> && (::uwvm2::parser::wasm::concepts::wasm_feature<Fs> && ...) &&
                                         ::fast_io::dynamic_reserve_with_possible_static_stack_size<
            char_type,
            decltype(section_details(::std::declval<table_section_storage_t<Fs...> const&>().tables.index_unchecked(0uz)))>;

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(header_prefix, "\nTable[");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(header_suffix, "]:\n");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_prefix, " - localtable[");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_table_prefix, "] -> table[");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_body_prefix, "]: {");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_suffix, "}\n");

        enum class stage : unsigned char
        {
            init,
            header_prefix,
            header_count,
            header_suffix,
            row_prefix,
            row_local_index,
            row_table_prefix,
            row_table_index,
            row_body_prefix,
            row_body,
            row_suffix,
            done
        };

        struct context
        {
            stage curr_stage{};
            ::std::size_t offset{};
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_counter{};
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 localdef_counter{};

            template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
                requires context_body_supported<char_type, Fs...>
            inline constexpr ::fast_io::context_print_result<char_type*> print_context_define(
                table_section_storage_section_details_wrapper_t<Fs...> const table_section_details_wrapper,
                char_type* curr,
                char_type* end) noexcept
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(table_section_details_wrapper.table_section_storage_ptr == nullptr || table_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
                {
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                }
#endif

                if(curr == end) [[unlikely]] { return {curr, false}; }

                auto const table_section_span{table_section_details_wrapper.table_section_storage_ptr->sec_span};
                auto const table_section_size{static_cast<::std::size_t>(table_section_span.sec_end - table_section_span.sec_begin)};

                if(table_section_size == 0uz || this->curr_stage == stage::done) { return {curr, true}; }

                auto const& tables{table_section_details_wrapper.table_section_storage_ptr->tables};
                auto const table_size{tables.size()};

                for(;;)
                {
                    switch(this->curr_stage)
                    {
                        case stage::init:
                        {
                            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                                *table_section_details_wrapper.all_sections_ptr)};
                            static_assert(importsec.importdesc_count > 1uz);
                            this->table_counter =
                                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size());
                            this->curr_stage = stage::header_prefix;
                            break;
                        }
                        case stage::header_prefix:
                        {
                            if(!emit_literal(curr, end, header_prefix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::header_count;
                            break;
                        }
                        case stage::header_count:
                        {
                            if(!emit_reserve(curr, end, table_size, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::header_suffix;
                            break;
                        }
                        case stage::header_suffix:
                        {
                            if(!emit_literal(curr, end, header_suffix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = table_size == 0uz ? stage::done : stage::row_prefix;
                            break;
                        }
                        case stage::row_prefix:
                        {
                            if(this->localdef_counter == table_size)
                            {
                                this->curr_stage = stage::done;
                                break;
                            }

                            if(!emit_literal(curr, end, row_prefix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_local_index;
                            break;
                        }
                        case stage::row_local_index:
                        {
                            if(!emit_reserve(curr, end, this->localdef_counter, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_table_prefix;
                            break;
                        }
                        case stage::row_table_prefix:
                        {
                            if(!emit_literal(curr, end, row_table_prefix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_table_index;
                            break;
                        }
                        case stage::row_table_index:
                        {
                            if(!emit_reserve(curr, end, this->table_counter, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_body_prefix;
                            break;
                        }
                        case stage::row_body_prefix:
                        {
                            if(!emit_literal(curr, end, row_body_prefix<char_type>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_body;
                            break;
                        }
                        case stage::row_body:
                        {
                            auto const& curr_table{tables.index_unchecked(this->localdef_counter)};
                            if(!emit_dynamic_reserve(curr, end, section_details(curr_table), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_suffix;
                            break;
                        }
                        case stage::row_suffix:
                        {
                            if(!emit_literal(curr, end, row_suffix<char_type>(), this->offset)) { return {curr, false}; }
                            ++this->table_counter;
                            ++this->localdef_counter;
                            this->curr_stage = stage::row_prefix;
                            break;
                        }
                        case stage::done: return {curr, true};
                    }

                    if(curr == end) { return {curr, false}; }
                }
            }
        };
    }  // namespace details::table_section_print

    /// @brief Print the table section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, table_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       table_section_storage_section_details_wrapper_t<Fs...> const table_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(table_section_details_wrapper.table_section_storage_ptr == nullptr || table_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const table_section_span{table_section_details_wrapper.table_section_storage_ptr->sec_span};
        auto const table_section_size{static_cast<::std::size_t>(table_section_span.sec_end - table_section_span.sec_begin)};

        if(table_section_size)
        {
            auto const table_size{table_section_details_wrapper.table_section_storage_ptr->tables.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nTable[", table_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nTable[", table_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nTable[", table_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nTable[", table_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nTable[", table_size, U"]:\n");
            }

            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                *table_section_details_wrapper.all_sections_ptr)};
            static_assert(importsec.importdesc_count > 1uz);
            auto table_counter{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size())};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 localdef_counter{};

            for(auto const& curr_table: table_section_details_wrapper.table_section_storage_ptr->tables)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     " - localtable[",
                                                                     localdef_counter,
                                                                     "] -> table[",
                                                                     table_counter,
                                                                     "]: {",
                                                                     section_details(curr_table),
                                                                     "}\n");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L" - localtable[",
                                                                     localdef_counter,
                                                                     L"] -> table[",
                                                                     table_counter,
                                                                     L"]: {",
                                                                     section_details(curr_table),
                                                                     L"}\n");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8" - localtable[",
                                                                     localdef_counter,
                                                                     u8"] -> table[",
                                                                     table_counter,
                                                                     u8"]: {",
                                                                     section_details(curr_table),
                                                                     u8"}\n");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u" - localtable[",
                                                                     localdef_counter,
                                                                     u"] -> table[",
                                                                     table_counter,
                                                                     u"]: {",
                                                                     section_details(curr_table),
                                                                     u"}\n");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U" - localtable[",
                                                                     localdef_counter,
                                                                     U"] -> table[",
                                                                     table_counter,
                                                                     U"]: {",
                                                                     section_details(curr_table),
                                                                     U"}\n");
                }

                ++table_counter;
                ++localdef_counter;
            }
        }
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires details::table_section_print::context_body_supported<char_type, Fs...>
    inline constexpr auto print_context_type(::fast_io::io_reserve_type_t<char_type, table_section_storage_section_details_wrapper_t<Fs...>>) noexcept
    { return ::fast_io::io_type_t<::uwvm2::parser::wasm::standard::wasm1::features::details::table_section_print::context>{}; }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires details::table_section_print::context_body_supported<char_type, Fs...>
    inline constexpr ::std::size_t print_context_static_buffer_size(
        ::fast_io::io_reserve_type_t<char_type, table_section_storage_section_details_wrapper_t<Fs...>>) noexcept
    {
        constexpr auto buffer_size{::fast_io::details::dynamic_reserve_default_static_stack_size<char_type>()};
        return buffer_size;
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
