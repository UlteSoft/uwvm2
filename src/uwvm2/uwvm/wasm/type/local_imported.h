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
# include <vector>
# include <algorithm>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
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

    /// @brief   check if the type is a valid feature_list
    /// @details The type must have a specialization of feature_list_traits<>.
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
        inline static constexpr element_type values[length]{vals...};
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
                                // For all versions, only i32, i64, f32, f64, and v128 are supported.
                                if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32))
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>{};
                                }
                                else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64))
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>{};
                                }
                                else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32))
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>{};
                                }
                                else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64))
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>{};
                                }
                                else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128))
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<
                                        ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>{};
                                }
                                else
                                {
                                    static_assert(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                                                  "unsupported global value type");
                                }
                            }
                        }.template operator()<vals...[I]>()),  // This is an overloaded comma expression
                    ...);
            }(::std::make_index_sequence<tuple_size>{});
        }
    }

    template <typename FeatureList, auto... vals>
        requires is_feature_list<FeatureList> && (::std::same_as<decltype(vals), feature_list_final_value_type_t<FeatureList>> && ...)
    using import_function_result_tuple_t = decltype(get_import_function_result_tuple(wasm_value_container<FeatureList, vals...>{}))::Type;

    template <typename FeatureList, auto... vals>
        requires is_feature_list<FeatureList> && (::std::same_as<decltype(vals), feature_list_final_value_type_t<FeatureList>> && ...)
    inline consteval auto get_import_function_parameter_tuple(wasm_value_container<FeatureList, vals...>) noexcept
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
    using import_function_parameter_tuple_t = decltype(get_import_function_parameter_tuple(wasm_value_container<FeatureList, vals...>{}))::Type;

    template <typename Res, typename Params>
        requires (::fast_io::is_tuple<Res> && ::fast_io::is_tuple<Params>)
    struct local_imported_function_type_t
    {
        using result_type = Res;
        using parameter_type = Params;

        result_type res{};
        parameter_type params{};
    };

    /// @brief   check if the type is a local imported function type
    /// @details T is local_imported_function_type_t<..., ...>
    template <typename T>
    concept is_local_imported_function_type_t =
        requires {
            typename ::std::remove_cvref_t<T>::result_type;
            typename ::std::remove_cvref_t<T>::parameter_type;
        } && ::fast_io::is_tuple<typename ::std::remove_cvref_t<T>::result_type> && ::fast_io::is_tuple<typename ::std::remove_cvref_t<T>::parameter_type> &&
        ::std::same_as<::std::remove_cvref_t<T>,
                       local_imported_function_type_t<typename ::std::remove_cvref_t<T>::result_type, typename ::std::remove_cvref_t<T>::parameter_type>>;

    /// @brief   check if the type is a local imported function type
    /// @details
    /// ```cpp
    ///
    /// // external inline constexpr wasm_feature{wasip1{}, ...};
    ///
    /// template <typename FeatureList>
    /// struct func_A
    /// {
    ///     using res_type = import_function_result_tuple_t<FeatureList, value_type::i32>;
    ///     using para_type = import_function_result_tuple_t<FeatureList, value_type::i32, value_type::i64, value_type::i64, value_type::f32, value_type::i32>;
    ///
    ///     using local_imported_function_type = local_imported_function_type_t<res_type, para_type>; // check this
    /// };
    /// ```
    template <typename SingleFunction>
    concept has_local_imported_function_type = requires { typename ::std::remove_cvref_t<SingleFunction>::local_imported_function_type; } &&
                                               is_local_imported_function_type_t<typename ::std::remove_cvref_t<SingleFunction>::local_imported_function_type>;

    /// @brief   check if the type has a function call
    /// @details
    /// ```cpp
    ///
    /// // external inline constexpr wasm_feature{wasip1{}, ...};
    ///
    /// template <typename FeatureList>
    /// struct func_A
    /// {
    ///     using res_type = import_function_result_tuple_t<FeatureList, value_type::i32>;
    ///     using para_type = import_function_result_tuple_t<FeatureList, value_type::i32, value_type::i64, value_type::i64, value_type::f32, value_type::i32>;
    ///     using local_imported_function_type = local_imported_function_type_t<res_type, para_type>;
    ///
    ///     inline static constexpr void call(local_imported_function_type& func_type) noexcept { ... } // check this
    /// };
    /// ```
    template <typename SingleFunction>
    concept has_function_call =
        has_local_imported_function_type<SingleFunction> && requires(typename ::std::remove_cvref_t<SingleFunction>::local_imported_function_type& func_type) {
            { ::std::remove_cvref_t<SingleFunction>::call(func_type) } -> ::std::same_as<void>;
        };

    /// @brief   check if the type has a function name
    /// @details
    /// ```cpp
    /// struct func_A
    /// {
    ///     inline static constexpr ::uwvm2::utils::container::u8string_view function_name{"my_function"}; // check this
    /// };
    /// ```
    template <typename SingleFunction>
    concept has_function_name =
        requires { requires ::std::same_as<::std::remove_cvref_t<decltype(SingleFunction::function_name)>, ::uwvm2::utils::container::u8string_view>; };

    /// @brief   check if the type is a local imported function
    /// @details equivalent to `has_local_imported_function_type<SingleFunction> && has_function_name<SingleFunction> && has_function_call<SingleFunction>`
    template <typename SingleFunction>
    concept is_local_imported_function =
        has_local_imported_function_type<SingleFunction> && has_function_name<SingleFunction> && has_function_call<SingleFunction>;

    namespace details
    {
        template <typename T>
        struct is_local_imported_function_tuple_impl : ::std::false_type
        {
        };

        template <typename... Ts>
        struct is_local_imported_function_tuple_impl<::uwvm2::utils::container::tuple<Ts...>> : ::std::bool_constant<(is_local_imported_function<Ts> && ...)>
        {
        };
    }  // namespace details

    /// @brief   check if the type is a local imported function tuple
    /// @details This is a "tuple of functions" concept: all elements must satisfy is_local_imported_function.
    /// @note    The tuple type is expected to be ::uwvm2::utils::container::tuple<...>.
    template <typename T>
    concept is_local_imported_function_tuple =
        ::fast_io::is_tuple<::std::remove_cvref_t<T>> && details::is_local_imported_function_tuple_impl<::std::remove_cvref_t<T>>::value;

    /// @brief   check if LocalImport provides a valid local function list
    /// @details
    /// ```cpp
    /// struct my_local_import_module
    /// {
    ///     // A type alias (not a value) that lists all provided local imported functions.
    ///     using local_function_tuple = ::uwvm2::utils::container::tuple<func_A, func_B, ...>;
    /// };
    /// ```
    /// The tuple's element types must all satisfy is_local_imported_function.
    template <typename LocalImport>
    concept has_local_function_tuple = requires { typename ::std::remove_cvref_t<LocalImport>::local_function_tuple; } &&
                                       is_local_imported_function_tuple<typename ::std::remove_cvref_t<LocalImport>::local_function_tuple>;

    /// @brief   check has memory name
    /// @details
    /// ```cpp
    /// struct memory_1
    /// {
    ///     inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{"my_memory"}; // check this
    /// };
    /// ```
    template <typename SingleMemory>
    concept has_memory_name =
        requires { requires ::std::same_as<::std::remove_cvref_t<decltype(SingleMemory::memory_name)>, ::uwvm2::utils::container::u8string_view>; };

    /// @brief   check has page size
    /// @note    If the concept is unsatisfied, the default assumption is 64Kib.
    /// @note    The page size is 2 raised to the power of n.
    /// @details
    /// ```cpp
    /// struct memory_1
    /// {
    ///     inline static constexpr ::std::uint_least64_t page_size{64 * 1024}; // check this
    /// };
    /// ```
    template <typename SingleMemory>
    concept has_page_size = requires {
        requires ::std::same_as<::std::remove_cvref_t<decltype(SingleMemory::page_size)>, ::std::uint_least64_t>;
        requires ::std::has_single_bit(SingleMemory::page_size);
    };

    template <typename SingleMemory>
    concept can_manipulate_memory = requires(SingleMemory& mem, ::std::uint_least64_t grow_page_size) {
        /// @brief   grow the memory
        /// @details The memory will be grown by `grow_page_size` pages (delta), not to an absolute size.
        /// @note    When `grow_page_size == 1`, this means growing by exactly one WASM page.
        ///          The default WASM page size is 64KiB, but it can be customized via `SingleMemory::page_size` (see `has_page_size`).
        { memory_grow(mem, grow_page_size) } -> ::std::same_as<bool>;
        /// @brief   get the begin pointer of the memory
        /// @details The begin pointer of the memory.
        /// @note    The begin pointer is the first byte of the memory.
        { memory_begin(mem) } -> ::std::same_as<::std::byte*>;
        /// @brief   get the size of the memory
        /// @details The size of the memory.
        /// @note    The size is the number of pages in the memory. The byte size is `memory_size(mem) * page_size`.
        { memory_size(mem) } -> ::std::same_as<::std::uint_least64_t>;
    };

    /// @brief   check if the type is a local imported memory
    /// @details equivalent to `has_memory_name<SingleMemory> && can_manipulate_memory<SingleMemory>`
    /// @note    Non-mandatory has_page_size
    template <typename SingleMemory>
    concept is_local_imported_memory = has_memory_name<SingleMemory> && can_manipulate_memory<SingleMemory>;

    namespace details
    {
        template <typename T>
        struct is_local_imported_memory_tuple_impl : ::std::false_type
        {
        };

        template <typename... Ts>
        struct is_local_imported_memory_tuple_impl<::uwvm2::utils::container::tuple<Ts...>> : ::std::bool_constant<(is_local_imported_memory<Ts> && ...)>
        {
        };
    }  // namespace details

    /// @brief   check if the type is a local imported memory tuple
    /// @details This is a "tuple of functions" concept: all elements must satisfy is_local_imported_function.
    /// @note    The tuple type is expected to be ::uwvm2::utils::container::tuple<...>.
    template <typename T>
    concept is_local_imported_memory_tuple =
        ::fast_io::is_tuple<::std::remove_cvref_t<T>> && details::is_local_imported_memory_tuple_impl<::std::remove_cvref_t<T>>::value;

    /// @brief   check if LocalImport provides a valid local memory list
    /// @details
    /// ```cpp
    /// struct my_local_import_module
    /// {
    ///     // A type alias (not a value) that lists all provided local imported memories.
    ///     using local_memory_tuple = ::uwvm2::utils::container::tuple<memory_A, memory_B, ...>;
    /// };
    /// ```
    /// The tuple's element types must all satisfy is_local_imported_memory.
    template <typename LocalImport>
    concept has_local_memory_tuple = requires { typename ::std::remove_cvref_t<LocalImport>::local_memory_tuple; } &&
                                     is_local_imported_memory_tuple<typename ::std::remove_cvref_t<LocalImport>::local_memory_tuple>;

    template <typename FeatureList, auto val>
        requires is_feature_list<FeatureList> && ::std::same_as<decltype(val), feature_list_final_value_type_t<FeatureList>>
    inline consteval auto get_import_global_value_type(wasm_value_container<FeatureList, val>) noexcept
    {
        using final_value_type = feature_list_final_value_type_t<FeatureList>;

        if constexpr(::std::same_as<final_value_type, ::uwvm2::parser::wasm::standard::wasm1::type::value_type>)
        {
            // wasm1.0: i32 i64 f32 f64
            if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32{};
            }
            else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64)
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64{};
            }
            else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32)
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32{};
            }
            else if constexpr(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64)
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64{};
            }
            else
            {
                static_assert(val == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64, "invalid value type");
            }
        }
        else
        {
            // For all versions, only i32, i64, f32, f64, and v128 are supported.
            if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32))
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32{};
            }
            else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64))
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64{};
            }
            else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32))
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32{};
            }
            else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64))
            {
                return ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64{};
            }
            else if constexpr(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128))
            {
                return ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128{};
            }
            else
            {
                static_assert(val == static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128),
                              "unsupported global value type");
            }
        }
    }

    /// @brief   check has global name
    /// @details
    /// ```cpp
    /// struct global_1
    /// {
    ///     inline static constexpr ::uwvm2::utils::container::u8string_view global_name{"my_global"}; // check this
    /// };
    /// ```
    template <typename SingleGlobal>
    concept has_global_name =
        requires { requires ::std::same_as<::std::remove_cvref_t<decltype(SingleGlobal::global_name)>, ::uwvm2::utils::container::u8string_view>; };

    /// @brief   check has global mutable
    /// @note    If not provided, it is assumed to be immutable by default.
    /// @details
    /// ```cpp
    /// struct global_1
    /// {
    ///     inline static constexpr bool is_mutable{true}; // check this
    /// };
    /// ```
    template <typename SingleGlobal>
    concept has_global_mutable = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(SingleGlobal::is_mutable)>, bool>; };

    /// @brief   check is local imported global value type
    template <typename T>
    concept is_local_imported_global_value_type = ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32> ||
                                                  ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64> ||
                                                  ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32> ||
                                                  ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64> ||
                                                  ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>;

    /// @brief   check has global value type
    /// @details
    /// ```cpp
    /// struct global_1
    /// {
    ///     using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32; // check this
    /// };
    /// ```
    template <typename SingleGlobal>
    concept has_global_value_type = requires { typename ::std::remove_cvref_t<SingleGlobal>::value_type; } &&
                                    is_local_imported_global_value_type<typename ::std::remove_cvref_t<SingleGlobal>::value_type>;

    /// @brief   check has global get
    /// @details
    /// ```cpp
    /// struct global_1
    /// {
    ///     using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    ///     inline static constexpr value_type global_get(global_1& g) noexcept { ... } // check this (ADL)
    /// };
    template <typename SingleGlobal>
    concept has_global_get = has_global_value_type<SingleGlobal> && requires(SingleGlobal& g) {
        { global_get(g) } -> ::std::same_as<typename ::std::remove_cvref_t<SingleGlobal>::value_type>;
    };

    /// @brief   check has global set
    /// @details
    /// ```cpp
    /// struct global_1
    /// {
    ///     using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    ///     inline static constexpr void global_set(global_1& g, value_type v) noexcept { ... } // check this (ADL)
    /// };
    template <typename SingleGlobal>
    concept has_global_set = has_global_value_type<SingleGlobal> && requires(SingleGlobal& g, typename ::std::remove_cvref_t<SingleGlobal>::value_type v) {
        { global_set(g, v) } -> ::std::same_as<void>;
    };

    /// @brief   check can set global value
    /// @details If the global is mutable, it can be set.
    template <typename SingleGlobal>
    concept can_set_global_value = has_global_set<SingleGlobal> && has_global_mutable<SingleGlobal> && requires { requires SingleGlobal::is_mutable == true; };

    /// @brief   check if the type is a local imported global
    /// @details equivalent to `has_global_name<SingleGlobal> && has_global_value_type<SingleGlobal> && has_global_get<SingleGlobal>`
    /// @note    Non-mandatory can_set_global_value
    template <typename SingleGlobal>
    concept is_local_imported_global = has_global_name<SingleGlobal> && has_global_value_type<SingleGlobal> && has_global_get<SingleGlobal>;

    namespace details
    {
        template <typename T>
        struct is_local_imported_global_tuple_impl : ::std::false_type
        {
        };

        template <typename... Ts>
        struct is_local_imported_global_tuple_impl<::uwvm2::utils::container::tuple<Ts...>> : ::std::bool_constant<(is_local_imported_global<Ts> && ...)>
        {
        };
    }  // namespace details

    /// @brief   check if the type is a local imported global tuple
    /// @details This is a "tuple of functions" concept: all elements must satisfy is_local_imported_function.
    /// @note    The tuple type is expected to be ::uwvm2::utils::container::tuple<...>.
    template <typename T>
    concept is_local_imported_global_tuple =
        ::fast_io::is_tuple<::std::remove_cvref_t<T>> && details::is_local_imported_global_tuple_impl<::std::remove_cvref_t<T>>::value;

    /// @brief   check if LocalImport provides a valid local global list
    /// @details
    /// ```cpp
    /// struct my_local_import_module
    /// {
    ///     // A type alias (not a value) that lists all provided local imported globals.
    ///     using local_global_tuple = ::uwvm2::utils::container::tuple<global_A, global_B, ...>;
    /// };
    /// ```
    /// The tuple's element types must all satisfy is_local_imported_global.
    template <typename LocalImport>
    concept has_local_global_tuple = requires { typename ::std::remove_cvref_t<LocalImport>::local_global_tuple; } &&
                                     is_local_imported_global_tuple<typename ::std::remove_cvref_t<LocalImport>::local_global_tuple>;

    template <typename LocalImport>
    inline consteval bool has_duplicate_exported_name_impl() noexcept
    {
        using rcvmod_type = ::std::remove_cvref_t<LocalImport>;
        using name_type = ::uwvm2::utils::container::u8string_view;

        ::std::vector<name_type> names{};
        ::std::size_t total_count{};

        if constexpr(has_local_function_tuple<rcvmod_type>)
        {
            using func_tuple_type = typename rcvmod_type::local_function_tuple;
            total_count += ::fast_io::tuple_size<func_tuple_type>::value;
        }
        if constexpr(has_local_global_tuple<rcvmod_type>)
        {
            using global_tuple_type = typename rcvmod_type::local_global_tuple;
            total_count += ::fast_io::tuple_size<global_tuple_type>::value;
        }
        if constexpr(has_local_memory_tuple<rcvmod_type>)
        {
            using memory_tuple_type = typename rcvmod_type::local_memory_tuple;
            total_count += ::fast_io::tuple_size<memory_tuple_type>::value;
        }

        names.reserve(total_count);

        if constexpr(has_local_function_tuple<rcvmod_type>)
        {
            using func_tuple_type = typename rcvmod_type::local_function_tuple;
            []<typename... Ts>(::std::vector<name_type>& v, ::std::type_identity<::uwvm2::utils::container::tuple<Ts...>>) constexpr noexcept
            { (v.push_back(Ts::function_name), ...); }(names, ::std::type_identity<func_tuple_type>{});
        }
        if constexpr(has_local_global_tuple<rcvmod_type>)
        {
            using global_tuple_type = typename rcvmod_type::local_global_tuple;
            []<typename... Ts>(::std::vector<name_type>& v, ::std::type_identity<::uwvm2::utils::container::tuple<Ts...>>) constexpr noexcept
            { (v.push_back(Ts::global_name), ...); }(names, ::std::type_identity<global_tuple_type>{});
        }
        if constexpr(has_local_memory_tuple<rcvmod_type>)
        {
            using memory_tuple_type = typename rcvmod_type::local_memory_tuple;
            []<typename... Ts>(::std::vector<name_type>& v, ::std::type_identity<::uwvm2::utils::container::tuple<Ts...>>) constexpr noexcept
            { (v.push_back(Ts::memory_name), ...); }(names, ::std::type_identity<memory_tuple_type>{});
        }

        if(names.size() < 2uz) { return false; }

        ::std::sort(names.begin(), names.end(), ::uwvm2::utils::container::pred::u8string_view_less{});
        for(::std::size_t i{1uz}; i < names.size(); ++i)
        {
            if(::uwvm2::utils::container::pred::u8string_view_equal{}(names[i - 1uz], names[i])) { return true; }
        }

        return false;
    }

    template <typename LocalImport>
    concept no_duplicate_exported_name = !has_duplicate_exported_name_impl<LocalImport>();

    /// @brief   the result of getting a function type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct function_get_result_with_success_indicator_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> function_type{};
        ::uwvm2::utils::container::u8string_view function_name{};
        ::std::size_t index{};
        bool successed{};
    };

    /// @brief   the result of getting a function type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct function_get_result_t
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> function_type{};
        ::uwvm2::utils::container::u8string_view function_name{};
        ::std::size_t index{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct function_get_all_result_t
    {
        function_get_result_t<Fs...> const* begin{};
        function_get_result_t<Fs...> const* end{};
    };

    namespace details
    {
        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        struct local_imported_module_base_impl
        {
            virtual inline constexpr ~local_imported_module_base_impl() noexcept = default;
            virtual inline constexpr local_imported_module_base_impl* clone() const noexcept = 0;

            virtual inline constexpr bool init_local_imported_module() noexcept = 0;

            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
                get_function_information_from_index(::std::size_t index) const noexcept = 0;
            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
                get_function_information_from_name(::uwvm2::utils::container::u8string_view function_name) const noexcept = 0;
            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_all_result_t<Fs...> get_all_function_information() const noexcept = 0;
        };

        template <typename>
        inline constexpr bool dependent_false_v{false};

        template <typename T, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline consteval ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...> local_imported_storage_to_final_value_type() noexcept
        {
            using final_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;

            if constexpr(::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
            {
                return static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
            }
            else if constexpr(::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64);
            }
            else if constexpr(::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32);
            }
            else if constexpr(::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64);
            }
            else if constexpr(::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return static_cast<final_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128);
            }
            else
            {
                static_assert(dependent_false_v<T>, "unsupported local imported storage value type");
            }
        }

        template <typename Tuple, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs, ::std::size_t... I>
        inline consteval auto tuple_to_final_value_type_array_impl(::std::index_sequence<I...>) noexcept
        {
            using tuple_type = ::std::remove_cvref_t<Tuple>;
            using final_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;

            return ::uwvm2::utils::container::array<final_value_type, sizeof...(I)>{
                local_imported_storage_to_final_value_type<::std::remove_cvref_t<decltype(::fast_io::get<I>(::std::declval<tuple_type&>()))>, Fs...>()...};
        }

        template <typename Tuple, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline consteval auto tuple_to_final_value_type_array() noexcept
        {
            using tuple_type = ::std::remove_cvref_t<Tuple>;
            constexpr ::std::size_t tuple_size{::fast_io::tuple_size<tuple_type>::value};
            return tuple_to_final_value_type_array_impl<tuple_type, Fs...>(::std::make_index_sequence<tuple_size>{});
        }

        template <typename LocalImportedFunctionType, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        struct local_imported_function_signature_cache;

        template <typename ResTuple, typename ParaTuple, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        struct local_imported_function_signature_cache<::uwvm2::uwvm::wasm::type::local_imported_function_type_t<ResTuple, ParaTuple>, Fs...>
        {
            using final_value_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<Fs...>;
            using final_function_type = ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...>;

            inline static constexpr auto parameter_values{tuple_to_final_value_type_array<ParaTuple, Fs...>()};
            inline static constexpr auto result_values{tuple_to_final_value_type_array<ResTuple, Fs...>()};

            inline static constexpr final_function_type function_type{
                {parameter_values.data(), parameter_values.data() + parameter_values.size()},
                {result_values.data(),    result_values.data() + result_values.size()      }
            };
        };

        template <typename SingleFunction, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
            requires ::uwvm2::uwvm::wasm::type::is_local_imported_function<SingleFunction>
        inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...> make_function_get_result(::std::size_t index) noexcept
        {
            using func_type = ::std::remove_cvref_t<SingleFunction>;
            using local_func_type = typename func_type::local_imported_function_type;

            ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...> result{};
            result.function_type = local_imported_function_signature_cache<local_func_type, Fs...>::function_type;
            result.function_name = func_type::function_name;
            result.index = index;
            result.successed = true;
            return result;
        }

        template <::std::size_t N, typename FuncTuple, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
            get_function_information_from_index_impl(::std::size_t index) noexcept
        {
            using curr_tuple_type = ::std::remove_cvref_t<FuncTuple>;
            constexpr ::std::size_t tuple_size{::fast_io::tuple_size<curr_tuple_type>::value};

            if constexpr(N >= tuple_size) { return {}; }
            else
            {
                if(index != N) { return get_function_information_from_index_impl<N + 1uz, curr_tuple_type, Fs...>(index); }

                using func_type = ::std::remove_cvref_t<decltype(::fast_io::get<N>(::std::declval<curr_tuple_type&>()))>;
                return make_function_get_result<func_type, Fs...>(N);
            }
        }

        template <::std::size_t N, typename FuncTuple, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
            get_function_information_from_name_impl(::uwvm2::utils::container::u8string_view function_name) noexcept
        {
            using curr_tuple_type = ::std::remove_cvref_t<FuncTuple>;
            constexpr ::std::size_t tuple_size{::fast_io::tuple_size<curr_tuple_type>::value};

            if constexpr(N >= tuple_size) { return {}; }
            else
            {
                using func_type = ::std::remove_cvref_t<decltype(::fast_io::get<N>(::std::declval<curr_tuple_type&>()))>;

                if(function_name == func_type::function_name) { return make_function_get_result<func_type, Fs...>(N); }
                else
                {
                    return get_function_information_from_name_impl<N + 1uz, curr_tuple_type, Fs...>(function_name);
                }
            }
        }

        template <::uwvm2::uwvm::wasm::type::is_local_imported_module T, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
            requires ::uwvm2::uwvm::wasm::type::no_duplicate_exported_name<T>
        struct local_imported_module_derv_impl final : local_imported_module_base_impl<Fs...>
        {
            using rcvmod_type = ::std::remove_cvref_t<T>;

            rcvmod_type module;

            inline constexpr local_imported_module_derv_impl(T&& input_module) noexcept : module{::std::forward<T>(input_module)} {}

            virtual inline constexpr local_imported_module_base_impl<Fs...>* clone() const noexcept override
            {
                using Alloc = ::fast_io::native_global_allocator;

                if UWVM_IF_CONSTEVAL { return new local_imported_module_derv_impl<T, Fs...>{*this}; }
                else
                {
                    local_imported_module_base_impl<Fs...>* ptr{
                        reinterpret_cast<local_imported_module_base_impl<Fs...>*>(Alloc::allocate(sizeof(local_imported_module_derv_impl<T, Fs...>)))};
                    ::new(ptr) local_imported_module_derv_impl<T, Fs...>{*this};
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

            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
                get_function_information_from_index(::std::size_t index) const noexcept override
            {
                if constexpr(has_local_function_tuple<rcvmod_type>)
                {
                    using curr_func_tuple_type = typename ::std::remove_cvref_t<rcvmod_type>::local_function_tuple;
                    constexpr auto tuple_size{::fast_io::tuple_size<curr_func_tuple_type>::value};
                    if(index < tuple_size) { return get_function_information_from_index_impl<0uz, curr_func_tuple_type, Fs...>(index); }
                    else
                    {
                        return {};
                    }
                }
                else
                {
                    return {};
                }
            }

            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
                get_function_information_from_name(::uwvm2::utils::container::u8string_view function_name) const noexcept override
            {
                if constexpr(has_local_function_tuple<rcvmod_type>)
                {
                    using curr_func_tuple_type = typename ::std::remove_cvref_t<rcvmod_type>::local_function_tuple;
                    return get_function_information_from_name_impl<0uz, curr_func_tuple_type, Fs...>(function_name);
                }
                else
                {
                    return {};
                }
            }

            virtual inline constexpr ::uwvm2::uwvm::wasm::type::function_get_all_result_t<Fs...> get_all_function_information() const noexcept override 
            {
                /// @todo
                return {};
            }
        };
    }  // namespace details

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct local_imported_module
    {
        using Alloc = ::fast_io::native_global_allocator;

        details::local_imported_module_base_impl<Fs...>* ptr{};

        inline constexpr local_imported_module() noexcept = default;

        template <is_local_imported_module T>
            requires no_duplicate_exported_name<T>
        inline constexpr local_imported_module(T&& module) noexcept
        {
            if UWVM_IF_CONSTEVAL { this->ptr = new details::local_imported_module_derv_impl<T, Fs...>{::std::forward<T>(module)}; }
            else
            {
                this->ptr = reinterpret_cast<details::local_imported_module_derv_impl<T, Fs...>*>(
                    Alloc::allocate(sizeof(details::local_imported_module_derv_impl<T, Fs...>)));
                ::new(this->ptr) details::local_imported_module_derv_impl<T, Fs...>{::std::forward<T>(module)};
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

        inline constexpr bool init_local_imported_module() noexcept
        {
            if(this->ptr == nullptr) { return true; }
            return this->ptr->init_local_imported_module();
        }

        inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
            get_function_information_from_index(::std::size_t index) const noexcept
        {
            if(this->ptr == nullptr) { return {}; }
            return this->ptr->get_function_information_from_index(index);
        }

        inline constexpr ::uwvm2::uwvm::wasm::type::function_get_result_with_success_indicator_t<Fs...>
            get_function_information_from_name(::uwvm2::utils::container::u8string_view function_name) const noexcept
        {
            if(this->ptr == nullptr) { return {}; }
            return this->ptr->get_function_information_from_name(function_name);
        }

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
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::uwvm::wasm::type::local_imported_module<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::uwvm::wasm::type::local_imported_module<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
