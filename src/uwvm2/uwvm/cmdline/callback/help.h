/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-07
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
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/imported/wasi/feature/feature_push_macro.h>  // wasi
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    template <::std::size_t N>
    inline constexpr ::uwvm2::utils::container::array<char8_t, N + 1> u8nspace() noexcept
    {
        ::uwvm2::utils::container::array<char8_t, N + 1> res{};

        for(::std::size_t i{}; i != N; ++i) { res[i] = u8' '; }

        res[N] = u8'\0';

        return res;
    }

    // max parameter + left "  " + right "  "
    inline constexpr auto parameter_max_principal_name_size_u8nspace{u8nspace<::uwvm2::uwvm::cmdline::parameter_max_principal_name_size + 4>()};

    template <typename Stm>
    inline constexpr void help_output_singal_cate(Stm && stm, ::uwvm2::utils::cmdline::categorization cate) noexcept
    {
        for(auto const p: ::uwvm2::uwvm::cmdline::parameters)
        {
            if(p->cate != cate) { continue; }
            ::fast_io::io::perr(::std::forward<Stm>(stm),
                                u8"  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::left(p->name, ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size));
            ::fast_io::io::perrln(::std::forward<Stm>(stm),
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                  u8"  -----  ",
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                  p->describe);
            ::fast_io::io::perrln(::std::forward<Stm>(stm),
                                  parameter_max_principal_name_size_u8nspace.element,
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                  u8"Usage: ",
                                  ::uwvm2::utils::cmdline::print_usage(*p)
                                  // print_usage comes with UWVM_COLOR_U8_RST_ALL
            );
        }
    }

    template <typename Stm>
    inline constexpr void help_output_all(Stm && stm) noexcept
    {
        for(auto const p: ::uwvm2::uwvm::cmdline::parameters)
        {
            ::fast_io::io::perr(::std::forward<Stm>(stm),
                                u8"  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::left(p->name, ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size));
            ::fast_io::io::perrln(::std::forward<Stm>(stm),
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                  u8"  -----  ",
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                  p->describe);
            ::fast_io::io::perrln(::std::forward<Stm>(stm),
                                  parameter_max_principal_name_size_u8nspace.element,
                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_PURPLE),
                                  u8"Usage: ",
                                  ::uwvm2::utils::cmdline::print_usage(*p)
                                  // print_usage comes with UWVM_COLOR_U8_RST_ALL
            );
        }
    }

    #if defined(UWVM_MODULE)
extern "C++" UWVM_GNU_COLD
#else
UWVM_GNU_COLD inline constexpr
#endif 
 ::uwvm2::utils::cmdline::parameter_return_type help_callback(
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        // No copies will be made here.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        // Add raii locks while unlocking operations
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        // No copies will be made here.
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        auto currp1{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp1

        // Check for out-of-bounds and not-argument
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr] (end) ...
            // [  safe  ] unsafe (could be the module_end)
            //            ^^ currp1

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr para] ...
            // [     safe    ] unsafe (could be the module_end)
            //           ^^ currp1

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::global);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL

            // display other parameter
            ::fast_io::io::perr(u8log_output_ul,
                                // ln
                                u8"\n",
                                // debug
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  ",
                                ::fast_io::mnp::left(u8"<debug>", ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"  -----  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"--help debug",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to display the debug arguments." u8"\n\n",
                                // wasm
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  ",
                                ::fast_io::mnp::left(u8"<wasm>", ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"  -----  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"--help wasm",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to display the wasm arguments." u8"\n\n",
                                // log
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  ",
                                ::fast_io::mnp::left(u8"<log>", ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"  -----  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"--help log",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to display the log arguments." u8"\n\n",
#if defined(UWVM_IMPORT_WASI)
                                // wasi
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  ",
                                ::fast_io::mnp::left(u8"<wasi>", ::uwvm2::uwvm::cmdline::parameter_max_principal_name_size),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"  -----  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"--help wasi",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" to display the wasi arguments." u8"\n\n",
#endif
                                // endl
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            // Here, guard will perform destructors.
            return ::uwvm2::utils::cmdline::parameter_return_type::return_imme;
        }

        // [... curr arg1] ...
        // [     safe     ] unsafe (could be the module_end)
        //           ^^ currp1

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        if(auto const currp1_str{currp1->str}; currp1_str == u8"all")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n");
            help_output_all(u8log_output_ul);
            // help_output_all comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
        else if(currp1_str == u8"global")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  <global>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8":\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::global);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
        else if(currp1_str == u8"debug")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  <debug>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8":\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::debug);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
        else if(currp1_str == u8"wasm")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  <wasm>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8":\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::wasm);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
        else if(currp1_str == u8"log")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  <log>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8":\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::log);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
#if defined(UWVM_IMPORT_WASI)
        else if(currp1_str == u8"wasi")
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"Arguments:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_CYAN),
                                u8"  <wasi>",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8":\n");
            help_output_singal_cate(u8log_output_ul, ::uwvm2::utils::cmdline::categorization::wasi);
            // help_output_singal_cate comes with UWVM_COLOR_U8_RST_ALL
            ::fast_io::io::perrln(u8log_output_ul);
        }
#endif
        else
        {
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid Extra Help Name \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::help),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");

            // Here, guard will perform destructors.
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // Here, guard will perform destructors.
        return ::uwvm2::utils::cmdline::parameter_return_type::return_imme;
    }

}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/imported/wasi/feature/feature_pop_macro.h>  // wasi
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

