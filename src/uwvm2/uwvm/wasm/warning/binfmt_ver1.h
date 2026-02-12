/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <cstdint>
# include <climits>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
# include "warn_storage.h"
# include "type_section.h"
# include "import_section.h"
# include "function_section.h"
# include "table_section.h"
# include "memory_section.h"
# include "global_section.h"
# include "export_section.h"
# include "start_section.h"
# include "element_section.h"
# include "code_section.h"
# include "data_section.h"
# include "custom_section.h"
# include "final_check.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::warning
{
    inline constexpr void show_wasm_binfmt_ver1_warning(::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm) noexcept
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"show warning for binfmt ver1"};
#endif
        // The warning path should be determined in advance.
        UWVM_ASSERT(::uwvm2::uwvm::io::show_parser_warning == true);

        ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t warn_storage{};
        ::uwvm2::uwvm::wasm::warning::init_binfmt_ver1_warning_storage(warn_storage, wasm);

        // for __builtin_expect_with_probability

        [&]<typename... Secs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Secs...> const&) constexpr noexcept -> void
        {
            (::uwvm2::uwvm::wasm::warning::show_wasm_section_warning(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<Secs>{}, wasm, warn_storage), ...);
        }(wasm.get_curr_binfmt_version_wasm_storage<1u>().sections);

        ::uwvm2::uwvm::wasm::warning::show_wasm_final_check_warning(wasm, warn_storage);
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
