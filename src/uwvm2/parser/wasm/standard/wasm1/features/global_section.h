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
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
# include <bit>
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
# include <uwvm2/parser/wasm/standard/wasm1/const_expr/impl.h>
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
    struct global_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Global"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::global_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Fs...>> local_globals{};
    };

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::global_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* global_section_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::global_type & global_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [... global_curr] ...
        // [   safe        ] unsafe (could be the section_end)
        //      ^^ section_curr

        // Handling of scan_global_type is completely memory-safe

        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_global_type(global_r, section_curr, section_end, err);
    }

    /// @brief This function only performs type stack matching, not computation.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* parse_and_check_global_expr_valid(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::global_type const& global_r,                    // [adl] can be replaced
        ::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t& global_expr,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[3]: global
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 3uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const& imported_global{importsec.importdesc.index_unchecked(3uz)};
        auto const imported_global_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_global.size())};

        // [... global_curr ...] expr_curr ... 0x0B global_next ...
        // [   safe            ] unsafe (could be the section_end)
        //                       ^^ section_curr

        global_expr.begin = section_curr;

        // [... global_curr ...] expr_curr ... 0x0B global_next ...
        // [   safe            ] unsafe (could be the section_end)
        //                       ^^ global_expr.begin

        // Since global initialization expressions in wasm1.0 only allow instructions that “increment the data stack”, a simple check for whether an instruction
        // already exists can be used here to detect this.
        bool has_data_on_type_stack{};

        auto const curr_global_type{global_r.type};

        for(;;)
        {
            if(section_curr == section_end) [[unlikely]]
            {
                // [... global_curr ... expr_curr ...] (end) ...
                // [                   safe          ] unsafe (could be the section_end)
                //                                     ^^ section_curr

                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_terminator_not_found;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [... curr] ...
            // [  safe  ] unsafe (could be the section_end)
            //      ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type section_curr_c8;
            ::std::memcpy(::std::addressof(section_curr_c8), section_curr, sizeof(::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type));

            // Size equal to one does not need to do little-endian conversion
            static_assert(sizeof(section_curr_c8) == 1uz);

#if CHAR_BIT > 8
            section_curr_c8 &= static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFFu);
#endif

            if(section_curr_c8 ==
               static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::end))
            {
                // [... global_curr ... section_curr ... 0x0B] global_next ...
                // [                   safe                  ] unsafe (could be the section_end)
                //                                       ^^ section_curr

                ++section_curr;

                // [... global_curr ... section_curr ... 0x0B] global_next ...
                // [                   safe                  ] unsafe (could be the section_end)
                //                                             ^^ section_curr

                break;
            }
            else
            {
                switch(section_curr_c8)
                {
                    [[unlikely]] case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::end):
                    {
                        // checked before
                        ::std::unreachable();
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                    {
                        // Check the type stack. Since only will increase the type stack, only one bool check is needed.
                        if(has_data_on_type_stack) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        has_data_on_type_stack = true;

                        if(curr_global_type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_global_type);
                            err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr] ...
                        // [  safe  ] unsafe (could be the section_end)
                        //      ^^ section_curr

                        ++section_curr;

                        // [... curr] i32 ... ...
                        // [  safe  ] unsafe (could be the section_end)
                        //            ^^ section_curr

                        // parse_by_scan comes with built-in memory safety checks.

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 test_i32;

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        auto const [test_i32_next, test_i32_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(test_i32))};

                        if(test_i32_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(test_i32_err);
                        }

                        // [... curr i32 ...] ...
                        // [       safe     ] unsafe (could be the section_end)
                        //           ^^ section_curr

                        section_curr = reinterpret_cast<::std::byte const*>(test_i32_next);

                        // [... curr i32 ...] ...
                        // [       safe     ] unsafe (could be the section_end)
                        //                    ^^ section_curr

                        global_expr.opcodes.emplace_back(::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.i32 = test_i32},
                                                         ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const);

                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const):
                    {
                        // Check the type stack. Since only will increase the type stack, only one bool check is needed.
                        if(has_data_on_type_stack) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        has_data_on_type_stack = true;

                        if(curr_global_type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_global_type);
                            err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr] ...
                        // [  safe  ] unsafe (could be the section_end)
                        //      ^^ section_curr

                        ++section_curr;

                        // [... curr] i64 ... ...
                        // [  safe  ] unsafe (could be the section_end)
                        //            ^^ section_curr

                        // parse_by_scan comes with built-in memory safety checks.

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 test_i64;

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        auto const [test_i64_next, test_i64_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(test_i64))};

                        if(test_i64_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(test_i64_err);
                        }

                        // [... curr i64 ...] ...
                        // [       safe     ] unsafe (could be the section_end)
                        //           ^^ section_curr

                        section_curr = reinterpret_cast<::std::byte const*>(test_i64_next);

                        // [... curr i64 ...] ...
                        // [       safe     ] unsafe (could be the section_end)
                        //                    ^^ section_curr

                        global_expr.opcodes.emplace_back(::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.i64 = test_i64},
                                                         ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const);

                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const):
                    {
                        // Check the type stack. Since only will increase the type stack, only one bool check is needed.
                        if(has_data_on_type_stack) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        has_data_on_type_stack = true;

                        if(curr_global_type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_global_type);
                            err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr] ...
                        // [  safe  ] unsafe (could be the section_end)
                        //      ^^ section_curr

                        ++section_curr;

                        // [... curr] f32 xx xx xx ...
                        // [  safe  ] unsafe (could be the section_end)
                        //            ^^ section_curr

                        if(static_cast<::std::size_t>(section_end - section_curr) < 4uz) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr f32 xx xx xx ] ...
                        // [       safe           ] unsafe (could be the section_end)
                        //           ^^ section_curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 test_f32;

#if CHAR_BIT > 8
                        ::std::uint_least32_t temp_ul32{};
                        for(unsigned i{}; i != 4u; ++i)
                        {
                            auto curr_byte{::std::to_integer<::std::uint_least8_t>(*section_curr)};
                            curr_byte &= 0xFFu;
                            temp_ul32 |= curr_byte << (i * 8u);

                            // [... xx ...] ...
                            // [  safe    ] unsafe (could be the section_end)
                            //      ^^ section_curr

                            ++section_curr;

                            // [... xx ]...] ...
                            // [  safe ]...] unsafe (could be the section_end)
                            //          ^^ section_curr
                        }

                        test_f32 = ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(temp_ul32);
#else
                        static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32) == 4uz);
                        static_assert(sizeof(::std::uint_least32_t) == 4uz);

                        ::std::uint_least32_t temp_ul32;
                        ::std::memcpy(::std::addressof(temp_ul32), section_curr, 4uz);
                        // to little endian
                        temp_ul32 = ::fast_io::little_endian(temp_ul32);

                        test_f32 = ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(temp_ul32);

                        section_curr += 4u;
#endif

                        // [... curr f32 xx xx xx ] ...
                        // [       safe           ] unsafe (could be the section_end)
                        //                          ^^ section_curr

                        global_expr.opcodes.emplace_back(::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.f32 = test_f32},
                                                         ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const);

                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const):
                    {
                        // Check the type stack. Since only will increase the type stack, only one bool check is needed.
                        if(has_data_on_type_stack) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        has_data_on_type_stack = true;

                        if(curr_global_type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_global_type);
                            err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr] ...
                        // [  safe  ] unsafe (could be the section_end)
                        //      ^^ section_curr

                        ++section_curr;

                        // [... curr] f64 xx xx xx xx xx xx xx ...
                        // [  safe  ] unsafe (could be the section_end)
                        //            ^^ section_curr

                        if(static_cast<::std::size_t>(section_end - section_curr) < 8uz) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // [... curr f64 xx xx xx xx xx xx xx ] ...
                        // [       safe                       ] unsafe (could be the section_end)
                        //           ^^ section_curr

                        // There is no need to perform little-endian capture here, because 8 bytes of space is sufficient to obtain fp64.
                        // On platforms where charbit is not equal to 8, only cyclic reading is allowed.

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 test_f64;

#if CHAR_BIT > 8
                        ::std::uint_least64_t temp_ul64{};
                        for(unsigned i{}; i != 8u; ++i)
                        {
                            auto curr_byte{::std::to_integer<::std::uint_least8_t>(*section_curr)};
                            curr_byte &= 0xFFu;
                            temp_ul64 |= curr_byte << (i * 8u);

                            // [... xx ...] ...
                            // [  safe    ] unsafe (could be the section_end)
                            //      ^^ section_curr

                            ++section_curr;

                            // [... xx ]...] ...
                            // [  safe ]...] unsafe (could be the section_end)
                            //          ^^ section_curr
                        }

                        test_f64 = ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(temp_ul64);
#else
                        static_assert(sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64) == 8uz);
                        static_assert(sizeof(::std::uint_least64_t) == 8uz);

                        ::std::uint_least64_t temp_ul64;
                        ::std::memcpy(::std::addressof(temp_ul64), section_curr, 8uz);
                        // to little endian
                        temp_ul64 = ::fast_io::little_endian(temp_ul64);

                        test_f64 = ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(temp_ul64);

                        section_curr += 8u;
#endif

                        // [... curr f64 xx xx xx xx xx xx xx ] ...
                        // [       safe                       ] unsafe (could be the section_end)
                        //                                      ^^ section_curr

                        global_expr.opcodes.emplace_back(::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.f64 = test_f64},
                                                         ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const);

                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                    {
                        /// @see WebAssembly Release 1.0 (2019-07-20) § 3.4.10
                        ///      Globals, however, are not recursive. The effect of defining the limited context C's for validating the module's globals is that
                        ///      their initialization expressions can only access imported globals and nothing else.

                        // Check the type stack. Since only will increase the type stack, only one bool check is needed.
                        if(has_data_on_type_stack) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_should_be_only_one_element;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        has_data_on_type_stack = true;

                        // [... curr] ...
                        // [  safe  ] unsafe (could be the section_end)
                        //      ^^ section_curr

                        ++section_curr;

                        // [... curr] global_idx(u32) ... ...
                        // [  safe  ] unsafe (could be the section_end)
                        //            ^^ section_curr

                        // parse_by_scan comes with built-in memory safety checks.

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 test_global_idx;

                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                        auto const [test_global_idx_next,
                                    test_global_idx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(test_global_idx))};

                        if(test_global_idx_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(test_global_idx_err);
                        }

                        // [... curr global_idx(u32) ...] ...
                        // [       safe                 ] unsafe (could be the section_end)
                        //           ^^ section_curr

                        // check test_global_idx

                        if(test_global_idx >= imported_global_size) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32arr[0] = imported_global_size;
                            err.err_selectable.u32arr[1] = test_global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_illegal_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // check imported global type and mutability
                        auto const curr_imported_global_ptr{imported_global.index_unchecked(test_global_idx)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        if(curr_imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                        auto const& curr_imported_global{curr_imported_global_ptr->imports.storage.global};

                        // Check if imported global type matches current global type
                        if(curr_imported_global.type != curr_global_type) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_global_type);
                            err.err_selectable.u8arr[1] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_imported_global.type);
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // Check if imported global is immutable (mutable globals cannot be referenced in init expressions)
                        if(curr_imported_global.is_mutable) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = test_global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_mutable_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        section_curr = reinterpret_cast<::std::byte const*>(test_global_idx_next);
                        // [... curr global_idx(u32) ...] ...
                        // [       safe                 ] unsafe (could be the section_end)
                        //                                ^^ section_curr

                        global_expr.opcodes.emplace_back(
                            ::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.imported_global_idx = test_global_idx},
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
        }

        if(!has_data_on_type_stack) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_empty;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [... global_curr ... section_curr ... 0x0B] global_next ...
        // [                   safe                  ] unsafe (could be the section_end)
        //                                             ^^ section_curr

        global_expr.end = section_curr;

        // [... global_curr ... section_curr ... 0x0B] global_next ...
        // [                   safe                  ] unsafe (could be the section_end)
        //                                             ^^ global_expr.end

        return section_curr;
    }

    /// @brief Define the handler function for global_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<global_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse global section (id: 6)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get global_section_storage_t from storages
        auto& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t<Fs...>>(module_storage.sections)};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[3]: global
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 3uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const imported_global_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(3uz).size())};

        // check duplicate
        if(globalsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = globalsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        globalsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        globalsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | global_count ... global1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [global_count_next, global_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(global_count))};

        if(global_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_global_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_count_err);
        }

        // [before_section ... | global_count ...] global1 ...
        // [             safe                    ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(global_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(global_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(global_count) > parser_limit.max_global_sec_globals) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"globalsec_globals";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(global_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_global_sec_globals;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if(global_count > wasm_u32_max - imported_global_size) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.imp_def_num_exceed_u32max.type = 0x03;  // global
            err.err_selectable.imp_def_num_exceed_u32max.defined = global_count;
            err.err_selectable.imp_def_num_exceed_u32max.imported = imported_global_size;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        globalsec.local_globals.reserve(static_cast<::std::size_t>(global_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(global_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | global_count ...] global1 ...
        // [              safe                   ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [... global_curr] ...
            // [   safe        ] unsafe (could be the section_end)
            //      ^^ section_curr

            // check global counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(global_counter, section_curr, err) > global_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = global_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage global (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Fs...> local_global{};

            // [... global_curr] ... expr_curr ... 0x0B global_next ...
            // [   safe        ] unsafe (could be the section_end)
            //      ^^ section_curr

            // Function call matching via adl
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_global_section_global_handler<Fs...>);

            section_curr = global_section_global_handler(sec_adl, local_global.global, module_storage, section_curr, section_end, err, fs_para);

            // [... global_curr ...] expr_curr ... 0x0B global_next ...
            // [   safe            ] unsafe (could be the section_end)
            //                       ^^ section_curr

            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_parse_and_check_global_expr_valid<Fs...>);
            section_curr =
                parse_and_check_global_expr_valid(sec_adl, local_global.global, local_global.expr, module_storage, section_curr, section_end, err, fs_para);

            // [... global_curr ... expr_curr ... 0x0B] global_next ...
            // [                   safe               ] unsafe (could be the section_end)
            //                                          ^^ section_curr

            globalsec.local_globals.push_back_unchecked(::std::move(local_global));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check global counter match
        if(global_counter != global_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = global_counter;
            err.err_selectable.u32arr[1] = global_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the global section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct global_section_storage_section_details_wrapper_t
    {
        global_section_storage_t<Fs...> const* global_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr global_section_storage_section_details_wrapper_t<Fs...> section_details(
        global_section_storage_t<Fs...> const& global_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    {
        return {::std::addressof(global_section_storage), ::std::addressof(all_sections)};
    }

    /// @brief Print the global section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, global_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       global_section_storage_section_details_wrapper_t<Fs...> const global_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(global_section_details_wrapper.global_section_storage_ptr == nullptr || global_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const global_section_span{global_section_details_wrapper.global_section_storage_ptr->sec_span};
        auto const global_section_size{static_cast<::std::size_t>(global_section_span.sec_end - global_section_span.sec_begin)};

        if(global_section_size)
        {
            auto const global_size{global_section_details_wrapper.global_section_storage_ptr->local_globals.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nGlobal[", global_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nGlobal[", global_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nGlobal[", global_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nGlobal[", global_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nGlobal[", global_size, U"]:\n");
            }

            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                *global_section_details_wrapper.all_sections_ptr)};
            static_assert(importsec.importdesc_count > 3uz);
            auto global_counter{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(3uz).size())};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 localdef_counter{};

            for(auto const& curr_global: global_section_details_wrapper.global_section_storage_ptr->local_globals)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     " - localglobal[",
                                                                     localdef_counter,
                                                                     "] -> global[",
                                                                     global_counter,
                                                                     "]: {",
                                                                     section_details(curr_global.global),
                                                                     "}\n");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L" - localglobal[",
                                                                     localdef_counter,
                                                                     L"] -> global[",
                                                                     global_counter,
                                                                     L"]: {",
                                                                     section_details(curr_global.global),
                                                                     L"}\n");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8" - localglobal[",
                                                                     localdef_counter,
                                                                     u8"] -> global[",
                                                                     global_counter,
                                                                     u8"]: {",
                                                                     section_details(curr_global.global),
                                                                     u8"}\n");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u" - localglobal[",
                                                                     localdef_counter,
                                                                     u"] -> global[",
                                                                     global_counter,
                                                                     u"]: {",
                                                                     section_details(curr_global.global),
                                                                     u"}\n");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U" - localglobal[",
                                                                     localdef_counter,
                                                                     U"] -> global[",
                                                                     global_counter,
                                                                     U"]: {",
                                                                     section_details(curr_global.global),
                                                                     U"}\n");
                }

                ++global_counter;
                ++localdef_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
