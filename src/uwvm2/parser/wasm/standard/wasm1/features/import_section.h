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
 * @date        2025-04-27
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
import :types;
import :feature_def;
import :type_section;
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
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/vector.h>
# include <fast_io_dsal/string_view.h>
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
# include "types.h"
# include "feature_def.h"
# include "type_section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct import_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::fast_io::u8string_view section_name{u8"Import"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::import_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::fast_io::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>> imports{};

        inline static constexpr ::std::size_t importdesc_count{
            static_cast<::std::size_t>(decltype(::uwvm2::parser::wasm::standard::wasm1::features::final_extern_type_t<Fs...>{}.type)::external_type_end) + 1uz};
        ::fast_io::array<::fast_io::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...> const*>, importdesc_count> importdesc{};
    };

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const*
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_func_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> const*& funcptr_r,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [... import_func_type] type_index ... module_namelen (section_end)
        // [       safe         ] unsafe (could be the section_end)
        //                        ^^ section_curr

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        // get type index

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index{};
        auto const [type_index_next, type_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                              ::fast_io::mnp::leb128_get(type_index))};
        if(type_index_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_index_err);
        }

        // [... import_func_type type_index ...] module_namelen (section_end)
        // [              safe                 ] unsafe (could be the section_end)
        //                       ^^ section_curr

        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};
        ::std::size_t const type_section_count{typesec.types.size()};

        // check: type_index should less than type_section_count
        if(type_index >= type_section_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32 = type_index;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        funcptr_r = typesec.types.cbegin() + type_index;

        // set curr
        section_curr = reinterpret_cast<::std::byte const*>(type_index_next);
        // [... import_func_type type_index ...] module_namelen (section_end)
        // [              safe                 ] unsafe (could be the section_end)
        //                                       ^^ section_curr

        return section_curr;
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_table_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::table_type & table_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_table_type(table_r, section_curr, section_end, err);
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_memory_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::memory_type & memory_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_memory_type(memory_r, section_curr, section_end, err);
    }

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::table_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* extern_imports_global_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::global_type & global_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_global_type(global_r, section_curr, section_end, err);
    }

    /// @brief Define function for wasm1 external_types
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* define_extern_prefix_imports_handler(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...> & fit_imports,  // [adl] can be replaced
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Note that section_curr may be equal to section_end, which needs to be checked
        switch(fit_imports.type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                // fit_imports.storage.function won't be replaced.
                return extern_imports_func_handler(sec_adl, fit_imports.storage.function, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::table:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_table_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_table_handler(sec_adl, fit_imports.storage.table, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::memory:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_memory_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_memory_handler(sec_adl, fit_imports.storage.memory, module_storage, section_curr, section_end, err, fs_para);
            }
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::global:
            {
                static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_imports_global_handler<Fs...>);
                // Note that section_curr may be equal to section_end, which needs to be checked
                return extern_imports_global_handler(sec_adl, fit_imports.storage.global, module_storage, section_curr, section_end, err, fs_para);
            }
            default: ::fast_io::unreachable();  // never match, checked before
        }
        return section_curr;
    }

    /// @brief Define the handler function for type_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<import_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse import section (id: 2)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get import_section_storage_t from storages
        auto& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};

        // check duplicate
        if(importsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = importsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // There is no need to check it here, it can be checked later in handle_import_func with index
        // Turning on checking can instead cause non-typesection import dependencies to error out
#if 0
        // check has typesec
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

        if(!typesec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8arr[0] = typesec.section_id;
            err.err_selectable.u8arr[1] = importsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::forward_dependency_missing;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
#endif

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        importsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        importsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};
        // [before_section ... ] | import_count ... module_namelen ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 import_count{};

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [import_count_next, import_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(import_count))};

        if(import_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(import_count_err);
        }

        // [before_section ... | import_count ...] module_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                       ^^ section_curr

        importsec.imports.reserve(import_count);

        // Since all subsequent ones are pushback_unchecked, the capacity will not grow, so end is used to denote the end of the buffer
        auto const importsec_imports_end{importsec.imports.imp.end_ptr};

        // Since the internal end is to be used later, check to see if it is the size of the counter
        UWVM_ASSERT(static_cast<::std::size_t>(importsec_imports_end - importsec.imports.cbegin()) == import_count);

        section_curr = reinterpret_cast<::std::byte const*>(import_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | import_count ...] module_namelen ...
        // [        safe                         ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        ::fast_io::array<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, importdesc_count> importdesc_counter{};  // use for reserve
        // desc counter

        while(section_curr != section_end)
        {
            // Ensuring the existence of valid information

            // [...  module_namelen] ... module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [       safe        ] unsafe (could be the section_end)
            //       ^^ section_curr

            // check import counter
            // Ensure content is available before counting (section_curr != section_end)
            if(importsec.imports.cend() == importsec_imports_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = import_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage fit (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...> fit{};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 module_namelen{};
            auto const [module_namelen_next, module_namelen_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(module_namelen))};

            if(module_namelen_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_module_name_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(module_namelen_err);
            }

            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //       ^^ section_curr

            if(module_namelen == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_length_cannot_be_zero;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const import_module_name_too_length_ptr{section_curr};
            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //       ^^ import_module_name_too_length_ptr

            section_curr = reinterpret_cast<::std::byte const*>(module_namelen_next);
            // Note that section_curr may be equal to section_end

            // [...  module_namelen ...] module_name ... extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe          ] unsafe (could be the section_end)
            //                           ^^ section_curr

            // check modulenamelen
            if(static_cast<::std::size_t>(section_end - section_curr) < module_namelen) [[unlikely]]
            {
                err.err_curr = import_module_name_too_length_ptr;
                err.err_selectable.u32 = module_namelen;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_too_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ...] extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe                          ] unsafe (could be the section_end)
            //                          ^^ section_curr

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            // No access, security
            fit.module_name = ::fast_io::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), module_namelen};

            section_curr += module_namelen;  // safe

            // [...  module_namelen ... module_name ...] extern_namelen ... extern_name ... import_type extern_func ...
            // [         safe                          ] unsafe (could be the section_end)
            //                                           ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent parse_by_scan

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 extern_namelen{};
            auto const [extern_namelen_next, extern_namelen_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                          ::fast_io::mnp::leb128_get(extern_namelen))};

            if(extern_namelen_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_extern_name_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(extern_namelen_err);
            }

            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                          ^^ section_curr

            if(extern_namelen == static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(0u)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_length_cannot_be_zero;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const import_extern_name_too_length_ptr{section_curr};
            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                          ^^ import_extern_name_too_length_ptr

            section_curr = reinterpret_cast<::std::byte const*>(extern_namelen_next);
            // Note that section_curr may be equal to section_end

            // [...  module_namelen ... module_name ... extern_namelen ...] extern_name ... import_type extern_func ...
            // [                            safe                          ] unsafe (could be the section_end)
            //                                                              ^^ section_curr

            // check externnamelen
            if(static_cast<::std::size_t>(section_end - section_curr) < extern_namelen) [[unlikely]]
            {
                err.err_curr = import_extern_name_too_length_ptr;
                err.err_selectable.u32 = extern_namelen;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_too_length;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ...] import_type extern_func ...
            // [                                safe                                      ] unsafe (could be the section_end)
            //                                                             ^^ section_curr

            fit.extern_name = ::fast_io::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr), extern_namelen};

            section_curr += extern_namelen;  // safe
            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ...] import_type extern_func ...
            // [                                safe                                      ] unsafe (could be the section_end)
            //                                                                              ^^ section_curr

            // Note that section_curr may be equal to section_end

            if(section_curr == section_end) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_missing_import_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ... import_type] extern_func ...
            // [                                   safe                                               ] unsafe (could be the section_end)
            //                                                                             ^^ section_curr

            ::fast_io::freestanding::my_memcpy(::std::addressof(fit.imports.type), section_curr, sizeof(fit.imports.type));

            static_assert(sizeof(fit.imports.type) == 1);
            // Size equal to one does not need to do little-endian conversion

            // importdesc_count never > 256 (max=255+1), convert to unsigned
            if(static_cast<unsigned>(fit.imports.type) >= static_cast<unsigned>(importdesc_count)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u8 = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fit.imports.type);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_importdesc_prefix;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // use for reserve
            ++importdesc_counter.index_unchecked(
                static_cast<::std::size_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(fit.imports.type)));

            ++section_curr;

            // [...  module_namelen ... module_name ... extern_namelen ... extern_name ... import_type] extern_func ...
            // [                                   safe                                               ] unsafe (could be the section_end)
            //                                                                                          ^^ section_curr

            // Note that section_curr may be equal to section_end, checked in the subsequent define_extern_prefix_imports_handler

            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_extern_prefix_imports_handler<Fs...>,
                          "define_extern_prefix_imports_handler(...) not found");
            // handle it, fit.imports.type is always valid
            section_curr = define_extern_prefix_imports_handler(sec_adl, fit.imports, module_storage, section_curr, section_end, err, fs_para);

            importsec.imports.push_back_unchecked(::std::move(fit));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check import counter match
        if(importsec.imports.cend() != importsec_imports_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = static_cast<::std::uint_least32_t>(importsec.imports.size());
            err.err_selectable.u32arr[1] = import_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // reserve importsec_importdesc_curr, so can unchecked push_back
        auto const importsec_importdesc_begin{importsec.importdesc.begin()};
        auto importsec_importdesc_curr{importsec_importdesc_begin};
        for(auto importdesc_curr{importdesc_counter.begin()}; importdesc_curr != importdesc_counter.end(); ++importdesc_curr)
        {
            importsec_importdesc_curr->reserve(*importdesc_curr);
            ++importsec_importdesc_curr;
        }

        // Categorize each import type and then store the
        // Since importsec.imports will be no subsequent expansion, the address of the it will always be fixed
        for(auto const& imp_curr: importsec.imports)
        {
            auto const imp_curr_imports_type_wasm_byte{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(imp_curr.imports.type)};
            // imp_curr_imports_type_wasm_byte have been previously checked and never cross the line
#if __has_cpp_attribute(assume)
            [[assume(static_cast<unsigned>(imp_curr_imports_type_wasm_byte) < static_cast<unsigned>(importdesc_count))]];
#endif
            importsec_importdesc_begin[imp_curr_imports_type_wasm_byte].push_back_unchecked(::std::addressof(imp_curr));
        }
    }
}  // namespace uwvm2::parser::wasm::standard::wasm1::features

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
