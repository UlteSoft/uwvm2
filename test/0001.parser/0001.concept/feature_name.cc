/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-07
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

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#else
# error "Module testing is not currently supported"
#endif

struct feature1
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"<name>"};
};

static_assert(::uwvm2::parser::wasm::concepts::has_feature_name<feature1>);

struct feature2
{
    ::uwvm2::utils::container::u8string_view feature_name{u8"<name>"};
};

static_assert(::uwvm2::parser::wasm::concepts::has_feature_name<feature2>);  // Satisfy the concept, but subsequent operations will be wrong:

int main() {}
