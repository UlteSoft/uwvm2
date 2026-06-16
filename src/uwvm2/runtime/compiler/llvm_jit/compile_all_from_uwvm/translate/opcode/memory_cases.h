// Memory opcode validation cases.
// Load/store instructions carry a `memarg` immediate encoded as two unsigned LEB128 values:
// `align` is the log2 alignment hint and `offset` is the static address offset.  The validation
// helpers consume those immediates, require memory 0 to exist for WebAssembly 1.0/MVP, enforce the
// maximum legal alignment exponent for the access width, and validate the operand-stack contract.
// The JIT path records the validated instruction byte range so the emit pass can reparse the same
// memarg and generate either direct-memory IR or a runtime memory bridge call.
//
// WebAssembly 1.0/MVP has one default memory and i32 addresses.  Multi-memory must thread the
// selected memory index through validation and emission; memory64 must update the address,
// memory.size, and memory.grow stack types together with LLVM effective-address lowering.

// i32.load
// Stack effect: (i32 address) -> (i32).  Validates a 4-byte integer load with max alignment
// exponent 2; the actual little-endian read and bounds handling are emitted later.
case wasm1_code::i32_load:
{
    validate_mem_load(u8"i32.load", 2u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load
// Stack effect: (i32 address) -> (i64).  Validates an 8-byte integer load with max alignment
// exponent 3.
case wasm1_code::i64_load:
{
    validate_mem_load(u8"i64.load", 3u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.load
// Stack effect: (i32 address) -> (f32).  Validates a 4-byte floating-point load; the payload bits
// are interpreted as IEEE-754 f32 by the emit/runtime path.
case wasm1_code::f32_load:
{
    validate_mem_load(u8"f32.load", 2u, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.load
// Stack effect: (i32 address) -> (f64).  Validates an 8-byte floating-point load from linear
// memory.
case wasm1_code::f64_load:
{
    validate_mem_load(u8"f64.load", 3u, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load8_s
// Stack effect: (i32 address) -> (i32).  Validates a one-byte load whose byte is sign-extended to
// i32 by the JIT emitter or runtime bridge.
case wasm1_code::i32_load8_s:
{
    validate_mem_load(u8"i32.load8_s", 0u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load8_u
// Stack effect: (i32 address) -> (i32).  Validates a one-byte load whose byte is zero-extended to
// i32.
case wasm1_code::i32_load8_u:
{
    validate_mem_load(u8"i32.load8_u", 0u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load16_s
// Stack effect: (i32 address) -> (i32).  Validates a two-byte load with sign-extension to i32.
case wasm1_code::i32_load16_s:
{
    validate_mem_load(u8"i32.load16_s", 1u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.load16_u
// Stack effect: (i32 address) -> (i32).  Validates a two-byte load with zero-extension to i32.
case wasm1_code::i32_load16_u:
{
    validate_mem_load(u8"i32.load16_u", 1u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load8_s
// Stack effect: (i32 address) -> (i64).  Validates a one-byte load with sign-extension to i64.
case wasm1_code::i64_load8_s:
{
    validate_mem_load(u8"i64.load8_s", 0u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load8_u
// Stack effect: (i32 address) -> (i64).  Validates a one-byte load with zero-extension to i64.
case wasm1_code::i64_load8_u:
{
    validate_mem_load(u8"i64.load8_u", 0u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load16_s
// Stack effect: (i32 address) -> (i64).  Validates a two-byte load with sign-extension to i64.
case wasm1_code::i64_load16_s:
{
    validate_mem_load(u8"i64.load16_s", 1u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load16_u
// Stack effect: (i32 address) -> (i64).  Validates a two-byte load with zero-extension to i64.
case wasm1_code::i64_load16_u:
{
    validate_mem_load(u8"i64.load16_u", 1u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load32_s
// Stack effect: (i32 address) -> (i64).  Validates a four-byte load with sign-extension to i64.
case wasm1_code::i64_load32_s:
{
    validate_mem_load(u8"i64.load32_s", 2u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.load32_u
// Stack effect: (i32 address) -> (i64).  Validates a four-byte load with zero-extension to i64.
case wasm1_code::i64_load32_u:
{
    validate_mem_load(u8"i64.load32_u", 2u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store
// Stack effect: (i32 address, i32 value) -> ().  Validates a full-width 4-byte integer store.
case wasm1_code::i32_store:
{
    validate_mem_store(u8"i32.store", 2u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store
// Stack effect: (i32 address, i64 value) -> ().  Validates a full-width 8-byte integer store.
case wasm1_code::i64_store:
{
    validate_mem_store(u8"i64.store", 3u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.store
// Stack effect: (i32 address, f32 value) -> ().  Validates a 4-byte floating-point store; the
// value's exact IEEE bit pattern is written by the emit/runtime path.
case wasm1_code::f32_store:
{
    validate_mem_store(u8"f32.store", 2u, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.store
// Stack effect: (i32 address, f64 value) -> ().  Validates an 8-byte floating-point store.
case wasm1_code::f64_store:
{
    validate_mem_store(u8"f64.store", 3u, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store8
// Stack effect: (i32 address, i32 value) -> ().  Validates a one-byte store; the stored byte is the
// low 8 bits of the i32 value.
case wasm1_code::i32_store8:
{
    validate_mem_store(u8"i32.store8", 0u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.store16
// Stack effect: (i32 address, i32 value) -> ().  Validates a two-byte store; the stored bytes are
// the low 16 bits of the i32 value.
case wasm1_code::i32_store16:
{
    validate_mem_store(u8"i32.store16", 1u, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store8
// Stack effect: (i32 address, i64 value) -> ().  Validates a one-byte store from the low 8 bits of
// the i64 value.
case wasm1_code::i64_store8:
{
    validate_mem_store(u8"i64.store8", 0u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store16
// Stack effect: (i32 address, i64 value) -> ().  Validates a two-byte store from the low 16 bits of
// the i64 value.
case wasm1_code::i64_store16:
{
    validate_mem_store(u8"i64.store16", 1u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.store32
// Stack effect: (i32 address, i64 value) -> ().  Validates a four-byte store from the low 32 bits
// of the i64 value.
case wasm1_code::i64_store32:
{
    validate_mem_store(u8"i64.store32", 2u, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, instruction_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// memory.size
// Stack effect: () -> (i32 current_pages).  WebAssembly 1.0/MVP still encodes a memory index
// immediate; this validator decodes it as LEB128, requires the decoded value to be zero, and then
// pushes the current page-count result type.  Multi-memory will relax the zero restriction, and
// memory64 must revisit the result type.
case wasm1_code::memory_size:
{
    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // WebAssembly 1.0/MVP keeps `memory.size` on memory 0 only, but the immediate is still represented as an unsigned
    // LEB128 memory index in the binary stream. The correct validation contract is to decode first and then require the
    // decoded value to be zero.  Future multi-memory support should replace this with selected-memory existence/type
    // validation instead of removing the decode step.
    //
    // This compiler-integrated validator intentionally mirrors the standalone validator so both
    // paths accept the same set of well-formed MVP binaries, including non-canonical zero LEB128
    // encodings that remain valid under the W3C integer grammar.
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // Enforce the MVP semantic restriction on the decoded memidx value.
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.size";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: () -> (i32)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}

// memory.grow
// Stack effect: (i32 delta_pages) -> (i32 previous_pages_or_minus1).  The immediate follows the
// same WebAssembly 1.0/MVP memory-index rule as `memory.size`, and the delta operand must be i32.
// Multi-memory must select the requested memory; memory64 must revisit the delta/result types.
case wasm1_code::memory_grow:
{
    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // `memory.grow` uses the same WebAssembly 1.0/MVP reserved memidx rule. Keep this logic aligned with the standard
    // validator: malformed LEB128 is invalid encoding; well-formed non-zero memidx is illegal MVP use.  Future
    // multi-memory support must turn this into selected-memory validation and pass the chosen index to the emitter.
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memidx;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // Enforce the MVP semantic restriction on the decoded memidx value.
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.grow";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: (i32 delta_pages) -> (i32 previous_pages_or_minus1)
    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz); }

    if(auto const delta{try_pop_concrete_operand()}; delta.from_stack && delta.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)
        [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.memory_grow_delta_type_not_i32.delta_type = delta.type;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::memory_grow_delta_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
