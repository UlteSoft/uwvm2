/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include "warn_storage.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::warning
{
    /// @brief Accept the wasm1.1 data count section in the warning traversal without emitting a warning.
    /// @details Data count consistency is a parser final-check invariant, so this section has no runtime warning state to collect.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void show_wasm_section_warning(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>,
        [[maybe_unused]] ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
        [[maybe_unused]] ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
