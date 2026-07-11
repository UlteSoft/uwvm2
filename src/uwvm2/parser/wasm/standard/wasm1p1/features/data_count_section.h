/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Data count section
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
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include "def.h"
# include "feature_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief Parse and validate the wasm1.1 data count section payload.
    /// @details The payload is exactly one u32 LEB128 and is accepted only when the bulk-memory feature is enabled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
        auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        if(para.disable_bulk_memory) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.wasm1p1_feature_required.value =
                ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>::section_id;
            err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory;
            err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>(module_storage.sections)};

        if(datacountsec.present) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = datacountsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;
        datacountsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        datacountsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);
        datacountsec.present = true;

        auto section_curr{section_begin};

        // [before_section ... ] | count ... (section_end)
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr
        //
        // The payload of section 12 is exactly one u32. parse_by_scan proves the LEB128 read does not pass section_end.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count;
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        auto const [count_next, count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                    ::fast_io::mnp::leb128_get(count))};
        if(count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_data_count_section_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(count_err);
        }

        // parse_by_scan succeeded, so [section_curr, count_next) is now proven safe and count_next is inside [section_curr, section_end].
        //
        // Proof view before moving section_curr:
        // [before_section ...] count ... (count_next) ... (section_end)
        // [        safe      ] [ safe ]  unsafe (count_next could be section_end)
        //                       ^^ section_curr
        //                                  ^^ count_next
        //
        // Pointer move: now advance section_curr to the first byte after the checked count payload.
        section_curr = reinterpret_cast<::std::byte const*>(count_next);

        // Move view after assigning section_curr:
        // [before_section ... count ...] (section_end)
        // [            safe            ] unsafe (could be the section_end)
        //                               ^^ section_curr == count_next

        // Data-count payload has no trailing fields, so section_curr must be exactly section_end.
        if(section_curr != section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::unexpected_section_data;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            if(count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_begin;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        auto const& parser_limit{para.parser_limit};
        if(static_cast<::std::size_t>(count) > parser_limit.max_data_count_sec_count) [[unlikely]]
        {
            err.err_curr = section_begin;
            err.err_selectable.exceed_the_max_parser_limit.name = u8"datacountsec_count";
            err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(count);
            err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_data_count_sec_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        datacountsec.count = count;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
