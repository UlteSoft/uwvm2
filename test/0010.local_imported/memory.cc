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
#include <cstdint>

#include <uwvm2/uwvm/wasm/type/local_imported.h>

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    struct local_imported_memory_good
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"mem"};
        inline static constexpr ::std::uint_least64_t page_size{64u * 1024u};

        ::std::byte buf[64u * 1024u]{};
        ::std::uint_least64_t pages{1};

        friend bool memory_grow(local_imported_memory_good& mem, ::std::uint_least64_t grow_page_size) noexcept
        {
            mem.pages += grow_page_size;
            return true;
        }

        friend ::std::byte* memory_begin(local_imported_memory_good& mem) noexcept { return mem.buf; }

        friend ::std::uint_least64_t memory_size(local_imported_memory_good& mem) noexcept { return mem.pages; }
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_memory_name<local_imported_memory_good>);
    static_assert(::uwvm2::uwvm::wasm::type::has_page_size<local_imported_memory_good>);
    static_assert(::uwvm2::uwvm::wasm::type::can_manipulate_memory<local_imported_memory_good>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_memory<local_imported_memory_good>);

    struct local_imported_memory_bad
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"badmem"};

        friend bool memory_grow(local_imported_memory_bad&, ::std::uint_least64_t) noexcept { return true; }

        friend ::std::uint_least64_t memory_size(local_imported_memory_bad&) noexcept { return 0; }
    };

    static_assert(!::uwvm2::uwvm::wasm::type::can_manipulate_memory<local_imported_memory_bad>);
    static_assert(!::uwvm2::uwvm::wasm::type::is_local_imported_memory<local_imported_memory_bad>);

    using memory_tuple_good = ::uwvm2::utils::container::tuple<local_imported_memory_good>;
    using memory_tuple_bad = ::uwvm2::utils::container::tuple<local_imported_memory_bad>;

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_memory_tuple<memory_tuple_good>);
    static_assert(!::uwvm2::uwvm::wasm::type::is_local_imported_memory_tuple<memory_tuple_bad>);

    struct local_imported_module_with_good_memory_tuple
    {
        using local_memory_tuple = memory_tuple_good;
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_memory_tuple<local_imported_module_with_good_memory_tuple>);

    inline void odr_use_memory_adl() noexcept
    {
        local_imported_memory_good good{};
        (void)memory_begin(good);
        (void)memory_size(good);
        (void)memory_grow(good, 0);

        local_imported_memory_bad bad{};
        (void)memory_size(bad);
        (void)memory_grow(bad, 0);
    }

    struct demo_mem_import
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"demo_mem"};
        using local_memory_tuple = ::uwvm2::utils::container::tuple<local_imported_memory_good>;
        local_memory_tuple local_memory{};
    };

    inline int run_memory_module_tests() noexcept
    {
        ::uwvm2::uwvm::wasm::type::local_imported_module<wasm1> mod{demo_mem_import{}};

        auto const all{mod.get_all_memory_information()};
        if(static_cast<::std::size_t>(all.end - all.begin) != 1uz) { return 1; }
        if(all.begin[0].memory_name != u8"mem") { return 2; }
        if(all.begin[0].index != 0uz) { return 3; }
        if(all.begin[0].page_size != 64u * 1024u) { return 4; }

        if(mod.memory_page_size_from_index(0uz) != 64u * 1024u) { return 5; }
        if(mod.memory_size_from_index(0uz) != 1uz) { return 6; }
        if(mod.memory_begin_from_index(0uz) == nullptr) { return 7; }
        if(!mod.memory_grow_from_index(0uz, 2uz)) { return 8; }
        if(mod.memory_size_from_index(0uz) != 3uz) { return 9; }

        return 0;
    }
}  // namespace

int main()
{
    odr_use_memory_adl();
    return run_memory_module_tests();
}
