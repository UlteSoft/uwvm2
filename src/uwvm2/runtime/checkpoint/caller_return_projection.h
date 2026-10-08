/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "shadow_ledger.h"
# include <type_traits>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    // Bounded native typed DATA transformation for a caller whose child has
    // returned normally. It never authenticates that a child actually returned:
    // the future private runtime dispatcher must hold that real return event,
    // activation/continuation, publication, root and coherent-stop authority.
    // No caller tuple, shared_ptr, logical ordinal or result grants execution.
    class caller_return_projection
    {
        sealed_function_plan::owner plan_{};
        ::std::uint64_t waiting_site_{}, next_site_{};
        ::std::vector<types::core_value_type> result_types_{};
        caller_return_projection(sealed_function_plan::owner plan, ::std::uint64_t waiting,
            ::std::uint64_t next, ::std::vector<types::core_value_type>&& results) :
            plan_{::std::move(plan)}, waiting_site_{waiting}, next_site_{next}, result_types_{::std::move(results)} {}
        [[nodiscard]] static status scalar_value(native_value const& value, typed_slot expected) noexcept
        {
            if(value.declaration.type != expected.type || (expected.initialized && !value.declaration.initialized))
            { return status::invalid_layout; }
            if(!value.declaration.initialized)
            {
                if(expected.type.kind != types::value_kind::reference || expected.type.nullable)
                { return status::invalid_layout; }
                for(auto const byte : value.bits) { if(byte != ::std::byte{}) { return status::invalid_layout; } }
                return status::ok;
            }
            if(expected.type.kind != types::value_kind::reference) { return status::ok; }
            native_reference reference{};
            static_assert(sizeof(reference) <= native_slot_bytes);
            // [owned fixed native_value bits[16]] bits_end
            // [safe                            ] complete carrier extent before
            // copy into a constructed object; NEVER dereference any host token.
            ::std::memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null:
                    return expected.type.nullable && reference.storage.ptr == nullptr ? status::ok : status::invalid_reference;
                case kind::wasm_i31:
                {
                    if(expected.type.heap.is_defined() ||
                       (reference.storage.wasm_i31.bits & ~types::wasm_i31::value_mask) != 0u)
                    { return status::invalid_reference; }
                    auto const heap{static_cast<types::abstract_heap_type>(expected.type.heap.code)};
                    return heap == types::abstract_heap_type::i31 || heap == types::abstract_heap_type::eq ||
                           heap == types::abstract_heap_type::any ? status::ok : status::invalid_reference;
                }
                case kind::wasm_struct: case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                case kind::wasm_func_imported: case kind::wasm_func_defined:
                    return status::unavailable_resume; // No real retained root/store/host-resource census here.
                case kind::wasm_func: return status::invalid_reference; // Parser index is not a live value.
            }
            return status::invalid_reference;
        }
    public:
        using owner = ::std::shared_ptr<caller_return_projection const>;
        // Compiler metadata ONLY. The fused walk records an awaiting site after
        // call arguments/selector/reference are consumed, and a before-opcode
        // site at the actual next byte. The declared call result tuple comes
        // from that validated call type, not the physical callee SSA carriers.
        [[nodiscard]] static owner seal_compiler_data(sealed_function_plan::owner plan,
            ::std::uint64_t waiting, ::std::uint64_t next, ::std::span<types::core_value_type const> declared_results)
        {
            if(!plan || waiting == 0u || next == 0u || waiting == next ||
               waiting > plan->get().sites.size() || next > plan->get().sites.size()) { return {}; }
            auto const& function{plan->get()};
            // [actual sealed plan sites ... ordinal-1 ... N] sites_end
            // [safe                                        ] both dense IDs
            // checked BEFORE selecting their immutable metadata addresses.
            auto const& call{function.sites[static_cast<::std::size_t>(waiting - 1u)]};
            auto const& after{function.sites[static_cast<::std::size_t>(next - 1u)]};
            if(call.phase != frame_phase::awaiting_call_return || after.phase != frame_phase::before_opcode ||
               call.handlers != after.handlers || call.caller_return_offset != after.opcode_offset ||
               after.opcode_offset <= call.opcode_offset || call.local_count != after.local_count ||
               call.saved_parameter_count != after.saved_parameter_count || call.controls != after.controls ||
               declared_results.size() > function.profile->limits().slots_per_frame - call.slots.size() ||
               after.slots.size() != call.slots.size() + declared_results.size() ||
               after.operand_count != call.operand_count + declared_results.size()) { return {}; }
            auto const prefix{call.local_count + call.operand_count}; // validate_plan proved this bounded by slots.size().
            for(::std::size_t i{}; i != prefix; ++i)
            {
                // [call/after owned slots ... i<prefix<=both sizes] end
                // [safe] local/operand prefixes are bounded before indexing.
                if(call.slots[i] != after.slots[i]) { return {}; }
            }
            for(::std::size_t i{}; i != declared_results.size(); ++i)
            {
                // [after prefix ... result i ... saved parameters] end
                // [safe] results.size<=cap-call.size and exact target size
                // proved BEFORE prefix+i; no arithmetic overflow is possible.
                if(!known_type(declared_results[i]) || after.slots[prefix + i] != typed_slot{declared_results[i], true}) { return {}; }
            }
            for(::std::size_t i{}; i != call.saved_parameter_count; ++i)
            {
                // [call saved0..K] -> [after results then same saved0..K]
                // [safe] complete exact counts BEFORE either bounded index.
                if(call.slots[prefix + i] != after.slots[prefix + declared_results.size() + i]) { return {}; }
            }
            ::std::vector<types::core_value_type> results{declared_results.begin(), declared_results.end()};
            return owner{new caller_return_projection{::std::move(plan), waiting, next, ::std::move(results)}};
        }
        [[nodiscard]] sealed_function_plan::owner const& plan() const noexcept { return plan_; }
        [[nodiscard]] ::std::uint64_t waiting_site() const noexcept { return waiting_site_; }
        [[nodiscard]] ::std::uint64_t next_site() const noexcept { return next_site_; }
        [[nodiscard]] ::std::span<types::core_value_type const> result_types() const noexcept { return result_types_; }
        // Typed DATA only, failure atomic. Exceptions/traps/tail transfers do
        // not take this normal-return path; they require their own real pending
        // continuation and reference owners. Arbitrary public tuples cannot
        // invoke generated code or mint a returned-child/capture capability.
        [[nodiscard]] status project_typed_return_data(logical_frame const& waiting,
            ::std::span<native_value const> returned, logical_frame& result) const
        {
            if(!waiting.plan || waiting.plan.get() != plan_.get() || waiting.plan.owner_before(plan_) ||
               plan_.owner_before(waiting.plan) || !waiting.materialized || waiting.site != waiting_site_ ||
               waiting.identity.incarnation == 0u || waiting.identity.continuation == 0u || waiting.identity.runtime_epoch == 0u)
            { return status::invalid_activation; }
            auto const& call{plan_->get().sites[static_cast<::std::size_t>(waiting_site_ - 1u)]};
            auto const& after{plan_->get().sites[static_cast<::std::size_t>(next_site_ - 1u)]};
            if(waiting.values.size() != call.slots.size() || returned.size() != result_types_.size()) { return status::invalid_layout; }
            for(::std::size_t i{}; i != waiting.values.size(); ++i)
            {
                // [owned caller values and exact sealed declaration] end
                // [safe] complete equal counts BEFORE selecting either i slot.
                auto const checked{scalar_value(waiting.values[i], call.slots[i])}; if(checked != status::ok) { return checked; }
            }
            for(::std::size_t i{}; i != returned.size(); ++i)
            {
                // [producer-owned normal-result DATA tuple0..N] end
                // [safe] exact declared count BEFORE any result carrier read.
                auto const checked{scalar_value(returned[i], {result_types_[i], true})}; if(checked != status::ok) { return checked; }
            }
            logical_frame detached{}; detached.identity = waiting.identity; detached.plan = plan_;
            detached.site = next_site_; detached.materialized = true; detached.values.reserve(after.slots.size());
            auto const prefix{call.local_count + call.operand_count};
            for(::std::size_t i{}; i != prefix; ++i) { detached.values.push_back(waiting.values[i]); }
            for(auto const& value : returned) { detached.values.push_back(value); }
            for(::std::size_t i{prefix}; i != waiting.values.size(); ++i) { detached.values.push_back(waiting.values[i]); }
            // All metadata, complete values, reference envelopes and quotas
            // succeeded BEFORE one detached result move. Allocation failure
            // leaves the original result entirely untouched.
            static_assert(::std::is_nothrow_move_assignable_v<logical_frame>);
            result = ::std::move(detached); return status::ok;
        }
        [[nodiscard]] constexpr bool returned_child_execution_authority() const noexcept { return false; }
        [[nodiscard]] constexpr status executable_restore_capability() const noexcept { return status::unavailable_resume; }
    };
}
