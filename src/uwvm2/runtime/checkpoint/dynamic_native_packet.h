/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "shadow_ledger.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    // Independently owned typed native DATA, not capture/restore authority.
    // A runtime producer must first prove actual compiler frame provenance,
    // canonical publication/generation and private pause/root ownership before
    // it forms these bounded spans. Native ABI bits NEVER enter the database.
    class dynamic_native_packet
    {
        sealed_function_plan::owner plan_{};
        ::std::uint64_t site_{};
        ::std::vector<native_value> values_{};
        dynamic_native_packet(sealed_function_plan::owner plan, ::std::uint64_t site,
            ::std::vector<native_value>&& values) : plan_{::std::move(plan)}, site_{site}, values_{::std::move(values)} {}
        [[nodiscard]] static bool native_reference_envelope(native_reference ref, types::core_value_type type) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            bool known{};
            switch(ref.kind)
            {
                case kind::wasm_null: case kind::wasm_i31: case kind::wasm_struct:
                case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                case kind::wasm_func_imported: case kind::wasm_func_defined: known = true; break;
                case kind::wasm_func: return false;
            }
            if(!known) { return false; }
            if(ref.kind == kind::wasm_null) { return type.nullable; }
            // Only a conservative carrier envelope. A defined heap type still
            // requires actual module subtype/layout/store-owner verification.
            // wasm_extern also needs real host-vs-wrapper registry inspection;
            // no opaque address or copied tag can supply that proof here.
            if(type.heap.is_defined())
            {
                return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array ||
                       ref.kind == kind::wasm_func_imported || ref.kind == kind::wasm_func_defined;
            }
            switch(static_cast<types::abstract_heap_type>(type.heap.code))
            {
                case types::abstract_heap_type::any:
                    return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array ||
                           ref.kind == kind::wasm_i31 || ref.kind == kind::wasm_extern;
                case types::abstract_heap_type::eq:
                    return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array || ref.kind == kind::wasm_i31;
                case types::abstract_heap_type::i31: return ref.kind == kind::wasm_i31;
                case types::abstract_heap_type::struct_: return ref.kind == kind::wasm_struct;
                case types::abstract_heap_type::array: return ref.kind == kind::wasm_array;
                case types::abstract_heap_type::func:
                    return ref.kind == kind::wasm_func_imported || ref.kind == kind::wasm_func_defined;
                case types::abstract_heap_type::extern_: return ref.kind == kind::wasm_extern;
                case types::abstract_heap_type::exn: return ref.kind == kind::wasm_exn;
                case types::abstract_heap_type::none: case types::abstract_heap_type::nofunc:
                case types::abstract_heap_type::noextern: case types::abstract_heap_type::noexn: return false;
            }
            return false;
        }
    public:
        using owner = ::std::shared_ptr<dynamic_native_packet const>;
        [[nodiscard]] sealed_function_plan::owner const& plan() const noexcept { return plan_; }
        [[nodiscard]] ::std::uint64_t site() const noexcept { return site_; }
        [[nodiscard]] ::std::span<native_value const> values() const noexcept { return values_; }
        // `readable` in a site is a validation proof. A false nondefaultable
        // local may nevertheless contain an actual assigned value after a
        // conservative control merge. Canonical compiler-executed flags decide
        // whether to copy that value, without permitting an illegal local.get.
        // All flags are checked before reading ANY native payload slot.
        [[nodiscard]] static status copy_compiler_packet(sealed_function_plan::owner plan, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags, owner& result)
        {
            if(!plan || site == 0u || site > plan->get().sites.size()) { return status::invalid_plan; }
            auto const& declaration{plan->get().sites[static_cast<::std::size_t>(site - 1u)]};
            if(declaration.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / native_slot_bytes ||
               payload.size() != declaration.slots.size() * native_slot_bytes || actual_local_flags.size() != declaration.local_count)
            { return status::invalid_layout; }
            for(::std::size_t i{}; i != actual_local_flags.size(); ++i)
            {
                // [actual compiler-owned bounded original-index flag bytes] end
                // [safe                                                  ] i<N
                // and complete flags.size==local_count precede the flag read.
                if(actual_local_flags[i] > 1u || (declaration.slots[i].initialized && actual_local_flags[i] == 0u))
                { return status::invalid_layout; }
            }
            ::std::vector<native_value> detached{};
            detached.reserve(declaration.slots.size());
            for(::std::size_t i{}; i != declaration.slots.size(); ++i)
            {
                native_value value{}; value.declaration = declaration.slots[i];
                if(i < declaration.local_count) { value.declaration.initialized = actual_local_flags[i] != 0u; }
                // [complete actual producer payload ... slot i ...] payload_end
                // [safe                                          ] i<slot_count,
                // exact count*16<=PTRDIFF_MAX BEFORE offset/subspan changes.
                auto const bytes{payload.subspan(i * native_slot_bytes, native_slot_bytes)};
                if(!value.declaration.initialized)
                {
                    // Producer clears this slot and branches around the local
                    // alloca load. An unset local is never a fabricated null.
                    for(auto byte : bytes) { if(byte != ::std::byte{}) { return status::invalid_layout; } }
                }
                else
                {
                    ::std::memcpy(value.bits.data(), bytes.data(), native_slot_bytes);
                    if(value.declaration.type.kind == types::value_kind::reference)
                    {
                        native_reference ref{};
                        static_assert(sizeof(ref) <= native_slot_bytes);
                        // [owned complete 16-byte native slot] bits_end
                        // [safe                              ] bounded carrier
                        // copy into a live C++ object; never dereference a token.
                        ::std::memcpy(::std::addressof(ref), value.bits.data(), sizeof(ref));
                        if(!native_reference_envelope(ref, value.declaration.type)) { return status::invalid_reference; }
                    }
                }
                detached.push_back(value);
            }
            owner candidate{new dynamic_native_packet{::std::move(plan), site, ::std::move(detached)}};
            result.swap(candidate); return status::ok; // no partial publication on rejection/allocation failure
        }
        [[nodiscard]] status executable_restore_capability() const noexcept { return status::unavailable_resume; }
    };
}
