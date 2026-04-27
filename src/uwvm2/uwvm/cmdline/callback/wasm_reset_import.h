/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-28
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    namespace wasm_reset_import_details
    {
        inline constexpr void print_usage_error() noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_reset_import),
                                u8"\n\n");
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::utf::utf_error_code validate_wasm_name(
            ::uwvm2::utils::container::u8string_view name) noexcept
        {
            auto const [pos, err]{
                ::uwvm2::uwvm::wasm::feature::handle_text_format(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                 name.cbegin(),
                                                                 name.cend())};
            static_cast<void>(pos);
            return err;
        }

        inline constexpr void print_invalid_utf8_error(::uwvm2::utils::container::u8string_view field,
                                                       ::uwvm2::utils::container::u8string_view value,
                                                       ::uwvm2::utils::utf::utf_error_code err) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid ",
                                field,
                                u8": \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                value,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". WebAssembly names must be valid UTF-8. Reason: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                ::uwvm2::utils::utf::get_utf_error_description<char8_t>(err),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_reset_import),
                                u8"\n\n");
        }

        [[nodiscard]] inline constexpr bool validate_field(::uwvm2::utils::container::u8string_view field,
                                                           ::uwvm2::utils::container::u8string_view value) noexcept
        {
            auto const err{validate_wasm_name(value)};
            if(err == ::uwvm2::utils::utf::utf_error_code::success) [[likely]] { return true; }
            print_invalid_utf8_error(field, value, err);
            return false;
        }
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type wasm_reset_import_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                      para_begin,
                                                                                  ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                  ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto currp1{para_curr + 1u};
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            wasm_reset_import_details::print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto currp2{currp1 + 1u};
        if(currp2 == para_end || currp2->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            wasm_reset_import_details::print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto currp3{currp2 + 1u};
        if(currp3 == para_end || currp3->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            wasm_reset_import_details::print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto currp4{currp3 + 1u};
        if(currp4 == para_end || currp4->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            wasm_reset_import_details::print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto currp5{currp4 + 1u};
        if(currp5 == para_end || currp5->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            wasm_reset_import_details::print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp2->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp3->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp4->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp5->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto const module_name{::uwvm2::utils::container::u8string_view{currp1->str}};
        auto const import_module_name{::uwvm2::utils::container::u8string_view{currp2->str}};
        auto const import_extern_name{::uwvm2::utils::container::u8string_view{currp3->str}};
        auto const new_import_module_name{::uwvm2::utils::container::u8string_view{currp4->str}};
        auto const new_import_extern_name{::uwvm2::utils::container::u8string_view{currp5->str}};

        if(!wasm_reset_import_details::validate_field(u8"module name", module_name) ||
           !wasm_reset_import_details::validate_field(u8"import module name", import_module_name) ||
           !wasm_reset_import_details::validate_field(u8"import extern name", import_extern_name) ||
           !wasm_reset_import_details::validate_field(u8"new import module name", new_import_module_name) ||
           !wasm_reset_import_details::validate_field(u8"new import extern name", new_import_extern_name)) [[unlikely]]
        {
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        static_cast<void>(::uwvm2::uwvm::wasm::storage::upsert_configured_import_reset(
            module_name, import_module_name, import_extern_name, new_import_module_name, new_import_extern_name));

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
