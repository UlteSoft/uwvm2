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
    template <typename T>
    struct local_imported_module_reserve_type_t
    {
        // use for adl
        static_assert(::std::is_same_v<::std::remove_cvref_t<T>, T>,
                      "local_imported_module_reserve_type_t: typename 'T' cannot have refer and const attributes");
        explicit constexpr local_imported_module_reserve_type_t() noexcept = default;
    };

    template <typename T>
    inline constexpr local_imported_module_reserve_type_t<T> local_imported_module_reserve_type{};

    /// @brief   check if the type is a local imported module
    template <typename T>
    concept is_local_imported_module =
        requires(T&& t) { requires ::std::same_as<::std::remove_cvref_t<decltype(t.module_name)>, ::uwvm2::utils::container::u8string_view>; };

    /// @brief   check if the type can be initialized as a local imported module
    /// @details This function will be called during import initialization.
    template <typename T>
    concept can_init_local_imported_module = requires(T&& t) {
        { init_local_imported_module_define(local_imported_module_reserve_type<::std::remove_cvref_t<T>>, t) } -> ::std::same_as<bool>;
    };

    template <is_local_imported_module T>
    inline constexpr bool init_local_imported_module(T && t) noexcept
    {
        if constexpr(can_init_local_imported_module<T>)
        {
            return init_local_imported_module_define(local_imported_module_reserve_type<::std::remove_cvref_t<T>>, ::std::forward<T>(t));
        }
        else
        {
            return true;
        }
    }

    namespace details
    {
        struct local_imported_module_base_impl
        {
            virtual inline constexpr ~local_imported_module_base_impl() noexcept = default;
            virtual inline constexpr local_imported_module_base_impl* clone() const noexcept = 0;

            virtual inline constexpr bool init_local_imported_module() noexcept = 0;
        };

        template <is_local_imported_module T>
        struct local_imported_module_derv_impl final : local_imported_module_base_impl
        {
            using rcvmod_type = ::std::remove_cvref_t<T>;

            rcvmod_type module;

            inline constexpr local_imported_module_derv_impl(T&& input_module) noexcept : module{::std::forward<T>(input_module)} {}

            virtual inline constexpr local_imported_module_base_impl* clone() const noexcept override
            {
                using Alloc = ::fast_io::native_global_allocator;
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
                if consteval
# else
                if(__builtin_is_constant_evaluated())
# endif
                {
                    return new local_imported_module_derv_impl<T>{*this};
                }
                else
#endif
                {
                    local_imported_module_base_impl* ptr{
                        reinterpret_cast<local_imported_module_base_impl*>(Alloc::allocate(sizeof(local_imported_module_derv_impl<T>)))};
                    ::new(ptr) local_imported_module_derv_impl<T>{*this};
                    return ptr;
                }
            };

            virtual inline constexpr bool init_local_imported_module() noexcept override
            {
                if constexpr(can_init_local_imported_module<T>)
                {
                    return init_local_imported_module_define(local_imported_module_reserve_type<rcvmod_type>, module);
                }
                else
                {
                    return true;
                }
            }
        };
    }  // namespace details

    struct local_imported_module
    {
        using Alloc = ::fast_io::native_global_allocator;

        details::local_imported_module_base_impl* ptr{};

        constexpr local_imported_module() noexcept = default;

        template <is_local_imported_module T>
        constexpr local_imported_module(T&& module) noexcept
        {
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
            if consteval
# else
            if(__builtin_is_constant_evaluated())
# endif
            {
                this->ptr = new details::local_imported_module_derv_impl<T>{::std::forward<T>(module)};
            }
            else
#endif
            {
                this->ptr =
                    reinterpret_cast<details::local_imported_module_derv_impl<T>*>(Alloc::allocate(sizeof(details::local_imported_module_derv_impl<T>)));
                ::new(this->ptr) details::local_imported_module_derv_impl<T>{::std::forward<T>(module)};
            }
        };

        constexpr local_imported_module(local_imported_module const& other) noexcept
        {
            if(other.ptr) [[likely]] { this->ptr = other.ptr->clone(); }
        }

        constexpr local_imported_module& operator= (local_imported_module const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
                if consteval
# else
                if(__builtin_is_constant_evaluated())
# endif
                {
                    delete this->ptr;
                }
                else
#endif
                {
                    ::std::destroy_at(this->ptr);
                    Alloc::deallocate(this->ptr);
                }
            }

            if(other.ptr) [[likely]] { this->ptr = other.ptr->clone(); }
            else
            {
                this->ptr = nullptr;
            }

            return *this;
        }

        // only copy node ptr
        constexpr local_imported_module& copy_with_node_ptr(local_imported_module const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
                if consteval
# else
                if(__builtin_is_constant_evaluated())
# endif
                {
                    delete this->ptr;
                }
                else
#endif
                {
                    ::std::destroy_at(this->ptr);
                    Alloc::deallocate(this->ptr);
                }
            }

            if(other.ptr) [[likely]] { this->ptr = other.ptr->clone(); }
            else
            {
                this->ptr = nullptr;
            }

            return *this;
        }

        constexpr local_imported_module(local_imported_module&& other) noexcept : ptr{other.ptr} { other.ptr = nullptr; }

        constexpr local_imported_module& operator= (local_imported_module&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
                if consteval
# else
                if(__builtin_is_constant_evaluated())
# endif
                {
                    delete ptr;
                }
                else
#endif
                {
                    ::std::destroy_at(this->ptr);
                    Alloc::deallocate(this->ptr);
                }
            }

            this->ptr = other.ptr;
            other.ptr = nullptr;

            return *this;
        }

        constexpr ~local_imported_module() { clear(); }

        constexpr void clear() noexcept
        {
            if(this->ptr != nullptr)
            {
#if (__cpp_if_consteval >= 202106L || __cpp_lib_is_constant_evaluated >= 201811L) && __cpp_constexpr_dynamic_alloc >= 201907L
# if __cpp_if_consteval >= 202106L
                if consteval
# else
                if(__builtin_is_constant_evaluated())
# endif
                {
                    delete this->ptr;
                }
                else
#endif
                {
                    ::std::destroy_at(this->ptr);
                    Alloc::deallocate(this->ptr);
                }
            }
            this->ptr = nullptr;
        }
    };

}  // namespace uwvm2::uwvm::wasm::type

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::wasm::type::local_imported_module>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::wasm::type::local_imported_module>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
