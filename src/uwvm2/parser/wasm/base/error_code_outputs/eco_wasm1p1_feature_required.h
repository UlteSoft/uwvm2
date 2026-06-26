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

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// Without pragma once, this header file will be included in a specific code segment

/// @warning Extension point: keep this helper synchronized with wasm_binfmt1p1_feature_parameter and feature-gated parser diagnostics.
constexpr auto get_wasm1p1_feature_name{
    []<::std::integral char_type2>(::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type2>
    {
        switch(feature)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::multi_value:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"multi-value"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"multi-value"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"multi-value"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"multi-value"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"multi-value"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"bulk-memory"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"bulk-memory"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"bulk-memory"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"bulk-memory"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"bulk-memory"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"reference-types"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"reference-types"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"reference-types"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"reference-types"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"reference-types"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"sign-extension"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"sign-extension"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"sign-extension"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"sign-extension"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"sign-extension"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"nontrapping-float-to-int"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"nontrapping-float-to-int"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"nontrapping-float-to-int"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"nontrapping-float-to-int"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"nontrapping-float-to-int"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"simd"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"simd"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"simd"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"simd"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"simd"}; }
            }
            [[unlikely]] default:
            {
            /// @warning Extension point: reaching "unknown" here usually means a new wasm1p1 feature flag lacks ECO output.
            /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                if constexpr(::std::same_as<char_type2, char>) { return {"unknown"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"unknown"}; }
            }
        }
    }};
constexpr auto get_wasm1p1_subject_name{
    []<::std::integral char_type2>(::uwvm2::parser::wasm::base::wasm1p1_error_subject subject) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type2>
    {
        switch(subject)
        {
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_count_section:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"data count section"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"data count section"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"data count section"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"data count section"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"data count section"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"element segment"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"element segment"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"element segment"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"element segment"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"element segment"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"data segment"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"data segment"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"data segment"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"data segment"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"data segment"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_kind:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"element kind"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"element kind"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"element kind"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"element kind"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"element kind"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"reference type"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"reference type"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"reference type"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"reference type"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"reference type"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"table type"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"table type"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"table type"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"table type"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"table type"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"instruction"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"instruction"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"instruction"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"instruction"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"instruction"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"ref.null initializer"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"ref.null initializer"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"ref.null initializer"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"ref.null initializer"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"ref.null initializer"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"ref.func initializer"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"ref.func initializer"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"ref.func initializer"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"ref.func initializer"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"ref.func initializer"}; }
            }
            case ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"v128.const initializer"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"v128.const initializer"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"v128.const initializer"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"v128.const initializer"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"v128.const initializer"}; }
            }
            [[unlikely]] default:
            {
            /// @warning Extension point: new wasm1p1_error_subject values need feature-required ECO subject names here.
            /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                if constexpr(::std::same_as<char_type2, char>) { return {"unknown"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"unknown"}; }
            }
        }
    }};

if constexpr(::std::same_as<char_type, char>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             "uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             "[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             "(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             ") ",
                                                             "WebAssembly 1.1 ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             get_wasm1p1_subject_name.template operator()<char>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             " requires ",
                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                             get_wasm1p1_feature_name.template operator()<char>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             " (value ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ").",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                     "uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RED),
                                                     "[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     "(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     ") ",
                                                     "WebAssembly 1.1 ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     get_wasm1p1_subject_name.template operator()<char>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     " requires ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_LT_GREEN),
                                                     get_wasm1p1_feature_name.template operator()<char>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     " (value ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     ").",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, wchar_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             L"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             L"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             L") ",
                                                             L"WebAssembly 1.1 ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             get_wasm1p1_subject_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L" requires ",
                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                             get_wasm1p1_feature_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L" (value ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L").",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                     L"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
                                                     L"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     L") ",
                                                     L"WebAssembly 1.1 ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     get_wasm1p1_subject_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L" requires ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_LT_GREEN),
                                                     get_wasm1p1_feature_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L" (value ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L").",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char8_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             u8"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             u8"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             u8") ",
                                                             u8"WebAssembly 1.1 ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             get_wasm1p1_subject_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8" requires ",
                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                             get_wasm1p1_feature_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8" (value ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8").",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                     u8"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RED),
                                                     u8"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u8") ",
                                                     u8"WebAssembly 1.1 ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     get_wasm1p1_subject_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8" requires ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_LT_GREEN),
                                                     get_wasm1p1_feature_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8" (value ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8").",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char16_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             u"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             u"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             u") ",
                                                             u"WebAssembly 1.1 ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             get_wasm1p1_subject_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u" requires ",
                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                             get_wasm1p1_feature_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u" (value ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u").",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                     u"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RED),
                                                     u"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u") ",
                                                     u"WebAssembly 1.1 ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     get_wasm1p1_subject_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u" requires ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_LT_GREEN),
                                                     get_wasm1p1_feature_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u" (value ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u").",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char32_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             U"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             U"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             U") ",
                                                             U"WebAssembly 1.1 ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             get_wasm1p1_subject_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U" requires ",
                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                             get_wasm1p1_feature_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U" (value ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U").",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                     U"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RED),
                                                     U"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     U") ",
                                                     U"WebAssembly 1.1 ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     get_wasm1p1_subject_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_feature_required.subject),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U" requires ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_LT_GREEN),
                                                     get_wasm1p1_feature_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_feature_required.feature),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U" (value ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_feature_required.value),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U").",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}
