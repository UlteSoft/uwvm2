/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-12
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
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

struct Sec0
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec0"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{0};

    // Expand on Sec1 here
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec0>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test0\n");
}

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
struct Sec1
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec1"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{1};

    // Expand on Sec1 here
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec1<Fs...>>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test1\n");
}

struct Feature1
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"Feature1"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using binfmt_ver1_section_type = ::uwvm2::utils::container::tuple<Sec0, Sec1<Fs...>>;
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
struct Sec2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{2};

    // Expand on Sec2 here
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec2<Fs...>>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test2\n");
}

struct Sec3
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec3"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{3};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec3>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test3\n");
}

struct Feature2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"Feature2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using binfmt_ver1_section_type = ::uwvm2::utils::container::tuple<Sec2<Fs...>, Sec3>;
};

struct Feature3
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"Feature3"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
inline constexpr auto define_wasm_binfmt_parsering_strategy(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<Feature1>,
                                                            ::uwvm2::utils::container::tuple<Fs...>) noexcept
{
    return ::std::addressof(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Fs...>);
}

/*
root:                    wasm
                          |
                         /
                        /
binfmt:           binfmt_ver1(binfmt1)
                 /             |        \.
featurs:       Feature1     Feature2   Feature3
               /    \         /     \
sections:   Sec0 Sec1(ext) Sec2(ext) Sec3
*/

int main()
{
    /// @brief All feature
    constexpr ::uwvm2::utils::container::tuple<Feature1, Feature2, Feature3> all_features{};
    // [[maybe_unused]] using all_feature_t = decltype(all_features);

    /// @brief wasm binfmt ver1 features
    using wasm_binfmt_ver1_features_t =
        decltype(::uwvm2::parser::wasm::concepts::operation::get_specified_binfmt_feature_tuple_from_all_features_tuple<1>(all_features));
    constexpr wasm_binfmt_ver1_features_t wasm_binfmt1_features{};
    using wasm_binfmt_ver1_module_storage_t = decltype(::uwvm2::parser::wasm::concepts::operation::get_module_storage_type_from_tuple(wasm_binfmt1_features));
    using wasm_binfmt1_feature_parameter_t = decltype(::uwvm2::parser::wasm::concepts::get_feature_parameter_type_from_tuple(wasm_binfmt1_features));
    wasm_binfmt1_feature_parameter_t wasm_binfmt1_feature_parameter{};

    [[maybe_unused]] constexpr auto binfmt_ver1_handler{
        ::uwvm2::parser::wasm::concepts::operation::get_binfmt_handler_func_p_from_tuple<1>(wasm_binfmt1_features)};

    wasm_binfmt_ver1_module_storage_t wasm_module1{};

    ::uwvm2::parser::wasm::base::error_impl e{};

    ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t wasm_order{};

    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec0:\n");
    ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                   0,
                                                                                   nullptr,
                                                                                   nullptr,
                                                                                   e,
                                                                                   wasm_binfmt1_feature_parameter,
                                                                                   wasm_order,
                                                                                   nullptr);

    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec1:\n");
    ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                   1,
                                                                                   nullptr,
                                                                                   nullptr,
                                                                                   e,
                                                                                   wasm_binfmt1_feature_parameter,
                                                                                   wasm_order,
                                                                                   nullptr);
    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec2:\n");
    ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                   2,
                                                                                   nullptr,
                                                                                   nullptr,
                                                                                   e,
                                                                                   wasm_binfmt1_feature_parameter,
                                                                                   wasm_order,
                                                                                   nullptr);
    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec3:\n");
    ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                   3,
                                                                                   nullptr,
                                                                                   nullptr,
                                                                                   e,
                                                                                   wasm_binfmt1_feature_parameter,
                                                                                   wasm_order,
                                                                                   nullptr);
    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec4:\n");
    try
    {
        ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                       4,
                                                                                       nullptr,
                                                                                       nullptr,
                                                                                       e,
                                                                                       wasm_binfmt1_feature_parameter,
                                                                                       wasm_order,
                                                                                       nullptr);
    }
    catch(::fast_io::error e)
    {
        ::fast_io::perr(::uwvm2::uwvm::io::u8log_output, u8"test4, not found\n");
    }
}

/*

(stderr)

sec1:
test1

sec2:
test2

sec3:
test3

sec4:
test4, not found

*/

// macro
#include <uwvm2/utils/macro/pop_macros.h>
