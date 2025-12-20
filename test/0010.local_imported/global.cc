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
#include <cstring>
#include <memory>

#include <uwvm2/uwvm/wasm/type/local_imported.h>

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
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

    struct local_imported_global_i64_immutable
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{u8"g_i64_imm"};
        using value_type = wasm_i64;

        value_type value{};

        friend value_type global_get(local_imported_global_i64_immutable& g) noexcept { return g.value; }
    };

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

    struct demo_global_import
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"demo_global"};
        using local_global_tuple = ::uwvm2::utils::container::tuple<local_imported_global_i32, local_imported_global_i64_immutable>;
        local_global_tuple local_global{};
    };

    inline int run_global_module_tests() noexcept
    {
        ::uwvm2::uwvm::wasm::type::local_imported_module<wasm1> mod{demo_global_import{}};

        auto const all{mod.get_all_global_information()};
        if(static_cast<::std::size_t>(all.end - all.begin) != 2uz) { return 1; }

        if(all.begin[0].global_name != u8"g_i32") { return 2; }
        if(all.begin[0].index != 0uz) { return 3; }
        if(all.begin[0].value_type != value_type::i32) { return 4; }
        if(!all.begin[0].is_mutable) { return 5; }

        if(all.begin[1].global_name != u8"g_i64_imm") { return 6; }
        if(all.begin[1].index != 1uz) { return 7; }
        if(all.begin[1].value_type != value_type::i64) { return 8; }
        if(all.begin[1].is_mutable) { return 9; }

        if(mod.global_value_type_from_index(0uz) != value_type::i32) { return 10; }
        if(!mod.global_is_mutable_from_index(0uz)) { return 11; }
        if(mod.global_value_type_from_index(1uz) != value_type::i64) { return 12; }
        if(mod.global_is_mutable_from_index(1uz)) { return 13; }

        // set/get mutable i32
        {
            wasm_i32 const v{123};
            ::std::byte in[sizeof(wasm_i32)]{};
            ::std::memcpy(in, ::std::addressof(v), sizeof(wasm_i32));
            if(!mod.global_set_from_index(0uz, in)) { return 14; }

            ::std::byte out_bytes[sizeof(wasm_i32)]{};
            mod.global_get_from_index(0uz, out_bytes);
            wasm_i32 out{};
            ::std::memcpy(::std::addressof(out), out_bytes, sizeof(wasm_i32));
            if(out != 123) { return 15; }
        }

        // set immutable i64 should fail
        {
            wasm_i64 const v{456};
            ::std::byte in[sizeof(wasm_i64)]{};
            ::std::memcpy(in, ::std::addressof(v), sizeof(wasm_i64));
            if(mod.global_set_from_index(1uz, in)) { return 16; }

            ::std::byte out_bytes[sizeof(wasm_i64)]{};
            mod.global_get_from_index(1uz, out_bytes);
            wasm_i64 out{};
            ::std::memcpy(::std::addressof(out), out_bytes, sizeof(wasm_i64));
            if(out != 0) { return 17; }
        }

        return 0;
    }
}  // namespace

int main()
{
    odr_use_global_adl();
    return run_global_module_tests();
}
