/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <uwvm2/compiler/validation/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
#else
# error "Module testing is not currently supported"
#endif

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    if(data == nullptr) { return 0; }

    auto const* begin = reinterpret_cast<::std::byte const*>(data);
    auto const* end = begin + size;

    // Phase 1: parser check (must pass before running validator).
    ::uwvm2::parser::wasm::base::error_impl parse_err{};
    ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> module_storage{};
    try
    {
        module_storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(begin, end, parse_err, {});
    }
    catch(::fast_io::error const&)
    {
        return 0;
    }

    auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>(module_storage.sections)};
    auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

    auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Feature>>(module_storage.sections)};

    // Phase 2: validate each local function body.
    for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
    {
        auto const& code{codesec.codes.index_unchecked(local_idx)};
        auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::compiler::validation::error::code_validation_error_impl v_err{};
        try
        {
            ::uwvm2::compiler::validation::standard::wasm1::validate_code<Feature>(
                ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                module_storage,
                import_func_count + local_idx,
                code_begin_ptr,
                code_end_ptr,
                v_err);
        }
        catch(::fast_io::error const&)
        {
            // Expected on invalid inputs; libFuzzer will keep mutating.
        }
    }

    return 0;
}
