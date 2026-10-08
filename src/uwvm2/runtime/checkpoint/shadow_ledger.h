/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "materialization.h"
# include <array>
# include <cstring>
# include <uwvm2/object/global/ref.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    using native_reference = ::uwvm2::object::global::wasm_global_ref_t;
    struct activation_identity
    {
        ::std::uint64_t incarnation{}, parent{}, continuation{}, runtime_epoch{};
        friend bool operator==(activation_identity const&, activation_identity const&) = default;
    };
    struct native_value
    {
        typed_slot declaration{};
        // Private host ABI bits, NEVER wire bytes or debugger-visible pointers.
        // The GC/extern exporter must census actual typed owners and produce IDs.
        ::std::array<::std::byte, native_slot_bytes> bits{};
    };
    struct logical_frame
    {
        activation_identity identity{};
        sealed_function_plan::owner plan{};
        ::std::uint64_t site{};
        ::std::vector<native_value> values{};
        bool materialized{};

    };
    // Calling-thread-only native materialization ledger. A public scalar frame
    // ID cannot read it: the runtime must still use its actual participant,
    // canonical publication/control-block owner, generation lease and worldstop.
    // No method below exposes a snapshot/restore management capability.
    class shadow_ledger
    {
        compilation_profile::owner profile_{};
        ::std::vector<logical_frame> frames_{};
        status failure_{status::ok};
        ::std::uint64_t pending_tail_continuation_{}, pending_tail_parent_{};
        void fail(status result) noexcept { if(failure_ == status::ok) { failure_ = result; } }
        [[nodiscard]] static bool runtime_reference(native_reference reference) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null: case kind::wasm_i31: case kind::wasm_struct:
                case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                case kind::wasm_func_imported: case kind::wasm_func_defined: return true;
                case kind::wasm_func: return false; // Parser function index is not a live reference.
            }
            return false;
        }
        [[nodiscard]] static bool typed_reference_envelope(native_reference ref, types::core_value_type type) noexcept
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
        explicit shadow_ledger(compilation_profile::owner profile) : profile_{::std::move(profile)}
        { if(!profile_) { failure_ = status::invalid_profile; } }
        shadow_ledger(shadow_ledger const&) = delete;
        shadow_ledger& operator=(shadow_ledger const&) = delete;
        [[nodiscard]] status failure() const noexcept { return failure_; }
        // Native runtime reports a failed actual owner/ABI/allocation check;
        // poisoning closes capture and can never mint pause/restore authority.
        void poison(status reason) noexcept { fail(reason == status::ok ? status::invalid_activation : reason); }
        [[nodiscard]] ::std::size_t size() const noexcept { return frames_.size(); }
        // Owned recording DATA only. A failed producer has stopped updating
        // its history, so stale copies can outlive real generated leave hooks.
        // The terminal shutdown caller must FIRST authenticate its actual
        // private cancellation and independently prove native/GC frame cleanup.
        // Keep failure sticky: discarding history grants no capture, GC census,
        // execution drain, join or restore capability. Healthy recording stays
        // untouched and must still retire through its exact generated hooks.
        [[nodiscard]] bool discard_failed_recording() noexcept
        {
            if(failure_ == status::ok) { return false; }
            frames_.clear(); pending_tail_continuation_ = pending_tail_parent_ = 0u;
            return true;
        }
        // Borrow only while the actual thread is parked. A span itself is not
        // a pause ticket, native-root census or registry publication lease.
        [[nodiscard]] ::std::span<logical_frame const> frames_while_actually_stopped() const noexcept { return frames_; }
        [[nodiscard]] status enter(activation_identity actual, sealed_function_plan::owner plan)
        {
            if(failure_ != status::ok) { return failure_; }
            if(!plan || plan->get().profile != profile_ ||
               plan->get().profile.owner_before(profile_) || profile_.owner_before(plan->get().profile))
            { fail(status::invalid_plan); return failure_; }
            if(plan->get().producer_availability != status::ok)
            { fail(plan->get().producer_availability); return failure_; }
            if(actual.incarnation == 0u || actual.continuation == 0u || actual.runtime_epoch == 0u ||
               actual.parent != (frames_.empty() ? 0u : frames_.back().identity.incarnation) ||
               (!frames_.empty() && actual.runtime_epoch != frames_.back().identity.runtime_epoch))
            { fail(status::invalid_activation); return failure_; }
            if(profile_->limits().frames != 0u && frames_.size() >= profile_->limits().frames) { fail(status::quota_exceeded); return failure_; }
            if(pending_tail_continuation_ != 0u)
            {
                if(actual.continuation != pending_tail_continuation_ || actual.parent != pending_tail_parent_)
                { fail(status::invalid_activation); return failure_; }
                pending_tail_continuation_ = pending_tail_parent_ = 0u;
            }
            else if(!frames_.empty() && (!frames_.back().materialized ||
                frames_.back().plan->get().sites[frames_.back().site - 1u].phase != frame_phase::awaiting_call_return))
            { fail(status::unmaterialized); return failure_; }
            frames_.push_back({actual, ::std::move(plan)}); return status::ok;
        }
        [[nodiscard]] status materialize(activation_identity actual, ::std::uint64_t site_id,
            ::std::span<::std::byte const> actual_native_slots)
        {
            if(failure_ != status::ok) { return failure_; }
            if(frames_.empty() || frames_.back().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            auto& frame{frames_.back()};
            if(site_id == 0u || site_id > frame.plan->get().sites.size()) { fail(status::invalid_layout); return failure_; }
            auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
            if(site.slots.size() > (static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / native_slot_bytes) ||
               actual_native_slots.size() != site.slots.size() * native_slot_bytes)
            { fail(status::invalid_layout); return failure_; }
            ::std::vector<native_value> captured{}; captured.reserve(site.slots.size());
            for(::std::size_t i{}; i != site.slots.size(); ++i)
            {
                auto const& declaration{site.slots[i]};
                native_value value{}; value.declaration = declaration;
                // [actual compiler-owned 16-byte slots ... i ... count] end
                // [safe                                                ] i<count
                // and exact count*16/ptrdiff bounds above precede subspan.
                // The pointer changes only inside that one actual host range.
                auto const bytes{actual_native_slots.subspan(i * native_slot_bytes, native_slot_bytes)};
                if(!declaration.initialized)
                {
                    // The compiler emitted zero stores, never a load from an
                    // uninitialized nondefaultable local. Reject forged bits.
                    for(auto byte : bytes) { if(byte != ::std::byte{}) { fail(status::invalid_layout); return failure_; } }
                }
                else
                {
                    ::std::memcpy(value.bits.data(), bytes.data(), native_slot_bytes);
                    if(declaration.type.kind == types::value_kind::reference)
                    {
                        static_assert(sizeof(native_reference) <= native_slot_bytes);
                        native_reference reference{};
                        // [one already bounded native ABI slot] end
                        // [safe                               ] complete real
                        // carrier copied into a constructed C++ object; no
                        // pointed-to GC/host object is read or inferred here.
                        ::std::memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
                        if(!runtime_reference(reference) || (!declaration.type.nullable &&
                           reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null))
                        { fail(status::invalid_reference); return failure_; }
                    }
                }
                captured.push_back(value);
            }
            frame.values.swap(captured); frame.site = site_id; frame.materialized = true; return status::ok;
        }
        // Typed native DATA commit only. The actual runtime bridge authenticates
        // its current activation/publication/compiler packet before this call.
        // A tuple/plan/identity provided by a native client never grants capture,
        // GC-root ownership or executable continuation authority here.
        [[nodiscard]] status materialize_validated_native_data(activation_identity actual,
            sealed_function_plan::owner const& actual_plan, ::std::uint64_t site_id,
            ::std::span<native_value const> values)
        {
            if(failure_ != status::ok) { return failure_; }
            if(frames_.empty() || frames_.back().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            auto& frame{frames_.back()};
            if(!actual_plan || frame.plan.get() != actual_plan.get() || frame.plan.owner_before(actual_plan) ||
               actual_plan.owner_before(frame.plan) || site_id == 0u || site_id > frame.plan->get().sites.size())
            { fail(status::invalid_plan); return failure_; }
            // [actual sealed same-control-block plan sites ... site_id-1 ... N] end
            // [safe] complete ID bound BEFORE metadata indexing; no public
            // shared_ptr or matching raw address is treated as runtime permission.
            auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
            if(values.size() != site.slots.size()) { fail(status::invalid_layout); return failure_; }
            for(::std::size_t i{}; i != values.size(); ++i)
            {
                // [validated owned DATA values and exact declarations0..N] end
                // [safe] full equal counts BEFORE selecting either i cell.
                auto const& value{values[i]}; auto const& expected{site.slots[i]};
                if(value.declaration.type != expected.type || (expected.initialized && !value.declaration.initialized))
                { fail(status::invalid_layout); return failure_; }
                if(!value.declaration.initialized)
                {
                    if(i >= site.local_count || expected.type.kind != types::value_kind::reference || expected.type.nullable)
                    { fail(status::invalid_layout); return failure_; }
                    for(auto byte : value.bits)
                    { if(byte != ::std::byte{}) { fail(status::invalid_layout); return failure_; } }
                    continue;
                }
                if(expected.type.kind == types::value_kind::reference)
                {
                    native_reference ref{};
                    static_assert(sizeof(ref) <= native_slot_bytes);
                    // [owned complete native_value::bits[16]] bits_end
                    // [safe] constructed carrier copy only; never dereference
                    // an opaque token, parsed function index or native address.
                    ::std::memcpy(::std::addressof(ref), value.bits.data(), sizeof(ref));
                    if(!typed_reference_envelope(ref, expected.type))
                    { fail(status::invalid_reference); return failure_; }
                }
            }
            // All types/actual initialized states/counts are checked before one
            // detached allocation/copy. Failure leaves the old frame untouched;
            // recording failure is sticky and cannot mint a new stop capability.
            ::std::vector<native_value> detached{values.begin(), values.end()};
            frame.values.swap(detached); frame.site = site_id; frame.materialized = true;
            return status::ok;
        }

        // Called BEFORE the compiler emits musttail; never inserts work between
        // musttail and its required return. Typed successors retain continuation.
        [[nodiscard]] status leave(activation_identity actual, bool typed_tail) noexcept
        {
            if(failure_ != status::ok) { return failure_; }
            if(frames_.empty() || frames_.back().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            if(typed_tail)
            { pending_tail_continuation_ = actual.continuation; pending_tail_parent_ = actual.parent; }
            frames_.pop_back(); return status::ok;
        }
        [[nodiscard]] status host_import_admission(bool has_actual_record_replay_adapter) noexcept
        {
            if(failure_ != status::ok) { return failure_; }
            if(!has_actual_record_replay_adapter) { fail(status::non_replayable_import); }
            return failure_;
        }
        [[nodiscard]] bool has_complete_materialized_frames() const noexcept
        {
            if(failure_ != status::ok || frames_.empty() || pending_tail_continuation_ != 0u) { return false; }
            for(auto const& frame : frames_) { if(!frame.materialized) { return false; } }
            return true;
        }
        // A cold typed population visitor, not a root-registration capability.
        // Actual runtime must call it BEFORE any GC can reclaim these recorded
        // carriers, keep the resulting independently owned roots for the entire
        // actual collection ticket, and also census current native/static/EH
        // roots. Entry-only recording currently cannot authorize that census.
        // Never register this heap-backed history as a strict-LIFO frame_root.
        template<typename Visitor>
        [[nodiscard]] status visit_recorded_native_references(Visitor&& visitor) const
        {
            if(failure_ != status::ok) { return failure_; }
            for(auto const& frame : frames_)
            {
                if(!frame.materialized) { return status::unmaterialized; }
                for(auto const& value : frame.values)
                {
                    if(!value.declaration.initialized || value.declaration.type.kind != types::value_kind::reference) { continue; }
                    native_reference reference{};
                    static_assert(sizeof(reference) <= native_slot_bytes);
                    // [owned complete native_value::bits[16]] bits_end
                    // [safe                                ] fixed extent >=
                    // sizeof the constructed carrier; no GC object is read.
                    ::std::memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
                    if(!runtime_reference(reference) || !visitor(reference)) { return status::invalid_reference; }
                }
            }
            return status::ok;
        }
        [[nodiscard]] status executable_restore_capability() const noexcept
        { return status::unavailable_resume; } // No native-PC jump or fabricated dispatcher.
    };
}
