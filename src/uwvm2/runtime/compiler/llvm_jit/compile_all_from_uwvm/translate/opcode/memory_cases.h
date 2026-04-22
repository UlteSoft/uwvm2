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

    // MVP keeps `memory.size` on memory 0 only, but the immediate is still represented as an
    // unsigned LEB128 memory index in the binary stream. The correct validation contract is to
    // decode first and then require the decoded value to be zero.
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
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
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

    // `memory.grow` uses the same reserved memidx rule. Keep this logic aligned with the standard
    // validator: malformed LEB128 is invalid encoding; well-formed non-zero memidx is illegal MVP use.
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
    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz);
    }

    if(auto const delta{try_pop_concrete_operand()}; delta.from_stack &&
                                                   delta.type != ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32)
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
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
