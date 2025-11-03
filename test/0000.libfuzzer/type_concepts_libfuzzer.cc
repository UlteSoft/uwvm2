/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

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

    auto const b0 = data[0];

    // check value_type concept (fast, constant time)
    try
    {
        (void)::uwvm2::parser::wasm::standard::wasm1::features::define_check_typesec_value_type<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Feature>>{},
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(b0));
    }
    catch(...){ }

    // dispatch by prefix (ensure curr not before begin; allow curr==end)
    try
    {
        if(b0 == 0x60u && size > 1u)
        {
            (void)::uwvm2::parser::wasm::standard::wasm1::features::handle_type_prefix_functype<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Feature>>{},
                strg,
                begin + 1,
                end,
                err,
                fs_para);
        }
        else
        {
            auto const prefix = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::function_type_prefix>(b0);
            (void)::uwvm2::parser::wasm::standard::wasm1::features::define_type_prefix_handler<Feature>(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Feature>>{},
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Feature>>{},
                prefix,
                strg,
                size > 1u ? begin + 1 : begin,
                end,
                err,
                fs_para,
                begin);
        }
    }
    catch(...){ }

    // optional duplicate-types checker (no heavy work if typesec is empty)
    try
    {
        ::uwvm2::parser::wasm::standard::wasm1::features::define_check_duplicate_types<Feature>(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Feature>>{},
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                ::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Feature>>{},
            strg,
            begin,
            end,
            err,
            fs_para);
    }
    catch(...){ }

    return 0;
}


