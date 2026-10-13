// Trusted owned-function DATA selection only. This cannot create an event,
// cursor, execution lease, code borrow, register request or VM/host stop.
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <span>
# include "native_wasm_step_boundary.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_wasm_call_continuation
{
    struct selection
    {
        native_disassembly::instruction decoded{};
        ::std::uintptr_t continuation{};
        [[nodiscard]] explicit constexpr operator bool() const noexcept { return continuation != 0u; }
    };
    // The current genuine runtime capture/owner/ticket/generation/epoch and
    // kernel trap must be reauthenticated by the host before enabling any event.
    // Decode FORWARD from the actual function entry; no numeric PC is read.
    template<typename DisplayDecoder, typename SemanticDecoder>
    [[nodiscard]] inline selection prepare(DisplayDecoder& display, SemanticDecoder& semantics,
        ::std::span<::std::uint8_t const> owned, ::std::uintptr_t owner_begin,
        ::std::uintptr_t owner_end, ::std::uintptr_t pc, ::std::span<::std::uint8_t const> instruction_code = {}) noexcept
    {
        selection out{};
        if(!display || !semantics || owner_begin == 0u || owner_end <= owner_begin || owned.empty() ||
           owner_end - owner_begin != owned.size() ||
           pc < owner_begin || pc >= owner_end) { return out; }
        auto const page{native_disassembly::decode_window_with(display, owned, owner_begin, pc, 0, 0, 1u, instruction_code)};
        if(!page.available || page.count != 1u || !page.instructions[0u] || page.instructions[0u].pc != pc) { return out; }
        out.decoded = page.instructions[0u];
        auto const offset{static_cast<::std::size_t>(pc - owner_begin)};
        // [owned function bytes0 ... offset ... exact size] owned_end
        // [safe                                            ] exact interval
        //  ^^ and pc membership were checked BEFORE this bounded suffix borrow.
        auto const instruction{semantics.decode(pc, {owned.data() + offset, owned.size() - offset})};
        if(!instruction || !instruction.safe_for_call_continuation() ||
           instruction.semantics().size != out.decoded.size || out.decoded.size == 0u ||
           out.decoded.size >= owned.size() - offset) { return out; }
        // [current complete call pc ... size] [remaining same-owner bytes] end
        // [safe                            ] size < owner_end-pc above;
        //                     ^^ scalar continuation addition cannot wrap.
        auto const delay{instruction.call_delay_bytes()};
        if(delay)
        {
            if(delay!=4u || out.decoded.size!=4u || delay>=owned.size()-offset-out.decoded.size) { return out; }
            auto const slot{semantics.decode(pc+4u,owned.subspan(offset+4u))};
            if(!slot || slot.semantics().size!=4u || !slot.safe_for_single_instruction() ||
               !native_wasm_step_boundary::exact_boundary(display,owned,owner_begin,pc+4u,instruction_code)) { return out; }
        }
        auto const continuation{pc + out.decoded.size + delay};
        if(!native_wasm_step_boundary::exact_boundary(display, owned, owner_begin, continuation, instruction_code)) { return out; }
        out.continuation = continuation;
        return out;
    }
}
