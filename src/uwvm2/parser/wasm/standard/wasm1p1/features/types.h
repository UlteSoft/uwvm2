/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     antecedent dependency: WebAssembly Release 1.0 (2019-07-20)
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
# include <bit>
# include <climits>
# include <cstddef>
# include <cstdint>
# include <cstring>
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
# include "feature_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    inline constexpr bool is_valid_value_type(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_value_type(value_type); }

    /// @brief Check whether a wasm1.1 type-section value type is enabled by runtime feature flags.
    /// @details This 3-argument hook is intentionally named differently from the wasm1 two-argument hook.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_typesec_value_type_with_feature_parameter(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para); }

    /// @brief Check whether a wasm1.1 code-section local value type is enabled by runtime feature flags.
    /// @details This 3-argument hook is intentionally named differently from the wasm1 two-argument hook.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_codesec_value_type_with_feature_parameter(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para); }

    /// @brief Parse a wasm1.1 table type from a table section.
    /// @details Reference types are gated by runtime feature flags before limit parsing continues.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* table_section_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type & table_r,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [before_table_type ...] reftype limits ... (section_end)
        // [        safe         ] unsafe (could be the section_end)
        //                         ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_cannot_find_element;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [before_table_type ... reftype] limits ... (section_end)
        // [        safe                 ] unsafe
        //                        ^^ section_curr
        //
        // section_curr != section_end proves that the one-byte reftype read is in bounds.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte elemtype;
        ::std::memcpy(::std::addressof(elemtype), section_curr, sizeof(elemtype));

#if CHAR_BIT > 8
        elemtype = static_cast<decltype(elemtype)>(static_cast<::std::uint_least8_t>(elemtype) & 0xFFu);
#endif

        auto const reftype{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(elemtype)};
        if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_reference_type(
               ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(reftype)) ||
           !::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(reftype, fs_para)) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.wasm1p1_reference_type.value = elemtype;
            err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        table_r.reftype = reftype;
        // section_curr points at the one-byte table reftype already proven safe by section_curr != section_end and validated above.
        // Pointer move: advance to the limits payload.
        ++section_curr;

        // [before_table_type ... reftype] limits ... (section_end)
        // [            safe             ] unsafe (could be the section_end)
        //                                 ^^ section_curr
        //
        // scan_limit_type continues with the same section_end bound and returns the first byte after limits.
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_limit_type(table_r.limits, section_curr, section_end, err);
    }

    /// @brief Parse a wasm1.1 table type from an import descriptor.
    /// @details Imports share the same table-type parser as local table definitions.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type & table_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        return table_section_table_handler(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<table_section_storage_t<Fs...>>{},
                                           table_r,
                                           module_storage,
                                           section_curr,
                                           section_end,
                                           err,
                                           fs_para);
    }

    /// @brief Parse a wasm1.1 global type from a global section.
    /// @details Extended value types are rejected unless their corresponding runtime feature flag is enabled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* global_section_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type & global_r,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [before_global_type ...] valtype mut ... (section_end)
        // [        safe          ] unsafe (could be the section_end)
        //                          ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [before_global_type ... valtype] mut ... (section_end)
        // [        safe                  ] unsafe
        //                         ^^ section_curr
        //
        // section_curr != section_end proves that the one-byte valtype read is in bounds.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte raw_type;
        ::std::memcpy(::std::addressof(raw_type), section_curr, sizeof(raw_type));
#if CHAR_BIT > 8
        raw_type = static_cast<decltype(raw_type)>(static_cast<::std::uint_least8_t>(raw_type) & 0xFFu);
#endif

        auto const value_type{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>(raw_type)};
        if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para)) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u8 = raw_type;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_valtype;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        global_r.type = value_type;
        // section_curr points at the one-byte valtype already proven safe by section_curr != section_end and validated above.
        // Pointer move: advance to the mutability byte.
        ++section_curr;

        // [before_global_type ... valtype] mut ... (section_end)
        // [            safe              ] unsafe (could be the section_end)
        //                                  ^^ section_curr
        if(section_curr == section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [before_global_type ... valtype mut] ... (section_end)
        // [            safe                  ] unsafe
        //                                 ^^ section_curr
        //
        // section_curr != section_end proves that the one-byte mutability read is in bounds.
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte mut;
        ::std::memcpy(::std::addressof(mut), section_curr, sizeof(mut));
#if CHAR_BIT > 8
        mut = static_cast<decltype(mut)>(static_cast<::std::uint_least8_t>(mut) & 0xFFu);
#endif

        if(mut != 0u && mut != 1u) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u8 = mut;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_mut;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        global_r.is_mutable = static_cast<bool>(mut);
        // section_curr points at the one-byte mutability flag already proven safe by section_curr != section_end and validated above.
        // Pointer move: advance to the first byte after the checked global type.
        ++section_curr;

        // [before_global_type ... valtype mut] tail ... (section_end)
        // [              safe                ] unsafe (could be the section_end)
        //                                      ^^ section_curr
        return section_curr;
    }

    /// @brief Parse a wasm1.1 global type from an import descriptor.
    /// @details Imports share the same global-type parser as local global definitions.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type & global_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        return global_section_global_handler(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>>{},
                                             global_r,
                                             module_storage,
                                             section_curr,
                                             section_end,
                                             err,
                                             fs_para);
    }

    /// @brief Parse and validate a wasm1.1 global initializer expression.
    /// @details Supports MVP constants plus reference and v128 constants when their subfeatures are enabled.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* parse_and_check_global_expr_valid(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type const& global_r,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& global_expr,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 3uz);
        // importdesc has at least four buckets by static_assert; bucket 3 is the global-import bucket.
        auto const& imported_global{importsec.importdesc.index_unchecked(3uz)};
        auto const imported_global_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global.size())};

        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        // importdesc bucket 0 is the function-import bucket.
        auto const imported_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};
        auto const defined_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};
        auto const all_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_func_size + defined_func_size)};

        global_expr.begin = section_curr;
        bool has_data_on_type_stack{};

        using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        for(;;)
        {
            // [before_expr ...] opcode ... expr_tail ... end ... (section_end)
            // [      safe     ] unsafe (could be the section_end)
            //                   ^^ section_curr
            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_terminator_not_found;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [before_expr ... opcode] ... expr_tail ... end ... (section_end)
            // [      safe            ] unsafe
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
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                {
                    if(global_r.type != value_type::i32) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::i32);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte i32.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... opcode] i32_leb ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    //
                    // parse_by_scan bounds-checks the LEB128 immediate against section_end.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 value;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(value))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the i32 LEB128 immediate, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked i32 LEB128 immediate.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.i32 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const):
                {
                    if(global_r.type != value_type::i64) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::i64);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte i64.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... opcode] i64_leb ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan bounds-checks the LEB128 immediate against section_end.
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 value;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(value))};
                    if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                    }

                    // [before_expr ... opcode i64_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                         ^^ section_curr

                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the i64 LEB128 immediate, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked i64 LEB128 immediate.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... opcode i64_leb ...] expr_tail ... end ... (section_end)
                    // [               safe               ] unsafe (could be the section_end)
                    //                                      ^^ section_curr

                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.i64 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const):
                {
                    if(global_r.type != value_type::f32) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::f32);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte f32.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the fixed-width f32 payload.
                    ++section_curr;

                    // [before_expr ... opcode] f32_payload[4] ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    if(static_cast<::std::size_t>(section_end - section_curr) < 4uz) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... opcode f32_payload[4]] ... expr_tail ... end ... (section_end)
                    // [         safe                        ] unsafe
                    //                         ^^ section_curr
                    // The length check above proves [section_curr, section_curr + 4) is safe inside the current section.
                    ::std::uint_least32_t raw;
                    ::std::memcpy(::std::addressof(raw), section_curr, 4uz);
                    raw = ::fast_io::little_endian(raw);
                    auto value{::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(raw)};
                    // Pointer move: advance by the 4 bytes proven safe by the fixed-width payload check.
                    section_curr += 4uz;

                    // [before_expr ... opcode f32_payload[4]] expr_tail ... end ... (section_end)
                    // [                 safe                ] unsafe (could be the section_end)
                    //                                        ^^ section_curr

                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.f32 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const):
                {
                    if(global_r.type != value_type::f64) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::f64);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte f64.const opcode already proven safe by the opcode read.
                    // Pointer move: advance to the fixed-width f64 payload.
                    ++section_curr;

                    // [before_expr ... opcode] f64_payload[8] ... expr_tail ... end ... (section_end)
                    // [         safe         ] unsafe (could be the section_end)
                    //                          ^^ section_curr
                    if(static_cast<::std::size_t>(section_end - section_curr) < 8uz) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... opcode f64_payload[8]] ... expr_tail ... end ... (section_end)
                    // [         safe                        ] unsafe
                    //                         ^^ section_curr
                    // The length check above proves [section_curr, section_curr + 8) is safe inside the current section.
                    ::std::uint_least64_t raw;
                    ::std::memcpy(::std::addressof(raw), section_curr, 8uz);
                    raw = ::fast_io::little_endian(raw);
                    auto value{::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(raw)};
                    // Pointer move: advance by the 8 bytes proven safe by the fixed-width payload check.
                    section_curr += 8uz;

                    // [before_expr ... opcode f64_payload[8]] expr_tail ... end ... (section_end)
                    // [                 safe                ] unsafe (could be the section_end)
                    //                                        ^^ section_curr
                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.f64 = value},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const);
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
                    //
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
                    auto const& curr_imported_global{curr_imported_global_ptr->imports.storage.global};
                    if(curr_imported_global.type != global_r.type) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_imported_global.type);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
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
                    //                                            ^^ section_curr

                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.imported_global_idx = global_idx},
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get);
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD0u):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(!para.enable_reference_types) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xD0u;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
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
                    auto const ref_type{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(raw_ref)};
                    auto const ref_value_type{::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(ref_type)};
                    if(!::uwvm2::parser::wasm::standard::wasm1p1::type::is_valid_reference_type(ref_value_type)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_reference_type.value = raw_ref;
                        err.err_selectable.wasm1p1_reference_type.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_reference_type;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    if(global_r.type != ref_value_type) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_reference_type_mismatch.expected =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
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
                    // [              safe             ] unsafe (could be the section_end)
                    //                                  ^^ section_curr
                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.ref_null_type = raw_ref},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD0u));
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD2u):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(!para.enable_reference_types) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xD2u;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    if(global_r.type != value_type::funcref) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_reference_type_mismatch.expected =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.wasm1p1_reference_type_mismatch.actual =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::funcref);
                        err.err_selectable.wasm1p1_reference_type_mismatch.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_reference_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    // section_curr points at the one-byte ref.func opcode already proven safe by the opcode read.
                    // Pointer move: advance to the funcidx LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... ref.func] funcidx ... expr_tail ... end ... (section_end)
                    // [         safe           ] unsafe (could be the section_end)
                    //                           ^^ section_curr

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

                    if(func_idx >= all_func_size) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_func_index_exceeds_maxvul.idx = func_idx;
                        err.err_selectable.wasm1p1_func_index_exceeds_maxvul.maxval = all_func_size;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_init_ref_func_index_exceeds_maxvul;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the funcidx LEB128, and next marks the next expression byte.
                    // Pointer move: advance section_curr to the next expression byte after the checked funcidx.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... ref.func funcidx ...] expr_tail ... end ... (section_end)
                    // [                safe                ] unsafe (could be the section_end)
                    //                                      ^^ section_curr
                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.ref_func_idx = func_idx},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD2u));
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFDu):
                {
                    auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                    if(!para.enable_simd) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.wasm1p1_feature_required.value = 0xFDu;
                        err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd;
                        err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                    if(global_r.type != value_type::v128) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(global_r.type);
                        err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type::v128);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // section_curr points at the one-byte SIMD prefix already proven safe by the opcode read.
                    // Pointer move: advance to the SIMD subopcode LEB128 immediate.
                    ++section_curr;

                    // [before_expr ... simd_prefix] simd_subopcode ... v128_payload[16] ... end ... (section_end)
                    // [            safe          ] unsafe (could be the section_end)
                    //                              ^^ section_curr
                    //
                    // parse_by_scan bounds-checks the SIMD subopcode LEB128 against section_end.
                    wasm_u32 simd_subopcode;
                    auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                     reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                     ::fast_io::mnp::leb128_get(simd_subopcode))};
                    if(perr != ::fast_io::parse_code::ok || simd_subopcode != static_cast<wasm_u32>(0x0Cu)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_instruction;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr == ::fast_io::parse_code::ok ? ::fast_io::parse_code::invalid : perr);
                    }
                    // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                    // Proof view before moving section_curr: section_curr still points at the SIMD subopcode LEB128, and next marks the v128 payload.
                    // Pointer move: advance section_curr to the fixed-width v128 payload.
                    section_curr = reinterpret_cast<::std::byte const*>(next);

                    // [before_expr ... simd_prefix simd_subopcode ...] v128_payload[16] ... end ... (section_end)
                    // [                     safe                     ] unsafe (could be the section_end)
                    //                                                  ^^ section_curr

                    if(static_cast<::std::size_t>(section_end - section_curr) <
                       sizeof(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t)) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    // [before_expr ... simd_prefix simd_subopcode v128_payload[16]] expr_tail ... end ... (section_end)
                    // [                              safe                         ] unsafe (could be the section_end)
                    //                                             ^^ section_curr

                    // The length check above proves [section_curr, section_curr + 16) is safe inside the current section.
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_v128_storage_t v128_value{};
                    ::std::memcpy(::std::addressof(v128_value), section_curr, sizeof(v128_value));
                    // Pointer move: advance by the 16 bytes proven safe by the fixed-width payload check.
                    section_curr += sizeof(v128_value);

                    // [before_expr ... simd_prefix simd_subopcode v128_payload[16]] expr_tail ... end ... (section_end)
                    // [                              safe                         ] unsafe (could be the section_end)
                    //                                                               ^^ section_curr
                    global_expr.opcodes.reserve(1uz);
                    global_expr.opcodes.emplace_back_unchecked(
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.v128 = v128_value},
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xFDu));
                    break;
                }
                [[unlikely]] default:
                {
                    /// @warning Extension point: new global const-expression opcodes need storage, type checking, and initializer evaluation before this fallback.
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
        // [            safe            ] unsafe (could be the section_end)
        //                                ^^ section_curr
        global_expr.end = section_curr;
        return section_curr;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
