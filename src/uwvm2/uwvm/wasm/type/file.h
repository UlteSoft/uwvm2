/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <new>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include "para.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    struct wasm_file_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        // support binfmt ver1 currently
        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");

        inline static constexpr ::std::size_t sizeof_wasm_file_module_storage_u{sizeof(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t)};

        union wasm_file_module_storage_u UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
        {
            // This type can be initialized with all zeros
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t wasm_binfmt_ver1_storage;
            static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<decltype(wasm_binfmt_ver1_storage)> &&
                          ::fast_io::freestanding::is_zero_default_constructible_v<decltype(wasm_binfmt_ver1_storage)>);

            /// @todo wasm component module (storage)

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte wasm_file_module_storage_u_reserve[sizeof_wasm_file_module_storage_u]{};

            // destructor of 'wasm_file_module_storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~wasm_file_module_storage_u() {}

            // The release of wasm_file_module_storage_u is managed by struct wasm1_element_t, there is no issue of raii resources being unreleased.
        };

        // wasm file name
        ::uwvm2::utils::container::u8cstring_view file_name{};
        // Accurate module names that must work
        ::uwvm2::utils::container::u8string_view module_name{};
        // Recording the binfmt version of the wasm module
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_ver{};
        // Memory-mapped or memory-copy (for platforms that don't support memory mapping) open wasm files
        ::fast_io::native_file_loader wasm_file{};
        // Module parsing results
        wasm_file_module_storage_u wasm_module_storage{};
        // wasm_parameter_t
        ::uwvm2::uwvm::wasm::type::wasm_parameter_t wasm_parameter{};
        // (Optional) Module name + symbol name
        ::uwvm2::parser::wasm_custom::customs::name_storage_t wasm_custom_name{};

        // Since the default is to initialize to binfmt 0, there is no need to do any constructs
        inline constexpr wasm_file_t() noexcept = default;

        inline explicit constexpr wasm_file_t(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 new_binfmt_ver) noexcept : binfmt_ver{new_binfmt_ver}
        {
            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::new(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage)) decltype(this->wasm_module_storage.wasm_binfmt_ver1_storage){};
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }
        }

        inline constexpr wasm_file_t(wasm_file_t const& other) noexcept = delete;

        inline constexpr wasm_file_t(wasm_file_t&& other) noexcept :
            file_name{::std::move(other.file_name)}, module_name{::std::move(other.module_name)}, binfmt_ver{::std::move(other.binfmt_ver)},
            wasm_file{::std::move(other.wasm_file)}, wasm_parameter{::std::move(other.wasm_parameter)}, wasm_custom_name{::std::move(other.wasm_custom_name)}
        {
            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::new(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage)) decltype(this->wasm_module_storage.wasm_binfmt_ver1_storage){
                        ::std::move(other.wasm_module_storage.wasm_binfmt_ver1_storage)};
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }
        }

        inline constexpr wasm_file_t& operator= (wasm_file_t const& other) noexcept = delete;

        inline constexpr wasm_file_t& operator= (wasm_file_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::std::destroy_at(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage));
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }

            this->file_name = ::std::move(other.file_name);
            this->module_name = ::std::move(other.module_name);
            this->binfmt_ver = ::std::move(other.binfmt_ver);
            this->wasm_file = ::std::move(other.wasm_file);
            this->wasm_parameter = ::std::move(other.wasm_parameter);
            this->wasm_custom_name = ::std::move(other.wasm_custom_name);

            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::new(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage)) decltype(this->wasm_module_storage.wasm_binfmt_ver1_storage){
                        ::std::move(other.wasm_module_storage.wasm_binfmt_ver1_storage)};
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }

            return *this;
        }

        inline constexpr ~wasm_file_t()
        {
            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::std::destroy_at(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage));
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }
        }

        inline constexpr void change_binfmt_ver(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 ver) noexcept
        {
            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::std::destroy_at(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage));
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }

            this->binfmt_ver = ver;

            switch(this->binfmt_ver)
            {
                case 1u:
                {
                    ::new(::std::addressof(this->wasm_module_storage.wasm_binfmt_ver1_storage)) decltype(this->wasm_module_storage.wasm_binfmt_ver1_storage){};
                    break;
                }
                /// @todo wasm component module: judge
                [[unlikely]] default:
                {
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
                    break;
                }
            }
        }

        template <::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_ver>
        inline constexpr auto& get_curr_binfmt_version_wasm_storage() noexcept
        {
            static_assert(binfmt_ver == 1u, "Unsupported binfmt version");
            static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            /// @todo wasm component module: judge
            if constexpr(binfmt_ver == 1u) { return this->wasm_module_storage.wasm_binfmt_ver1_storage; }
        }

        template <::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_ver>
        inline constexpr auto const& get_curr_binfmt_version_wasm_storage() const noexcept
        {
            static_assert(binfmt_ver == 1u, "Unsupported binfmt version");
            static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            /// @todo wasm component module: judge
            if constexpr(binfmt_ver == 1u) { return this->wasm_module_storage.wasm_binfmt_ver1_storage; }
        }
    };
}  // namespace uwvm2::uwvm::wasm::storage

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::wasm::type::wasm_file_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
