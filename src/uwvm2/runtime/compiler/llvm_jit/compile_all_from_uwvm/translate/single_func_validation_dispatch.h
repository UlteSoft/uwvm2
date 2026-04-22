// Keep a monolithic opcode switch in the LLVM JIT translator so the host
// compiler can still lower it into a jump table or other efficient dispatch
// structure, while the per-opcode-family logic lives in smaller headers.

// [before_section ... ] | opbase opextent
// [        safe       ] | unsafe (could be the section_end)
//                         ^^ code_curr

// A WebAssembly function with type `() -> ()` can have no meaningful runtime
// work, but its bytecode stream still must contain a valid terminating `end`.
for(;;)
{
    auto const instruction_begin{code_curr};

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

    wasm1_code curr_opbase;  // no initialization necessary
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
    bool llvm_jit_instruction_emitted_inline{};
    auto const disable_inline_llvm_jit_emission{[&]()
                                                {
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
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            emit_llvm_jit_active = false;
        }
    }
}
