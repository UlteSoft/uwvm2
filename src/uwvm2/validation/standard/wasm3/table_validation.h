/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/validation/error/impl.h>
# include "memory_immediate.h"
# include "address_limits.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    template<typename AddressTypeAt>
    inline constexpr void require_table64_policy(bool enabled, ::std::size_t count, AddressTypeAt&& address_type_at,
        ::std::byte const* code_begin, ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(enabled) { return; }
        for(::std::size_t index{}; index != count; ++index)
        {
            // Parsed import/local counts fit u32. The loop proves the resolver's
            // metadata index is in bounds before borrowing any declaration.
            if(address_type_at(static_cast<::std::uint_least32_t>(index)) != storage_address_type::i64) { continue; }
            // code ... (code_end)
            // unsafe (could be code_end)
            // ^^ err_curr borrows code_begin for a declaration-policy diagnostic;
            //    no opcode is read and an empty function body remains safe here.
            err.err_curr = code_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {
                .value = 4u, .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::table64,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
