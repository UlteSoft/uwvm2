// This header is intentionally included inside `validate_runtime_local_func`, where the validation state, bytecode cursor,
// operand stack, control stack, and optional LLVM JIT emit state are all local variables.  Keeping the dispatch body here
// avoids threading a very large state object through every opcode family while still keeping the opcode groups split into
// readable include files.
//
// The switch currently dispatches WebAssembly 1.0/MVP primary opcodes (`wasm1_code`).  When later WebAssembly proposals
// add opcode spaces that are not representable by this one-byte MVP enum, extend this dispatch layer and the included
// opcode-family files together so validation and optional LLVM emission stay in lockstep.
//
// Keep a monolithic opcode switch in the LLVM JIT translator so the host compiler can still lower it into a jump table or
// other efficient dispatch structure, while the per-opcode-family logic lives in smaller headers.

// [before_section ... ] | opbase opextent
// [        safe       ] | unsafe (could be the section_end)
//                         ^^ code_curr

// A WebAssembly function with type `() -> ()` can have no meaningful runtime
// work, but its bytecode stream still must contain a valid terminating `end`.
for(;;)
{
    auto const instruction_begin{code_curr};
    if(emit_llvm_jit_active && instruction_begin >= code_begin && instruction_begin < code_end)
    {
        // Record the offset before validation advances the cursor.  Tiered OSR metadata and fallback diagnostics both
        // need the source opcode offset, not the offset after immediates have been consumed.
        // [code_begin ... instruction_begin ...] | code_end
        // [               safe                 ] | unsafe (one-past)
        // Both source pointers belong to this validated expression span. The
        // checked byte offset is also used by optional full-JIT debug points.
        llvm_jit_emit_state.current_wasm_op_offset = static_cast<::std::size_t>(instruction_begin - code_begin);
    }
    else if(emit_llvm_jit_active) { llvm_jit_emit_state.current_wasm_op_offset = SIZE_MAX; }

    if(code_curr == code_end) [[unlikely]]
    {
        // [... ] | (end)
        // [safe] | unsafe (could be the section_end)
        //          ^^ code_curr

        // Validation completes only after consuming `end`. Reaching the raw end
        // of the bytecode buffer here therefore means the function body is missing
        // its terminating instruction.
        // [decoded prefix] remaining bytes ... | code_end
        // [readable bytes]                    | one-past is not dereferenced
        // ^^ code_curr -> err.err_curr: diagnostic copy; it may equal code_end.
        err.err_curr = code_curr;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_end;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // opbase ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    // The bytecode pointer may be unaligned.  Use memcpy instead of dereferencing a wasm1_code pointer so the dispatch is
    // well-defined on strict-alignment targets.
    wasm1_code curr_opbase;  // no initialization necessary
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
    bool llvm_jit_instruction_emitted_inline{};
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work)
    {
        // [code_begin ... current checked opcode ...] | code_end
        // [safe same expression allocation          ] | one-past
        // Offset only; observing never advances or reads this pointer.
        native_eh_work->start(static_cast<unsigned>(curr_opbase),
            static_cast<::std::size_t>(instruction_begin - code_begin), control_flow_stack.size());
    }
#endif
    auto const disable_inline_llvm_jit_emission{[&]() constexpr noexcept
                                                {
                                                    // Validation continues even if inline LLVM emission is no longer
                                                    // possible.  Clearing the output storage tells the caller to use the
                                                    // interpreter/tiered fallback rather than a partially emitted module.
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
                                                    if(native_eh_work) { native_eh_work->stop(native_eh_leaf_observer::decline::emission); }
#endif
                                                    emit_llvm_jit_active = false;
                                                    if(emitted_llvm_jit_ir_storage != nullptr)
                                                    {
                                                        // [same pinned expression: code_begin ... instruction_begin] end
                                                        // [safe] dispatch proved a live opcode BEFORE copying the checked
                                                        // scalar offset; no cursor advances and no byte is read again.
                                                        emitted_llvm_jit_ir_storage->note_first_decline(
                                                            llvm_jit_compiler_decline_stage::instruction_or_function_finish,
                                                            local_func_storage.function_index,
                                                            llvm_jit_emit_state.current_wasm_op_offset,
                                                            static_cast<unsigned>(curr_opbase));
                                                        emitted_llvm_jit_ir_storage->discard_emission_preserving_first_decline();
                                                    }
                                                }};

    // Attach an opcode provenance line before either inline or generic lowering.
    // The byte span/offset is already checked above; metadata adds no decoding pass
    // or executable debug poll, and invalid code never publishes this module.
    if(emit_llvm_jit_active &&
       !emit_runtime_local_func_llvm_jit_native_provenance(llvm_jit_emit_state, llvm_jit_emit_state.current_wasm_op_offset))
    { disable_inline_llvm_jit_emission(); }

    if(emit_llvm_jit_active && llvm_jit_emit_state.pending_numeric_plan != nullptr &&
       !pending_numeric_primary_opcode(static_cast<unsigned>(curr_opbase)))
    { disable_inline_llvm_jit_emission(); }

    // A lexical tentative compiler site, not a stop/resume permission. The
    // exact pre-op semantic locals/operand types come from THIS fused walk;
    // no body scan or physical LLVM-type projection is introduced. A failed
    // opcode rolls back this last tentative cell before any code publication.
    llvm_jit_checkpoint_opcode_transaction checkpoint_opcode_transaction{llvm_jit_emit_state};
    auto observer_stage{[&](runtime_local_func_llvm_jit_emit_state_t& actual) -> bool
        { return checkpoint_stage_observer_site(checkpoint_opcode_transaction,actual); }};
    struct observer_query_scope
    {
        runtime_local_func_llvm_jit_emit_state_t& actual;
        ~observer_query_scope() { actual.checkpoint_observer_site = {}; }
    } observer_query_lifetime{llvm_jit_emit_state};
    if(emit_llvm_jit_active && llvm_jit_emit_state.checkpoint_observer_controls != nullptr)
    {
        llvm_jit_emit_state.checkpoint_observer_site = {::std::addressof(observer_stage),
            +[](void* context,runtime_local_func_llvm_jit_emit_state_t& actual) -> bool
            { return (*static_cast<decltype(observer_stage)*>(context))(actual); }};
        if(curr_opbase == wasm1_code::end && control_flow_stack.size() == 1u &&
           !checkpoint_stage_observer_site(checkpoint_opcode_transaction,llvm_jit_emit_state))
        { disable_inline_llvm_jit_emission(); }
    }
    if(emit_llvm_jit_active && llvm_jit_emit_state.checkpoint_observer_controls == nullptr &&
       llvm_jit_emit_state.checkpoint_plan != nullptr && curr_opbase != wasm1_code::end &&
       !is_polymorphic && control_flow_stack.size() == 1u && control_flow_stack.back().type == block_type::function)
    {
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        auto const plan{llvm_jit_emit_state.checkpoint_plan};
        if(plan->producer_availability == checkpoint::status::ok &&
           (operand_stack.size() > SIZE_MAX - local_virtual_registers.size() ||
            !checkpoint::native_workspace_fits(local_virtual_registers.size(), local_virtual_registers.size() + operand_stack.size())))
        { plan->producer_availability = checkpoint::status::quota_exceeded; }
        if(plan->compiler_failure == checkpoint::status::ok && plan->producer_availability == checkpoint::status::ok && !plan->sites.empty() &&
           local_virtual_registers.size() <= plan->profile->limits().slots_per_frame &&
           operand_stack.size() <= plan->profile->limits().slots_per_frame - local_virtual_registers.size())
        {
            checkpoint::safepoint_layout site{plan->sites.front()};
            site.opcode_offset = llvm_jit_emit_state.current_wasm_op_offset;
            site.operand_count = operand_stack.size();
            bool complete{site.slots.size() == local_virtual_registers.size()};
            for(::std::size_t index{}; complete && index != local_virtual_registers.size(); ++index)
            {
                // [actual complete declaration + staged local arrays0..N] end
                // [safe] equal counts above BEFORE preserving original index.
                bool readable{};
                if(!debug_local_initialization.initialized(debug_local_initialization.context, index, readable))
                { complete = false; break; }
                site.slots[index].initialized = readable;
            }
            for(::std::size_t index{}; complete && index != operand_stack.size(); ++index)
            {
                // [actual pre-op semantic operand deque0 ... index ... N] end
                // [safe] index<N before borrow. Bottom without an actual live
                // type/payload remains unavailable, never a zero/SSA guess.
                auto const& operand{operand_stack[index]};
                if(operand.is_unknown) { complete = false; break; }
                auto const type{operand.has_core_type ? operand.core_type :
                    ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(operand.type)};
                if(!checkpoint::known_type(type)) { complete = false; break; }
                site.slots.push_back({type, true});
            }
            if(complete && !checkpoint_opcode_transaction.stage_before_opcode(::std::move(site)))
            { disable_inline_llvm_jit_emission(); }
        }
        // Unsupported control/EH/Bot/resource sites keep current_site==0.
        // They do NOT reject valid Wasm or change its original lowering.
    }

    // The opcode byte is in the checked expression span above. Emit ordinary
    // instruction observations before validation consumes immediates/operands;
    // invalid Wasm never publishes this module. Structural target points are
    // deferred until their loop/else/end emitters have selected the proper block.
    if(emit_llvm_jit_active && curr_opbase != wasm1_code::loop && curr_opbase != wasm1_code::else_ && curr_opbase != wasm1_code::end &&
       !emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(llvm_jit_emit_state)) [[unlikely]]
    {
        disable_inline_llvm_jit_emission();
    }

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch"
#elif defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch"
#endif
    switch(curr_opbase)
    {
#include "opcode/control_flow_cases.h"
#include "opcode/branch_cases.h"
#include "opcode/call_cases.h"
#include "opcode/variable_cases.h"
#include "opcode/memory_cases.h"
#include "opcode/const_compare_cases.h"
#include "opcode/int_numeric_cases.h"
#include "opcode/float_numeric_convert_cases.h"
#include "opcode/wasm1p1_cases.h"
#include "opcode/threads_cases.h"
#include "opcode/gc_cases.h"
        [[unlikely]] default:
        {
            // [decoded prefix] remaining bytes ... | code_end
            // [readable bytes]                    | one-past is not dereferenced
            // ^^ code_curr -> err.err_curr: diagnostic copy; it may equal code_end.
            err.err_curr = code_curr;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            break;
        }
    }
#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

    if(emit_llvm_jit_active && !llvm_jit_instruction_emitted_inline)
    {
        // Most opcode cases validate only and leave IR emission to the single-instruction emitter.  Cases that need
        // validation-local data may emit inline and set `llvm_jit_instruction_emitted_inline` themselves.
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    if(emit_llvm_jit_active && !emit_runtime_local_func_llvm_jit_native_numeric_values(llvm_jit_emit_state))
    { disable_inline_llvm_jit_emission(); }
    if(emit_llvm_jit_active && !checkpoint_opcode_transaction.commit_after_fused_opcode_validation())
    { disable_inline_llvm_jit_emission(); }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work && emit_llvm_jit_active)
    {
        // [validated instruction_begin ... code_curr] | code_end
        // [safe; code_curr may be this same one-past]  | never read by observer
        // Every immediate/operand and final original emission succeeded here.
        native_eh_work->commit(static_cast<::std::size_t>(code_curr - code_begin));
    }
#endif
}
