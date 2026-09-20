/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/features/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm2
{
    using wasm2_code_version = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_code_version;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm2_code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // ROS intentionally shares the mature eager wasm1p1 validator. Its gates
        // read the split Wasm 2.0 policy fields, so this is a policy adapter rather
        // than a second code generator or a separate deferred validation path.
        auto wasm2_fs_para{fs_para};
        auto& wasm2_para{
            ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm2_fs_para)};
        wasm2_para.cli_mode =
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::direct_wasm2;
        ::uwvm2::validation::standard::wasm1p1::validate_code(
            ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
            module_storage,
            function_index,
            code_begin,
            code_end,
            err,
            wasm2_fs_para);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm2_code_version code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> fs_para{};
        ::uwvm2::validation::standard::wasm2::validate_code(
            code_version, module_storage, function_index, code_begin, code_end, err, fs_para);
    }

    [[nodiscard]] inline constexpr bool use_wasm2_runtime_validation_strategy(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_binfmt1p1_feature_parameter const& para) noexcept
    {
        using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;
        return para.cli_mode == cli_mode::direct_wasm2 || para.cli_mode == cli_mode::scoped;
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code_with_runtime_policy(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        if(use_wasm2_runtime_validation_strategy(para))
        {
            ::uwvm2::validation::standard::wasm2::validate_code(
                wasm2_code_version{}, module_storage, function_index, code_begin, code_end, err, fs_para);
        }
        else
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(
                ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                module_storage,
                function_index,
                code_begin,
                code_end,
                err,
                fs_para);
        }
    }
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm1_code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm1::validate_code(
            wasm1_code_version{}, module_storage, function_index, code_begin, code_end, err);
    }
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm1p1_code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm1p1::validate_code(
            wasm1p1_code_version{}, module_storage, function_index, code_begin, code_end, err, fs_para);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm1p1_code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm1p1::validate_code(
            wasm1p1_code_version{}, module_storage, function_index, code_begin, code_end, err);
    }
}

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm2::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm2_code_version code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm2::validate_code(
            code_version, module_storage, function_index, code_begin, code_end, err, fs_para);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_code(
        wasm2_code_version code_version,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        ::uwvm2::validation::standard::wasm2::validate_code(
            code_version, module_storage, function_index, code_begin, code_end, err);
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
