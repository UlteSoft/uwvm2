/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

// Included after wasm_module_storage_t is complete. Standalone includes also work.
#ifndef UWVM_MODULE
# include "wasm_module.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    namespace gc_segment_details
    {
        struct data_window
        {
            ::std::byte const* begin{};
            ::std::size_t size{};
        };

        template <typename T>
        [[nodiscard]] inline bool pointer_pair_size(T const* begin, T const* end,
                                                    ::std::size_t& count) noexcept
        {
            if(begin == nullptr || end == nullptr)
            {
                count = 0uz;
                return begin == end;
            }
            // Both pointers are an immutable module-owned segment pair. Integer
            // arithmetic avoids undefined unrelated-pointer subtraction if corrupt
            // host storage accidentally supplies a mismatched pair.
            auto const first{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const last{reinterpret_cast<::std::uintptr_t>(end)};
            if(last < first || (last - first) % sizeof(T) != 0uz) { return false; }
            count = (last - first) / sizeof(T);
            return count <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(T);
        }

        [[nodiscard]] inline gc_object_status data_at(wasm_module_storage_t const* module,
            ::std::uint_least32_t index, data_window& result) noexcept
        {
            if(module == nullptr) { return gc_object_status::invalid_store; }
            if(static_cast<::std::size_t>(index) >= module->local_defined_data_vec_storage.size())
            { return gc_object_status::invalid_type; }
            // [0, segment count) the parser/initializer created this live record.
            auto const& record{module->local_defined_data_vec_storage.index_unchecked(index).data};
            auto const payload{load_wasm_data_segment_payload(record)};
            ::std::size_t count{};
            if(!pointer_pair_size(payload.byte_begin, payload.byte_end, count))
            { return gc_object_status::invalid_value; }
            result = {payload.byte_begin, count};
            return gc_object_status::ok;
        }

        enum class element_kind : unsigned char { empty, func_index, funcref, externref, gc_ref };
        struct element_window
        {
            wasm_element_payload_t payload{};
            element_kind kind{element_kind::empty};
            ::std::size_t size{};
        };

        [[nodiscard]] inline gc_object_status element_at(wasm_module_storage_t const* module,
            ::std::uint_least32_t index, element_window& result) noexcept
        {
            if(module == nullptr) { return gc_object_status::invalid_store; }
            if(static_cast<::std::size_t>(index) >= module->local_defined_element_vec_storage.size())
            { return gc_object_status::invalid_type; }
            // [0, segment count) this initialized immutable segment record is live.
            auto const& record{module->local_defined_element_vec_storage.index_unchecked(index).element};
            auto const payload{load_wasm_element_segment_payload(record)};
            ::std::size_t func_count{}, ref_count{}, extern_count{}, gc_ref_count{};
            if(!pointer_pair_size(payload.funcidx_begin, payload.funcidx_end, func_count) ||
               !pointer_pair_size(payload.funcref_begin, payload.funcref_end, ref_count) ||
               !pointer_pair_size(payload.externref_begin, payload.externref_end, extern_count) ||
               !pointer_pair_size(payload.gc_ref_begin, payload.gc_ref_end, gc_ref_count))
            { return gc_object_status::invalid_value; }
            auto const nonempty{static_cast<unsigned>(func_count != 0uz) +
                                static_cast<unsigned>(ref_count != 0uz) +
                                static_cast<unsigned>(extern_count != 0uz) +
                                static_cast<unsigned>(gc_ref_count != 0uz)};
            if(nonempty > 1u) { return gc_object_status::invalid_value; }
            result.payload = payload;
            if(func_count != 0uz) { result.kind = element_kind::func_index; result.size = func_count; }
            else if(ref_count != 0uz) { result.kind = element_kind::funcref; result.size = ref_count; }
            else if(extern_count != 0uz) { result.kind = element_kind::externref; result.size = extern_count; }
            else if(gc_ref_count != 0uz) { result.kind = element_kind::gc_ref; result.size = gc_ref_count; }
            return gc_object_status::ok;
        }

        [[nodiscard]] inline gc_object_status element_reference(wasm_module_storage_t const* module,
            element_window const& window, ::std::size_t index, gc_reference& result) noexcept
        {
            if(index >= window.size) { return gc_object_status::out_of_bounds; }
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using table_kind = local_defined_table_elem_storage_type_t;
            result = {};
            switch(window.kind)
            {
                case element_kind::func_index:
                {
                    // [0, window.size) index selects a complete function-index entry.
                    auto const function_index{static_cast<::std::size_t>(window.payload.funcidx_begin[index])};
                    auto const imported_count{module->imported_function_vec_storage.size()};
                    auto const local_count{module->local_defined_function_vec_storage.size()};
                    if(function_index < imported_count)
                    {
                        result.storage.ptr = const_cast<void*>(static_cast<void const*>(
                            ::std::addressof(module->imported_function_vec_storage.index_unchecked(function_index))));
                        result.kind = ref_kind::wasm_func_imported;
                    }
                    else if(function_index - imported_count < local_count)
                    {
                        result.storage.ptr = const_cast<void*>(static_cast<void const*>(
                            ::std::addressof(module->local_defined_function_vec_storage.index_unchecked(function_index - imported_count))));
                        result.kind = ref_kind::wasm_func_defined;
                    }
                    else { return gc_object_status::invalid_value; }
                    return gc_object_status::ok;
                }
                case element_kind::funcref:
                {
                    // [0, window.size) index selects one canonical runtime funcref.
                    auto const& entry{window.payload.funcref_begin[index]};
                    if(entry.type == table_kind::func_ref_imported)
                    {
                        result.storage.ptr = const_cast<void*>(static_cast<void const*>(entry.storage.imported_ptr));
                        result.kind = entry.storage.imported_ptr == nullptr ? ref_kind::wasm_null : ref_kind::wasm_func_imported;
                    }
                    else if(entry.type == table_kind::func_ref_defined)
                    {
                        result.storage.ptr = const_cast<void*>(static_cast<void const*>(entry.storage.defined_ptr));
                        result.kind = entry.storage.defined_ptr == nullptr ? ref_kind::wasm_null : ref_kind::wasm_func_defined;
                    }
                    else { return gc_object_status::invalid_value; }
                    return gc_object_status::ok;
                }
                case element_kind::externref:
                {
                    // [0, window.size) index selects an opaque host reference, never dereferenced.
                    result.storage.ptr = window.payload.externref_begin[index];
                    result.kind = result.storage.ptr == nullptr ? ref_kind::wasm_null : ref_kind::wasm_extern;
                    return gc_object_status::ok;
                }
                case element_kind::gc_ref:
                {
                    // [gc_ref_begin, gc_ref_end) is immutable module-owned storage;
                    // [safe                       ] index < window.size was checked above.
                    // Preserve all 16 bytes: i31 bits and struct/array kind are not pointers.
                    result = window.payload.gc_ref_begin[index];
                    return gc_object_status::ok;
                }
                case element_kind::empty: return gc_object_status::out_of_bounds;
            }
            return gc_object_status::invalid_value;
        }

        [[nodiscard]] inline gc_object_status materialize_elements(wasm_module_storage_t const* module,
            element_window const& window, ::std::size_t offset, ::std::size_t length,
            ::std::unique_ptr<gc_reference[]>& result) noexcept
        {
            if(offset > window.size || length > window.size - offset)
            { return gc_object_status::out_of_bounds; }
            if(length == 0uz) { return gc_object_status::ok; }
            if(length > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(gc_reference))
            { return gc_object_status::size_overflow; }
            result.reset(new(::std::nothrow) gc_reference[length]{});
            if(!result) { return gc_object_status::out_of_memory; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [offset, offset+length) was checked by subtraction above.
                auto const status{element_reference(module, window, offset + i, result[i])};
                if(status != gc_object_status::ok) { return status; }
            }
            return gc_object_status::ok;
        }
    }

    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_new_data(gc_object_store* store,
        wasm_module_storage_t const* module, ::std::uint_least32_t type_index, ::std::uint_least32_t data_index,
        ::std::size_t source_offset, ::std::size_t length, gc_reference* result) noexcept
    {
        if(store == nullptr || result == nullptr) { return gc_object_status::invalid_store; }
        gc_segment_details::data_window data{};
        auto const status{gc_segment_details::data_at(module, data_index, data)};
        if(status != gc_object_status::ok) { return status; }
        return store->array_new_data(type_index, data.begin, data.size, source_offset, length, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_new_elem(gc_object_store* store,
        wasm_module_storage_t const* module, ::std::uint_least32_t type_index, ::std::uint_least32_t element_index,
        ::std::size_t source_offset, ::std::size_t length, gc_reference* result) noexcept
    {
        if(store == nullptr || result == nullptr) { return gc_object_status::invalid_store; }
        gc_segment_details::element_window element{};
        auto const status{gc_segment_details::element_at(module, element_index, element)};
        if(status != gc_object_status::ok) { return status; }
        ::std::unique_ptr<gc_reference[]> refs{};
        auto const materialized{gc_segment_details::materialize_elements(module, element, source_offset, length, refs)};
        if(materialized != gc_object_status::ok) { return materialized; }
        return store->array_new_elements(type_index, refs.get(), length, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_init_data(gc_object_store* store,
        wasm_module_storage_t const* module, ::std::uint_least32_t type_index, ::std::uint_least32_t data_index,
        gc_reference const* array, ::std::size_t destination_offset, ::std::size_t source_offset,
        ::std::size_t length) noexcept
    {
        if(store == nullptr || array == nullptr) { return gc_object_status::invalid_store; }
        gc_segment_details::data_window data{};
        auto const status{gc_segment_details::data_at(module, data_index, data)};
        if(status != gc_object_status::ok) { return status; }
        return store->array_init_data(type_index, *array, destination_offset,
                                      data.begin, data.size, source_offset, length);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_init_elem(gc_object_store* store,
        wasm_module_storage_t const* module, ::std::uint_least32_t type_index, ::std::uint_least32_t element_index,
        gc_reference const* array, ::std::size_t destination_offset, ::std::size_t source_offset,
        ::std::size_t length) noexcept
    {
        if(store == nullptr || array == nullptr) { return gc_object_status::invalid_store; }
        gc_segment_details::element_window element{};
        auto const status{gc_segment_details::element_at(module, element_index, element)};
        if(status != gc_object_status::ok) { return status; }
        ::std::unique_ptr<gc_reference[]> refs{};
        auto const materialized{gc_segment_details::materialize_elements(module, element, source_offset, length, refs)};
        if(materialized != gc_object_status::ok) { return materialized; }
        return store->array_init_elements(type_index, *array, destination_offset, refs.get(), length);
    }
}
