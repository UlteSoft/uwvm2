/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
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

#include <cstddef>
#include <memory>

#include <uwvm2/uwvm/wasm/type/local_imported.h>

namespace
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

    struct local_imported_global_i32
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{u8"g_i32"};
        inline static constexpr bool is_mutable{true};
        using value_type = wasm_i32;

        value_type value{};

        friend value_type global_get(local_imported_global_i32& g) noexcept { return g.value; }

        friend void global_set(local_imported_global_i32& g, value_type v) noexcept { g.value = v; }
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_global_name<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::has_global_mutable<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::has_global_value_type<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::has_global_get<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::has_global_set<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::can_set_global_value<local_imported_global_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_global<local_imported_global_i32>);

    struct local_imported_global_v128
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{u8"g_v128"};
        using value_type = wasm_v128;

        value_type value{};

        friend value_type global_get(local_imported_global_v128& g) noexcept { return g.value; }
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_global_value_type<local_imported_global_v128>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_global<local_imported_global_v128>);

    struct local_imported_global_bad_type
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{u8"g_bad"};
        using value_type = ::uwvm2::utils::container::u8string_view;
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_global_value_type<local_imported_global_bad_type>);
    static_assert(!::uwvm2::uwvm::wasm::type::is_local_imported_global<local_imported_global_bad_type>);

    struct local_imported_module_with_good_global_tuple
    {
        using local_global_tuple = ::uwvm2::utils::container::tuple<local_imported_global_i32, local_imported_global_v128>;
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_global_tuple<local_imported_module_with_good_global_tuple>);

    inline void odr_use_global_adl() noexcept
    {
        local_imported_global_i32 gi32{};
        (void)global_get(gi32);
        global_set(gi32, {});

        local_imported_global_v128 gv128{};
        (void)global_get(gv128);

        local_imported_global_bad_type gbad{};
        (void)::std::addressof(gbad);
    }
}  // namespace

int main()
{
    odr_use_global_adl();
    return 0;
}
