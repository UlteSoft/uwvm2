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
    struct element_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Element"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::element_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...>> elems{};
    };

    /// @brief This function only performs type stack matching, not computation.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* parse_and_check_element_expr_valid(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<element_section_storage_t<Fs...>> sec_adl,
        [[maybe_unused]] ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...> const& wet,  // [adl] can be replaced
        ::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t& element_expr,               // [adl] can be replaced
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

        // [table_idx ...] expr ... 0x0B func_count ... func ... next_table_idx ...
        // [     safe    ] unsafe (could be the section_end)
        //                 ^^ section_curr

        element_expr.begin = section_curr;

        // [table_idx ...] expr ... 0x0B func_count ... func ... next_table_idx ...
        // [     safe    ] unsafe (could be the section_end)
        //                 ^^ element_expr.begin

        // Since global initialization expressions in wasm1.0 only allow instructions that “increment the data stack”, a simple check for whether an instruction
        // already exists can be used here to detect this.
        bool has_data_on_type_stack{};

        for(;;)
        {
            if(section_curr == section_end) [[unlikely]]
            {
                // [... ] (end) ...
                // [safe] unsafe (could be the section_end)
                //        ^^ section_curr

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

                        // type matched

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

                        element_expr.opcodes.emplace_back(::uwvm2::parser::wasm::standard::wasm1::const_expr::base_const_expr_opcode_storage_u{.i32 = test_i32},
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

                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64);
                        err.err_selectable.u8arr[1] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
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

                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32);
                        err.err_selectable.u8arr[1] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
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

                        err.err_curr = section_curr;
                        err.err_selectable.u8arr[0] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64);
                        err.err_selectable.u8arr[1] =
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
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
                        if(curr_imported_global.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
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

                        element_expr.opcodes.emplace_back(
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

        // The result type i32 is implicitly guaranteed by branch constraints.

        if(!has_data_on_type_stack) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_stack_empty;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [... global_curr ... section_curr ... 0x0B] global_next ...
        // [                   safe                  ] unsafe (could be the section_end)
        //                                             ^^ section_curr

        element_expr.end = section_curr;

        // [... global_curr ... section_curr ... 0x0B] global_next ...
        // [                   safe                  ] unsafe (could be the section_end)
        //                                             ^^ element_expr.end

        return section_curr;
    }

    /// @brief Define function for wasm1 wasm1_element_t
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_handler_element_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<element_section_storage_t<Fs...>> sec_adl,
        decltype(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>{}.storage)& fet_storage,  // [adl]
        decltype(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>{}.type) const fet_type,   // [adl]
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [table_idx ...] expr ... 0x0B func_count ... func ... next_table_idx ...
        // [     safe    ] unsafe (could be the section_end)
        //                 ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_elem_storage_t<Fs...>& wet{fet_storage.table_idx};
        auto const elem_table_idx{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(fet_type)};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[0]: func, importdesc[1]: table
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 1uz);  // Ensure that subsequent index visits do not cross boundaries

        // get function_storage_t from storages
        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        auto const defined_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};
        auto const imported_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};
        // Addition does not overflow, Dependency Pre-Fill Pre-Check
        auto const all_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_func_size + imported_func_size)};

        // get table_section_storage_t from storages
        auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t<Fs...>>(module_storage.sections)};
        auto const defined_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(tablesec.tables.size())};
        auto const imported_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(1uz).size())};
        // Addition does not overflow, Dependency Pre-Fill Pre-Check
        auto const all_table_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_table_size + imported_table_size)};

        // check table index, all_table_size is limited to 1 in wasm1.0.
        if(elem_table_idx >= all_table_size) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.elem_table_index_exceeds_maxvul.idx = elem_table_idx;
            err.err_selectable.elem_table_index_exceeds_maxvul.maxval = all_table_size;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_table_index_exceeds_maxvul;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        wet.table_idx = elem_table_idx;

        // [table_idx ...] expr ... 0x0B func_count ... func ... next_table_idx ...
        // [     safe    ] unsafe (could be the section_end)
        //                 ^^ section_curr

        // Types provided internally without adding concepts
        section_curr = parse_and_check_element_expr_valid(sec_adl, wet, wet.expr, module_storage, section_curr, section_end, err, fs_para);

        // [table_idx ... expr ... 0x0B] func_count ... func ... next_table_idx ...
        // [           safe            ] unsafe (could be the section_end)
        //                               ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 funcidx_count;

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [funcidx_count_next, funcidx_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                    ::fast_io::mnp::leb128_get(funcidx_count))};

        if(funcidx_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_funcidx_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(funcidx_count_err);
        }

        // [table_idx ... expr ... 0x0B func_count ...] func ... next_table_idx ...
        // [                     safe                 ] unsafe (could be the section_end)
        //                              ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(funcidx_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(funcidx_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(funcidx_count) > parser_limit.max_elem_sec_funcidx) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"elemsec_funcidx";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(funcidx_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_elem_sec_funcidx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        wet.vec_funcidx.reserve(static_cast<::std::size_t>(funcidx_count));

        section_curr = reinterpret_cast<::std::byte const*>(funcidx_count_next);

        // [table_idx ... expr ... 0x0B func_count ...] func ... next_table_idx ...
        // [                     safe                 ] unsafe (could be the section_end)
        //                                              ^^ section_curr

        for(::std::size_t funcidx_counter{}; funcidx_counter != funcidx_count; ++funcidx_counter)
        {
            // [ ...] func_curr ... func_next ...
            // [safe] unsafe (could be the section_end)
            //        ^^ section_curr

            // No boundary check is needed here, parse_by_scan comes with its own checks

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 funcidx;

            auto const [funcidx_next, funcidx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(funcidx))};

            if(funcidx_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_funcidx;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(funcidx_err);
            }

            // [ ... func_curr ...] func_next ...
            // [       safe       ] unsafe (could be the section_end)
            //       ^^ section_curr

            if(funcidx >= all_func_size) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.elem_func_index_exceeds_maxvul.idx = funcidx;
                err.err_selectable.elem_func_index_exceeds_maxvul.maxval = all_func_size;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_func_index_exceeds_maxvul;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            wet.vec_funcidx.push_back_unchecked(funcidx);

            section_curr = reinterpret_cast<::std::byte const*>(funcidx_next);

            // [ ... func_curr ...] func_next ...
            // [       safe       ] unsafe (could be the section_end)
            //                      ^^ section_curr
        }

        return section_curr;
    }

    /// @brief Define the handler function for element_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<element_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse element section (id: 9)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get element_section_storage_t from storages
        auto& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<element_section_storage_t<Fs...>>(module_storage.sections)};

        // check duplicate
        if(elemsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = elemsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        elemsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        elemsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | elem_count ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 elem_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [elem_count_next, elem_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(elem_count))};

        if(elem_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(elem_count_err);
        }

        // [before_section ... ] | elem_count ...] (end)
        // [        safe                         ] unsafe (could be the section_end)
        //                         ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(elem_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(elem_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(elem_count) > parser_limit.max_elem_sec_elems) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"elemsec_elems";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(elem_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_elem_sec_elems;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        elemsec.elems.reserve(static_cast<::std::size_t>(elem_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 elem_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(elem_count_next);  // never out of bounds

        // [before_section ... ] | elem_count ...] table_idx
        // [        safe                         ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [elem_kind] ...
            // [   safe  ] unsafe (could be the section_end)
            //  ^^ section_curr

            // check elem counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(elem_counter, section_curr, err) > elem_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = elem_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // be used to store
            ::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...> fet{};

            // [elem_kind] ...
            // [   safe  ] unsafe (could be the section_end)
            //  ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 elem_kind;  // No initialization necessary

            auto const [elem_kind_next, elem_kind_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                ::fast_io::mnp::leb128_get(elem_kind))};

            if(elem_kind_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                if constexpr(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Fs...>,
                                            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>>)
                {
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_table_idx;
                }
                else
                {
                    // In wasm 1.1 the table idx can only be 0 for expansion using wasm 1.0, so the type of error reported is different
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_elem_kind;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(elem_kind_err);
            }

            // [elem_kind ...] ...
            // [     safe    ] unsafe (could be the section_end)
            //  ^^ section_curr

            fet.type = static_cast<decltype(fet.type)>(elem_kind);

            section_curr = reinterpret_cast<::std::byte const*>(elem_kind_next);

            // [elem_kind ...] ...
            // [     safe    ] unsafe (could be the section_end)
            //                 ^^ section_curr

            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_handle_element_type<Fs...>);

            section_curr = define_handler_element_type(sec_adl, fet.storage, fet.type, module_storage, section_curr, section_end, err, fs_para);

            // [elem_kind ... ...] (end)
            // [     safe        ] unsafe (could be the section_end)
            //                     ^^ section_curr

            elemsec.elems.push_back_unchecked(::std::move(fet));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check elem counter match
        if(elem_counter != elem_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = elem_counter;
            err.err_selectable.u32arr[1] = elem_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::element_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the element section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct element_section_storage_section_details_wrapper_t
    {
        element_section_storage_t<Fs...> const* element_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr element_section_storage_section_details_wrapper_t<Fs...> section_details(
        element_section_storage_t<Fs...> const& element_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    {
        return {::std::addressof(element_section_storage), ::std::addressof(all_sections)};
    }

    /// @brief Print the element section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, element_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       element_section_storage_section_details_wrapper_t<Fs...> const element_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(element_section_details_wrapper.element_section_storage_ptr == nullptr || element_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const element_section_span{element_section_details_wrapper.element_section_storage_ptr->sec_span};
        auto const element_section_size{static_cast<::std::size_t>(element_section_span.sec_end - element_section_span.sec_begin)};

        if(element_section_size)
        {
            auto const element_size{element_section_details_wrapper.element_section_storage_ptr->elems.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nElement[", element_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nElement[", element_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nElement[", element_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nElement[", element_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nElement[", element_size, U"]:\n");
            }

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 element_counter{};

            for(auto const& curr_element: element_section_details_wrapper.element_section_storage_ptr->elems)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    " - element[",
                                                                    element_counter,
                                                                    "]: ",
                                                                    section_details(curr_element, *element_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    L" - element[",
                                                                    element_counter,
                                                                    L"]: ",
                                                                    section_details(curr_element, *element_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    u8" - element[",
                                                                    element_counter,
                                                                    u8"]: ",
                                                                    section_details(curr_element, *element_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    u" - element[",
                                                                    element_counter,
                                                                    u"]: ",
                                                                    section_details(curr_element, *element_section_details_wrapper.all_sections_ptr));
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                    U" - element[",
                                                                    element_counter,
                                                                    U"]: ",
                                                                    section_details(curr_element, *element_section_details_wrapper.all_sections_ptr));
                }

                ++element_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
