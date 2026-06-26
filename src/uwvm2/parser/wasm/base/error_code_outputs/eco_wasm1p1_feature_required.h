/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

// Without pragma once, this header file will be included in a specific code segment

auto const wasm1p1_print_error = [&]<typename... Args>(Args&&... args)
{
    if constexpr(::std::same_as<char_type, char>)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                         "uwvm: ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RED),
                                                         "[error] ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                         "(offset=",
                                                         ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                         ") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
    }
    else if constexpr(::std::same_as<char_type, wchar_t>)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                         L"uwvm: ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
                                                         L"[error] ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                         L"(offset=",
                                                         ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                         L") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
    }
    else if constexpr(::std::same_as<char_type, char8_t>)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                         u8"uwvm: ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RED),
                                                         u8"[error] ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                         u8"(offset=",
                                                         ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                         u8") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
    }
    else if constexpr(::std::same_as<char_type, char16_t>)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                         u"uwvm: ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RED),
                                                         u"[error] ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                         u"(offset=",
                                                         ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                         u") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
    }
    else if constexpr(::std::same_as<char_type, char32_t>)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                         U"uwvm: ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RED),
                                                         U"[error] ",
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                         U"(offset=",
                                                         ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                         U") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    }
};

auto const wasm1p1_feature_name =
    [](auto const feature) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type>
{
    if constexpr(::std::same_as<char_type, char>)
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory: return "bulk-memory";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types: return "reference-types";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd: return "simd";
            default: return "unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, wchar_t>)
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory: return L"bulk-memory";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types: return L"reference-types";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd: return L"simd";
            default: return L"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char8_t>)
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory: return u8"bulk-memory";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types: return u8"reference-types";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd: return u8"simd";
            default: return u8"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char16_t>)
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory: return u"bulk-memory";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types: return u"reference-types";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd: return u"simd";
            default: return u"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char32_t>)
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory: return U"bulk-memory";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types: return U"reference-types";
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd: return U"simd";
            default: return U"unknown";
        }
    }
};

auto const wasm1p1_subject_name =
    [](auto const subject) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type>
{
    if constexpr(::std::same_as<char_type, char>)
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section: return "data count section";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment: return "element segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment: return "data segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind: return "element kind";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type: return "reference type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type: return "table type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null: return "ref.null initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func: return "ref.func initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const: return "v128.const initializer";
            default: return "unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, wchar_t>)
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section: return L"data count section";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment: return L"element segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment: return L"data segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind: return L"element kind";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type: return L"reference type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type: return L"table type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null: return L"ref.null initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func: return L"ref.func initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const: return L"v128.const initializer";
            default: return L"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char8_t>)
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section: return u8"data count section";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment: return u8"element segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment: return u8"data segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind: return u8"element kind";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type: return u8"reference type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type: return u8"table type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null: return u8"ref.null initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func: return u8"ref.func initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const: return u8"v128.const initializer";
            default: return u8"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char16_t>)
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section: return u"data count section";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment: return u"element segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment: return u"data segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind: return u"element kind";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type: return u"reference type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type: return u"table type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null: return u"ref.null initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func: return u"ref.func initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const: return u"v128.const initializer";
            default: return u"unknown";
        }
    }
    else if constexpr(::std::same_as<char_type, char32_t>)
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section: return U"data count section";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment: return U"element segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment: return U"data segment";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind: return U"element kind";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type: return U"reference type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type: return U"table type";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null: return U"ref.null initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func: return U"ref.func initializer";
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const: return U"v128.const initializer";
            default: return U"unknown";
        }
    }
};

if constexpr(::std::same_as<char_type, char>)
{
    wasm1p1_print_error("WebAssembly 1.1 ",
                        wasm1p1_subject_name(errout.err.err_selectable.wasm1p1_feature_required.subject),
                        " requires ",
                        wasm1p1_feature_name(errout.err.err_selectable.wasm1p1_feature_required.feature),
                        " (value ",
                        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                        ").");
}
else if constexpr(::std::same_as<char_type, wchar_t>)
{
    wasm1p1_print_error(L"WebAssembly 1.1 ",
                        wasm1p1_subject_name(errout.err.err_selectable.wasm1p1_feature_required.subject),
                        L" requires ",
                        wasm1p1_feature_name(errout.err.err_selectable.wasm1p1_feature_required.feature),
                        L" (value ",
                        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                        L").");
}
else if constexpr(::std::same_as<char_type, char8_t>)
{
    wasm1p1_print_error(u8"WebAssembly 1.1 ",
                        wasm1p1_subject_name(errout.err.err_selectable.wasm1p1_feature_required.subject),
                        u8" requires ",
                        wasm1p1_feature_name(errout.err.err_selectable.wasm1p1_feature_required.feature),
                        u8" (value ",
                        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                        u8").");
}
else if constexpr(::std::same_as<char_type, char16_t>)
{
    wasm1p1_print_error(u"WebAssembly 1.1 ",
                        wasm1p1_subject_name(errout.err.err_selectable.wasm1p1_feature_required.subject),
                        u" requires ",
                        wasm1p1_feature_name(errout.err.err_selectable.wasm1p1_feature_required.feature),
                        u" (value ",
                        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                        u").");
}
else if constexpr(::std::same_as<char_type, char32_t>)
{
    wasm1p1_print_error(U"WebAssembly 1.1 ",
                        wasm1p1_subject_name(errout.err.err_selectable.wasm1p1_feature_required.subject),
                        U" requires ",
                        wasm1p1_feature_name(errout.err.err_selectable.wasm1p1_feature_required.feature),
                        U" (value ",
                        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                        U").");
}
return;
