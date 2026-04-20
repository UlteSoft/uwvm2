case wasm1_code::unreachable:
{
    // `unreachable` makes the operand stack "polymorphic" (per Wasm validation rules):
    // after an unreachable point, the following instructions are type-checked under the
    // assumption that any required operands can be popped (and any results pushed),
    // because this code path will not execute at runtime; this suppresses false
    // operand-stack underflow/type errors until the control-flow merges/ends.

    // unreachable ...
    // [   safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    ++code_curr;

    // unreachable ...
    // [   safe  ] unsafe (could be the section_end)
    //             ^^ code_curr

    // In Wasm validation, `unreachable` resets the operand stack height to the current label's base,
    // and then makes the stack polymorphic for subsequent type-checking.
    if(!control_flow_stack.empty())
    {
        auto const base{control_flow_stack.back_unchecked().operand_stack_base};
        operand_stack_truncate_to(base);
    }

    is_polymorphic = true;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unreachable(llvm_jit_emit_state)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::nop:
{
    // nop    ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    ++code_curr;

    // nop    ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_nop(llvm_jit_emit_state)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::block:
{
    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ op_begin

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // block blocktype ...
    // [     safe    ] unsafe (could be the section_end)
    //       ^^ op_begin

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // block blocktype ...
    // [     safe    ] unsafe (could be the section_end)
    //                 ^^ op_begin

    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
    runtime_block_result_type block_result{};

    switch(blocktype_byte)
    {
        case 0x40u:
        {
            // empty result
            block_result = {};
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            // Unknown blocktype encoding; treat as invalid code.
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    control_flow_stack.push_back(
        {.result = block_result, .operand_stack_base = operand_stack.size(), .type = block_type::block, .polymorphic_base = is_polymorphic});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_block(llvm_jit_emit_state, block_result)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::loop:
{
    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // loop blocktype ...
    // [    safe    ] unsafe (could be the section_end)
    //      ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // loop blocktype ...
    // [    safe    ] unsafe (could be the section_end)
    //                ^^ code_curr

    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
    runtime_block_result_type block_result{};

    switch(blocktype_byte)
    {
        case 0x40u:
        {
            // empty result
            block_result = {};
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    control_flow_stack.push_back({.result = block_result,
                                  .operand_stack_base = operand_stack.size(),
                                  .type = block_type::loop,
                                  .polymorphic_base = is_polymorphic,
                                  .then_polymorphic_end = false});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_loop(llvm_jit_emit_state, block_result)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::if_:
{
    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // if blocktype ...
    // [   safe   ] unsafe (could be the section_end)
    //    ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte blocktype_byte;  // No initialization necessary
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // if blocktype ...
    // [   safe   ] unsafe (could be the section_end)
    //              ^^ code_curr

    // MVP blocktype: 0x40 (empty) or a single value type (i32/i64/f32/f64)
    runtime_block_result_type block_result{};

    switch(blocktype_byte)
    {
        case 0x40u:
        {
            block_result = {};
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Stack effect: (i32 cond) -> () before entering the then branch.
    if(!is_polymorphic && operand_stack.empty()) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.operand_stack_underflow.op_code_name = u8"if";
        err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
        err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(!operand_stack.empty())
    {
        auto const cond{operand_stack_pop_unchecked()};

        if(cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.if_cond_type_not_i32.cond_type = cond.type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    control_flow_stack.push_back(
        {.result = block_result, .operand_stack_base = operand_stack.size(), .type = block_type::if_, .polymorphic_base = is_polymorphic});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_if(llvm_jit_emit_state, block_result)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::else_:
{
    // else   ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // else   ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // else   ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(control_flow_stack.empty() || control_flow_stack.back_unchecked().type != block_type::if_) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto& if_frame{control_flow_stack.back_unchecked()};

    // Validate the then-branch result types/count before switching to else.
    // In polymorphic mode (e.g. then branch was unreachable), result checking is suppressed.
    if(!is_polymorphic)
    {
        auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
        auto const actual_count{operand_stack.size() - if_frame.operand_stack_base};

        bool mismatch{expected_count != actual_count};

        ::uwvm2::parser::wasm::standard::wasm1::type::value_type expected_type{};
        ::uwvm2::parser::wasm::standard::wasm1::type::value_type actual_type{};

        bool const expected_single{expected_count == 1uz};
        bool const actual_single{actual_count == 1uz};

        if(expected_single) { expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*if_frame.result.begin); }
        if(actual_single) { actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back().type); }

        if(!mismatch && expected_single && actual_single && expected_type != actual_type) { mismatch = true; }

        if(mismatch) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
            err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
            err.err_selectable.if_then_result_mismatch.expected_type = expected_type;
            err.err_selectable.if_then_result_mismatch.actual_type = actual_type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Record then-branch reachability to merge with else at `end`.
    if_frame.then_polymorphic_end = is_polymorphic;

    // Start else branch with the operand stack at if-entry height.
    operand_stack_truncate_to(if_frame.operand_stack_base);
    is_polymorphic = if_frame.polymorphic_base;

    // Mark that else has been consumed.
    if_frame.type = block_type::else_;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_else(llvm_jit_emit_state)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::end:
{
    // end    ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // end    ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // end    ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    // `end` closes the innermost control frame (block/loop/if/function) and checks that the current
    // operand stack matches the declared block result type.

    if(control_flow_stack.empty()) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const frame{control_flow_stack.back_unchecked()};
    bool const is_function_frame{frame.type == block_type::function};

    ::uwvm2::utils::container::u8string_view block_kind;  // no initialization necessary
    switch(frame.type)
    {
        case block_type::function:
        {
            block_kind = u8"function";
            break;
        }
        case block_type::block:
        {
            block_kind = u8"block";
            break;
        }
        case block_type::loop:
        {
            block_kind = u8"loop";
            break;
        }
        case block_type::if_:
        {
            block_kind = u8"if";
            break;
        }
        case block_type::else_:
        {
            block_kind = u8"if-else";
            break;
        }
        [[unlikely]] default:
        {
            block_kind = u8"block";
            break;
        }
    }

    auto const expected_count{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};

    // Special rule: an `if` with a non-empty result type must have an `else` branch, otherwise the
    // false branch would not produce the required values.
    if(frame.type == block_type::if_ && expected_count != 0uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.if_missing_else.expected_count = expected_count;
        err.err_selectable.if_missing_else.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_missing_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const base{frame.operand_stack_base};
    auto const stack_size{operand_stack.size()};
    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

    // Stack end rule:
    // - In reachable code, the stack at `end` must match the block result types exactly.
    // - In polymorphic (unreachable) code, stack underflow is permitted, but extra values are not.
    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
    {
        err.err_curr = op_begin;
        err.err_selectable.end_result_mismatch.block_kind = block_kind;
        err.err_selectable.end_result_mismatch.expected_count = expected_count;
        err.err_selectable.end_result_mismatch.actual_count = actual_count;

        if(expected_count == 1uz)
        {
            err.err_selectable.end_result_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin);
        }
        else
        {
            err.err_selectable.end_result_mismatch.expected_type = {};
        }

        if(actual_count == 1uz && stack_size != 0uz)
        {
            err.err_selectable.end_result_mismatch.actual_type =
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back().type);
        }
        else
        {
            err.err_selectable.end_result_mismatch.actual_type = {};
        }

        err.err_code = ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // If the stack has enough values to satisfy the expected results, check their types even in
    // polymorphic (unreachable) mode; only the underflow aspect is suppressed.
    if(expected_count != 0uz && actual_count >= expected_count)
    {
        for(::std::size_t i{}; i != expected_count; ++i)
        {
            auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
            auto const actual_type{operand_stack[stack_size - 1uz - i].type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.end_result_mismatch.block_kind = block_kind;
                err.err_selectable.end_result_mismatch.expected_count = expected_count;
                err.err_selectable.end_result_mismatch.actual_count = actual_count;
                err.err_selectable.end_result_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.end_result_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Leave the frame: discard any intermediate values and push the declared results for outer typing.
    operand_stack_truncate_to(base);
    for(::std::size_t i{}; i != expected_count; ++i) { operand_stack_push(frame.result.begin[i]); }

    // Restore / merge the polymorphic state.
    if(frame.type == block_type::else_)
    {
        // For if-else, continuation is unreachable only when both branches are unreachable.
        is_polymorphic = frame.polymorphic_base || (frame.then_polymorphic_end && is_polymorphic);
    }
    else
    {
        is_polymorphic = frame.polymorphic_base;
    }

    // Pop the control frame.
    control_flow_stack.pop_back_unchecked();

    // The function body is a single expression terminated by `end`. When the function frame is closed,
    // validation of this function is complete and `end` must be the last opcode in the body.
    if(is_function_frame)
    {
        if(code_curr != code_end) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::trailing_code_after_end;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if(emit_llvm_jit_active)
        {
            llvm_jit_instruction_emitted_inline = true;
            if(!try_emit_runtime_local_func_llvm_jit_end(llvm_jit_emit_state) ||
               !finalize_runtime_local_func_llvm_jit_emit_state(llvm_jit_emit_state, *emitted_llvm_jit_ir_storage))
                [[unlikely]]
            {
                disable_inline_llvm_jit_emission();
            }
        }

        return;
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_end(llvm_jit_emit_state)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
