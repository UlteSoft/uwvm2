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

    // allow potential global_get 0 by providing one imported global slot
    auto& importsec = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>(strg.sections);
    importsec.importdesc.index_unchecked(3uz).push_back_unchecked(nullptr);

    if(size == 0) { return 0; }

    try
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Feature> lg{};
        auto const* after_type = ::uwvm2::parser::wasm::standard::wasm1::features::global_section_global_handler<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Feature>>{},
            lg.global,
            strg,
            begin,
            end,
            err,
            fs_para);

        (void)::uwvm2::parser::wasm::standard::wasm1::features::parse_and_check_global_expr_valid<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Feature>>{},
            lg.global,
            lg.expr,
            strg,
            after_type,
            end,
            err,
            fs_para);
    }
    catch(...){ }

    return 0;
}


