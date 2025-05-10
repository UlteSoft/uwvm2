/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
# ifdef UWVM_TIMER
import uwvm2.utils.debug;
# endif
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1.section;
import uwvm2.parser.wasm.standard.wasm1.opcode;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import :def;
import :feature_def;
import :types;
#else
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <fast_io_dsal/vector.h>
# ifdef UWVM_TIMER
#  include <uwvm2/utils/debug/impl.h>
# endif
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct type_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::fast_io::u8string_view section_name{u8"Type"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::type_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::fast_io::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...>> types{};
    };

    /// @brief Define functions for value_type against wasm1 for checking value_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool define_check_value_type(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type value_type  // [adl] can be replaced
        ) noexcept
    {
        return uwvm2::parser::wasm::standard::wasm1::features::is_valid_value_type(value_type);
    }

    /// @brief      handle type_prefix: "functype"
    /// @details    Separate processing to facilitate reuse in subsequent expansion
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* handle_type_prefix_functype(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [... prefix] para_len ... para_begin ... res_len (para_end) ... res_begin ... prefix (res_end) ...
        // [   safe   ] unsafe (could be the section_end)
        //              ^^ section_curr

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        using value_type_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> const*;

        ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> ft{};

        // Add scope space to prevent subsequent access to variables that should not be accessed

        // parameter
        {
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 para_len{};
            auto const [next_para_len, err_para_len]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(para_len))};
            if(err_para_len != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_parameter_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(err_para_len);
            }

            // [... prefix para_len ...] para_begin ... res_len (para_end) ... res_begin ... prefix (res_end) ...
            // [           safe        ] unsafe (could be the section_end)
            //             ^^ section_curr

            section_curr = reinterpret_cast<::std::byte const*>(next_para_len);
            // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

            // [... prefix para_len ...] para_begin ... res_len (para_end) ... res_begin ... prefix (res_end) ...
            // [           safe        ] unsafe (could be the section_end)
            //                           ^^ section_curr

            if(static_cast<::std::size_t>(section_end - section_curr) < para_len) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = para_len;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_parameter_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [... prefix para_len ... para_begin ...] res_len (para_end) ... res_begin ... prefix (res_end) ...
            // [                 safe                 ] unsafe (could be the section_end)
            //                          ^^ section_curr

            // set parameters
            ft.parameter.begin = reinterpret_cast<value_type_const_may_alias_ptr>(section_curr);
            section_curr += para_len;

            // [... prefix para_len ... para_begin ...] res_len (para_end) ... res_begin ... prefix (res_end) ...
            // [                 safe                 ] unsafe (could be the section_end)
            //                                          ^^ section_curr
            //                          ^^ ft.parameter.begin
            //                                          ^^ ft.parameter.end

            ft.parameter.end = reinterpret_cast<value_type_const_may_alias_ptr>(section_curr);

            // check handler
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_check_value_type<Fs...>, "define_check_value_type(...) not found");
            // check parameters
            for(auto parameter_curr{ft.parameter.begin}; parameter_curr != ft.parameter.end; ++parameter_curr)
            {
                if(!define_check_value_type(sec_adl, *parameter_curr)) [[unlikely]]
                {
                    err.err_curr = reinterpret_cast<::std::byte const*>(parameter_curr);
                    err.err_selectable.u8 =
                        static_cast<::std::underlying_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>>>(*parameter_curr);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_value_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        // result
        {
            // [... prefix para_len ... para_begin ...] res_len (para_end) ... res_begin ... prefix (res_end) ...
            // [                 safe                 ] unsafe (could be the section_end)
            //                                          ^^ section_curr

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 result_len{};
            auto const [next_result_len, err_result_len]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(result_len))};
            if(err_result_len != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_result_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(err_result_len);
            }

            // [... prefix para_len ... para_begin ... res_len (para_end) ...] res_begin ... prefix (res_end) ...
            // [                           safe                              ] unsafe (could be the section_end)
            //                                         ^^ section_curr

            section_curr = reinterpret_cast<::std::byte const*>(next_result_len);
            // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

            // [... prefix para_len ... para_begin ... res_len (para_end) ...] res_begin ... prefix (res_end) ...
            // [                           safe                              ] unsafe (could be the section_end)
            //                                                                 ^^ section_curr

            /// @details    In the current version of WebAssembly, the length of the result type vector of a valid function type may be
            ///             at most 1. This restriction may be removed in future versions.
            /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.3
            constexpr bool allow_multi_value{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<Fs...>()};
            if constexpr(!allow_multi_value)
            {
                if(result_len > static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u)) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_value;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }

            if(static_cast<::std::size_t>(section_end - section_curr) < result_len) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = result_len;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_result_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [... prefix para_len ... para_begin ... res_len (para_end) ... res_begin ...] prefix (res_end) ...
            // [                           safe                                            ] unsafe (could be the section_end)
            //                                                                ^^ section_curr

            // set parameters
            ft.result.begin = reinterpret_cast<value_type_const_may_alias_ptr>(section_curr);
            section_curr += result_len;

            // [... prefix para_len ... para_begin ... res_len (para_end) ... res_begin ...] prefix (res_end) ...
            // [                           safe                                            ] unsafe (could be the section_end)
            //                                                                               ^^ section_curr
            //                                                                ^^ ft.result.begin
            //                                                                               ^^ ft.result.end

            ft.result.end = reinterpret_cast<value_type_const_may_alias_ptr>(section_curr);

            // check results
            for(auto result_curr{ft.result.begin}; result_curr != ft.result.end; ++result_curr)
            {
                if(!define_check_value_type(sec_adl, *result_curr)) [[unlikely]]
                {
                    err.err_curr = reinterpret_cast<::std::byte const*>(result_curr);
                    err.err_selectable.u8 =
                        static_cast<::std::underlying_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>>>(*result_curr);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_value_type;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
            }
        }

        // push back
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        typesec.types.push_back_unchecked(::std::move(ft));

        return section_curr;
    }

    /// @brief Define type_prefix for wasm1 Functions for checking type_prefix
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_type_prefix_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::function_type_prefix prefix,  // [adl] can be replaced
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::std::byte const* const prefix_module_ptr) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked

        // [... prefix] ...
        // [   safe   ] unsafe (could be the section_end)
        //              ^^ section_curr

        switch(prefix)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::function_type_prefix::functype:
            {
                // Note that section_curr may be equal to section_end, which needs to be checked
                return handle_type_prefix_functype(sec_adl, module_storage, section_curr, section_end, err, fs_para);
            }
            default:
                [[unlikely]]
                {
                    err.err_curr = prefix_module_ptr;
                    err.err_selectable.u8 =
                        static_cast<::std::underlying_type_t<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>>>(prefix);
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_prefix;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
        }
    }

    /// @brief Define the handler function for type_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<type_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse type section (id: 1)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get type_section_storage_t from storages
        auto& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

        // check duplicate
        if(typesec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = typesec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        typesec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        typesec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | type_count ... prefix ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_count{};

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [type_count_next, type_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(type_count))};

        if(type_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_count_err);
        }

        // [before_section ... | type_count ...] prefix ...
        // [             safe                  ] unsafe (could be the section_end)
        //                       ^^ section_curr

        typesec.types.reserve(type_count);

        // Since all subsequent ones are pushback_unchecked, the capacity will not grow, so end is used to denote the end of the buffer
        auto const typesec_types_end{typesec.types.imp.end_ptr};

        // Since the internal end is to be used later, check to see if it is the size of the counter
        UWVM_ASSERT(static_cast<::std::size_t>(typesec_types_end - typesec.types.cbegin()) == type_count);

        section_curr = reinterpret_cast<::std::byte const*>(type_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | type_count ...] prefix ...
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        while(section_curr != section_end)
        {
            // Ensuring the existence of valid information

            // [... prefix] ...
            // [   safe   ] unsafe (could be the section_end)
            //      ^^ section_curr

            // check type counter
            // Ensure content is available before counting (section_curr != section_end)
            if(typesec.types.cend() == typesec_types_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = type_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::type_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const prefix_module_ptr{section_curr};
            // [... prefix] ...
            // [   safe   ] unsafe (could be the section_end)
            //      ^^ prefix_module_ptr

            ::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<Fs...> prefix{};

            // no necessary to check, because (section_curr != section_end)
            ::fast_io::freestanding::my_memcpy(::std::addressof(prefix), section_curr, sizeof(prefix));
            // set section_curr to next
            // No sense check, never cross the line because (section_curr < section_end)

            static_assert(sizeof(prefix) == 1);
            // Size equal to one does not need to do little-endian conversion

            ++section_curr;
            // [... prefix] ...
            // [   safe   ] unsafe (could be the section_end)
            //              ^^ section_curr

            // Note that maybe (section_curr == section_end) now, follow-up needs to be checked (in define_type_prefix_handler)

            // check has func
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_type_prefix_handler<Fs...>, "define_type_prefix_handler(...) not found");

            // handle it
            section_curr = define_type_prefix_handler(sec_adl, prefix, module_storage, section_curr, section_end, err, fs_para, prefix_module_ptr);
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check type counter match
        if(typesec.types.cend() != typesec_types_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = static_cast<::std::uint_least32_t>(typesec.types.size());
            err.err_selectable.u32arr[1] = type_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::type_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
}  // namespace uwvm2::parser::wasm::standard::wasm1::features

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
