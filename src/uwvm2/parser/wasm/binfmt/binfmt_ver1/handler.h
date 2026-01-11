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
 * @date        2025-04-09
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
# include <vector>
# include <algorithm>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include "section.h"
# include "def.h"
# include "handler_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::binfmt::ver1
{
    namespace details
    {
        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void
            handle_all_binfmt_ver1_extensible_section_impl(bool&,
                                                           ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>&,
                                                           ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const,
                                                           ::std::byte const* const,
                                                           ::std::byte const* const,
                                                           ::uwvm2::parser::wasm::base::error_impl&,
                                                           ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
                                                           ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t&,
                                                           ::std::byte const* const) noexcept
        {
            // Note that section_begin may be equal to section_end

            // func without template SecCurr used for adl matching
            // do nothing
        }

        /// @throw ::fast_io::error
        template <typename SecCurr, typename... Secs, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void handle_all_binfmt_ver1_extensible_section_impl(
            bool& success,
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const section_id,
            ::std::byte const* const section_begin,
            ::std::byte const* const section_end,
            ::uwvm2::parser::wasm::base::error_impl& err,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
            ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
        {
            // Note that section_begin may be equal to section_end

            // Check if the current section has a handler function
            static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_section_id_and_handle_binfmt_ver1_extensible_section_define<SecCurr, Fs...>);

            /// @details    This can be optimized to jump tables, which is fine under llvm, not gcc or msvc.
            /// @see        test\0001.parser\0002.binfmt1\handle_all_binfmt_ver1_extensible_section_test_jmp_table.cc

            if(section_id == SecCurr::section_id)
            {
                handle_binfmt_ver1_extensible_section_define(::uwvm2::parser::wasm::concepts::feature_reserve_type<::std::remove_cvref_t<SecCurr>>,
                                                             module_storage,
                                                             section_begin,
                                                             section_end,
                                                             err,
                                                             fs_para,
                                                             wasm_order,
                                                             sec_id_module_ptr);
                success = true;
            }
            else
            {
                if constexpr(sizeof...(Secs) != 0)
                {
                    handle_all_binfmt_ver1_extensible_section_impl<Secs...>(success,
                                                                            module_storage,
                                                                            section_id,
                                                                            section_begin,
                                                                            section_end,
                                                                            err,
                                                                            fs_para,
                                                                            wasm_order,
                                                                            sec_id_module_ptr);
                }
            }
        }

    }  // namespace details

    /// @brief      handle all binfmt ver1 extensible section
    /// @throws     ::fast_io::error
    /// @see        test\0001.parser\0002.binfmt1\handle_all_binfmt_ver1_extensible_section.cc
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_all_binfmt_ver1_extensible_section(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> &
                                                                        module_storage,
                                                                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const section_id,
                                                                    ::std::byte const* const section_begin,
                                                                    ::std::byte const* const section_end,
                                                                    ::uwvm2::parser::wasm::base::error_impl& err,
                                                                    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
                                                                    ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
                                                                    ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
        // Note that section_begin may be equal to section_end

        // check is_series
        bool success{};

        // Avoid meaningless copies with references
#if __cpp_structured_bindings >= 202411L
        auto const& [... secs]{module_storage.sections};
        ::uwvm2::parser::wasm::binfmt::ver1::details::check_extensible_section_is_series<::std::remove_cvref_t<decltype(secs)>...>();

        // llvm can gen jump table here
        details::handle_all_binfmt_ver1_extensible_section_impl<::std::remove_cvref_t<decltype(secs)>...>(success,
                                                                                                          module_storage,
                                                                                                          section_id,
                                                                                                          section_begin,
                                                                                                          section_end,
                                                                                                          err,
                                                                                                          fs_para,
                                                                                                          wasm_order,
                                                                                                          sec_id_module_ptr);
#else
        [&]<typename... Secs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Secs...> const&) constexpr UWVM_THROWS
        {
            ::uwvm2::parser::wasm::binfmt::ver1::details::check_extensible_section_is_series<Secs...>();

            // llvm can gen jump table here
            details::handle_all_binfmt_ver1_extensible_section_impl<Secs...>(success,
                                                                             module_storage,
                                                                             section_id,
                                                                             section_begin,
                                                                             section_end,
                                                                             err,
                                                                             fs_para,
                                                                             wasm_order,
                                                                             sec_id_module_ptr);
        }(module_storage.sections);
#endif

        if(!success) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_section_id;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief This is a function that parses wasm binfmt ver1, and supports variable parameter template extensions.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    [[nodiscard]] inline constexpr wasm_binfmt_ver1_module_extensible_storage_t<Fs...> wasm_binfmt_ver1_handle_func(
        ::std::byte const* const module_begin,
        ::std::byte const* const module_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        if constexpr(sizeof...(Fs) == 0)
        {
            // If there is no feature then just check wasm format
            if(module_begin > module_end) [[unlikely]]
            {
                err.err_curr = module_begin;
                err.err_selectable.err_end = module_end;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_begin_pointer;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};

            ret.module_span.module_begin = module_begin;
            ret.module_span.module_end = module_end;

            ::std::byte const* module_curr{module_begin};

            // 00 61 73 6D 01 00 00 00 ...
            // unsafe (could be the module_end)
            // ^^ module_curr

            // Short-circuit summation, safe
            if(static_cast<::std::size_t>(module_end - module_curr) < 8uz || !::uwvm2::parser::wasm::binfmt::is_wasm_file_unchecked(module_curr)) [[unlikely]]
            {
                err.err_curr = module_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_wasm_file_format;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [00 61 73 6D 01 00 00 00] ...
            // [         safe          ] unsafe (could be the module_end)
            // ^^ module_curr

            return ret;
        }
        else
        {
            // Note that the pointer may have any two values, and since it will only subsequently check for not equal to rather than greater than, it is checked
            // here in one go

            if(module_begin > module_end) [[unlikely]]
            {
                err.err_curr = module_begin;
                err.err_selectable.err_end = module_end;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_begin_pointer;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};

            ret.module_span.module_begin = module_begin;
            ret.module_span.module_end = module_end;

            ::std::byte const* module_curr{module_begin};

            // 00 61 73 6D 01 00 00 00 sec_id sec_len ...
            // unsafe (could be the module_end)
            // ^^ module_curr

            // Short-circuit summation, safe
            if(static_cast<::std::size_t>(module_end - module_curr) < 8uz || !::uwvm2::parser::wasm::binfmt::is_wasm_file_unchecked(module_curr)) [[unlikely]]
            {
                err.err_curr = module_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_wasm_file_format;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [00 61 73 6D 01 00 00 00] sec_id sec_len ...
            // [         safe          ] unsafe (could be the module_end)
            // ^^ module_curr

            // uncheck binfmt version (module_curr + 4uz), user-selected, or auto-detect (in uwvm2/parser/wasm/binfmt/base/base.h)

            module_curr += 8uz;  // safe (8uz)

            // [00 61 73 6D 01 00 00 00] sec_id sec_len ...
            // [         safe          ] unsafe (could be the module_end)
            //                           ^^ module_curr

            // Note that module_curr may be equal to module_end: the wasm module may be empty (no sections),
            // which is valid per spec. In this case, skip the section loop and run final checks.
            if(module_curr == module_end) [[unlikely]]
            {
                if constexpr(has_final_check_handler<Fs...>)
                {
                    constexpr ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::binfmt::ver1::final_final_check_t<Fs...>>
                        final_adl{};
                    define_final_check(final_adl, ret, module_end, err, fs_para);
                }

                return ret;
            }

            // [00 61 73 6D 01 00 00 00 sec_id] sec_len ...
            // [         safe                 ] unsafe (could be the module_end)
            //                          ^^ module_curr

            // max_section_id_map_sec_id_t is used to store the maximum section ID and the corresponding section ID
            max_section_id_map_sec_id_t max_order{};

            // First loop with module_curr != module_end, so can use do-while.

            do
            {
                // existence of valid information

                // Each time loop, module_curr is less than module_end.

                // [... sec_id] sec_len ...
                // [   safe   ] unsafe (could be the module_end)
                //      ^^ module_curr

                auto const sec_id_module_ptr{module_curr};  // for error
                // [... sec_id] sec_len ...
                // [   safe   ] unsafe (could be the module_end)
                //      ^^ sec_id_module_ptr

                // get section type
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte sec_id;
                ::std::memcpy(::std::addressof(sec_id), module_curr, sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte));

                // Avoid high invalid byte problem for platforms with CHAR_BIT greater than 8
#if CHAR_BIT > 8
                sec_id = static_cast<decltype(sec_id)>(static_cast<::std::uint_least8_t>(sec_id) & 0xFFu);
#endif

                static_assert(sizeof(sec_id) == 1uz);
                // Size equal to one does not need to do little-endian conversion

                // All standard sections (except custom sections, id: 0) must be in canonical order.

                ::uwvm2::parser::wasm::binfmt::ver1::wasm_order_t curr_order_sec_id;  // No initialization necessary

                if constexpr(::uwvm2::parser::wasm::binfmt::ver1::has_section_id_sequential_mapping_table_define<Fs...>)
                {
                    constexpr auto& order_sec_id_table{
                        ::uwvm2::parser::wasm::binfmt::ver1::final_section_sequential_packer_t<Fs...>::section_id_sequential_mapping_table};
                    constexpr auto order_sec_id_table_size{order_sec_id_table.size()};
                    if(sec_id >= order_sec_id_table_size) [[unlikely]] { curr_order_sec_id = 0u; }
                    else
                    {
                        curr_order_sec_id = order_sec_id_table.index_unchecked(sec_id);
                    }
                }
                else
                {
                    curr_order_sec_id = sec_id;
                }

                if(curr_order_sec_id != 0u)
                {
                    // There is no need to check for duplicate (sec_id <= max_section_id) sections here,
                    // duplicate sections are checked inside the section parsing.

                    if(curr_order_sec_id < max_order.max_section_id) [[unlikely]]
                    {
                        err.err_curr = module_curr;
                        // Enter the original section ID here for easy identification.
                        err.err_selectable.u8arr[0] = sec_id;
                        err.err_selectable.u8arr[1] = max_order.max_section_id_map_sec_id;  // Used for output
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_section_canonical_order;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }

                    max_order.max_section_id = curr_order_sec_id;
                    max_order.max_section_id_map_sec_id = sec_id;  // Used for output
                }

                // get section length
                ++module_curr;

                // [... sec_id] sec_len ... sec_begin ... sec_id (sec_end)
                // [   safe   ] unsafe (could be the module_end)
                //              ^^ module_curr

                // Note that module_curr may be equal to module_end
                // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 sec_len;  // No initialization necessary
                auto const [sec_len_next, sec_len_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(module_curr),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(module_end),
                                                                                ::fast_io::mnp::leb128_get(sec_len))};

                if(sec_len_err != ::fast_io::parse_code::ok) [[unlikely]]
                {
                    err.err_curr = module_curr;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_section_length;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(sec_len_err);
                }

                // [... sec_id sec_len ...] sec_begin ... sec_id (sec_end)
                // [       safe           ] unsafe (could be the module_end)
                //             ^^ module_curr

                // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
                constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
                if constexpr(size_t_max < wasm_u32_max)
                {
                    // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                    if(sec_len > size_t_max) [[unlikely]]
                    {
                        err.err_curr = module_curr;
                        err.err_selectable.u64 = static_cast<::std::uint_least64_t>(sec_len);
                        err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                    }
                }

                // set curr to next
                module_curr = reinterpret_cast<::std::byte const*>(sec_len_next);

                // [... sec_id sec_len ...] sec_begin ... sec_id (sec_end)
                // [       safe           ] unsafe (could be the module_end)
                //                          ^^ module_curr

                // check length
                if(static_cast<::std::size_t>(module_end - module_curr) < static_cast<::std::size_t>(sec_len)) [[unlikely]]
                {
                    err.err_curr = module_curr;
                    err.err_selectable.u32 = sec_len;
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_section_length;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                // [... sec_id sec_len ... sec_begin ...] next_sec_id ...
                // [                safe                ] unsafe (could be the module_end)
                //                         ^^ module_curr

                auto const sec_end{module_curr + sec_len};
                // Safe memory space from [module_curr, sec_end)
                // [... sec_id sec_len ... sec_begin ...] next_sec_id ...
                // [                safe                ] unsafe (could be the module_end)
                //                         ^^ module_curr
                //                                        ^^ sec_len

                // Note that module_curr may be equal to sec_end (sec_len == 0), need check after call handle_all_binfmt_ver1_extensible_section
                handle_all_binfmt_ver1_extensible_section(ret, sec_id, module_curr, sec_end, err, fs_para, max_order, sec_id_module_ptr);

                module_curr = sec_end;

                // [... sec_id sec_len ... sec_begin ...] next_sec_id ...
                // [                safe                ] unsafe (could be the module_end)
                //                                        ^^ module_curr

                // No point in checking if dif, module_curr is less than module_end, after ++module_curr in the next round of loops possibly module_curr ==
                // module_curr will be checked in ::fast_io::parse_by_scan and throw ::fast_io::parse_code::end_of_file
            }
            while(
#if UWVM_HAS_BUILTIN(__builtin_expect_with_probability)
                __builtin_expect_with_probability(module_curr != module_end, true, ::uwvm2::parser::wasm::binfmt::ver1::probability_from_Fs<Fs...>)
#else
                module_curr != module_end
#endif
            );

            // (module_curr != module_end):
            // [... sec_id sec_len ... sec_begin ... next_sec_id] ...
            // [                safe                            ] unsafe (could be the module_end)
            //                                       ^^ module_curr
            // or
            // (module_curr == module_end):
            // [... sec_id sec_len ... sec_begin ...] (sec_end) ...
            // [                safe                ] unsafe (could be the module_end)
            //                                        ^^ module_curr

            if constexpr(has_final_check_handler<Fs...>)
            {
                constexpr ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::binfmt::ver1::final_final_check_t<Fs...>> final_adl{};
                define_final_check(final_adl, ret, module_end, err, fs_para);
            }

            return ret;
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
