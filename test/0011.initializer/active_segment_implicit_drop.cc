/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <cstddef>

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload;
    using ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload;
    using ::uwvm2::uwvm::runtime::storage::wasm_data_storage_t;
    using ::uwvm2::uwvm::runtime::storage::wasm_element_storage_t;

    [[nodiscard]] inline constexpr bool element_drop_hides_every_payload() noexcept
    {
        wasm_element_storage_t::func_idx_t func_indices[1]{};
        void* extern_refs[1]{};
        wasm_element_storage_t element{};
        element.funcidx_begin = func_indices;
        element.funcidx_end = func_indices + 1;
        element.externref_begin = extern_refs;
        element.externref_end = extern_refs + 1;

        drop_wasm_element_segment_payload(element);
        auto const payload{load_wasm_element_segment_payload(element)};
        return wasm_element_segment_is_dropped(element) && payload.funcidx_begin == nullptr && payload.funcidx_end == nullptr &&
               payload.funcref_begin == nullptr && payload.funcref_end == nullptr && payload.externref_begin == nullptr && payload.externref_end == nullptr &&
               element.funcidx_begin == func_indices && element.externref_begin == extern_refs;
    }

    [[nodiscard]] inline constexpr bool data_drop_hides_payload() noexcept
    {
        ::std::byte bytes[1]{};
        wasm_data_storage_t data{};
        data.byte_begin = bytes;
        data.byte_end = bytes + 1;

        drop_wasm_data_segment_payload(data);
        auto const payload{load_wasm_data_segment_payload(data)};
        return wasm_data_segment_is_dropped(data) && payload.byte_begin == nullptr && payload.byte_end == nullptr && data.byte_begin == bytes;
    }

    static_assert(element_drop_hides_every_payload());
    static_assert(data_drop_hides_payload());
}

int main() noexcept {}
