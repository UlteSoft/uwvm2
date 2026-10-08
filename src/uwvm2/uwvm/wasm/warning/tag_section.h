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
    /// @brief Mark function types referenced by Core 3 local tags as used.
    /// @details Tag descriptors are already bounded; this traversal adds no guest execution work.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void show_wasm_section_warning(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>,
        [[maybe_unused]] ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
        [[maybe_unused]] ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
        auto const& module{wasm.get_curr_binfmt_version_wasm_storage<1u>()};
        auto const& tags{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module.sections)};
        for(auto index : tags.type_indices)
        {
            if(index < warn_storage.unused_type_checker.size())
            { warn_storage.unused_type_checker.index_unchecked(index) = true; }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
