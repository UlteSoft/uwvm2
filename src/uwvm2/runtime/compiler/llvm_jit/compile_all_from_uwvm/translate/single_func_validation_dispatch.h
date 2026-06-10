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
    if(emit_llvm_jit_active && instruction_begin >= code_begin)
    {
        // Record the offset before validation advances the cursor.  Tiered OSR metadata and fallback diagnostics both
        // need the source opcode offset, not the offset after immediates have been consumed.
        llvm_jit_emit_state.current_wasm_op_offset = static_cast<::std::size_t>(instruction_begin - code_begin);
    }

    if(code_curr == code_end) [[unlikely]]
    {
        // [... ] | (end)
        // [safe] | unsafe (could be the section_end)
        //          ^^ code_curr

        // Validation completes only after consuming `end`. Reaching the raw end
        // of the bytecode buffer here therefore means the function body is missing
        // its terminating instruction.
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
    auto const disable_inline_llvm_jit_emission{[&]() constexpr noexcept {
                                                    // Validation continues even if inline LLVM emission is no longer
                                                    // possible.  Clearing the output storage tells the caller to use the
                                                    // interpreter/tiered fallback rather than a partially emitted module.
                                                    emit_llvm_jit_active = false;
                                                    if(emitted_llvm_jit_ir_storage != nullptr) { *emitted_llvm_jit_ir_storage = {}; }
                                                }};

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
        [[unlikely]] default:
        {
            err.err_curr = code_curr;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            break;
        }
    }

    if(emit_llvm_jit_active && !llvm_jit_instruction_emitted_inline)
    {
        // Most opcode cases validate only and leave IR emission to the single-instruction emitter.  Cases that need
        // validation-local data may emit inline and set `llvm_jit_instruction_emitted_inline` themselves.
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]] { emit_llvm_jit_active = false; }
    }
}
