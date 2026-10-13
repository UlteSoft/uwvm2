/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <type_traits>
# include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    enum class gc_static_root_status : unsigned char
    {
        ok, invalid_cohort, invalid_storage, incomplete_imports,
        host_roots_required, uninitialized_global, rejected_reference, size_overflow
    };

    struct gc_static_root_result
    {
        gc_static_root_status status{gc_static_root_status::ok};
        ::std::size_t visited{};
    };

    namespace gc_static_root_details
    {
        using cohort = ::std::span<wasm_module_storage_t const* const>;

        template<class Record, class Member>
        [[nodiscard]] inline bool contains(cohort modules, Record const* candidate, Member member) noexcept
        {
            if(candidate == nullptr) { return false; }
            for(auto const* module : modules)
            {
                // [strongly leased, quiescent native module records]
                // [safe                                             ] No guest
                // pointer is dereferenced: compare exact native record identities.
                for(auto const& record : module->*member)
                { if(::std::addressof(record) == candidate) { return true; } }
            }
            return false;
        }

        [[nodiscard]] inline bool reference_shape(gc_reference const& value) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(value.kind)
            {
                case kind::wasm_null: case kind::wasm_i31:
                case kind::wasm_func_imported: case kind::wasm_func_defined:
                case kind::wasm_extern: case kind::wasm_exn:
                case kind::wasm_struct: case kind::wasm_array: return true;
                // Parser-only function indices are never runtime references.
                case kind::wasm_func: return false;
            }
            return false;
        }

        template<class Element, class Vector>
        [[nodiscard]] inline bool payload_slice(Vector const& owned, Element const* begin,
                                                Element const* end, ::std::size_t& offset,
                                                ::std::size_t& length) noexcept
        {
            offset = length = 0uz;
            if(begin == nullptr || end == nullptr) { return begin == end; }
            if(owned.empty()) { return false; }
            constexpr auto host_limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
            if(owned.size() > host_limit / sizeof(Element)) { return false; }
            auto const base{reinterpret_cast<::std::uintptr_t>(owned.data())};
            auto const first{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const last{reinterpret_cast<::std::uintptr_t>(end)};
            auto const extent{owned.size() * sizeof(Element)};
            if(base > (::std::numeric_limits<::std::uintptr_t>::max)() - extent ||
               first < base || last < first || last - base > extent ||
               (first - base) % sizeof(Element) != 0uz || (last - first) % sizeof(Element) != 0uz)
            { return false; }
            // [owned allocation ... first ... last ... allocation end]
            // [safe                                                   ] Compare
            // integer identities first; never subtract unrelated native pointers.
            // Reconstruct every read through the owning vector, not the supplied
            // begin/end pointers. Empty one-past slices perform no dereference.
            offset = static_cast<::std::size_t>((first - base) / sizeof(Element));
            length = static_cast<::std::size_t>((last - first) / sizeof(Element));
            return true;
        }

        [[nodiscard]] inline gc_static_root_status check_imports(cohort modules) noexcept
        {
            ::std::size_t table_count{}, global_count{};
            for(auto const* module : modules)
            {
                auto const tables{module->imported_table_vec_storage.size()};
                auto const globals{module->imported_global_vec_storage.size()};
                if(tables > SIZE_MAX - table_count || globals > SIZE_MAX - global_count)
                { return gc_static_root_status::size_overflow; }
                table_count += tables;
                global_count += globals;
            }
            for(auto const* module : modules)
            {
                for(auto const& imported : module->imported_table_vec_storage)
                {
                    auto const* current{::std::addressof(imported)};
                    ::std::size_t hops{};
                    for(;;)
                    {
                        // [cohort-owned import record] current is proven native
                        // storage before its discriminant/active union is read.
                        if(hops++ == table_count || !contains(modules, current,
                            &wasm_module_storage_t::imported_table_vec_storage))
                        { return gc_static_root_status::incomplete_imports; }
                        using link = imported_table_storage_t::imported_table_link_kind;
                        if(current->link_kind == link::defined)
                        {
                            if(!contains(modules, current->target.defined_ptr,
                                &wasm_module_storage_t::local_defined_table_vec_storage))
                            { return gc_static_root_status::incomplete_imports; }
                            break; // Its actual owner is enumerated once below.
                        }
                        if(current->link_kind != link::imported)
                        { return gc_static_root_status::incomplete_imports; }
                        // [current live record] -> [next cohort record or invalid]
                        // [safe               ] Only copy this pointer; prove
                        // exact membership at the next iteration before reading it.
                        current = current->target.imported_ptr;
                    }
                }
                for(auto const& imported : module->imported_global_vec_storage)
                {
                    auto const* current{::std::addressof(imported)};
                    ::std::size_t hops{};
                    for(;;)
                    {
                        // [cohort-owned native import record] check membership
                        // before following an alias or inspecting its active union.
                        if(hops++ == global_count || !contains(modules, current,
                            &wasm_module_storage_t::imported_global_vec_storage))
                        { return gc_static_root_status::incomplete_imports; }
                        using link = imported_global_storage_t::imported_global_link_kind;
                        if(current->link_kind == link::defined)
                        {
                            if(!contains(modules, current->target.defined_ptr,
                                &wasm_module_storage_t::local_defined_global_vec_storage))
                            { return gc_static_root_status::incomplete_imports; }
                            break;
                        }
                        if(current->link_kind == link::local_imported)
                        {
                            // A privileged host provider can keep VM tokens
                            // outside module storage. Its root registrations and
                            // quiescence must be implemented before admitting it;
                            // invoking an arbitrary provider under a stop lock is
                            // neither root enumeration nor proof of its lifetime.
                            return current->target.local_imported.module_ptr == nullptr ?
                                gc_static_root_status::incomplete_imports : gc_static_root_status::host_roots_required;
                        }
                        if(current->link_kind != link::imported)
                        { return gc_static_root_status::incomplete_imports; }
                        // [live import record] -> [candidate next record]
                        // [safe             ] Verify before dereferencing next.
                        current = current->target.imported_ptr;
                    }
                }
            }
            return gc_static_root_status::ok;
        }
    }

    // Exact static roots for an admitted, strongly leased module cohort. ALL
    // guest/native readers, table/global mutators, element initializers and
    // teardown must be stopped throughout the call. This helper does not pause,
    // collect, discover an omitted module, validate token membership, or retain
    // a native handle. Activation/host/exception roots must be supplied elsewhere.
    // A visitor receives a complete carrier by value, never a borrowing pointer.
    // It must not reenter, mutate, sweep, retain a storage view or throw; reject
    // invalid token membership before any collector commits. A partial failed
    // visitation is not a usable root set. This code adds no guest hot-path work.
    template<class Visitor>
    [[nodiscard]] inline gc_static_root_result visit_quiescent_cohort_static_roots(
        ::std::span<wasm_module_storage_t const* const> modules, Visitor&& visitor) noexcept
    {
        static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, gc_reference>);
        gc_static_root_result result{};
        auto const fail{[&](gc_static_root_status status) noexcept
        { result.status = status; return result; }};
        for(::std::size_t index{}; index != modules.size(); ++index)
        {
            // [caller-owned native cohort] Each pointer must be a strongly leased
            // module object; a raw/guest address cannot confer that authority.
            if(modules[index] == nullptr) { return fail(gc_static_root_status::invalid_cohort); }
            for(::std::size_t previous{}; previous != index; ++previous)
            { if(modules[previous] == modules[index]) { return fail(gc_static_root_status::invalid_cohort); } }
        }
        auto const imports{gc_static_root_details::check_imports(modules)};
        if(imports != gc_static_root_status::ok) { return fail(imports); }
        auto const visit{[&](gc_reference reference) noexcept
        {
            if(result.visited == SIZE_MAX)
            { result.status = gc_static_root_status::size_overflow; return false; }
            if(!gc_static_root_details::reference_shape(reference) || !visitor(reference))
            { result.status = gc_static_root_status::rejected_reference; return false; }
            ++result.visited;
            return true;
        }};
        for(auto const* module : modules)
        {
            for(auto const& global : module->local_defined_global_vec_storage)
            {
                if(global.init_state != wasm_global_init_state::initialized)
                { return fail(gc_static_root_status::uninitialized_global); }
                // Only the initialized native type tag selects the union member.
                // Numeric/vector bytes, even if identical to a VM token, are not
                // roots. Do not scan global storage or Wasm linear memory.
                using global_kind = ::uwvm2::object::global::global_type;
                switch(global.global.kind)
                {
                    case global_kind::wasm_ref:
                        if(!visit(global.global.storage.ref)) { return result; }
                        break;
                    case global_kind::wasm_i32: case global_kind::wasm_i64:
                    case global_kind::wasm_f32: case global_kind::wasm_f64:
                    case global_kind::wasm_v128: break;
                    default: return fail(gc_static_root_status::invalid_storage);
                }
            }
            for(auto const& table : module->local_defined_table_vec_storage)
            {
                for(auto const& slot : table.elems)
                {
                    if(static_cast<unsigned>(slot.type) > static_cast<unsigned>(
                        local_defined_table_elem_storage_type_t::gc_array_ref))
                    { return fail(gc_static_root_status::invalid_storage); }
                    // The codec selects only the active typed slot payload;
                    // complete null/i31/extern/exn carriers stay distinguishable.
                    if(!visit(runtime_table_slot_to_gc_reference(slot))) { return result; }
                }
            }
            for(auto const& record : module->local_defined_element_vec_storage)
            {
                auto const& element{record.element};
                if(wasm_element_segment_is_dropped(element)) { continue; }
                // Initialization selects exactly one payload representation.
                // Reject mixed/incomplete categories before treating a native
                // backing vector as the published contents of this instance.
                auto const representations{
                    static_cast<unsigned>(element.funcidx_begin != nullptr || element.funcidx_end != nullptr) +
                    static_cast<unsigned>(element.funcref_begin != nullptr || element.funcref_end != nullptr) +
                    static_cast<unsigned>(element.externref_begin != nullptr || element.externref_end != nullptr) +
                    static_cast<unsigned>(element.gc_ref_begin != nullptr || element.gc_ref_end != nullptr)};
                if(representations > 1u || ((element.funcidx_begin == nullptr) != (element.funcidx_end == nullptr)))
                { return fail(gc_static_root_status::invalid_storage); }
                ::std::size_t offset{}, length{};
                if(!gc_static_root_details::payload_slice(module->element_expr_gc_ref_vec_storage,
                    element.gc_ref_begin, element.gc_ref_end, offset, length))
                { return fail(gc_static_root_status::invalid_storage); }
                for(::std::size_t index{}; index != length; ++index)
                {
                    // [owned complete reference payload] offset+index < size,
                    // proved without arithmetic on the untrusted pointer pair.
                    if(!visit(module->element_expr_gc_ref_vec_storage.index_unchecked(offset + index))) { return result; }
                }
                if(!gc_static_root_details::payload_slice(module->element_expr_funcref_vec_storage,
                    element.funcref_begin, element.funcref_end, offset, length))
                { return fail(gc_static_root_status::invalid_storage); }
                for(::std::size_t index{}; index != length; ++index)
                {
                    auto const& slot{module->element_expr_funcref_vec_storage.index_unchecked(offset + index)};
                    if(static_cast<unsigned>(slot.type) > static_cast<unsigned>(
                        local_defined_table_elem_storage_type_t::gc_array_ref))
                    { return fail(gc_static_root_status::invalid_storage); }
                    if(!visit(runtime_table_slot_to_gc_reference(slot))) { return result; }
                }
                if(!gc_static_root_details::payload_slice(module->element_expr_externref_vec_storage,
                    element.externref_begin, element.externref_end, offset, length))
                { return fail(gc_static_root_status::invalid_storage); }
                for(::std::size_t index{}; index != length; ++index)
                {
                    // [owned opaque host references] Never dereference a host
                    // address; a wrapper token must reach the collector codec.
                    gc_reference reference{};
                    reference.storage.ptr = module->element_expr_externref_vec_storage.index_unchecked(offset + index);
                    reference.kind = reference.storage.ptr == nullptr ?
                        ::uwvm2::object::global::wasm_ref_kind::wasm_null :
                        ::uwvm2::object::global::wasm_ref_kind::wasm_extern;
                    if(!visit(reference)) { return result; }
                }
                // Legacy funcidx payloads refer only to immutable function
                // instances. Their entire owning module cohort is already leased;
                // they contain neither aggregate nor exn/extern tokens to scan.
            }
        }
        return result;
    }
}
