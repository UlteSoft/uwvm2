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

static inline ::std::pair<::std::uint32_t, ::std::size_t> read_uleb(::std::uint8_t const* p, ::std::size_t n)
{
    ::std::uint32_t v{}; ::std::size_t i{}; ::std::uint32_t shift{};
    while(i < n && i < 5){ ::std::uint8_t b = p[i++]; v |= static_cast<::std::uint32_t>(b & 0x7Fu) << shift; if(!(b & 0x80u)) break; shift += 7u; }
    return {v, i};
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    auto const* begin = reinterpret_cast<::std::byte const*>(data);
    auto const* end = begin + size;

    ::uwvm2::parser::wasm::base::error_impl err{};
    ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
    ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature> strg{};

    // allow one imported func & table so indices can be in-range
    auto& importsec = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>(strg.sections);
    // Reserve capacity before using push_back_unchecked to avoid writing to null storage
    importsec.importdesc.index_unchecked(0uz).reserve(1uz);
    importsec.importdesc.index_unchecked(1uz).reserve(1uz);
    importsec.importdesc.index_unchecked(0uz).push_back_unchecked(nullptr); // func
    importsec.importdesc.index_unchecked(1uz).push_back_unchecked(nullptr); // table

    if(size == 0) { return 0; }

    try
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::final_element_type_t<Feature> fet{};
        // decode first LEB as elem table index (kind)
        auto const [tbl, used] = read_uleb(data, size);
        fet.type = static_cast<decltype(fet.type)>(tbl);
        auto const* curr = used < size ? begin + used : end;

        (void)::uwvm2::parser::wasm::standard::wasm1::features::define_handler_element_type<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Feature>>{},
            fet.storage,
            fet.type,
            strg,
            curr,
            end,
            err,
            fs_para);
    }
    catch(...){ }

    return 0;
}


