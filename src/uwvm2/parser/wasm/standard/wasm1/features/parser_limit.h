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
 * @date        2025-04-16
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

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <compare>
# include <algorithm>
# include <compare>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{

    template <::std::unsigned_integral From>
    inline constexpr ::std::size_t saturating_cast_size_t(From value) noexcept
    {
        if constexpr(::std::numeric_limits<From>::max() <= ::std::numeric_limits<::std::size_t>::max()) { return static_cast<::std::size_t>(value); }
        else
        {
            constexpr auto max_val{static_cast<From>(::std::numeric_limits<::std::size_t>::max())};
            return value > max_val ? ::std::numeric_limits<::std::size_t>::max() : static_cast<::std::size_t>(value);
        }
    }

    inline constexpr ::std::size_t default_max_code_sec_codes{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_code_locals{saturating_cast_size_t(65536u)};
    inline constexpr ::std::size_t default_max_data_sec_entries{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_elem_sec_funcidx{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_elem_sec_elems{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_export_sec_exports{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_global_sec_globals{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_import_sec_imports{saturating_cast_size_t(262144u)};
    inline constexpr ::std::size_t default_max_memory_sec_memories{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_table_sec_tables{saturating_cast_size_t(1024u)};
    inline constexpr ::std::size_t default_max_type_sec_types{saturating_cast_size_t(262144u)};

    struct wasm1_parser_limit_t
    {
        ::std::size_t max_code_sec_codes{default_max_code_sec_codes};
        ::std::size_t max_code_locals{default_max_code_locals};
        ::std::size_t max_data_sec_entries{default_max_data_sec_entries};
        ::std::size_t max_elem_sec_funcidx{default_max_elem_sec_funcidx};
        ::std::size_t max_elem_sec_elems{default_max_elem_sec_elems};
        ::std::size_t max_export_sec_exports{default_max_export_sec_exports};
        ::std::size_t max_global_sec_globals{default_max_global_sec_globals};
        ::std::size_t max_import_sec_imports{default_max_import_sec_imports};
        ::std::size_t max_memory_sec_memories{default_max_memory_sec_memories};
        ::std::size_t max_table_sec_tables{default_max_table_sec_tables};
        ::std::size_t max_type_sec_types{default_max_type_sec_types};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
