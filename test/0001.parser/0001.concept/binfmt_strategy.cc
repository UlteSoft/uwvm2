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
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

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
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"1"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
};

struct binfmt_ver1_module_storage_t
{
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr binfmt_ver1_module_storage_t handle_func(::uwvm2::utils::container::tuple<Fs...>, ::std::byte const*, ::std::byte const*) UWVM_THROWS
{
    return {};
}

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr auto define_wasm_binfmt_parsering_strategy(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<feature1>, ::uwvm2::utils::container::tuple<Fs...>) noexcept
{
    return ::std::addressof(handle_func<Fs...>);
}

struct feature2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
};

static_assert(::uwvm2::parser::wasm::concepts::has_wasm_binfmt_parsering_strategy<feature1>);

int main() { ::uwvm2::parser::wasm::concepts::operation::check_has_duplicate_binfmt_handler<feature1, feature2>(); }

// macro
#include <uwvm2/utils/macro/pop_macros.h>
