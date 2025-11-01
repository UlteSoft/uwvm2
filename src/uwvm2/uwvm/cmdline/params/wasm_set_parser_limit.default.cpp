/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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

// std
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/depend/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/custom/customs/impl.h>
#endif

namespace uwvm2::uwvm::cmdline::params::details
{
    UWVM_GNU_COLD extern ::uwvm2::utils::cmdline::parameter_return_type
        wasm_set_parser_limit_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
                                       ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                       ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        auto currp{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp

        // Check for out-of-bounds and not-argument
        if(currp == para_end || currp->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr] (end) ...
            // [  safe  ] unsafe (could be the module_end)
            //            ^^ currp

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr para] ...
            // [     safe    ] unsafe (could be the module_end)
            //           ^^ currp

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_parser_limit),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg1] arg2 ...
        // [     safe    ] unsafe (could be the module_end)
        //           ^^ currp

        // Setting the argument is already taken
        currp->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        // name
        auto const set_type_name{currp->str};

        ++currp;
        // [... curr arg1] arg2 ...
        // [     safe    ] unsafe (could be the module_end)
        //                 ^^ currp

        // Check for out-of-bounds and not-argument
        if(currp == para_end || currp->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr arg1] (end) ...
            // [  safe       ] unsafe (could be the module_end)
            //                 ^^ currp

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr arg1 arg2] ...
            // [     safe         ] unsafe (could be the module_end)
            //                ^^ currp

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_parser_limit),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg1 arg2] ...
        // [     safe         ] unsafe (could be the module_end)
        //                ^^ currp

        // Setting the argument is already taken
        currp->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto const limit_size_t_str{currp->str};

        ::std::size_t limit;  // No initialization necessary
        auto const [next, err]{::fast_io::parse_by_scan(limit_size_t_str.cbegin(), limit_size_t_str.cend(), limit)};

        // parse u32 error
        if(err != ::fast_io::parse_code::ok || next != limit_size_t_str.cend()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid limit (size_t): \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                limit_size_t_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_parser_limit),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // binfmt 1
        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");

        // wasm1
        auto& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>(
            ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
        auto& wasm1_parser_limit{wasm1_feapara_r.parser_limit};

        // custom name
        auto& name_parser_para{::uwvm2::uwvm::wasm::custom::customs::name_parser_param};
        auto& name_parser_limit{name_parser_para.name_parser_limit};

        if(set_type_name == u8"codesec_codes") { wasm1_parser_limit.max_code_sec_codes = limit; }
        else if(set_type_name == u8"code_locals") { wasm1_parser_limit.max_code_locals = limit; }
        else if(set_type_name == u8"datasec_entries") { wasm1_parser_limit.max_data_sec_entries = limit; }
        else if(set_type_name == u8"elemsec_funcidx") { wasm1_parser_limit.max_elem_sec_funcidx = limit; }
        else if(set_type_name == u8"elemsec_elems") { wasm1_parser_limit.max_elem_sec_elems = limit; }
        else if(set_type_name == u8"exportsec_exports") { wasm1_parser_limit.max_export_sec_exports = limit; }
        else if(set_type_name == u8"globalsec_globals") { wasm1_parser_limit.max_global_sec_globals = limit; }
        else if(set_type_name == u8"importsec_imports") { wasm1_parser_limit.max_import_sec_imports = limit; }
        else if(set_type_name == u8"memorysec_memories") { wasm1_parser_limit.max_memory_sec_memories = limit; }
        else if(set_type_name == u8"tablesec_tables") { wasm1_parser_limit.max_table_sec_tables = limit; }
        else if(set_type_name == u8"typesec_types") { wasm1_parser_limit.max_type_sec_types = limit; }
        else if(set_type_name == u8"custom_name_funcnames") { name_parser_limit.max_name_sec_function_names = limit; }
        else if(set_type_name == u8"custom_name_codelocal_funcs") { name_parser_limit.max_name_sec_code_local_functions = limit; }
        else if(set_type_name == u8"custom_name_codelocal_name_per_funcs") { name_parser_limit.max_name_sec_code_local_names_per_function = limit; }
        else
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid type: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                set_type_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\".\n" u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Available types: \n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"              - codesec_codes (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_code_sec_codes,
                                u8")\n" u8"              - code_locals (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_code_locals,
                                u8")\n" u8"              - datasec_entries (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_data_sec_entries,
                                u8")\n" u8"              - elemsec_funcidx (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_elem_sec_funcidx,
                                u8")\n" u8"              - elemsec_elems (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_elem_sec_elems,
                                u8")\n" u8"              - exportsec_exports (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_export_sec_exports,
                                u8")\n" u8"              - globalsec_globals (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_global_sec_globals,
                                u8")\n" u8"              - importsec_imports (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_import_sec_imports,
                                u8")\n" u8"              - memorysec_memories (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_memory_sec_memories,
                                u8")\n" u8"              - tablesec_tables (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_table_sec_tables,
                                u8")\n" u8"              - typesec_types (default=",
                                ::uwvm2::parser::wasm::standard::wasm1::features::default_max_type_sec_types,
                                u8")\n" u8"              - custom_name_funcnames (default=",
                                ::uwvm2::parser::wasm_custom::customs::default_max_name_sec_function_names,
                                u8")\n" u8"              - custom_name_codelocal_funcs (default=",
                                ::uwvm2::parser::wasm_custom::customs::default_max_name_sec_code_local_functions,
                                u8")\n" u8"              - custom_name_codelocal_name_per_funcs (default=",
                                ::uwvm2::parser::wasm_custom::customs::default_max_name_sec_code_local_names_per_function,
                                u8")\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

// This cpp may not be the end of the translation unit, it may be included in other cpp files. So it needs to be pop.
// macro
#include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
#include <uwvm2/utils/macro/pop_macros.h>
