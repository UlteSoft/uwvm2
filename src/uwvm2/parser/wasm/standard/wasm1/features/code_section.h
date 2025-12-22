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
 * @date        2025-07-07
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
# include <uwvm2/utils/intrinsics/impl.h>
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
    struct code_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Code"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::code_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...>> codes{};
    };

    /// @brief Define functions for value_type against wasm1 for checking value_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_codesec_value_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type value_type  // [adl] can be replaced
        ) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::is_valid_value_type(value_type); }

    /// @brief Define the handler function for code_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<code_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse code section (id: 10)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get code_section_storage_t from storages
        auto& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<code_section_storage_t<Fs...>>(module_storage.sections)};

        // function section
        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        auto const defined_func_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};

        // check duplicate
        if(codesec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = codesec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        codesec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        codesec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | code_count ... code1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 code_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [code_count_next, code_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(code_count))};

        if(code_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_code_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(code_count_err);
        }

        // [before_section ... | code_count ...] code1 ...
        // [                 safe              ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // Check that the number of codes is the same as the number of defined functions
        // Since the wasm section is sequential, the function must appear before or not appear in the global section. Here, only one verification is required.
        if(code_count != defined_func_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = code_count;
            err.err_selectable.u32arr[1] = defined_func_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::code_ne_defined_func;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            // The code and function share the same limit.

            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(code_count) > parser_limit.max_code_sec_codes) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"codesec_codes";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(code_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_code_sec_codes;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        codesec.codes.reserve(static_cast<::std::size_t>(code_count));

        section_curr = reinterpret_cast<::std::byte const*>(code_count_next);  // never out of bounds

        // [before_section ... | code_count ...] code1 ...
        // [                 safe              ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 code_counter{};  // use for check

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [ ... body_size] ... local_count ... locals ...
            // [     safe     ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check table counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(code_counter, section_curr, err) > code_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = code_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::code_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage table (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_code_t<Fs...> code{};

            // [ ... body_size] ... local_count ... locals ...
            // [     safe     ] unsafe (could be the section_end)
            //       ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 body_size;

            auto const [body_size_next, body_size_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                ::fast_io::mnp::leb128_get(body_size))};

            if(body_size_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_code_body_size;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(body_size_err);
            }

            // [ ... body_size ...] local_count(code_body_begin) ... ...
            // [        safe      ] unsafe (could be the section_end)
            //       ^^ section_curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(body_size > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(body_size);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            section_curr = reinterpret_cast<::std::byte const*>(body_size_next);

            // [ ... body_size ...] local_count(code_body_begin) ... ...
            // [        safe      ] unsafe (could be the section_end)
            //                      ^^ section_curr

            if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(body_size)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = body_size;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_code_body_size;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ... body_size ...] local_count(code_body_begin) ... ...] (code_body_end)
            // [        safe      ]        .....                        ] unsafe (could be the section_end)
            //                      ^^ section_curr

            code.body.code_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_curr);

            // [ ... body_size ...] local_count(code_body_begin) ... ...] (code_body_end)
            // [        safe      ]        .....                        ] unsafe (could be the section_end)
            //                      ^^ code.body.code_begin

            // Subsequent boundary adjustments section_end to code_end
            auto const code_end{section_curr + body_size};

            // Prefetching makes the next round of processing faster
            ::uwvm2::utils::intrinsics::universal::prefetch<::uwvm2::utils::intrinsics::universal::pfc_mode::read,
                                                            ::uwvm2::utils::intrinsics::universal::pfc_level::L2,
                                                            ::uwvm2::utils::intrinsics::universal::ret_policy::keep>(code_end);

            code.body.code_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(code_end);

            // [ ... body_size ...] local_count(code_body_begin) ... ...] (code_body_end)
            // [        safe      ]        .....                        ] unsafe (could be the section_end)
            //                                                            ^^ code_end
            //                                                            ^^ code.body.code_end

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_count;

            auto const [local_count_next, local_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                    ::fast_io::mnp::leb128_get(local_count))};

            if(local_count_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_local_count;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_count_err);
            }

            // [ ... body_size ... local_count(code_body_begin) ...] clocal_n ... clocal_type next_clocal_n ... code ...]
            // [                     safe                          ]   ......                                           ] unsafe
            //                     ^^ section_curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(local_count > size_t_max) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u64 = static_cast<::std::uint_least64_t>(local_count);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            if constexpr((::std::same_as<wasm1, Fs> || ...))
            {
                auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
                auto const& parser_limit{wasm1_feapara_r.parser_limit};
                if(static_cast<::std::size_t>(local_count) > parser_limit.max_code_locals) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.exceed_the_max_parser_limit.name = u8"code_locals";
                    err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(local_count);
                    err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_code_locals;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            code.locals.reserve(static_cast<::std::size_t>(local_count));

            section_curr = reinterpret_cast<::std::byte const*>(local_count_next);

            // [ ... body_size ... local_count(code_body_begin) ...] clocal_n ... clocal_type next_clocal_n ... code ...]
            // [                     safe                          ]   ......                                           ] unsafe
            //                                                       ^^ section_curr

            // The final list of local variables obtained by concatenating all groups must not exceed u32max.
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 all_clocal_counter{};

            // Undeterministic boundaries need to use quantity loops (No local_end)
            for(::std::size_t local_counter{}; local_counter != local_count; ++local_counter)
            {
                ::uwvm2::parser::wasm::standard::wasm1::features::final_local_entry_t<Fs...> fle{};

                // [ ... body_size ... local_count(code_body_begin) ...] clocal_n ... clocal_type next_clocal_n ... code ...]
                // [                     safe                          ]    ......                                          ] unsafe
                //                                                       ^^ section_curr

                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 clocal_n;

                auto const [clocal_n_next, clocal_n_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                  ::fast_io::mnp::leb128_get(clocal_n))};

                if(clocal_n_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_clocal_n;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(clocal_n_err);
                }

                // [ ... body_size ... local_count(code_body_begin) ... clocal_n ...] clocal_type next_clocal_n ... code ...]
                // [                             safe                               ]    ......                             ] unsafe
                //                                                      ^^ section_curr

                // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if constexpr(size_t_max < wasm_u32_max)
                {
                    // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                    if(clocal_n > size_t_max) [[unlikely]]
                    {
                        err.err_curr = section_curr;
                        err.err_selectable.u64 = static_cast<::std::uint_least64_t>(clocal_n);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                // The final list of local variables obtained by concatenating all groups must not exceed u32max.
                if(clocal_n > wasm_u32_max - all_clocal_counter) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::final_list_of_locals_exceeds_the_maximum_value_of_u32max;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                all_clocal_counter += clocal_n;

                fle.count = clocal_n;

                section_curr = reinterpret_cast<::std::byte const*>(clocal_n_next);

                // [ ... body_size ... local_count(code_body_begin) ... clocal_n ...] clocal_type next_clocal_n ... code ...]
                // [                             safe                               ]    ......                             ] unsafe
                //                                                                    ^^ section_curr

                if(section_curr == code_end) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::code_missing_local_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // [ ... body_size ... local_count(code_body_begin) ... clocal_n ... clocal_type] next_clocal_n ... code ...]
                // [                                   safe                                     ]  ......                   ] unsafe
                //                                                                   ^^ section_curr

                ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> fvt;

                ::std::memcpy(::std::addressof(fvt), section_curr, sizeof(::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>));

                // Size equal to one does not need to do little-endian conversion
                static_assert(sizeof(fvt) == 1uz);

                // check fvt
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_check_codesec_value_type<Fs...>);
                if(!define_check_codesec_value_type(sec_adl, fvt)) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_selectable.u8 = static_cast<::std::uint_least8_t>(
                        static_cast<::std::underlying_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>>>(fvt));
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_value_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                fle.type = fvt;

                ++section_curr;

                // [ ... body_size ... local_count(code_body_begin) ... clocal_n ... clocal_type] next_clocal_n ... code ...]
                // [                                   safe                                     ] ......                    ] unsafe
                //                                                                                ^^ section_curr

                code.locals.push_back_unchecked(::std::move(fle));
            }

            code.all_local_count = all_clocal_counter;

            code.body.expr_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_curr);

            // [ ...] [(expr_begin) ...] (code_end)
            // [safe] [     safe       ] unsafe (could be the section_end)
            //         ^^ section_curr

            // minimum check end (0x0B)
            // At least one byte, with the last byte being 0x0B.
            if(section_curr == code_end) [[unlikely]]
            {
                // Equivalent to "static_cast<::std::size_t>(section_curr - code_end) < 1uz"
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::missing_code_body_end;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [ ...] [(expr_begin) ...] [0x0B] (code_end)
            // [safe] [   scope safe   ] [safe] unsafe (could be the section_end)
            //         ^^ section_curr

            auto const code_end_ptr{reinterpret_cast<::std::byte const*>(code_end) - 1u};

            // [ ...] [(expr_begin) ...] [0x0B] (code_end)
            // [safe] [   scope safe   ] [safe] unsafe (could be the section_end)
            //                            ^^ code_end_ptr

            if(*code_end_ptr != static_cast<::std::byte>(::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::end)) [[unlikely]]
            {
                err.err_curr = code_end_ptr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::missing_code_body_end;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // Parsing of the expression will be completed later.

            section_curr = reinterpret_cast<::std::byte const*>(code_end);

            // Subsequent boundary adjustments code_end to section_end

            // [ ...] (section_end)
            // or
            // [ ...] (code_end)
            // [safe] unsafe (could be the section_end)
            //        ^^ section_curr

            codesec.codes.push_back_unchecked(::std::move(code));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check code counter match
        if(code_counter != code_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = code_counter;
            err.err_selectable.u32arr[1] = code_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::code_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the code section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct code_section_storage_section_details_wrapper_t
    {
        code_section_storage_t<Fs...> const* code_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr code_section_storage_section_details_wrapper_t<Fs...> section_details(
        code_section_storage_t<Fs...> const& code_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(code_section_storage), ::std::addressof(all_sections)}; }

    /// @brief Print the code section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, code_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       code_section_storage_section_details_wrapper_t<Fs...> const code_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(code_section_details_wrapper.code_section_storage_ptr == nullptr || code_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const code_section_span{code_section_details_wrapper.code_section_storage_ptr->sec_span};
        auto const code_section_size{static_cast<::std::size_t>(code_section_span.sec_end - code_section_span.sec_begin)};

        if(code_section_size)
        {
            auto const code_size{code_section_details_wrapper.code_section_storage_ptr->codes.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nCode[", code_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nCode[", code_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nCode[", code_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nCode[", code_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nCode[", code_size, U"]:\n");
            }

            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                *code_section_details_wrapper.all_sections_ptr)};
            static_assert(importsec.importdesc_count > 0uz);
            auto func_counter{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(0uz).size())};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 code_counter{};

            for(auto const& curr_code: code_section_details_wrapper.code_section_storage_ptr->codes)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     " - code[",
                                                                     code_counter,
                                                                     "] -> func[",
                                                                     func_counter,
                                                                     "]: {all size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.code_begin),
                                                                     ", local count: ",
                                                                     curr_code.all_local_count,
                                                                     ", expr size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.expr_begin),
                                                                     "}\n");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L" - code[",
                                                                     code_counter,
                                                                     L"] -> func[",
                                                                     func_counter,
                                                                     L"]: {all size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.code_begin),
                                                                     L", local count: ",
                                                                     curr_code.all_local_count,
                                                                     L", expr size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.expr_begin),
                                                                     L"}\n");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8" - code[",
                                                                     code_counter,
                                                                     u8"] -> func[",
                                                                     func_counter,
                                                                     u8"]: {all size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.code_begin),
                                                                     u8", local count: ",
                                                                     curr_code.all_local_count,
                                                                     u8", expr size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.expr_begin),
                                                                     u8"}\n");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u" - code[",
                                                                     code_counter,
                                                                     u"] -> func[",
                                                                     func_counter,
                                                                     u"]: {all size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.code_begin),
                                                                     u", local count: ",
                                                                     curr_code.all_local_count,
                                                                     u", expr size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.expr_begin),
                                                                     u"}\n");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U" - code[",
                                                                     code_counter,
                                                                     U"] -> func[",
                                                                     func_counter,
                                                                     U"]: {all size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.code_begin),
                                                                     U", local count: ",
                                                                     curr_code.all_local_count,
                                                                     U", expr size: ",
                                                                     static_cast<::std::size_t>(curr_code.body.code_end - curr_code.body.expr_begin),
                                                                     U"}\n");
                }

                ++func_counter;
                ++code_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
