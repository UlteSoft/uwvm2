/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE
# include <cstddef>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
# include "value.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
    enum class payload_root_status : unsigned char
    { ok, invalid_payload, host_codec_required, rejected_reference, size_overflow };

    struct payload_root_result
    {
        payload_root_status status{payload_root_status::ok};
        ::std::size_t visited{};
    };

    namespace payload_root_details
    {
        [[nodiscard]] inline bool runtime_reference_kind(
            ::uwvm2::object::global::wasm_global_ref_t const& reference) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null: case kind::wasm_i31:
                case kind::wasm_func_imported: case kind::wasm_func_defined:
                case kind::wasm_extern: case kind::wasm_exn:
                case kind::wasm_struct: case kind::wasm_array: return true;
                case kind::wasm_func: return false; // Parser indices are not runtime identities.
            }
            return false;
        }
    }

    // Enumerate one strongly owned, immutable exception instance. The caller
    // retains its value_ref through visitation and supplies all in-flight,
    // caught, rethrown and externally held instances to the collection domain.
    // This helper neither discovers those owners nor activates collection.
    // A visitor receives a complete carrier by VALUE, never a payload borrow.
    // It must not mutate, allocate, sweep, reenter, retain a borrow or throw.
    // Token membership/liveness belongs to the collector codec; a known kind
    // alone cannot authorize dereferencing its payload. Discard failed results.
    // No construction, ordinary instruction or numeric memory path calls this.
    template<class Visitor>
    [[nodiscard]] inline payload_root_result visit_immutable_exception_wasm_roots(
        value const& instance, Visitor&& visitor) noexcept
    {
        using reference = ::uwvm2::object::global::wasm_global_ref_t;
        static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, reference>);
        payload_root_result result{};
        auto const fields{instance.fields()};
        // [strongly owned immutable field allocation ... end]
        // [safe                                             ] Preflight every
        // kind and extent before publishing any roots. No pointer or view is
        // retained, and iteration stays within the instance's field span.
        for(auto const& field : fields)
        {
            auto const bits{field.bits()};
            auto const width{payload_width(field.kind())};
            if(width == 0uz || bits.size() != width)
            { return {payload_root_status::invalid_payload, 0uz}; }
            if(field.kind() == payload_kind::wasm_reference)
            {
                if(bits.size() != sizeof(reference))
                { return {payload_root_status::invalid_payload, 0uz}; }
                reference carrier{};
                // [complete owned carrier bytes][aligned local reference]
                // [safe                        ] Copy kind and payload together;
                // do not interpret token-shaped numeric/vector bytes as roots.
                ::std::memcpy(::std::addressof(carrier), bits.data(), sizeof(carrier));
                if(!payload_root_details::runtime_reference_kind(carrier))
                { return {payload_root_status::invalid_payload, 0uz}; }
            }
            else if(field.kind() == payload_kind::reference)
            {
                void const* native_null{};
                // [one owned canonical native identity] No address is followed.
                // Its representation lacks a Wasm kind: a non-null host handle
                // or encoded i31 needs its privileged codec, never a guess.
                // Compare the native null REPRESENTATION without materializing
                // arbitrary host/i31 bits as a pointer object.
                if(field.root() || ::std::memcmp(bits.data(), ::std::addressof(native_null), sizeof(native_null)) != 0)
                { return {payload_root_status::host_codec_required, 0uz}; }
            }
        }
        for(auto const& field : fields)
        {
            if(field.kind() != payload_kind::wasm_reference) { continue; }
            if(result.visited == (::std::numeric_limits<::std::size_t>::max)())
            { result.status = payload_root_status::size_overflow; return result; }
            reference carrier{};
            // [preflighted immutable full carrier] Publication cannot resize
            // or retag this field. Copy before invoking the collector visitor.
            ::std::memcpy(::std::addressof(carrier), field.bits().data(), sizeof(carrier));
            if(!visitor(carrier))
            { result.status = payload_root_status::rejected_reference; return result; }
            ++result.visited;
        }
        return result;
    }
}
