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

    // Prepare minimal type section for func handler: allow few typeidx
    {
        auto& typesec = ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Feature>>(strg.sections);
        typesec.types.resize(4u);
    }

    // choose importdesc kind by first byte
    ::std::uint8_t const kind = data[0] & 3u; // 0..3
    auto const* curr = size > 1u ? begin + 1 : begin;

    try
    {
        switch(kind)
        {
            case 0: // func -> read typeidx
            {
                ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Feature> const* funcptr{};
                (void)::uwvm2::parser::wasm::standard::wasm1::features::extern_imports_func_handler<Feature>(
                    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>{},
                    funcptr,
                    strg,
                    curr,
                    end,
                    err,
                    fs_para);
                break;
            }
            case 1: // table
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::table_type tab{};
                (void)::uwvm2::parser::wasm::standard::wasm1::features::extern_imports_table_handler<Feature>(
                    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>{},
                    tab,
                    strg,
                    curr,
                    end,
                    err,
                    fs_para);
                break;
            }
            case 2: // memory
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::memory_type mem{};
                (void)::uwvm2::parser::wasm::standard::wasm1::features::extern_imports_memory_handler<Feature>(
                    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>{},
                    mem,
                    strg,
                    curr,
                    end,
                    err,
                    fs_para);
                break;
            }
            default: // global
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::global_type g{};
                (void)::uwvm2::parser::wasm::standard::wasm1::features::extern_imports_global_handler<Feature>(
                    ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                        ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Feature>>{},
                    g,
                    strg,
                    curr,
                    end,
                    err,
                    fs_para);
                break;
            }
        }
    }
    catch(...){ }

    return 0;
}


