/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

struct F1
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F1"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

};

struct F2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_table{false};
};

struct F3
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F3"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_table{true};
};

int main()
{
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<F1>() == false);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<F1, F2>() == false);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<F1, F3>() == true);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<F1, F2, F3>() == true);
}

// macro
#include <uwvm2/utils/macro/pop_macros.h>
