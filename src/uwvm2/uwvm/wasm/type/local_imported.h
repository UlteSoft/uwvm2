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
# include <array>
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <new>
# include <memory>
# include <type_traits>
# include <utility>
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
    // use for adl
    template <typename T>
    struct local_imported_module_reserve_type_t
    {
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

    /// @note Since the type section of WASM may contain numerous elements unrelated to functions, a separate function type system must be employed here.
    ///       Concurrently, the function type system will only extend the base types, leaving all others unchanged.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct feature_list
    {
    };

    template <typename FeatureList>
    struct feature_list_traits;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct feature_list_traits<feature_list<Fs...>>
    {
        using final_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;
        inline static constexpr bool allow_multi_result_vector{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<Fs...>()};
    };

    template <typename FeatureList>
    concept is_feature_list = requires { typename feature_list_traits<::std::remove_cvref_t<FeatureList>>::final_value_type; };

    template <typename FeatureList>
        requires is_feature_list<FeatureList>
    using feature_list_final_value_type_t = typename feature_list_traits<::std::remove_cvref_t<FeatureList>>::final_value_type;

    template <typename FeatureList, auto... vals>
        requires is_feature_list<FeatureList> && (::std::same_as<decltype(vals), feature_list_final_value_type_t<FeatureList>> && ...)
    struct wasm_value_container
    {
        using element_type = feature_list_final_value_type_t<FeatureList>;

        inline static constexpr ::std::size_t length{sizeof...(vals)};
        inline static constexpr ::std::array<element_type, sizeof...(vals)> values{vals...};
    };

    template <typename FeatureList, auto... vals>
        requires is_feature_list<FeatureList> && (::std::same_as<decltype(vals), feature_list_final_value_type_t<FeatureList>> && ...)
    inline consteval auto get_import_function_result_tuple(wasm_value_container<FeatureList, vals...>) noexcept
    {
        constexpr bool allow_multi_value{feature_list_traits<::std::remove_cvref_t<FeatureList>>::allow_multi_result_vector};

        constexpr ::std::size_t tuple_size{sizeof...(vals)};
        if constexpr(allow_multi_value)
        {
#if __cpp_contracts >= 202502L
            contract_assert(tuple_size <= 1uz);
#else
            if(tuple_size > 1uz) { ::fast_io::fast_terminate(); }
#endif
        }

        using final_value_type = feature_list_final_value_type_t<FeatureList>;

        if constexpr(tuple_size == 0uz) { return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<>{}; }
        else
        {
            return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            {
                return (
                    (
                        []<final_value_type val>() constexpr noexcept
                        {
                            if constexpr(::std::same_as<final_value_type, ::uwvm2::parser::wasm::standard::wasm1::type::value_type>)
                            {
                                // wasm1.0: i32 i64 f32 f64
                                if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>{};
                                }
                                else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>{};
                                }
                                else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>{};
                                }
                                else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>{};
                                }
                                else
                                {
                                    static_assert(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64, "invalid value type");
                                }
                            }
                            else
                            {
                                /// @todo support v128
                                static_assert(::std::same_as<final_value_type, ::uwvm2::parser::wasm::standard::wasm1::type::value_type>, "not supported yet");
                            }
                        }.template operator()<vals...[I]>()),  // This is an overloaded comma expression
                    ...);
            }(::std::make_index_sequence<tuple_size>{});
        }
    }

    template <typename FeatureList, auto... vals>
        requires is_feature_list<FeatureList> && (::std::same_as<decltype(vals), feature_list_final_value_type_t<FeatureList>> && ...)
    using import_function_result_tuple_t = decltype(get_import_function_result_tuple(wasm_value_container<FeatureList, vals...>{}))::Type;

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

                if UWVM_IF_CONSTEVAL { return new local_imported_module_derv_impl<T>{*this}; }
                else
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

        inline constexpr local_imported_module() noexcept = default;

        template <is_local_imported_module T>
        inline constexpr local_imported_module(T&& module) noexcept
        {
            if UWVM_IF_CONSTEVAL { this->ptr = new details::local_imported_module_derv_impl<T>{::std::forward<T>(module)}; }
            else
            {
                this->ptr =
                    reinterpret_cast<details::local_imported_module_derv_impl<T>*>(Alloc::allocate(sizeof(details::local_imported_module_derv_impl<T>)));
                ::new(this->ptr) details::local_imported_module_derv_impl<T>{::std::forward<T>(module)};
            }
        };

        inline constexpr local_imported_module(local_imported_module const& other) noexcept
        {
            if(other.ptr) [[likely]] { this->ptr = other.ptr->clone(); }
        }

        inline constexpr local_imported_module& operator= (local_imported_module const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
                if UWVM_IF_CONSTEVAL { delete this->ptr; }
                else
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
        inline constexpr local_imported_module& copy_with_node_ptr(local_imported_module const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
                if UWVM_IF_CONSTEVAL { delete this->ptr; }
                else
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

        inline constexpr local_imported_module(local_imported_module&& other) noexcept : ptr{other.ptr} { other.ptr = nullptr; }

        inline constexpr local_imported_module& operator= (local_imported_module&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->ptr != nullptr)
            {
                if UWVM_IF_CONSTEVAL { delete this->ptr; }
                else
                {
                    ::std::destroy_at(this->ptr);
                    Alloc::deallocate(this->ptr);
                }
            }

            this->ptr = other.ptr;
            other.ptr = nullptr;

            return *this;
        }

        inline constexpr ~local_imported_module() { clear(); }

        inline constexpr void clear() noexcept
        {
            if(this->ptr != nullptr)
            {
                if UWVM_IF_CONSTEVAL { delete this->ptr; }
                else
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
