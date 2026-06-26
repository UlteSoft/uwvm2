/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Data segment flags 0, 1, and 2
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
# include "def.h"
# include "feature_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif


UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    namespace wasm1p1_data_details
    {
        /// @brief Parse a constant expression that must produce an i32 memory offset.
        /// @details This helper is shared by active data segments and active element segments.
        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const*
            parse_and_check_i32_const_expr_valid(::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...>& expr,
                                                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                                 ::std::byte const* section_curr,
                                                 ::std::byte const* const section_end,
                                                 ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
        {
            auto const& importsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
            constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
            static_assert(importdesc_count > 3uz);
            // importdesc has at least four buckets by static_assert; bucket 3 is the global-import bucket.
            auto const& imported_global{importsec.importdesc.index_unchecked(3uz)};
            auto const& globalsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t<Fs...>>(module_storage.sections)};
            auto const imported_global_size{imported_global.size()};
            auto const all_global_size{imported_global_size + globalsec.local_globals.size()};

            expr.begin = section_curr;
            bool has_data_on_type_stack{};
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

                switch(opcode)
                {
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                    {
                        has_data_on_type_stack = true;
                        // section_curr points at the one-byte i32.const opcode already proven safe by the opcode read.
                        // Pointer move: advance to the beginning of the LEB128 immediate.
                        ++section_curr;

                        // [before_expr ... opcode] i32_leb ... expr_tail ... end ... (section_end)
                        // [         safe         ] unsafe (could be the section_end)
                        //                         ^^ section_curr
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

                        // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                        // Proof view before moving section_curr: section_curr still points at the first checked byte, and next marks the first byte after it.
                        // Pointer move: advance section_curr to the first byte after the checked i32 LEB128 immediate.

                        // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                        // [               safe               ] unsafe (could be the section_end)
                        //                         ^^ section_curr

                        section_curr = reinterpret_cast<::std::byte const*>(next);

                        // [before_expr ... opcode i32_leb ...] expr_tail ... end ... (section_end)
                        // [               safe               ] unsafe (could be the section_end)
                        //                                      ^^ section_curr
                        expr.opcodes.reserve(1uz);
                        expr.opcodes.emplace_back_unchecked(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.i32 = value},
                                                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const);
                        break;
                    }
                    case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                        ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                    {
                        has_data_on_type_stack = true;
                        // section_curr points at the one-byte global.get opcode already proven safe by the opcode read.
                        // Pointer move: advance to the beginning of the LEB128 global index.
                        ++section_curr;

                        // [before_expr ... opcode] globalidx_leb ... expr_tail ... end ... (section_end)
                        // [         safe         ] unsafe (could be the section_end)
                        //                         ^^ section_curr
                        //
                        // parse_by_scan bounds-checks the LEB128 global index before it is used for index validation.
                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_idx;
                        auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                         reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                         ::fast_io::mnp::leb128_get(global_idx))};
                        if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_illegal_data;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                        }

                        // [before_expr ... opcode globalidx_leb ...] expr_tail ... end ... (section_end)
                        // [         safe                           ] unsafe (could be the section_end)
                        //                         ^^ section_curr

                        auto const global_idx_uz{static_cast<::std::size_t>(global_idx)};
                        if(global_idx_uz >= all_global_size) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_global_size);
                            err.err_selectable.u32arr[1] = global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_illegal_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte global_type_byte{};
                        bool global_is_mutable{};

                        if(global_idx_uz < imported_global_size)
                        {
                            auto const curr_imported_global_ptr{imported_global.index_unchecked(global_idx_uz)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            if(curr_imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                            auto const& curr_imported_global{curr_imported_global_ptr->imports.storage.global};
                            global_type_byte = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_imported_global.type);
                            global_is_mutable = curr_imported_global.is_mutable;
                        }
                        else
                        {
                            auto const& curr_defined_global{globalsec.local_globals.index_unchecked(global_idx_uz - imported_global_size).global};
                            global_type_byte = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(curr_defined_global.type);
                            global_is_mutable = curr_defined_global.is_mutable;
                        }

                        if(global_type_byte != static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u8arr[0] = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(
                                ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
                            err.err_selectable.u8arr[1] = global_type_byte;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_type_mismatch;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        if(global_is_mutable) [[unlikely]]
                        {
                            err.err_curr = section_curr;
                            err.err_selectable.u32 = global_idx;
                            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::init_const_expr_ref_mutable_imported_global;
                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                        }

                        // parse_by_scan succeeded, so [section_curr, next) is now proven safe and next is inside [section_curr, section_end].
                        // Proof view before moving section_curr: section_curr still points at the first checked byte, and next marks the first byte after it.
                        // Pointer move: advance section_curr to the first byte after the checked global index.
                        section_curr = reinterpret_cast<::std::byte const*>(next);

                        // [before_expr ... opcode globalidx_leb ...] expr_tail ... end ... (section_end)
                        // [                  safe                  ] unsafe (could be the section_end)
                        //                                           ^^ section_curr
                        expr.opcodes.reserve(1uz);
                        expr.opcodes.emplace_back_unchecked(
                            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_opcode_storage_u{.global_idx = global_idx},
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get);
                        break;
                    }
                    [[unlikely]] default:
                    {
                        /// @warning Extension point: a new const-expression opcode for data offsets must be parsed and validated before this fallback.
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

            // section_curr has been advanced past the checked end opcode, so the whole expression byte range is safe.
            // [before_expr ... expr ... end] tail ... (section_end)
            // [            safe           ] unsafe (could be the section_end)
            //                              ^^ section_curr
            expr.end = section_curr;
            return section_curr;
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
            get_all_memory_count(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage) noexcept
        {
            auto const& importsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
            constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
            static_assert(importdesc_count > 2uz);
            auto const& memorysec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section_storage_t<Fs...>>(module_storage.sections)};
            auto const defined_memory_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(memorysec.memories.size())};
            // importdesc has at least three buckets by static_assert; bucket 2 is the memory-import bucket.
            auto const imported_memory_size{
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(2uz).size())};
            return static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_memory_size + imported_memory_size);
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::std::byte const* parse_data_bytes(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_storage_t<Fs...>& data_storage,
                                                             ::std::byte const* section_curr,
                                                             ::std::byte const* const section_end,
                                                             ::uwvm2::parser::wasm::base::error_impl& err) UWVM_THROWS
        {
            using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;
            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            // [before_data_bytes ...] byte_size ... byte_begin ... (section_end)
            // [          safe       ] unsafe (could be the section_end)
            //                        ^^ section_curr
            //
            // byte_size is a bounded LEB128 read; parse_by_scan never reads past section_end.
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 byte_size_count;
            auto const [byte_size_count_next, byte_size_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                            ::fast_io::mnp::leb128_get(byte_size_count))};
            if(byte_size_count_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_data_byte_size_count;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(byte_size_count_err);
            }

            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                if(byte_size_count > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(byte_size_count);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            // parse_by_scan succeeded, so [section_curr, byte_size_count_next) is now proven safe and byte_size_count_next is inside [section_curr,
            // section_end]. Proof view before moving section_curr: section_curr still points at the byte-size LEB128, and byte_size_count_next marks the
            // payload start. Pointer move: advance section_curr to the first byte of the data payload.
            section_curr = reinterpret_cast<::std::byte const*>(byte_size_count_next);

            // [before_data_bytes ... byte_size ...] byte_begin ... (section_end)
            // [               safe                ] unsafe (could be the section_end)
            //                                      ^^ section_curr

            // The subtraction is safe because section_curr was produced inside [section_begin, section_end].
            if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(byte_size_count)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = byte_size_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_data_byte_size_count;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // The length check above proves [section_curr, section_curr + byte_size_count) is safe inside the current section.
            // Pointer storage: byte.begin records the beginning of that checked byte range.

            // [before_data_bytes ... byte_size ...] byte_begin ... byte_end ... (section_end)
            // [               safe                ] [      safe      ] unsafe
            //                                      ^^ section_curr
            //                                      ^^ data_storage.byte.begin

            data_storage.byte.begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_curr);
            // Pointer move: advance by byte_size_count bytes, which were proven safe by the length check above.
            section_curr += byte_size_count;
            // Pointer storage: byte.end records the one-past-end pointer of the checked byte range.
            data_storage.byte.end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_curr);

            // [before_data_bytes ... byte_size ... byte_begin ... byte_end] tail ... (section_end)
            // [                          safe                            ] unsafe (could be the section_end)
            //                                                             ^^ section_curr
            //                                      ^^ data_storage.byte.begin
            //                                                             ^^ data_storage.byte.end

            return section_curr;
        }
    }  // namespace wasm1p1_data_details

    /// @brief Parse one wasm1.1 data segment according to its leading flag.
    /// @details Flags 1 and 2 are accepted only when bulk memory is enabled; all payload reads are bounded by section_end.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_handler_data_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<data_section_storage_t<Fs...>> sec_adl,
        decltype(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>{}.storage)& fdt_storage,
        decltype(::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>{}.type) const fdt_type,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto& data_storage{fdt_storage.segment};
        auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        auto const all_memory_size{wasm1p1_data_details::get_all_memory_count(module_storage)};

        switch(fdt_type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_type_t::active_implicit:
            {
                data_storage.active = true;
                data_storage.memory_idx = 0u;
                if(data_storage.memory_idx >= all_memory_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32arr[0] = data_storage.memory_idx;
                    err.err_selectable.u32arr[1] = all_memory_size;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::data_memory_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // parse_and_check_i32_const_expr_valid checks the whole i32 constant expression against section_end and returns its end pointer.
                // Pointer move: replace section_curr with the first byte after the checked offset expression.
                section_curr = wasm1p1_data_details::parse_and_check_i32_const_expr_valid(data_storage.expr, module_storage, section_curr, section_end, err);

                // [before_data_payload ... offset_expr ...] byte_size ... byte_begin ... (section_end)
                // [                 safe                  ] unsafe (could be the section_end)
                //                                           ^^ section_curr
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_type_t::passive:
            {
                if(!para.enable_bulk_memory) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.wasm1p1_feature_required.value = static_cast<wasm_u32>(fdt_type);
                    err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory;
                    err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                data_storage.active = false;
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_type_t::active_explicit:
            {
                if(!para.enable_bulk_memory) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.wasm1p1_feature_required.value = static_cast<wasm_u32>(fdt_type);
                    err.err_selectable.wasm1p1_feature_required.feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory;
                    err.err_selectable.wasm1p1_feature_required.subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                data_storage.active = true;

                // [before_data_payload ...] memoryidx ... expr ... byte_size ... byte_begin ... (section_end)
                // [          safe         ] unsafe (could be the section_end)
                //                          ^^ section_curr

                // memoryidx is a bounded LEB128 read; parse_by_scan never reads past section_end.
                wasm_u32 memory_idx;
                auto const [memory_idx_next, memory_idx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                      ::fast_io::mnp::leb128_get(memory_idx))};
                if(memory_idx_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_data_memory_idx;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(memory_idx_err);
                }

                // [before_data_payload ... memoryidx ...] expr ... byte_size ... byte_begin ... (section_end)
                // [          safe                       ] unsafe (could be the section_end)
                //                          ^^ section_curr

                if(memory_idx >= all_memory_size) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u32arr[0] = memory_idx;
                    err.err_selectable.u32arr[1] = all_memory_size;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::data_memory_index_exceeds_maxvul;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                data_storage.memory_idx = memory_idx;
                // parse_by_scan succeeded, so [section_curr, memory_idx_next) is now proven safe and memory_idx_next is inside [section_curr, section_end].
                // Proof view before moving section_curr: section_curr still points at the memoryidx LEB128, and memory_idx_next marks the offset expression.
                // Pointer move: advance section_curr to the first byte after the checked memoryidx.

                section_curr = reinterpret_cast<::std::byte const*>(memory_idx_next);

                // [before_data_payload ... memoryidx ...] expr ... byte_size ... byte_begin ... (section_end)
                // [                safe                 ] unsafe (could be the section_end)
                //                                        ^^ section_curr

                // parse_and_check_i32_const_expr_valid checks the whole i32 constant expression against section_end and returns its end pointer.
                // Pointer move: replace section_curr with the first byte after the checked offset expression.
                section_curr = wasm1p1_data_details::parse_and_check_i32_const_expr_valid(data_storage.expr, module_storage, section_curr, section_end, err);

                // [before_data_payload ... memoryidx ... offset_expr ...] byte_size ... byte_begin ... (section_end)
                // [                         safe                       ] unsafe (could be the section_end)
                //                                                      ^^ section_curr
                break;
            }
            [[unlikely]] default:
            {
                /// @warning Extension point: add new wasm1p1_data_type_t flags here before accepting them in the parser.
                err.err_curr = section_curr;
                err.err_selectable.u32 = static_cast<wasm_u32>(fdt_type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_invalid_data_segment_flag;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        // parse_data_bytes checks byte_size and the payload against section_end before returning the first byte after the data segment.
        return wasm1p1_data_details::parse_data_bytes(data_storage, section_curr, section_end, err);
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
