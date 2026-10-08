/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include "native_owned_instruction_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_next_policy
{
    enum class action : unsigned char { unavailable, single_instruction };
    enum class reason : unsigned char
    {
        none, incomplete_decode, call_continuation_unavailable,
        caller_unwind_unavailable, branch_continuation_unavailable,
        trap_instruction, other_control_flow, inconsistent_descriptor,
        current_native_trap_required, decoder_disagreement,
        call_target_unavailable, call_continuation_event_unavailable,
        call_continuation_capacity_exhausted, return_continuation_event_unavailable,
        return_continuation_capacity_exhausted
    };
    struct selection
    {
        action operation{action::unavailable};
        reason unavailable_reason{reason::incomplete_decode};
        ::std::size_t instruction_size{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept
        { return operation == action::single_instruction; }
    };

    // This is scalar policy DATA, not permission to execute or read a native
    // address. The controller must independently authenticate a current real
    // trap, stopped participant, private activation and live code generation,
    // then decode bytes copied by the runtime's actual publication transaction.
    // Only the conservative owned-byte decoder constructs decoded_instruction;
    // a legacy MC flow classification alone cannot select execution. Ordinary
    // MC flow flags do not model every system/prefix/interrupt-shadow operation.
    // Neither a console request nor a caller-populated classification authorizes
    // a backend request. Recheck that same stop before releasing its actual gate.
    //
    // The first native-next implementation admits only a fully decoded ordinary
    // instruction. Qualified one-instruction execution gives the same behavior
    // as ni here. The whole-owner boundary layer may separately authenticate
    // BOTH direct branch successors for SI and NI. Calls need an actual temporary
    // continuation stop; returns need actual caller ownership. Neither is granted
    // by this scalar ordinary-only selector. Missing capabilities never become a
    // made-up successful next, a guessed stack return or an unlimited TF loop.
    [[nodiscard]] constexpr selection choose(
        native_owned_instruction_semantics::decoded_instruction const& instruction,
        ::std::size_t copied_available) noexcept
    {
        using enum native_instruction_semantics::flow;
        auto const& decoded{instruction.semantics()};
        if(!instruction || !decoded || decoded.size == 0u || decoded.size > copied_available) { return {}; }
        reason why{};
        switch(decoded.kind)
        {
            case ordinary:
                if(!instruction.safe_for_single_instruction() || decoded.may_change_pc || decoded.conditional_branch ||
                   decoded.indirect_branch || decoded.barrier)
                { why = reason::inconsistent_descriptor; }
                else { return {action::single_instruction, reason::none, decoded.size}; }
                break;
            case call: why = reason::call_continuation_unavailable; break;
            case return_instruction: why = reason::caller_unwind_unavailable; break;
            case branch: why = reason::branch_continuation_unavailable; break;
            case trap: why = reason::trap_instruction; break;
            case other_control: why = reason::other_control_flow; break;
            default: return {};
        }
        return {action::unavailable, why, decoded.size};
    }
}
