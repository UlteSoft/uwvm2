/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
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

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1_feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1_feature = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1_feature, wasm1p1_feature>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1_feature, wasm1p1_feature>;

    inline constexpr void enable_all_wasm1p1_features(fs_para_t& fs_para) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.enable_multi_value = true;
        para.enable_reference_types = true;
        para.enable_bulk_memory = true;
        para.enable_sign_extension = true;
        para.enable_nontrapping_float_to_int = true;
        para.enable_simd = true;
        para.controllable_allow_multi_result_vector = false;
        para.controllable_allow_multi_table = false;
    }
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    if(data == nullptr) { return 0; }

    auto const* begin = reinterpret_cast<::std::byte const*>(data);
    auto const* end = begin + size;

    fs_para_t fs_para{};
    enable_all_wasm1p1_features(fs_para);

    ::uwvm2::parser::wasm::base::error_impl parse_err{};
    module_storage_t module_storage{};
    try
    {
        module_storage =
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, parse_err, fs_para);
    }
    catch(::fast_io::error const&)
    {
        return 0;
    }

    if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return 0; }

    auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};
    auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

    auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};

    for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
    {
        auto const& code{codesec.codes.index_unchecked(local_idx)};
        auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::validation::error::code_validation_error_impl v_err{};
        try
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(
                ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                module_storage,
                import_func_count + local_idx,
                code_begin_ptr,
                code_end_ptr,
                v_err,
                fs_para);
        }
        catch(::fast_io::error const&)
        {
            // Expected on invalid function bodies; libFuzzer will keep mutating.
        }
    }

    return 0;
}
