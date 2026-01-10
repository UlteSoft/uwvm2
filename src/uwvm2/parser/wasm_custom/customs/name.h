/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-09
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
# include <limits>
# include <concepts>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include "name_error.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm_custom::customs
{
    // WebAssembly 1.0 does not specify that the index in the name segment must fall within the upper limit of the module's actual definition; it only requires
    // that the index be unique and appear in ascending order. Therefore, a map is used here.
    struct name_storage_t
    {
        ::uwvm2::utils::container::u8string_view module_name{};
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, ::uwvm2::utils::container::u8string_view>
            function_name{};
        ::uwvm2::utils::container::unordered_flat_map<
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32,
            ::uwvm2::utils::container::unordered_flat_map<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32, ::uwvm2::utils::container::u8string_view>>
            code_local_name{};

        bool has_analyzed{};
    };

    inline constexpr ::std::size_t default_max_name_sec_function_names{::uwvm2::parser::wasm::standard::wasm1::features::saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_name_sec_code_local_functions{::uwvm2::parser::wasm::standard::wasm1::features::saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_name_sec_code_local_names_per_function{
        ::uwvm2::parser::wasm::standard::wasm1::features::saturating_cast_size_t(65536u)};

    struct name_parser_limit_t
    {
        ::std::size_t max_name_sec_function_names{default_max_name_sec_function_names};
        ::std::size_t max_name_sec_code_local_functions{default_max_name_sec_code_local_functions};
        ::std::size_t max_name_sec_code_local_names_per_function{default_max_name_sec_code_local_names_per_function};
    };

    struct name_parser_param_t
    {
        name_parser_limit_t name_parser_limit{};
    };

    inline constexpr void parse_name_storage(name_storage_t & ns,
                                             ::std::byte const* const begin,
                                             ::std::byte const* const end,
                                             ::uwvm2::utils::container::vector<::uwvm2::parser::wasm_custom::customs::name_err_t>& err,
                                             name_parser_param_t const& param) noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse custom section: name"};
#endif

        // The name section should appear only once in a module, and only after the data section.
        // Here, check that only one remains.
        // After the data section is checked by the parser.

        // The name section should appear only once in a module

        if(ns.has_analyzed)
        {
            err.emplace_back(begin, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_name_section);
            return;
        }

        ns.has_analyzed = true;

        // Since wasm specifies that custom section processing failures should not throw exceptions
        // The custom section is not as strict as the standard section, so there is no need to extend the template type with variable parameters.

        auto curr{begin};

        // State machine and invariants:
        // - has_analyzed: module-level "only once". Ensures the name section is parsed at most once.
        // - max_section_id: subsection canonical order (0 -> 1 -> 2, non-decreasing).
        // - ct_1 / ct_2: non-structural error short-circuit flags at subsection / sub-map scopes (jump curr to map_end and stop current scope to avoid
        // cascading errors).

        // [... ] section_id ...
        // [safe] unsafe (could be the end)
        //        ^^ curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_section_id{};
        bool has_prev_section_id{};

        while(curr != end)
        {
            // [...  section_id] name_map_length ...
            // [     safe      ] unsafe (could be the end)
            //       ^^ curr

            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

            auto const section_id_ptr{curr};

            // Parse as u32 leb128 for WABT compatibility (WABT accepts non-canonical encodings here).
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 section_id;  // No initialization necessary

            auto const [section_id_next, section_id_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                                                  ::fast_io::mnp::leb128_get(section_id))};

            if(section_id_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.emplace_back(section_id_ptr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_section_id);
                return;
            }

            curr = reinterpret_cast<::std::byte const*>(section_id_next);

            // Each subsection may occur at most once, and in order of increasing id.
            if(has_prev_section_id && section_id <= max_section_id) [[unlikely]]
            {
                err.emplace_back(section_id_ptr,
                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_section_canonical_order,
                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                     .u8arr{static_cast<::std::uint_least8_t>(section_id), static_cast<::std::uint_least8_t>(max_section_id)}
                });
                // This only provides a warning and does not prevent further parsing.
            }

            has_prev_section_id = true;
            max_section_id = section_id;

            // [...  section_id] name_map_length ...
            // [     safe      ] unsafe (could be the end)
            //                   ^^ curr

            // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_map_length;  // No initialization necessary

            auto const [name_map_length_next, name_map_length_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(end),
                                                                                            ::fast_io::mnp::leb128_get(name_map_length))};

            if(name_map_length_err != ::fast_io::parse_code::ok) [[unlikely]]
            {
                err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_name_map_length);
                // The outermost structure has an error and cannot be parsed further, so everything must be terminated.
                return;
            }

            // [...  section_id name_map_length ...] ...
            // [             safe                  ] unsafe (could be the end)
            //                   ^^ curr

            // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                if(name_map_length > size_t_max) [[unlikely]]
                {
                    // name map exceeds the platform maximum length (size_t), then pointer addition may have overflow undefined behavior and exit directly.
                    err.emplace_back(curr,
                                     ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                     ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(name_map_length)});
                    // The outermost layer has a structural error, which may cause an overflow and prevent further parsing, resulting in termination of the
                    // entire process.
                    return;
                }
            }

            curr = reinterpret_cast<::std::byte const*>(name_map_length_next);

            // [...  section_id name_map_length ...] ...
            // [             safe                  ] unsafe (could be the end)
            //                                       ^^ curr

            if(static_cast<::std::size_t>(end - curr) < static_cast<::std::size_t>(name_map_length)) [[unlikely]]
            {
                // name map exceeds the end - curr, then pointer addition may have overflow undefined behavior and exit directly.
                err.emplace_back(curr,
                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_name_map_length,
                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = name_map_length});
                // The outermost layer has a structural error, which may cause an overflow and prevent further parsing, resulting in termination of the entire
                // process.
                return;
            }

            // [...  section_id name_map_length ... ... ...] (end)
            // [             safe                  ]... ...] unsafe (could be the end)
            //                                      ^^ curr

            // Subsequent boundary adjustments end to map_end
            auto const map_end{curr + name_map_length};

            // [...  section_id name_map_length ... ... ...] (map_end)
            // [             safe                  ]... ...] unsafe (could be the map_end)
            //                                               ^^ map_end

            // Empty subsections are allowed (e.g. module name subsection with size=0).
            if(curr == map_end) [[unlikely]] { continue; }

            switch(section_id)
            {
                    // The previous check is of the total size type and is not relevant to the subsequent checks

                case 0u:
                {
                    // module name

                    if(!ns.module_name.empty()) [[unlikely]]
                    {
                        err.emplace_back(section_id_ptr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_module_name);
                        curr = map_end;
                        // Duplicate content, skip
                        // End of current map
                        continue;
                    }

                    // [... ] module_name_length ... name_begin ... (map_end)
                    // [safe] unsafe (could be the map_end)
                    //        ^^ curr

                    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 module_name_length;  // No initialization necessary

                    auto const [module_name_length_next,
                                module_name_length_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                 reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                 ::fast_io::mnp::leb128_get(module_name_length))};

                    if(module_name_length_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_module_name_length);
                        curr = map_end;
                        // Structural error, unable to continue parsing
                        // End of current map
                        continue;
                    }

                    // [...  module_name_length ...] name_begin ... (map_end)
                    // [           safe            ] unsafe (could be the map_end)
                    //       ^^ curr

                    // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                    if constexpr(size_t_max < wasm_u32_max)
                    {
                        // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                        if(module_name_length > size_t_max) [[unlikely]]
                        {
                            err.emplace_back(
                                curr,
                                ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(module_name_length)});
                            curr = map_end;
                            // Exceeding the architectural limits may cause subsequent overflows and other issues, making further parsing impossible.
                            // End of current map
                            continue;
                        }
                    }

                    curr = reinterpret_cast<::std::byte const*>(module_name_length_next);

                    // [...  module_name_length ...] name_begin ... (map_end)
                    // [           safe            ] unsafe (could be the map_end)
                    //                               ^^ curr

                    if(static_cast<::std::size_t>(map_end - curr) < static_cast<::std::size_t>(module_name_length)) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_module_name_length,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = module_name_length});
                        curr = map_end;
                        // Exceeding the architectural limits may cause subsequent overflows and other issues, making further parsing impossible.
                        // End of current map
                        continue;
                    }

                    // [...  module_name_length ... name_begin ...] (map_end)
                    // [                  safe                    ] unsafe (could be the map_end)
                    //                              ^^ curr

                    auto const module_name_begin{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                    // [...  module_name_length ... name_begin ...] (map_end)
                    // [                  safe                    ] unsafe (could be the map_end)
                    //                              ^^ module_name_begin

                    curr += module_name_length;

                    // [...  module_name_length ... name_begin ...] (map_end)
                    // [                  safe                    ] unsafe (could be the map_end)
                    //                                              ^^ curr

                    auto const module_name_end{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                    // [...  module_name_length ... name_begin ...] (map_end)
                    // [                  safe                    ] unsafe (could be the map_end)
                    //                                              ^^ module_name_end

                    ::uwvm2::utils::container::u8string_view const module_name_tmp{module_name_begin,
                                                                                   static_cast<::std::size_t>(module_name_end - module_name_begin)};

                    // Check if it is legal utf-8

                    auto const [utf8pos, utf8err]{
                        ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(module_name_tmp.cbegin(),
                                                                                                                               module_name_tmp.cend())};

                    if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                    {
                        err.emplace_back(reinterpret_cast<::std::byte const*>(utf8pos),
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_char_sequence,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = static_cast<::std::uint_least32_t>(utf8err)});
                        curr = map_end;
                        // This is a non-structural error, so parsing can continue. However, this map only has one entry, so we can move directly to the next
                        // map. End of current paragraph
                        continue;
                    }

                    // The value is set here in the
                    ns.module_name = module_name_tmp;

                    if(curr != map_end) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_data_exists,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.err_uz = static_cast<::std::size_t>(map_end - curr)});
                        curr = map_end;
                        // This is a non-structural error, so parsing can continue. However, this map only has one entry, so we can move directly to the next
                        // map. End of current map
                        continue;
                    }

                    break;
                }
                case 1u:
                {
                    // func name

                    if(!ns.function_name.empty()) [[unlikely]]
                    {
                        err.emplace_back(section_id_ptr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_function_name);
                        curr = map_end;
                        // Duplicate content, skip
                        // End of current map
                        continue;
                    }

                    // [... ] name_count ... (map_end)
                    // [safe] unsafe (could be the map_end)
                    //        ^^ curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_count;  // number of naming in names
                    auto const [name_count_next, name_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                          ::fast_io::mnp::leb128_get(name_count))};

                    if(name_count_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_name_count);
                        curr = map_end;
                        // Structural error, unable to continue parsing
                        // End of current map
                        continue;
                    }

                    // [...  name_count ...] ... (map_end)
                    // [       safe        ] unsafe (could be the map_end)
                    //       ^^ curr

                    // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                    if constexpr(size_t_max < wasm_u32_max)
                    {
                        // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                        if(name_count > size_t_max) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(name_count)});
                            curr = map_end;
                            // Exceeding the architectural limits may cause subsequent overflows and other issues, making further parsing impossible.
                            // End of current map
                            continue;
                        }
                    }

                    if(name_count > param.name_parser_limit.max_name_sec_function_names) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::exceed_the_max_name_parser_limit,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                             .exceed_the_max_name_parser_limit = {.name = u8"custom_name_funcnames",
                                                                                  .value = name_count,
                                                                                  .maxval = param.name_parser_limit.max_name_sec_function_names}
                        });
                        curr = map_end;
                        // End of current map
                        continue;
                    }

                    ns.function_name.reserve(name_count);

                    curr = reinterpret_cast<::std::byte const*>(name_count_next);

                    // [...  name_count ...] ... (map_end)
                    // [       safe        ] unsafe (could be the map_end)
                    //                       ^^ curr

                    bool ct_1{};

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 last_func_idx{};

                    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_counter{}; name_counter != name_count; ++name_counter)
                    {
                        // [... ] func_idx ... name_len ... ... (map_end)
                        // [safe] unsafe (could be the map_end)
                        //        ^^ curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;
                        auto const [func_index_next, func_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                              ::fast_io::mnp::leb128_get(func_index))};

                        // There is no need to do any checking here as it is map.

                        if(func_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_index);
                            // Encountered error leb128, could not continue parsing, don't know if there is a problem with subsequent bytes
                            curr = map_end;
                            // End of current map
                            // Implementation of ct_1 via state variables
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            ct_1 = true;
                            break;
                        }

                        // [...  func_idx ...] name_len ... ... (map_end)
                        // [        safe     ] unsafe (could be the map_end)
                        //       ^^ curr

                        if(func_index < last_func_idx) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_index_order,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                                 .u32arr{func_index, last_func_idx}
                            });

                            curr = map_end;
                            // This is not a structural error, but subsequent content cannot be parsed, which may lead to subsequent structural errors, so
                            // parsing cannot continue.
                            // End of current map
                            // Implementation of ct_1 via state variables
                            ct_1 = true;
                            break;
                        }

                        last_func_idx = func_index;

                        auto const func_index_ptr{curr};

                        curr = reinterpret_cast<::std::byte const*>(func_index_next);

                        // [...  func_idx ...] name_len ... ... (map_end)
                        // [        safe     ] unsafe (could be the map_end)
                        //                     ^^ curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_name_length;
                        auto const [func_name_length_next,
                                    func_name_length_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                   ::fast_io::mnp::leb128_get(func_name_length))};

                        // There is no need to do any checking here as it is map.

                        if(func_name_length_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_name_length);
                            // Encountered error leb128, could not continue parsing, don't know if there is a problem with subsequent bytes
                            curr = map_end;
                            // End of current map
                            // Implementation of ct_1 via state variables
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            ct_1 = true;
                            break;
                        }

                        // [...  func_idx ... name_len ...] ... (map_end)
                        // [             safe             ] unsafe (could be the map_end)
                        //                    ^^ curr

                        if constexpr(size_t_max < wasm_u32_max)
                        {
                            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                            if(func_name_length > size_t_max) [[unlikely]]
                            {
                                err.emplace_back(
                                    curr,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(func_name_length)});
                                curr = map_end;
                                // End of current map
                                // Implementation of ct_1 via state variables
                                // Non-structural error -> skip this subsection to avoid cascading errors
                                ct_1 = true;
                                break;
                            }
                        }

                        curr = reinterpret_cast<::std::byte const*>(func_name_length_next);

                        // [...  func_idx ... name_len ...] ... (map_end)
                        // [             safe             ] unsafe (could be the map_end)
                        //                                  ^^ curr

                        if(static_cast<::std::size_t>(map_end - curr) < static_cast<::std::size_t>(func_name_length)) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_function_name_length,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = func_name_length});
                            curr = map_end;
                            // End of current map
                            // Implementation of ct_1 via state variables
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            ct_1 = true;
                            break;
                        }

                        // [...  func_idx ... name_len ... name ...] (map_end)
                        // [                  safe                 ] unsafe (could be the map_end)
                        //                                 ^^ curr

                        auto const func_name_begin{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                        // [...  func_idx ... name_len ... name ...] (map_end)
                        // [                  safe                 ] unsafe (could be the map_end)
                        //                                 ^^ func_name_begin

                        curr += func_name_length;

                        // [...  func_idx ... name_len ... name ...] (map_end)
                        // [                  safe                 ] unsafe (could be the map_end)
                        //                                           ^^ curr

                        auto const func_name_end{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                        // [...  func_idx ... name_len ... name ...] (map_end)
                        // [                  safe                 ] unsafe (could be the map_end)
                        //                                           ^^ func_name_end

                        ::uwvm2::utils::container::u8string_view const func_name_tmp{func_name_begin,
                                                                                     static_cast<::std::size_t>(func_name_end - func_name_begin)};

                        // Subsequent parsing will not directly skip the parsing of this map.

                        // Check if it is legal utf-8

                        auto const [utf8pos, utf8err]{
                            ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(func_name_tmp.cbegin(),
                                                                                                                                   func_name_tmp.cend())};

                        if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                        {
                            err.emplace_back(reinterpret_cast<::std::byte const*>(utf8pos),
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_char_sequence,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = static_cast<::std::uint_least32_t>(utf8err)});
                            curr = map_end;
                            // End of current paragraph
                            continue;
                        }

                        // check if unique index
                        // Use try_emplace to combine lookup and insertion operations, avoiding two hash lookups
                        if(!ns.function_name.try_emplace(func_index, func_name_tmp).second) [[unlikely]]
                        {
                            err.emplace_back(func_index_ptr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_func_idx,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = func_index});
                            curr = map_end;
                            // End of current paragraph
                            continue;
                        }
                    }

                    if(ct_1) [[unlikely]]
                    {
                        // End of current map
                        // ct_1 is just a state variable that only controls the jumps
                        // already set "curr = map_end"
                        // Non-structural error -> skip this subsection to avoid cascading errors
                        continue;
                    }

                    if(curr != map_end) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_data_exists,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.err_uz = static_cast<::std::size_t>(map_end - curr)});
                        curr = map_end;
                        // End of current map
                        continue;
                    }

                    break;
                }
                case 2u:
                {
                    // local name

                    if(!ns.code_local_name.empty()) [[unlikely]]
                    {
                        err.emplace_back(section_id_ptr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_local_name);
                        curr = map_end;
                        // End of current map
                        continue;
                    }

                    // [... ] local_count ... ... (map_end)
                    // [safe] unsafe (could be the map_end)
                    //        ^^ curr

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_count;  // number of naming in names
                    auto const [local_count_next, local_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                            ::fast_io::mnp::leb128_get(local_count))};

                    if(local_count_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_local_count);
                        curr = map_end;
                        // End of current map
                        continue;
                    }

                    // [...  local_count ...] ... (map_end)
                    // [        safe        ] unsafe (could be the map_end)
                    //       ^^ curr

                    // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                    if constexpr(size_t_max < wasm_u32_max)
                    {
                        // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                        if(local_count > size_t_max) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(local_count)});
                            curr = map_end;
                            // End of current map
                            continue;
                        }
                    }

                    if(local_count > param.name_parser_limit.max_name_sec_code_local_functions) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::exceed_the_max_name_parser_limit,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                             .exceed_the_max_name_parser_limit = {.name = u8"custom_name_codelocal_funcs",
                                                                                  .value = local_count,
                                                                                  .maxval = param.name_parser_limit.max_name_sec_code_local_functions}
                        });
                        curr = map_end;
                        // End of current map
                        continue;
                    }

                    ns.code_local_name.reserve(local_count);

                    curr = reinterpret_cast<::std::byte const*>(local_count_next);
                    // [...  local_count ...] ... (map_end)
                    // [        safe        ] unsafe (could be the map_end)
                    //                        ^^ curr

                    bool ct_1{};

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 last_function_idx{};

                    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_counter{}; local_counter != local_count; ++local_counter)
                    {
                        // [... ] function_index ... ... (map_end)
                        // [safe] unsafe (could be the map_end)
                        //        ^^ curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_index;  // number of naming in names
                        auto const [function_index_next, function_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                                      ::fast_io::mnp::leb128_get(function_index))};

                        if(function_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_index);
                            curr = map_end;
                            // End of current map
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            ct_1 = true;
                            break;
                        }

                        // [...  function_index ...] ... (map_end)
                        // [         safe          ] unsafe (could be the map_end)
                        //       ^^ curr

                        if(function_index < last_function_idx) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_index_order,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                                 .u32arr{function_index, last_function_idx}
                            });

                            curr = map_end;
                            // End of current map
                            ct_1 = true;
                            break;
                        }

                        last_function_idx = function_index;

                        auto const function_index_ptr{curr};

                        curr = reinterpret_cast<::std::byte const*>(function_index_next);

                        // [...  function_index ...] function_local_count ... (map_end)
                        // [         safe          ] unsafe (could be the map_end)
                        //                           ^^ curr

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_local_count;  // number of naming in names
                        auto const [function_local_count_next,
                                    function_local_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                       ::fast_io::mnp::leb128_get(function_local_count))};

                        if(function_local_count_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_local_count);
                            curr = map_end;
                            // End of current map
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            ct_1 = true;
                            break;
                        }

                        // [...  function_index ... function_local_count ...] ... (map_end)
                        // [                      safe                      ] unsafe (could be the map_end)
                        //                          ^^ curr

                        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
                        if constexpr(size_t_max < wasm_u32_max)
                        {
                            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                            if(function_local_count > size_t_max) [[unlikely]]
                            {
                                err.emplace_back(
                                    curr,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(function_local_count)});
                                curr = map_end;
                                // End of current map
                                // Non-structural error -> skip this subsection to avoid cascading errors
                                ct_1 = true;
                                break;
                            }
                        }

                        curr = reinterpret_cast<::std::byte const*>(function_local_count_next);

                        // [...  function_index ... function_local_count ...] ... (map_end)
                        // [                      safe                      ] unsafe (could be the map_end)
                        //                                                    ^^ curr

                        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32,
                                                                      ::uwvm2::utils::container::u8string_view>
                            ns_code_local_name_function_index{};

                        if(function_local_count > param.name_parser_limit.max_name_sec_code_local_names_per_function) [[unlikely]]
                        {
                            err.emplace_back(
                                curr,
                                ::uwvm2::parser::wasm_custom::customs::name_err_type_t::exceed_the_max_name_parser_limit,
                                ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                    .exceed_the_max_name_parser_limit = {.name = u8"custom_name_codelocal_name_per_funcs",
                                                                         .value = function_local_count,
                                                                         .maxval = param.name_parser_limit.max_name_sec_code_local_names_per_function}
                            });
                            curr = map_end;
                            // End of current map
                            continue;
                        }

                        ns_code_local_name_function_index.reserve(function_local_count);

                        bool ct_2{};

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 last_function_local_index{};

                        for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_local_counter{}; function_local_counter != function_local_count;
                            ++function_local_counter)
                        {
                            // [... ] function_local_index ... ... (map_end)
                            // [safe] unsafe (could be the map_end)
                            //        ^^ curr

                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_local_index;  // number of naming in names
                            auto const [function_local_index_next,
                                        function_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                           reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                           ::fast_io::mnp::leb128_get(function_local_index))};

                            if(function_local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
                            {
                                err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_local_index);
                                curr = map_end;
                                // Structural error, unable to continue parsing
                                // End of current map
                                // Non-structural error -> skip this subsection to avoid cascading errors
                                ct_2 = true;
                                break;
                            }

                            // [...  function_local_index ...] ... (map_end)
                            // [            safe             ] unsafe (could be the map_end)
                            //       ^^ curr

                            if(function_local_index < last_function_local_index) [[unlikely]]
                            {
                                err.emplace_back(curr,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_local_index_order,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                                     .u32arr{function_local_index, last_function_local_index}
                                });
                                curr = map_end;
                                // This is not a structural error, but subsequent content cannot be parsed, which may lead to subsequent structural errors, so
                                // parsing cannot continue.
                                // End of current map
                                ct_2 = true;
                                break;
                            }

                            last_function_local_index = function_local_index;

                            auto const function_local_index_ptr{curr};

                            curr = reinterpret_cast<::std::byte const*>(function_local_index_next);

                            // [...  function_local_index ...] function_local_name_length ... ... (map_end)
                            // [            safe             ] unsafe (could be the map_end)
                            //                                 ^^ curr

                            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_local_name_length;  // number of naming in names
                            auto const [function_local_name_length_next,
                                        function_local_name_length_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                                 reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                                 ::fast_io::mnp::leb128_get(function_local_name_length))};

                            if(function_local_name_length_err != ::fast_io::parse_code::ok) [[unlikely]]
                            {
                                err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_local_name_length);
                                curr = map_end;
                                // Structural error, unable to continue parsing
                                // End of current map
                                // Non-structural error -> skip this subsection to avoid cascading errors
                                ct_2 = true;
                                break;
                            }

                            // [...  function_local_index ... function_local_name_length ...] ... (map_end)
                            // [                             safe                           ] unsafe (could be the map_end)
                            //                                ^^ curr

                            if constexpr(size_t_max < wasm_u32_max)
                            {
                                // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
                                if(function_local_name_length > size_t_max) [[unlikely]]
                                {
                                    err.emplace_back(curr,
                                                     ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                                     ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{
                                                         .u64 = static_cast<::std::uint_least64_t>(function_local_name_length)});
                                    curr = map_end;
                                    // Exceeding the architectural limits may cause subsequent overflows and other issues, making further parsing impossible.
                                    // End of current map
                                    // Non-structural error -> skip this subsection to avoid cascading errors
                                    ct_2 = true;
                                    break;
                                }
                            }

                            curr = reinterpret_cast<::std::byte const*>(function_local_name_length_next);

                            // [...  function_local_index ... function_local_name_length ...] ... (map_end)
                            // [                             safe                           ] unsafe (could be the map_end)
                            //                                                                ^^ curr

                            if(static_cast<::std::size_t>(map_end - curr) < static_cast<::std::size_t>(function_local_name_length)) [[unlikely]]
                            {
                                err.emplace_back(curr,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_function_local_name_length,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = function_local_name_length});
                                curr = map_end;
                                // Exceeding the architectural limits may cause subsequent overflows and other issues, making further parsing impossible.
                                // End of current map
                                // Non-structural error -> skip this subsection to avoid cascading errors
                                ct_2 = true;
                                break;
                            }

                            // [...  function_local_index ... function_local_name_length ... name ...] (map_end)
                            // [                             safe                                    ] unsafe (could be the map_end)
                            //                                                                ^^ curr

                            auto const function_local_name_begin{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                            // [...  function_local_index ... function_local_name_length ... name ...] (map_end)
                            // [                             safe                                    ] unsafe (could be the map_end)
                            //                                                                ^^ function_local_name_begin

                            curr += function_local_name_length;

                            // [...  function_local_index ... function_local_name_length ... name ...] (map_end)
                            // [                             safe                                    ] unsafe (could be the map_end)
                            //                                                                         ^^ curr

                            auto const function_local_name_end{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                            // [...  function_local_index ... function_local_name_length ... name ...] (map_end)
                            // [                             safe                                    ] unsafe (could be the map_end)
                            //                                                                         ^^ function_local_name_end

                            ::uwvm2::utils::container::u8string_view const function_local_name_begin_tmp{
                                function_local_name_begin,
                                static_cast<::std::size_t>(function_local_name_end - function_local_name_begin)};

                            // Subsequent parsing will not directly skip the parsing of this map.

                            // Check if it is legal utf-8

                            auto const [utf8pos,
                                        utf8err]{::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(
                                function_local_name_begin_tmp.cbegin(),
                                function_local_name_begin_tmp.cend())};

                            if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                            {
                                err.emplace_back(reinterpret_cast<::std::byte const*>(utf8pos),
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_char_sequence,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = static_cast<::std::uint_least32_t>(utf8err)});
                                curr = map_end;
                                // Non-structural error, all structures are intact, discard this content, and continue parsing subsequent content.
                                // End of current paragraph
                                continue;
                            }

                            // check has unique index
                            // Use try_emplace to combine lookup and insertion operations, avoiding two hash lookups
                            if(!ns_code_local_name_function_index.try_emplace(function_local_index, function_local_name_begin_tmp).second) [[unlikely]]
                            {
                                err.emplace_back(function_local_index_ptr,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_code_local_name_function_index,
                                                 ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = function_local_index});
                                curr = map_end;
                                // Non-structural error, all structures are intact, discard this content, and continue parsing subsequent content.
                                // End of current paragraph
                                continue;
                            }
                        }

                        if(ct_2) [[unlikely]]
                        {
                            ct_1 = true;
                            // Non-structural error -> skip this subsection to avoid cascading errors
                            break;
                        }

                        // Subsequent parsing will not directly skip the parsing of this map.

                        // Use try_emplace to combine lookup and insertion operations, avoiding two hash lookups
                        if(!ns.code_local_name.try_emplace(function_index, ::std::move(ns_code_local_name_function_index)).second) [[unlikely]]
                        {
                            err.emplace_back(function_index_ptr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::duplicate_code_function_index,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = function_index});
                            curr = map_end;
                            // Non-structural error, all structures are intact, discard this content, and continue parsing subsequent content.
                            // End of current paragraph
                            continue;
                        }
                    }

                    if(ct_1) [[unlikely]]
                    {
                        // End of current map
                        // ct_1 is just a state variable that only controls the jumps
                        // already set "curr = map_end"
                        // Non-structural error -> skip this subsection to avoid cascading errors
                        continue;
                    }

                    if(curr != map_end) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_data_exists,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.err_uz = static_cast<::std::size_t>(map_end - curr)});
                        curr = map_end;
                        // Non-structural error, all structures are intact, discard this content, and continue parsing subsequent content.
                        // End of current map
                        continue;
                    }

                    break;
                }
                case 4u:
                case 5u:
                case 6u:
                case 7u:
                case 8u:
                case 9u:
                case 11u:
                {
                    // type/table/memory/global/elem/data/tag name map:
                    // name_count { index, name }*

                    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_count;  // number of naming in names
                    auto const [name_count_next, name_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                          ::fast_io::mnp::leb128_get(name_count))};

                    if(name_count_err != ::fast_io::parse_code::ok) [[unlikely]]
                    {
                        err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_name_count);
                        curr = map_end;
                        continue;
                    }

                    if constexpr(size_t_max < wasm_u32_max)
                    {
                        if(name_count > size_t_max) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(name_count)});
                            curr = map_end;
                            continue;
                        }
                    }

                    curr = reinterpret_cast<::std::byte const*>(name_count_next);

                    // WABT's ReadCount early-outs when an erroneous large count is used (assumes each item takes at least 1 byte).
                    if(static_cast<::std::size_t>(map_end - curr) < static_cast<::std::size_t>(name_count)) [[unlikely]]
                    {
                        err.emplace_back(curr,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_name_count,
                                         ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = name_count});
                        curr = map_end;
                        continue;
                    }

                    bool ct_1{};

                    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_counter{}; name_counter != name_count; ++name_counter)
                    {
                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 index;
                        auto const [index_next, index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                    ::fast_io::mnp::leb128_get(index))};

                        if(index_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_index);
                            curr = map_end;
                            ct_1 = true;
                            break;
                        }

                        curr = reinterpret_cast<::std::byte const*>(index_next);

                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 name_length;
                        auto const [name_length_next, name_length_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(curr),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(map_end),
                                                                                                ::fast_io::mnp::leb128_get(name_length))};

                        if(name_length_err != ::fast_io::parse_code::ok) [[unlikely]]
                        {
                            err.emplace_back(curr, ::uwvm2::parser::wasm_custom::customs::name_err_type_t::invalid_function_name_length);
                            curr = map_end;
                            ct_1 = true;
                            break;
                        }

                        if constexpr(size_t_max < wasm_u32_max)
                        {
                            if(name_length > size_t_max) [[unlikely]]
                            {
                                err.emplace_back(
                                    curr,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_type_t::size_exceeds_the_maximum_value_of_size_t,
                                    ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u64 = static_cast<::std::uint_least64_t>(name_length)});
                                curr = map_end;
                                ct_1 = true;
                                break;
                            }
                        }

                        curr = reinterpret_cast<::std::byte const*>(name_length_next);

                        if(static_cast<::std::size_t>(map_end - curr) < static_cast<::std::size_t>(name_length)) [[unlikely]]
                        {
                            err.emplace_back(curr,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_function_name_length,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = name_length});
                            curr = map_end;
                            ct_1 = true;
                            break;
                        }

                        auto const name_begin_tmp{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};
                        curr += name_length;
                        auto const name_end_tmp{reinterpret_cast<char8_t_const_may_alias_ptr>(curr)};

                        ::uwvm2::utils::container::u8string_view const name_tmp{
                            name_begin_tmp, static_cast<::std::size_t>(name_end_tmp - name_begin_tmp)};

                        auto const [utf8pos, utf8err]{
                            ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(name_tmp.cbegin(),
                                                                                                                                   name_tmp.cend())};

                        if(utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                        {
                            err.emplace_back(reinterpret_cast<::std::byte const*>(utf8pos),
                                             ::uwvm2::parser::wasm_custom::customs::name_err_type_t::illegal_char_sequence,
                                             ::uwvm2::parser::wasm_custom::customs::name_err_storage_t{.u32 = static_cast<::std::uint_least32_t>(utf8err)});
                            curr = map_end;
                            ct_1 = true;
                            break;
                        }
                    }

                    if(ct_1) [[unlikely]]
                    {
                        // End of current map
                        // already set "curr = map_end"
                        continue;
                    }

                    // WABT skips any remaining bytes in these subsections.
                    curr = map_end;
                    break;
                }
                [[unlikely]] default:
                {
                    // Unknown subsection IDs are allowed; skip this subsection payload.
                    curr = map_end;
                    break;
                }

                    // Subsequent boundary adjustments map_end to end
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
