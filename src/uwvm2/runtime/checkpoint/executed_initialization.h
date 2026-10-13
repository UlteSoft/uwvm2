/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "materialization.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    // Executed nondefaultable-local initialization DATA. Distinct from the
    // validator's conservative proof restored at block/else/end. This component
    // alone authorizes no runtime capture, guest execution or restoration.
    class executed_local_initialization
    {
        sealed_function_plan::owner plan_{};
        ::std::size_t parameter_count_{};
        ::std::vector<::std::uint8_t> flags_{};
        status state_{status::invalid_plan};
        [[nodiscard]] ::std::span<typed_slot const> declaration() const noexcept
        {
            if(!plan_ || plan_->get().sites.empty()) { return {}; }
            auto const& entry{plan_->get().sites.front()};
            if(entry.local_count > entry.slots.size()) { return {}; }
            // [immutable independently owned entry typed slots ...] slots_end
            // [safe                                               ] validated
            // local_count<=slots.size before subspan; no LLVM/native pointers.
            return ::std::span<typed_slot const>{entry.slots}.first(entry.local_count);
        }
    public:
        explicit executed_local_initialization(sealed_function_plan::owner plan, ::std::size_t actual_parameter_count) :
            plan_{::std::move(plan)}, parameter_count_{actual_parameter_count}
        {
            if(!plan_ || plan_->get().sites.empty()) { return; }
            auto const& entry{plan_->get().sites.front()};
            if(entry.phase != frame_phase::before_opcode || entry.operand_count != 0u ||
               entry.saved_parameter_count != 0u || entry.local_count != entry.slots.size() ||
               parameter_count_ > entry.local_count) { return; }
            for(::std::size_t i{}; i != entry.local_count; ++i)
            {
                auto const& local{entry.slots[i]};
                // Parameters carry real incoming values, including nonnull refs.
                // Other nondefaultable locals start unset. The actual module's
                // parameter count is DATA here, not a manager/capture credential.
                bool const expected{i < parameter_count_ || local.type.kind != types::value_kind::reference || local.type.nullable};
                if(local.initialized != expected) { return; }
            }
            flags_.reserve(entry.local_count);
            for(auto const& local : entry.slots) { flags_.push_back(static_cast<::std::uint8_t>(local.initialized)); }
            state_ = status::ok;
        }
        [[nodiscard]] status state() const noexcept { return state_; }
        [[nodiscard]] ::std::span<::std::uint8_t const> flags() const noexcept { return flags_; }
        [[nodiscard]] bool readable(::std::size_t original_local) const noexcept
        { return state_ == status::ok && original_local < flags_.size() && flags_[original_local] != 0u; }
        // A genuine compiler-emitted local.set/tee edge will update its OWN
        // native flag after the actual value store. A speculative/nonexecuted
        // branch does not call this operation. The scalar index is not authority.
        [[nodiscard]] status record_executed_assignment(::std::size_t original_local) noexcept
        {
            if(state_ != status::ok) { return state_; }
            if(original_local >= flags_.size()) { return status::invalid_layout; }
            flags_[original_local] = 1u; return status::ok;
        }
        // No control-merge proof rollback exists here. Physical assignment lives
        // until a genuine new Wasm activation/self-tail local reset executes.
        [[nodiscard]] status reset_after_actual_new_activation() noexcept
        {
            if(state_ != status::ok) { return state_; }
            auto const locals{declaration()};
            if(locals.size() != flags_.size()) { return status::invalid_plan; }
            for(::std::size_t i{}; i != flags_.size(); ++i) { flags_[i] = static_cast<::std::uint8_t>(locals[i].initialized); }
            return status::ok;
        }
        // Bounded detached flag validation for future approved resume glue.
        // Success means ONLY compatible initialization bytes, never proof of
        // live frames/operand/control/EH/GC roots, private pause or replay.
        [[nodiscard]] status replace_compatible_flag_data(::std::span<::std::uint8_t const> bytes)
        {
            if(state_ != status::ok) { return state_; }
            auto const locals{declaration()};
            if(bytes.size() != flags_.size() || bytes.size() != locals.size()) { return status::invalid_layout; }
            for(::std::size_t i{}; i != bytes.size(); ++i)
            {
                // [caller-owned exact bounded flag bytes] bytes_end
                // [safe                                ] i<count; 0 never
                // clears parameters or a defaultable local's initialized state.
                if(bytes[i] > 1u || (locals[i].initialized && bytes[i] == 0u)) { return status::invalid_layout; }
            }
            ::std::vector<::std::uint8_t> detached{bytes.begin(), bytes.end()};
            flags_.swap(detached); return status::ok; // failure before swap cannot mutate the current flags
        }
        [[nodiscard]] status executable_restore_capability() const noexcept { return status::unavailable_resume; }
    };
}
