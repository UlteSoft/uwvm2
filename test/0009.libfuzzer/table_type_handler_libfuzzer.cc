/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/base/impl.h>
#else
# error "Module testing is not currently supported"
#endif

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    auto const* begin = reinterpret_cast<::std::byte const*>(data);
    auto const* end = begin + size;

    ::uwvm2::parser::wasm::base::error_impl err{};
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
    ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> strg{};

    if(size == 0) { return 0; }

    try
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::final_table_type<Feature> table{};
        (void)::uwvm2::parser::wasm::standard::wasm1::features::table_section_table_handler<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Feature>>{},
            table,
            strg,
            begin,
            end,
            err,
            fs_para);
    }
    catch(...){ }

    return 0;
}


