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

    // make each category have one available index so in-range indices exist
    auto& importsec = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>(strg.sections);
    // Reserve capacity before using push_back_unchecked to avoid writing to null storage
    importsec.importdesc.index_unchecked(0uz).reserve(1uz);
    importsec.importdesc.index_unchecked(1uz).reserve(1uz);
    importsec.importdesc.index_unchecked(2uz).reserve(1uz);
    importsec.importdesc.index_unchecked(3uz).reserve(1uz);
    importsec.importdesc.index_unchecked(0uz).push_back_unchecked(nullptr); // func
    importsec.importdesc.index_unchecked(1uz).push_back_unchecked(nullptr); // table
    importsec.importdesc.index_unchecked(2uz).push_back_unchecked(nullptr); // memory
    importsec.importdesc.index_unchecked(3uz).push_back_unchecked(nullptr); // global

    // choose exportdesc kind and feed LEB index from payload
    ::std::uint8_t const kind = data[0] & 3u; // 0..3
    auto const* curr = size > 1u ? begin + 1 : begin;

    try
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_export_type<Feature> fwet{};
        fwet.type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::external_types>(kind);

        (void)::uwvm2::parser::wasm::standard::wasm1::features::define_handler_export_index<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Feature>>{},
            fwet.storage,
            fwet.type,
            strg,
            curr,
            end,
            err,
            fs_para);
    }
    catch(...){ }

    return 0;
}


