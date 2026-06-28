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
 * @date        2025-05-03
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    struct custom_section_storage_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Custom"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::custom_sec)};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::section::custom_section> customs{};
    };

    /// @brief Define the handler function for type_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<custom_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        [[maybe_unused]] ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse custom section (id: 0)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        ::uwvm2::parser::wasm::standard::wasm1::section::custom_section cs{};

        cs.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        cs.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | name_len ... name ... custom_begin ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_len;  // No initialization necessary
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        auto const [next_name_len, err_name_len]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                          ::fast_io::mnp::leb128_get(name_len))};

        if(err_name_len != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_custom_name_length;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(err_name_len);
        }

        // [before_section ... | name_len ...] name ... custom_begin ...
        // [               safe              ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(name_len > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(name_len);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        section_curr = reinterpret_cast<::std::byte const*>(next_name_len);
        // [before_section ... | name_len ...] name ... custom_begin ...
        // [               safe              ] unsafe (could be the section_end)
        //                                     ^^ section_curr

        // check name length
        if(static_cast<::std::size_t>(section_end - section_curr) < static_cast<::std::size_t>(name_len)) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32 = name_len;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_custom_name_length;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // [before_section ... | name_len ... name ...] custom_begin ...
        // [               safe                       ] unsafe (could be the section_end)
        //                                    ^^ section_curr

        cs.custom_name = ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), name_len};

        // wasm custom section name is encoded as UTF-8 (Name); validate it.
        {
            auto const [utf8pos,
                        utf8err]{::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(cs.custom_name.cbegin(),
                                                                                                                                        cs.custom_name.cend())};

            if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
            {
                err.err_curr = reinterpret_cast<::std::byte const*>(utf8pos);
                err.err_selectable.u32 = static_cast<::std::uint_least32_t>(utf8err);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_char_sequence;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        // check custom order
        if constexpr(::uwvm2::parser::wasm::binfmt::ver1::has_custom_section_sequential_mapping_table_define<Fs...>)
        {
            constexpr auto& custom_order_sec_id_hash_table{
                ::uwvm2::parser::wasm::binfmt::ver1::final_section_sequential_packer_t<Fs...>::custom_section_sequential_mapping_table};
            auto const curr_section_order{
                ::uwvm2::parser::wasm::binfmt::ver1::details::find_from_custom_section_sequential_mapping_table(custom_order_sec_id_hash_table,
                                                                                                                cs.custom_name)};

            if(curr_section_order != 0u)
            {
                // There is no need to check for duplicate (sec_id <= max_section_id) sections here,
                // duplicate sections are checked inside the section parsing.

                if(curr_section_order < wasm_order.max_section_id) [[unlikely]]
                {
                    err.err_curr = section_curr;
                    // Enter the original section ID here for easy identification.
                    err.err_selectable.illegal_custom_section_order.custom_name = cs.custom_name;
                    err.err_selectable.illegal_custom_section_order.custom_order = curr_section_order;
                    err.err_selectable.illegal_custom_section_order.wasm_order = wasm_order.max_section_id;
                    err.err_selectable.illegal_custom_section_order.wasm_sec = wasm_order.max_section_id_map_sec_id;  // Used for output
                    err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_custom_section_order;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }

                wasm_order.max_section_id = curr_section_order;
                wasm_order.max_section_id_map_sec_id = 0u;  // Used for output, custom section id is 0
            }
        }

        // [before_section ... | name_len ... name ...] custom_begin ...
        // [               safe                       ] unsafe (could be the section_end)
        //                                    ^^ section_curr

        section_curr += name_len;
        // [before_section ... | name_len ... name ...] custom_begin ...
        // [               safe                       ] unsafe (could be the section_end)
        //                                              ^^ section_curr

        // Note that section_curr may be equal to section_end
        // Subsequently you need to check custom_begin with section_end
        cs.custom_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_curr);

        // get custom_section_storage_t from storages
        auto& customsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<custom_section_storage_t>(module_storage.sections)};

        // storage
        customsec.customs.push_back(::std::move(cs));
    }

    /// @brief Wrapper for the custom section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct custom_section_storage_section_details_wrapper_t
    {
        custom_section_storage_t const* custom_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr custom_section_storage_section_details_wrapper_t<Fs...> section_details(
        custom_section_storage_t const& custom_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(custom_section_storage), ::std::addressof(all_sections)}; }

    namespace details::custom_section_print
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

        inline constexpr bool emit_u8string(char8_t*& curr,
                                            char8_t* end,
                                            ::uwvm2::utils::container::u8string_view view,
                                            ::std::size_t& offset) noexcept
        {
            auto const literal_size{view.size()};
            auto const remain{literal_size - offset};
            auto const space{static_cast<::std::size_t>(end - curr)};
            auto const count{remain < space ? remain : space};

            curr = ::fast_io::freestanding::my_copy_n(view.cbegin() + offset, count, curr);
            offset += count;

            if(offset == literal_size)
            {
                offset = 0uz;
                return true;
            }

            return false;
        }

        template <typename T>
        inline constexpr bool emit_reserve(char8_t*& curr, char8_t* end, T value, ::std::size_t& offset) noexcept
        {
            using value_type = ::std::remove_cvref_t<T>;
            constexpr ::std::size_t reserve_size{print_reserve_size(::fast_io::io_reserve_type<char8_t, value_type>)};

            if(offset == 0uz && static_cast<::std::size_t>(end - curr) >= reserve_size)
            {
                curr = print_reserve_define(::fast_io::io_reserve_type<char8_t, value_type>, curr, value);
                return true;
            }

            char8_t buffer[reserve_size];
            auto const buffer_end{print_reserve_define(::fast_io::io_reserve_type<char8_t, value_type>, buffer, value)};
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

        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(header_literal, "Customs:\n");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_prefix, " - custom (");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_size_prefix, "): size = ");
        UWVM_WASM_UTILS_DEFINE_CONTEXT_LITERAL(row_suffix, "\n");

        enum class stage : unsigned char
        {
            header,
            row_prefix,
            row_name,
            row_size_prefix,
            row_size,
            row_suffix,
            done
        };

        struct context
        {
            stage curr_stage{};
            ::std::size_t offset{};
            ::std::size_t custom_counter{};

            template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
            inline constexpr ::fast_io::context_print_result<char8_t*> print_context_define(
                custom_section_storage_section_details_wrapper_t<Fs...> const custom_section_details_wrapper,
                char8_t* curr,
                char8_t* end) noexcept
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(custom_section_details_wrapper.custom_section_storage_ptr == nullptr || custom_section_details_wrapper.all_sections_ptr == nullptr)
                    [[unlikely]]
                {
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                }
#endif

                if(curr == end) [[unlikely]] { return {curr, false}; }

                auto const& customs{custom_section_details_wrapper.custom_section_storage_ptr->customs};
                auto const custom_size{customs.size()};

                for(;;)
                {
                    switch(this->curr_stage)
                    {
                        case stage::header:
                        {
                            if(!emit_literal(curr, end, header_literal<char8_t>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = custom_size == 0uz ? stage::done : stage::row_prefix;
                            break;
                        }
                        case stage::row_prefix:
                        {
                            if(this->custom_counter == custom_size)
                            {
                                this->curr_stage = stage::done;
                                break;
                            }

                            if(!emit_literal(curr, end, row_prefix<char8_t>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_name;
                            break;
                        }
                        case stage::row_name:
                        {
                            auto const curr_custom_name{customs.index_unchecked(this->custom_counter).custom_name};
                            if(!emit_u8string(curr, end, curr_custom_name, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_size_prefix;
                            break;
                        }
                        case stage::row_size_prefix:
                        {
                            if(!emit_literal(curr, end, row_size_prefix<char8_t>(), this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_size;
                            break;
                        }
                        case stage::row_size:
                        {
                            auto const& curr_custom{customs.index_unchecked(this->custom_counter)};
                            auto const curr_custom_size{static_cast<::std::size_t>(curr_custom.sec_span.sec_end - curr_custom.custom_begin)};
                            if(!emit_reserve(curr, end, curr_custom_size, this->offset)) { return {curr, false}; }
                            this->curr_stage = stage::row_suffix;
                            break;
                        }
                        case stage::row_suffix:
                        {
                            if(!emit_literal(curr, end, row_suffix<char8_t>(), this->offset)) { return {curr, false}; }
                            ++this->custom_counter;
                            this->curr_stage = stage::row_prefix;
                            break;
                        }
                        case stage::done: return {curr, true};
                    }

                    if(curr == end) { return {curr, false}; }
                }
            }
        };
    }  // namespace details::custom_section_print

    /// @brief Print the custom section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, custom_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       custom_section_storage_section_details_wrapper_t<Fs...> const custom_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(custom_section_details_wrapper.custom_section_storage_ptr == nullptr || custom_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "Customs:\n"); }
        else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"Customs:\n"); }
        else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"Customs:\n"); }
        else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"Customs:\n"); }
        else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"Customs:\n"); }

        for(auto const& curr_custom: custom_section_details_wrapper.custom_section_storage_ptr->customs)
        {
            auto const curr_custom_name{curr_custom.custom_name};

            auto const curr_custom_size{static_cast<::std::size_t>(curr_custom.sec_span.sec_end - curr_custom.custom_begin)};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                " - custom (",
                                                                ::fast_io::mnp::code_cvt(curr_custom_name),
                                                                "): size = ",
                                                                curr_custom_size);
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                L" - custom (",
                                                                ::fast_io::mnp::code_cvt(curr_custom_name),
                                                                L"): size = ",
                                                                curr_custom_size);
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                // No need to convert to UTF-8
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                u8" - custom (",
                                                                curr_custom_name,
                                                                u8"): size = ",
                                                                curr_custom_size);
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                u" - custom (",
                                                                ::fast_io::mnp::code_cvt(curr_custom_name),
                                                                u"): size = ",
                                                                curr_custom_size);
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<true>(::std::forward<Stm>(stream),
                                                                U" - custom (",
                                                                ::fast_io::mnp::code_cvt(curr_custom_name),
                                                                U"): size = ",
                                                                curr_custom_size);
            }
        }
    }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires ::std::same_as<char_type, char8_t>
    inline constexpr auto print_context_type(::fast_io::io_reserve_type_t<char_type, custom_section_storage_section_details_wrapper_t<Fs...>>) noexcept
    { return ::fast_io::io_type_t<::uwvm2::parser::wasm::standard::wasm1::features::details::custom_section_print::context>{}; }

    template <::std::integral char_type, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires ::std::same_as<char_type, char8_t>
    inline constexpr ::std::size_t print_context_static_buffer_size(
        ::fast_io::io_reserve_type_t<char_type, custom_section_storage_section_details_wrapper_t<Fs...>>) noexcept
    {
        constexpr auto buffer_size{::fast_io::details::dynamic_reserve_default_static_stack_size<char_type>()};
        return buffer_size;
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
