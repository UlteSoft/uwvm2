/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-12
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import parser.wasm.base;
import parser.wasm.concepts;
import parser.wasm.standard.wasm1.type;
import parser.wasm.binfmt.binfmt_ver1;
import uwvm.io;
import uwvm.wasm.storage;
#else
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

int main()
{
    {
        ::fast_io::obuf_file cf{u8"error_code_test_c.log"};
        ::fast_io::wobuf_file wcf{u8"error_code_test_wc.log"};
        ::fast_io::u8obuf_file u8cf{u8"error_code_test_u8c.log"};
        ::fast_io::u16obuf_file u16cf{u8"error_code_test_u16c.log"};
        ::fast_io::u32obuf_file u32f{u8"error_code_test_u32c.log"};
        ::uwvm2::parser::wasm::base::error_output_t errout;
        errout.err.err_selectable.u64 = 0xcdcdcdcdcdcdcdcd;
        for(::std::uint_least32_t i{}; i != static_cast<::std::uint_least32_t>(::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_not_match_the_actual_number) + 1u; ++i)
        {
            errout.err.err_code = static_cast<::uwvm2::parser::wasm::base::wasm_parse_error_code>(i);
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
