/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-12
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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

int main()
{
    {
        ::fast_io::basic_obuf<::fast_io::u8native_io_observer> obuf_u8err{::fast_io::u8err()};

        ::fast_io::obuf_file cf{u8"error_code_test_c.log"};
        ::fast_io::wobuf_file wcf{u8"error_code_test_wc.log"};
        ::fast_io::u8obuf_file u8cf{u8"error_code_test_u8c.log"};
        ::fast_io::u16obuf_file u16cf{u8"error_code_test_u16c.log"};
        ::fast_io::u32obuf_file u32f{u8"error_code_test_u32c.log"};
        ::uwvm2::parser::wasm::base::error_output_t errout;

        for(::std::uint_least32_t i{}; i != static_cast<::std::uint_least32_t>(::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit) + 1u;
            ++i)
        {
            // Specialization of the addressing section
            switch(static_cast<::uwvm2::parser::wasm::base::wasm_parse_error_code>(i))
            {
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_imports_of_the_same_import_type:
                {
                    errout.err.err_selectable.duplic_imports.module_name = u8"module_name";
                    errout.err.err_selectable.duplic_imports.extern_name = u8"extern_name";
                    errout.err.err_selectable.duplic_imports.type = 0u;  // func
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_char_sequence:
                {
                    errout.err.err_selectable.u32 = 0x00;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max:
                {
                    errout.err.err_selectable.imp_def_num_exceed_u32max.type = 0x00;
                    errout.err.err_selectable.imp_def_num_exceed_u32max.defined = 114514;
                    errout.err.err_selectable.imp_def_num_exceed_u32max.imported = 1919810;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_exports_of_the_same_export_type:
                {
                    errout.err.err_selectable.duplic_exports.export_name = u8"test";
                    errout.err.err_selectable.duplic_exports.type = 0u;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::exported_index_exceeds_maxvul:
                {
                    errout.err.err_selectable.exported_index_exceeds_maxvul.idx = 114514;
                    errout.err.err_selectable.exported_index_exceeds_maxvul.maxval = 1919810;
                    errout.err.err_selectable.exported_index_exceeds_maxvul.type = 0;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::start_index_exceeds_maxvul:
                {
                    errout.err.err_selectable.start_index_exceeds_maxvul.idx = 114514;
                    errout.err.err_selectable.start_index_exceeds_maxvul.maxval = 1919810;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_table_index_exceeds_maxvul:
                {
                    errout.err.err_selectable.elem_table_index_exceeds_maxvul.idx = 114514;
                    errout.err.err_selectable.elem_table_index_exceeds_maxvul.maxval = 1919810;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::elem_func_index_exceeds_maxvul:
                {
                    errout.err.err_selectable.elem_func_index_exceeds_maxvul.idx = 114514;
                    errout.err.err_selectable.elem_func_index_exceeds_maxvul.maxval = 1919810;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_custom_section_order:
                {
                    errout.err.err_selectable.illegal_custom_section_order.custom_name = u8"custom_name";
                    errout.err.err_selectable.illegal_custom_section_order.custom_order = 0u;
                    errout.err.err_selectable.illegal_custom_section_order.wasm_order = 0u;
                    errout.err.err_selectable.illegal_custom_section_order.wasm_sec = 0u;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit:
                {
                    errout.err.err_selectable.exceed_the_max_parser_limit.name = u8"custom_name";
                    errout.err.err_selectable.exceed_the_max_parser_limit.value = 0u;
                    errout.err.err_selectable.exceed_the_max_parser_limit.maxval = 0u;
                    break;
                }
                case ::uwvm2::parser::wasm::base::wasm_parse_error_code::memory_section_resolved_exceeded_the_maximum_value:
                {
                    errout.err.err_selectable.u32 = 70000u;
                    break;
                }
                default:
                {
                    errout.err.err_selectable.u64 = 0xcdcdcdcdcdcdcdcd;
                    break;
                }
            }

            errout.err.err_code = static_cast<::uwvm2::parser::wasm::base::wasm_parse_error_code>(i);

            {
                ::uwvm2::parser::wasm::base::error_output_t obuf_u8err_errout{errout};
                obuf_u8err_errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                obuf_u8err_errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
#endif
                ::fast_io::io::perrln(obuf_u8err, obuf_u8err_errout);
            }

            ::fast_io::io::perrln(cf, errout);
            ::fast_io::io::perrln(wcf, errout);
            ::fast_io::io::perrln(u8cf, errout);
            ::fast_io::io::perrln(u16cf, errout);
            ::fast_io::io::perrln(u32f, errout);
        }
    }
}

/*

(stderr)

sec1:
test1

sec2:
test2

sec3:
test3

sec4:
test4, not found

*/

// macro
#include <uwvm2/utils/macro/pop_macros.h>
