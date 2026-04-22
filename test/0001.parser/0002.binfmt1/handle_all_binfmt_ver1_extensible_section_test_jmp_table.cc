/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @details     This example is used to test whether a jump table can be generated instead of a continuous if judgment
 * @version     2.0.0
 * @date        2025-04-13
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

#pragma push_macro("GNU_NOINLINE")
#undef GNU_NOINLINE
#if __has_cpp_attribute(__gnu__::__noinline__)
# define GNU_NOINLINE [[__gnu__::__noinline__]]
#endif

struct Sec0
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec0"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{0};

    // Expand on Sec1 here
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
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
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
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
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
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
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
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

struct Sec4
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec4"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{4};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec4>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test4\n");
}

struct Sec5
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec5"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{5};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec5>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test5\n");
}

struct Sec6
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec6"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{6};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec6>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test6\n");
}

struct Sec7
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec7"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{7};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec7>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test7\n");
}

struct Sec8
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec8"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{8};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec8>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test8\n");
}

struct Sec9
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec9"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{9};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec9>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test9\n");
}

struct Sec10
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec10"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{10};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec10>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test10\n");
}

struct Sec11
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec11"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{11};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec11>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test11\n");
}

struct Sec12
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec12"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{12};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec12>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test12\n");
}

struct Sec13
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec13"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{13};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec13>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test13\n");
}

struct Sec14
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec14"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{14};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec14>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test14\n");
}

struct Sec15
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec15"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{15};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec15>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test15\n");
}

struct Sec16
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec16"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{16};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec16>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test16\n");
}

struct Sec17
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec17"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{17};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec17>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test17\n");
}

struct Sec18
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec18"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{18};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec18>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test18\n");
}

struct Sec19
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec19"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{19};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec19>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test19\n");
}

struct Sec20
{
    inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Sec20"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{20};

    // Unexpandable section
};

template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
GNU_NOINLINE inline constexpr void handle_binfmt_ver1_extensible_section_define(
    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec20>>,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    [[maybe_unused]] ::std::byte const* section_begin,
    [[maybe_unused]] ::std::byte const* section_end,
    ::uwvm2::parser::wasm::base::error_impl&,
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const&,
    [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
    ::std::byte const* const)
{
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8log_output, u8"test20\n");
}

struct Feature2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"Feature2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using binfmt_ver1_section_type = ::uwvm2::utils::container::
        tuple<Sec2<Fs...>, Sec3, Sec4, Sec5, Sec6, Sec7, Sec8, Sec9, Sec10, Sec11, Sec12, Sec13, Sec14, Sec15, Sec16, Sec17, Sec18, Sec19, Sec20>;
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
                 |          /      \
sections:      Sec1(ext) Sec2(ext) Sec3
*/

int main(int argc, char**)
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

    ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t wasm_order{};

    ::uwvm2::parser::wasm::base::error_impl e{};

    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"sec", argc, u8":\n");
    try
    {
        ::uwvm2::parser::wasm::binfmt::ver1::handle_all_binfmt_ver1_extensible_section(wasm_module1,
                                                                                       argc,
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


*/

// macro
#pragma pop_macro("GNU_NOINLINE")
#include <uwvm2/utils/macro/pop_macros.h>
