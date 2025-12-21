/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <tuple>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::section_detail
{
    struct section_details_t
    {
        ::uwvm2::uwvm::wasm::type::all_module_t const* all_module_ptr{};
    };

    inline constexpr section_details_t section_details(::uwvm2::uwvm::wasm::type::all_module_t const& module_storage) noexcept
    {
        return {::std::addressof(module_storage)};
    }

    template <typename T>
    inline constexpr decltype(auto) section_details_adl_caller(T && t) noexcept
    {
        // C++ Rule: Additional ADL occurs only when the unqualified lookup resolves to a “function name (or function template name)”. If the unqualified name
        // resolves to a non-function entity such as a “variable/object/type name”, no further ADL is performed. If section_details is used directly. Here, the
        // unqualified name section_details has already been bound to the “formal parameter variable,” so ADL will not include the section_details intended for
        // the call in the candidate set.
        return section_details(::std::forward<T>(t));
    }

    /// @brief Print the section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, section_details_t>, Stm && stream, section_details_t const section_details)
    {
        if(section_details.all_module_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& module_storage{*section_details.all_module_ptr};

        switch(module_storage.type)
        {
            case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(module_storage.module_storage_ptr.wf == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

                switch(module_storage.module_storage_ptr.wf->binfmt_ver)
                {
                    case 1u:
                    {
                        // binfmt ver 1
                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                "======================\n" "Executed Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                "\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                "\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                "\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                "\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                L"======================\n" L"Executed Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                L"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                L"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                L"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                L"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                u8"======================\n" u8"Executed Module Name: ",
                                module_storage.module_storage_ptr.wf->module_name,
                                u8"\nFile Path: ",
                                module_storage.module_storage_ptr.wf->file_name,
                                u8"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                u8"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                u8"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                u"======================\n" u"Executed Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                u"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                u"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                u"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                u"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                U"======================\n" U"Executed Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                U"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                U"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                U"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                U"\n\n");
                        }

                        break;
                    }
                    default:
                    {
                        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                        /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }

                break;
            }
            case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(module_storage.module_storage_ptr.wf == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
                switch(module_storage.module_storage_ptr.wf->binfmt_ver)
                {
                    // binfmt ver 1
                    case 1u:
                    {
                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                "======================\n" "Preloaded Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                "\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                "\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                "\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                "\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                L"======================\n" L"Preloaded Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                L"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                L"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                L"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                L"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                u8"======================\n" u8"Preloaded Module Name: ",
                                module_storage.module_storage_ptr.wf->module_name,
                                u8"\nFile Path: ",
                                module_storage.module_storage_ptr.wf->file_name,
                                u8"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                u8"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                u8"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                u"======================\n" u"Preloaded Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                u"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                u"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                u"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                u"\n\n");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(
                                ::std::forward<Stm>(stream),
                                U"======================\n" U"Preloaded Module Name: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->module_name),
                                U"\nFile Path: ",
                                ::fast_io::mnp::code_cvt(module_storage.module_storage_ptr.wf->file_name),
                                U"\nBinary Format Version: ",
                                module_storage.module_storage_ptr.wf->binfmt_ver,
                                U"\n",
                                section_details_adl_caller(module_storage.module_storage_ptr.wf->get_curr_binfmt_version_wasm_storage<1u>()),
                                U"\n\n");
                        }

                        break;
                    }
                    default:
                    {
                        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                        /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }

                break;
            }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(module_storage.module_storage_ptr.wd == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                auto const& wd{*module_storage.module_storage_ptr.wd};

                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                constexpr auto get_final_value_type_from_tuple{
                    []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(::uwvm2::utils::container::tuple<Fs...>) consteval noexcept
                    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>{}; }};

                using curr_value_type = ::std::remove_cvref_t<decltype(get_final_value_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features))>;
                static_assert(sizeof(curr_value_type) == 1uz, "curr_value_type is not 1 byte");

                // module information + func size

                auto const func_begin{wd.wasm_dl_storage.capi_function_vec.function_begin};
                auto const func_size{wd.wasm_dl_storage.capi_function_vec.function_size};

                if(func_begin == nullptr && func_size != 0uz) [[unlikely]]
                {
                    // If the function is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a non-zero uz, a
                    // memory safety issue will arise.
                    ::fast_io::fast_terminate();
                }

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "======================\nPreloaded DL Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wd.module_name),
                                                                     "\nFile Path: ",
                                                                     ::fast_io::mnp::code_cvt(wd.file_name),
                                                                     "\nFunction[",
                                                                     func_size,
                                                                     "]:\n");
                }
                if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"======================\nPreloaded DL Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wd.module_name),
                                                                     L"\nFile Path: ",
                                                                     ::fast_io::mnp::code_cvt(wd.file_name),
                                                                     L"\nFunction[",
                                                                     func_size,
                                                                     L"]:\n");
                }
                if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"======================\nPreloaded DL Module Name: ",
                                                                     wd.module_name,
                                                                     u8"\nFile Path: ",
                                                                     wd.file_name,
                                                                     u8"\nFunction[",
                                                                     func_size,
                                                                     u8"]:\n");
                }
                if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"======================\nPreloaded DL Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wd.module_name),
                                                                     u"\nFile Path: ",
                                                                     ::fast_io::mnp::code_cvt(wd.file_name),
                                                                     u"\nFunction[",
                                                                     func_size,
                                                                     u"]:\n");
                }
                if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"======================\nPreloaded DL Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wd.module_name),
                                                                     U"\nFile Path: ",
                                                                     ::fast_io::mnp::code_cvt(wd.file_name),
                                                                     U"\nFunction[",
                                                                     func_size,
                                                                     U"]:\n");
                }

                // function

                ::std::size_t func_counter{};

                auto const func_end{func_begin + func_size};
                for(auto func_curr{func_begin}; func_curr != func_end; ++func_curr)
                {
                    ::uwvm2::utils::container::u8string_view const func_name{reinterpret_cast<char8_t_const_may_alias_ptr>(func_curr->func_name_ptr),
                                                                             func_curr->func_name_length};

                    if constexpr(::std::same_as<char_type, char>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         " - func[",
                                                                         func_counter++,
                                                                         "] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         "> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         L" - func[",
                                                                         func_counter++,
                                                                         L"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         L"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u8" - func[",
                                                                         func_counter++,
                                                                         u8"] <",
                                                                         func_name,
                                                                         u8"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u" - func[",
                                                                         func_counter++,
                                                                         u"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         u"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         U" - func[",
                                                                         func_counter++,
                                                                         U"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         U"> sig: (");
                    }

                    if(func_curr->para_type_vec_begin == nullptr && func_curr->para_type_vec_size != 0uz) [[unlikely]]
                    {
                        // If the parameter type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                        // non-zero uz, a memory safety issue will arise.
                        ::fast_io::fast_terminate();
                    }

                    auto const para_type_vec_end{func_curr->para_type_vec_begin + func_curr->para_type_vec_size};
                    for(auto para_curr{func_curr->para_type_vec_begin}; para_curr != para_type_vec_end; ++para_curr)
                    {
                        if(para_curr != func_curr->para_type_vec_begin) [[likely]]
                        {
                            if constexpr(::std::same_as<char_type, char>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", ");
                            }
                            if constexpr(::std::same_as<char_type, wchar_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                            }
                            if constexpr(::std::same_as<char_type, char8_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                            }
                            if constexpr(::std::same_as<char_type, char16_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                            }
                            if constexpr(::std::same_as<char_type, char32_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                            }
                        }

                        curr_value_type curr_value_type_storage;  // no initialize
                        ::std::memcpy(::std::addressof(curr_value_type_storage), para_curr, sizeof(curr_value_type_storage));

                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                    }

                    if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ") -> "); }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U") -> ");
                    }

                    if(func_curr->res_type_vec_size == 0uz)
                    {
                        // output "nil"

                        // When `res_type_vec_size` is 0, `begin_ptr` may be `nullptr` or any valid pointer.

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "nil\n");
                        }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"nil\n");
                        }
                    }
                    else if(func_curr->res_type_vec_size == 1uz)
                    {
                        if(func_curr->res_type_vec_begin == nullptr) [[unlikely]]
                        {
                            // If the result type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                            // non-zero uz, a memory safety issue will arise.
                            ::fast_io::fast_terminate();
                        }

                        // output "i32"

                        curr_value_type curr_value_type_storage;  // no initialize
                        ::std::memcpy(::std::addressof(curr_value_type_storage), func_curr->res_type_vec_begin, sizeof(curr_value_type_storage));

                        // print_freestanding<true> includes a newline
                        ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                    }
                    else
                    {
                        // output "(i32, i32)"

                        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "("); }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"(");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"(");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"(");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"(");
                        }

                        if(func_curr->res_type_vec_begin == nullptr) [[unlikely]]
                        {
                            // If the result type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                            // non-zero uz, a memory safety issue will arise.
                            ::fast_io::fast_terminate();
                        }

                        auto const res_type_vec_end{func_curr->res_type_vec_begin + func_curr->res_type_vec_size};
                        for(auto res_curr{func_curr->res_type_vec_begin}; res_curr != res_type_vec_end; ++res_curr)
                        {
                            if(res_curr != func_curr->res_type_vec_begin) [[likely]]
                            {
                                if constexpr(::std::same_as<char_type, char>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", ");
                                }
                                if constexpr(::std::same_as<char_type, wchar_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                                }
                                if constexpr(::std::same_as<char_type, char8_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                                }
                                if constexpr(::std::same_as<char_type, char16_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                                }
                                if constexpr(::std::same_as<char_type, char32_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                                }
                            }

                            curr_value_type curr_value_type_storage;  // no initialize
                            ::std::memcpy(::std::addressof(curr_value_type_storage), res_curr, sizeof(curr_value_type_storage));

                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                        }

                        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ")\n"); }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L")\n");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8")\n");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u")\n");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U")\n");
                        }
                    }
                }

                // custom handler

                auto const handler_begin{wd.wasm_dl_storage.capi_custom_handler_vec.custom_handler_begin};
                auto const handler_size{wd.wasm_dl_storage.capi_custom_handler_vec.custom_handler_size};

                if(handler_begin == nullptr && handler_size != 0uz) [[unlikely]]
                {
                    // If the custom handler vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                    // non-zero uz, a memory safety issue will arise.
                    ::fast_io::fast_terminate();
                }

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "Custom Handler[", handler_size, "]:\n");
                }
                if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"Custom Handler[", handler_size, L"]:\n");
                }
                if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"Custom Handler[", handler_size, u8"]:\n");
                }
                if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"Custom Handler[", handler_size, u"]:\n");
                }
                if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"Custom Handler[", handler_size, U"]:\n");
                }

                ::std::size_t handler_counter{};

                auto const handler_end{handler_begin + handler_size};
                for(auto handler_curr{handler_begin}; handler_curr != handler_end; ++handler_curr)
                {
                    ::uwvm2::utils::container::u8string_view const custom_name{reinterpret_cast<char8_t_const_may_alias_ptr>(handler_curr->custom_name_ptr),
                                                                               handler_curr->custom_name_length};

                    if constexpr(::std::same_as<char_type, char>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         " - custom[",
                                                                         handler_counter++,
                                                                         "] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         ">\n");
                    }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         L" - custom[",
                                                                         handler_counter++,
                                                                         L"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         L">\n");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u8" - custom[",
                                                                         handler_counter++,
                                                                         u8"] <",
                                                                         custom_name,
                                                                         u8">\n");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u" - custom[",
                                                                         handler_counter++,
                                                                         u"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         u">\n");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         U" - custom[",
                                                                         handler_counter++,
                                                                         U"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         U">\n");
                    }
                }

                // print_freestanding<true> includes a newline
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream));

                break;
            }
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case ::uwvm2::uwvm::wasm::type::module_type_t::weak_symbol:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(module_storage.module_storage_ptr.wws == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                auto const& wws{*module_storage.module_storage_ptr.wws};

                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                constexpr auto get_final_value_type_from_tuple{
                    []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(::uwvm2::utils::container::tuple<Fs...>) consteval noexcept
                    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>{}; }};

                using curr_value_type = ::std::remove_cvref_t<decltype(get_final_value_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features))>;
                static_assert(sizeof(curr_value_type) == 1uz, "curr_value_type is not 1 byte");

                // module information + func size

                auto const func_begin{wws.wasm_wws_storage.capi_function_vec.function_begin};
                auto const func_size{wws.wasm_wws_storage.capi_function_vec.function_size};

                if(func_begin == nullptr && func_size != 0uz) [[unlikely]]
                {
                    // If the function is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a non-zero uz, a
                    // memory safety issue will arise.
                    ::fast_io::fast_terminate();
                }

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "======================\nWeak Symbol Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wws.module_name),
                                                                     "\nFunction[",
                                                                     func_size,
                                                                     "]:\n");
                }
                if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"======================\nWeak Symbol Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wws.module_name),
                                                                     L"\nFunction[",
                                                                     func_size,
                                                                     L"]:\n");
                }
                if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"======================\nWeak Symbol Module Name: ",
                                                                     wws.module_name,
                                                                     u8"\nFunction[",
                                                                     func_size,
                                                                     u8"]:\n");
                }
                if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"======================\nWeak Symbol Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wws.module_name),
                                                                     u"\nFunction[",
                                                                     func_size,
                                                                     u"]:\n");
                }
                if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"======================\nWeak Symbol Module Name: ",
                                                                     ::fast_io::mnp::code_cvt(wws.module_name),
                                                                     U"\nFunction[",
                                                                     func_size,
                                                                     U"]:\n");
                }

                // function

                ::std::size_t func_counter{};

                auto const func_end{func_begin + func_size};
                for(auto func_curr{func_begin}; func_curr != func_end; ++func_curr)
                {
                    ::uwvm2::utils::container::u8string_view const func_name{reinterpret_cast<char8_t_const_may_alias_ptr>(func_curr->func_name_ptr),
                                                                             func_curr->func_name_length};

                    if constexpr(::std::same_as<char_type, char>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         " - func[",
                                                                         func_counter++,
                                                                         "] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         "> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         L" - func[",
                                                                         func_counter++,
                                                                         L"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         L"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u8" - func[",
                                                                         func_counter++,
                                                                         u8"] <",
                                                                         func_name,
                                                                         u8"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u" - func[",
                                                                         func_counter++,
                                                                         u"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         u"> sig: (");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         U" - func[",
                                                                         func_counter++,
                                                                         U"] <",
                                                                         ::fast_io::mnp::code_cvt(func_name),
                                                                         U"> sig: (");
                    }

                    if(func_curr->para_type_vec_begin == nullptr && func_curr->para_type_vec_size != 0uz) [[unlikely]]
                    {
                        // If the parameter type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                        // non-zero uz, a memory safety issue will arise.
                        ::fast_io::fast_terminate();
                    }

                    auto const para_type_vec_end{func_curr->para_type_vec_begin + func_curr->para_type_vec_size};
                    for(auto para_curr{func_curr->para_type_vec_begin}; para_curr != para_type_vec_end; ++para_curr)
                    {
                        if(para_curr != func_curr->para_type_vec_begin) [[likely]]
                        {
                            if constexpr(::std::same_as<char_type, char>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", ");
                            }
                            if constexpr(::std::same_as<char_type, wchar_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                            }
                            if constexpr(::std::same_as<char_type, char8_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                            }
                            if constexpr(::std::same_as<char_type, char16_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                            }
                            if constexpr(::std::same_as<char_type, char32_t>)
                            {
                                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                            }
                        }

                        curr_value_type curr_value_type_storage;  // no initialize
                        ::std::memcpy(::std::addressof(curr_value_type_storage), para_curr, sizeof(curr_value_type_storage));

                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                    }

                    if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ") -> "); }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u") -> ");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U") -> ");
                    }

                    if(func_curr->res_type_vec_size == 0uz)
                    {
                        // output "nil"

                        // When `res_type_vec_size` is 0, `begin_ptr` may be `nullptr` or any valid pointer.

                        if constexpr(::std::same_as<char_type, char>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "nil\n");
                        }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"nil\n");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"nil\n");
                        }
                    }
                    else if(func_curr->res_type_vec_size == 1uz)
                    {
                        if(func_curr->res_type_vec_begin == nullptr) [[unlikely]]
                        {
                            // If the result type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                            // non-zero uz, a memory safety issue will arise.
                            ::fast_io::fast_terminate();
                        }

                        // output "i32"

                        curr_value_type curr_value_type_storage;  // no initialize
                        ::std::memcpy(::std::addressof(curr_value_type_storage), func_curr->res_type_vec_begin, sizeof(curr_value_type_storage));

                        // print_freestanding<true> includes a newline
                        ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                    }
                    else
                    {
                        // output "(i32, i32)"

                        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "("); }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"(");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"(");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"(");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"(");
                        }

                        if(func_curr->res_type_vec_begin == nullptr) [[unlikely]]
                        {
                            // If the result type vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                            // non-zero uz, a memory safety issue will arise.
                            ::fast_io::fast_terminate();
                        }

                        auto const res_type_vec_end{func_curr->res_type_vec_begin + func_curr->res_type_vec_size};
                        for(auto res_curr{func_curr->res_type_vec_begin}; res_curr != res_type_vec_end; ++res_curr)
                        {
                            if(res_curr != func_curr->res_type_vec_begin) [[likely]]
                            {
                                if constexpr(::std::same_as<char_type, char>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", ");
                                }
                                if constexpr(::std::same_as<char_type, wchar_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                                }
                                if constexpr(::std::same_as<char_type, char8_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                                }
                                if constexpr(::std::same_as<char_type, char16_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                                }
                                if constexpr(::std::same_as<char_type, char32_t>)
                                {
                                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                                }
                            }

                            curr_value_type curr_value_type_storage;  // no initialize
                            ::std::memcpy(::std::addressof(curr_value_type_storage), res_curr, sizeof(curr_value_type_storage));

                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details_adl_caller(curr_value_type_storage));
                        }

                        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ")\n"); }
                        if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L")\n");
                        }
                        if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8")\n");
                        }
                        if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u")\n");
                        }
                        if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U")\n");
                        }
                    }
                }

                // custom handler

                auto const handler_begin{wws.wasm_wws_storage.capi_custom_handler_vec.custom_handler_begin};
                auto const handler_size{wws.wasm_wws_storage.capi_custom_handler_vec.custom_handler_size};

                if(handler_begin == nullptr && handler_size != 0uz) [[unlikely]]
                {
                    // If the custom handler vector is not defined, it should be nullptr and 0uz, which is permissible. However, if it is nullptr plus a
                    // non-zero uz, a memory safety issue will arise.
                    ::fast_io::fast_terminate();
                }

                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "Custom Handler[", handler_size, "]:\n");
                }
                if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"Custom Handler[", handler_size, L"]:\n");
                }
                if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"Custom Handler[", handler_size, u8"]:\n");
                }
                if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"Custom Handler[", handler_size, u"]:\n");
                }
                if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"Custom Handler[", handler_size, U"]:\n");
                }

                ::std::size_t handler_counter{};

                auto const handler_end{handler_begin + handler_size};
                for(auto handler_curr{handler_begin}; handler_curr != handler_end; ++handler_curr)
                {
                    ::uwvm2::utils::container::u8string_view const custom_name{reinterpret_cast<char8_t_const_may_alias_ptr>(handler_curr->custom_name_ptr),
                                                                               handler_curr->custom_name_length};

                    if constexpr(::std::same_as<char_type, char>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         " - custom[",
                                                                         handler_counter++,
                                                                         "] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         ">\n");
                    }
                    if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         L" - custom[",
                                                                         handler_counter++,
                                                                         L"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         L">\n");
                    }
                    if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u8" - custom[",
                                                                         handler_counter++,
                                                                         u8"] <",
                                                                         custom_name,
                                                                         u8">\n");
                    }
                    if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         u" - custom[",
                                                                         handler_counter++,
                                                                         u"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         u">\n");
                    }
                    if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         U" - custom[",
                                                                         handler_counter++,
                                                                         U"] <",
                                                                         ::fast_io::mnp::code_cvt(custom_name),
                                                                         U">\n");
                    }
                }

                // print_freestanding<true> includes a newline
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream));

                break;
            }
#endif
            case ::uwvm2::uwvm::wasm::type::module_type_t::local_import:
            {
                /// @todo local imported module section details
                break;
            }
            [[unlikely]] default:
            {
                /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
        }
    }

    inline constexpr void print_section_details() noexcept
    {
        ::fast_io::basic_obuf<::fast_io::u8native_io_observer> u8out_obuf{::fast_io::u8out()};

#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"print section details"};
#endif

        ::fast_io::io::print(u8out_obuf, u8"All Module Section Details:\n\n");

        for(auto const& curr_module: ::uwvm2::uwvm::wasm::storage::all_module) { ::fast_io::io::print(u8out_obuf, section_details(curr_module.second)); }
    }

}  // namespace uwvm2::uwvm::wasm::section_detail

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
