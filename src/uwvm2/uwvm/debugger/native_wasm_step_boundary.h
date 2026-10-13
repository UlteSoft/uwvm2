/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <span>
# include "native_disassembly_window.h"
# include "native_owned_instruction_semantics.h"
# include "native_next_policy.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_wasm_step_boundary
{
    struct selection
    {
        bool executable{};
        native_next_policy::reason unavailable_reason{native_next_policy::reason::incomplete_decode};
        native_disassembly::instruction decoded{};
        ::std::uintptr_t first_successor{}, second_successor{};
        // Exact allowed (PC,NPC) pairs after a SPARC delayed branch. These
        // owned-code scalar facts acquire authority only in a sealed runtime plan.
        ::std::array<::std::uintptr_t, 2u> successor_npc{};
        ::std::array<bool, 2u> successor_delay_slot{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return executable; }
    };

    // Bounded owned-code DATA only. A caller must first authenticate the SAME
    // actual pause/participant/engine-owner/generation/epoch as these bytes.
    // No display position becomes a pointer; this helper cannot mint a native
    // request, change a PC, unpark a participant, or operate on a host function.
    template<typename DisplayDecoder>
    [[nodiscard]] inline bool exact_boundary(DisplayDecoder& display,
        ::std::span<::std::uint8_t const> owned, ::std::uintptr_t owner_begin,
        ::std::uintptr_t pc, ::std::span<::std::uint8_t const> instruction_code = {}) noexcept
    {
        auto const page{native_disassembly::decode_window_with(display, owned, owner_begin, pc, 0, 0, 1u, instruction_code)};
        return page.available && page.count == 1u && page.instructions[0u] && page.instructions[0u].pc == pc;
    }

    // Pre-execution containment for both si and ni. Never execute a call,
    // return, register/memory-indirect transfer, system/trap/prefix operation or
    // unknown instruction merely to discover its new owner afterwards.
    // Direct conditional branches require BOTH reachable native successors.
    // The complete function image is copied once by the private runtime; MC
    // contexts are constructed once by the caller and reused for this query.
    // Separate future runtime-issued cursor/CFI proofs may admit other genuine
    // Wasm targets. An entire engine text section is never one Wasm function.
    template<typename DisplayDecoder, typename SemanticDecoder>
    [[nodiscard]] inline selection prepare(DisplayDecoder& display, SemanticDecoder& semantics,
        ::std::span<::std::uint8_t const> owned, ::std::uintptr_t owner_begin,
        ::std::uintptr_t owner_end, ::std::uintptr_t pc, bool ordinary_next, ::std::uintptr_t pending_npc = 0u,
        ::std::span<::std::uint8_t const> instruction_code = {}, bool pending_delay_slot = false) noexcept
    {
        selection out{};
        if(!display || !semantics || owner_begin == 0u || owner_end <= owner_begin ||
           owned.empty() ||
           owner_end - owner_begin != owned.size() || pc < owner_begin || pc >= owner_end)
        { return out; }
        auto const page{native_disassembly::decode_window_with(display, owned, owner_begin, pc, 0, 0, 2u, instruction_code)};
        if(!page.available || page.count != 2u || !page.instructions[0u] || page.instructions[0u].pc != pc)
        { return out; }
        out.decoded = page.instructions[0u];
        auto const offset{static_cast<::std::size_t>(pc - owner_begin)};
        // [owned function bytes0 ... offset ... exact size] owned_end
        // [safe                                            ] pc in exact
        //  ^^ owner above BEFORE this forward suffix borrow; never cast pc.
        auto const decoded{semantics.decode(pc, {owned.data() + offset, owned.size() - offset})};
        if(!decoded || decoded.semantics().size != out.decoded.size ||
           out.decoded.size == 0u || out.decoded.size > owned.size() - offset)
        { out.unavailable_reason = native_next_policy::reason::decoder_disagreement; return out; }
        auto const next{native_next_policy::choose(decoded, owned.size() - offset)};
        using flow = native_instruction_semantics::flow;
        auto const kind{decoded.semantics().kind};
        // Scalar ordinary-only policy cannot authenticate a branch destination.
        // NI may use the SAME stronger whole-owner successor proof as SI below.
        // This exception admits no call, indirect transfer or return: every
        // reachable direct branch target still needs its exact owned boundary.
        if(ordinary_next && !next &&
           !(kind == flow::branch && decoded.safe_for_same_owner_direct_branch()))
        { out.unavailable_reason = next.unavailable_reason; return out; }
        // [current owned instruction pc ... size] owner_end
        // [safe                                 ] size<=owner_end-pc above;
        //                    ^^ checked scalar fallthrough, not pointer math.
        auto const fallthrough{pc + out.decoded.size};
        if(pending_delay_slot || (pending_npc != 0u && pending_npc != fallthrough))
        {
            // A real authenticated SPARC trap can be parked in a delay slot.
            // Its private kernel NPC, never a public register/address request,
            // is the next instruction. Nested control transfers are refused.
            if(pending_npc == 0u || kind != flow::ordinary || !decoded.safe_for_single_instruction() || out.decoded.size != 4u ||
               pending_npc < owner_begin || pending_npc >= owner_end ||
               !exact_boundary(display, owned, owner_begin, pending_npc, instruction_code))
            { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
            out.first_successor = pending_npc;
        }
        else if(kind == flow::ordinary && decoded.safe_for_single_instruction())
        {
            // The same entry walk already decoded the immediate successor.
            // A mapping DATA gap, incomplete instruction or owner-end filler
            // cannot become a fallthrough. Distant branch/NPC proofs remain
            // separate exact forward queries.
            if(fallthrough >= owner_end || !page.instructions[1u] || page.instructions[1u].pc != fallthrough)
            { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
            out.first_successor = fallthrough;
        }
        else if(kind == flow::branch && decoded.safe_for_same_owner_direct_branch())
        {
            auto const& destination{decoded.destination()};
            if(!destination || destination.display_pc < owner_begin || destination.display_pc >= owner_end ||
               !exact_boundary(display, owned, owner_begin, destination.display_pc, instruction_code))
            { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
            if(decoded.delayed_branch_pair())
            {
                // MIPS signal frames have no independently restorable NPC.
                // Never trap inside the delay slot or change its branch state.
                // Retire this branch together with its one ordinary delay
                // instruction, then trap at a proved taken/fallthrough boundary.
                // Nested control, self/delay targets and owner exits refuse.
                if(out.decoded.size != 4u || owner_end - pc <= 8u ||
                   destination.display_pc == pc || destination.display_pc == fallthrough ||
                   !exact_boundary(display, owned, owner_begin, fallthrough, instruction_code) ||
                   !exact_boundary(display, owned, owner_begin, pc + 8u, instruction_code))
                { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                auto const slot{semantics.decode(fallthrough,owned.subspan(offset + 4u))};
                if(!slot || slot.semantics().size != 4u || !slot.safe_for_single_instruction())
                { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                out.first_successor = destination.display_pc;
                if(destination.conditional) { out.second_successor = pc + 8u; }
                out.executable = true; out.unavailable_reason = native_next_policy::reason::none;
                return out;
            }
            if(decoded.delayed_direct_branch())
            {
                if(out.decoded.size != 4u || owner_end - pc <= 8u ||
                   !exact_boundary(display, owned, owner_begin, fallthrough, instruction_code) ||
                   !exact_boundary(display, owned, owner_begin, pc + 8u, instruction_code))
                { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                if(decoded.annulled_direct_branch() && !destination.conditional)
                {
                    // Static always/never annulled branches execute NO delay
                    // instruction. Authenticate the exact resulting PC AND NPC;
                    // never install a fictitious trap in the skipped slot.
                    auto const successor{destination.never_taken ? pc+8u : destination.display_pc};
                    if(owner_end-successor<=4u ||
                       !exact_boundary(display,owned,owner_begin,successor+4u,instruction_code))
                    { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                    out.first_successor=successor;
                    out.successor_npc[0u]=successor+4u;
                    out.executable=true;out.unavailable_reason=native_next_policy::reason::none;
                    return out;
                }
                out.first_successor = fallthrough; // Execute BRANCH only, park BEFORE the delay instruction.
                out.successor_npc[0u] = destination.never_taken ? pc+8u : destination.display_pc;
                out.successor_delay_slot[0u] = true;
                if(destination.conditional)
                {
                    out.second_successor = decoded.annulled_direct_branch() ? pc + 8u : fallthrough;
                    if(decoded.annulled_direct_branch() && (owner_end - pc <= 12u ||
                       !exact_boundary(display, owned, owner_begin, pc + 12u, instruction_code)))
                    { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                    out.successor_npc[1u] = decoded.annulled_direct_branch() ? pc + 12u : pc + 8u;
                    out.successor_delay_slot[1u] = !decoded.annulled_direct_branch();
                }
                out.executable = true; out.unavailable_reason = native_next_policy::reason::none;
                return out;
            }
            out.first_successor = destination.display_pc;
            if(destination.conditional)
            {
                if(fallthrough >= owner_end || !exact_boundary(display, owned, owner_begin, fallthrough, instruction_code))
                { out.unavailable_reason = native_next_policy::reason::branch_continuation_unavailable; return out; }
                out.second_successor = fallthrough;
            }
        }
        else
        {
            // Calls need an authenticated callee owner plus transactional owner
            // transfer; returns need actual private caller/continuation proof.
            // Refusal here leaves the original trap and its GPRs untouched.
            out.unavailable_reason = next.unavailable_reason;
            if(out.unavailable_reason == native_next_policy::reason::none)
            { out.unavailable_reason = native_next_policy::reason::other_control_flow; }
            return out;
        }
        out.executable = true;
        out.unavailable_reason = native_next_policy::reason::none;
        return out;
    }
}
