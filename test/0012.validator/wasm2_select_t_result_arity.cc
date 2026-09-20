/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly 2.0 typed-select result arity validation
 * @details     The select_t immediate is a vector whose length must be exactly one.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/concepts/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;

    // The second local function contains select_t with an empty result-type vector (0x1c 0x00).
    inline constexpr ::std::uint8_t module_bytes[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u, 0x03u, 0x03u,
        0x02u, 0x00u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x02u, 0x07u, 0x06u, 0x01u, 0x02u, 0x74u, 0x69u, 0x00u,
        0x01u, 0x09u, 0x05u, 0x01u, 0x01u, 0x00u, 0x01u, 0x00u, 0x0au, 0x14u, 0x02u, 0x02u, 0x00u, 0x0bu, 0x0fu, 0x00u,
        0x00u, 0x41u, 0x41u, 0x01u, 0x1cu, 0x00u, 0x41u, 0x70u, 0x00u, 0x5cu, 0x00u, 0x0du, 0x00u, 0x0bu};

    [[nodiscard]] int run_test() noexcept
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(module_bytes)};
        auto const* const end{begin + sizeof(module_bytes)};
        fs_para_t fs_para{};

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        module_storage_t module_storage{};
        try
        {
            module_storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, end, parse_err, fs_para);
        }
        catch(::fast_io::error const&)
        {
            return 1;
        }
        if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return 2; }

        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module_storage.sections)};
        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module_storage.sections)};
        if(codesec.codes.size() != 2uz) { return 3; }

        auto const& code{codesec.codes.index_unchecked(1uz)};
        ::uwvm2::validation::error::code_validation_error_impl validation_err{};
        try
        {
            ::uwvm2::validation::concepts::dispatch_validate_code(
                module_storage,
                importsec.importdesc.index_unchecked(0u).size() + 1uz,
                reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                reinterpret_cast<::std::byte const*>(code.body.code_end),
                validation_err,
                fs_para);
        }
        catch(::fast_io::error const&)
        {
            return validation_err.err_code == ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate ? 0 : 4;
        }
        return 5;
    }
}

int main()
{
    return run_test();
}
