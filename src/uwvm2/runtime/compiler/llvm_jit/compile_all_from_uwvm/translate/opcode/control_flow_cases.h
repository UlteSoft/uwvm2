    // Structured-control validation for the WebAssembly primary opcode set. Block signatures retain the complete
    // parameter/result tuples resolved from inline blocktypes or signed-s33 type indices.

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
        if(!try_emit_runtime_local_func_llvm_jit_unreachable(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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
        if(!try_emit_runtime_local_func_llvm_jit_nop(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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
    //        ^^ code_curr

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);

    enter_control_frame(op_begin, u8"block", block_type::block, block_signature);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_block(llvm_jit_emit_state, block_signature)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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

    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);

    enter_control_frame(op_begin, u8"loop", block_type::loop, block_signature);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_loop(llvm_jit_emit_state, block_signature)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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

    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);

    // Stack effect before entering the then branch: (params..., i32 cond) -> (params...).
    auto const param_count{get_runtime_block_result_count(block_signature.params)};
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const required_stack_size_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{required_stack_size_overflows ? max_operand_stack_requirement : param_count + 1uz};
    if(!is_polymorphic && (required_stack_size_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"if", required_stack_size);
    }

    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack && !cond.is_unknown && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.if_cond_type_not_i32.cond_type = to_wasm1_diagnostic_value_type(cond.type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    enter_control_frame(op_begin, u8"if", block_type::if_, block_signature);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_if(llvm_jit_emit_state, block_signature)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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

    // Validate the then-branch result before switching to else.
    // Match `end`: polymorphic mode only relaxes underflow, but still rejects extra values
    // and still checks types when enough concrete values are present.
    auto const expected_count{get_runtime_block_result_count(if_frame.result)};
    auto const base{if_frame.operand_stack_base};
    auto const stack_size{operand_stack.size()};
    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
    {
        err.err_curr = op_begin;
        err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
        err.err_selectable.if_then_result_mismatch.actual_count = actual_count;

        if(expected_count == 1uz)
        {
            err.err_selectable.if_then_result_mismatch.expected_type =
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*if_frame.result.begin);
        }
        else
        {
            err.err_selectable.if_then_result_mismatch.expected_type = {};
        }

        if(actual_count == 1uz && stack_size != 0uz)
        {
            err.err_selectable.if_then_result_mismatch.actual_type =
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand_stack.back().type);
        }
        else
        {
            err.err_selectable.if_then_result_mismatch.actual_type = {};
        }

        err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(expected_count != 0uz)
    {
        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{if_frame.result.begin[expected_count - 1uz - i]};
            auto const& actual_operand{operand_stack[stack_size - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                err.err_selectable.if_then_result_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.if_then_result_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::if_then_result_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }


    // Start else with the original block parameters above the outer stack height.
    operand_stack_truncate_to(if_frame.operand_stack_base);
    operand_stack_push_types(if_frame.params);
    // As in the spec's push_ctrl(else, ...), the else-frame itself starts reachable.
    is_polymorphic = false;

    // Mark that else has been consumed.
    if_frame.type = block_type::else_;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_else(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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

    auto const expected_count{get_runtime_block_result_count(frame.result)};

    // A missing else is an implicit identity arm over the block parameters. Do not accept merely equal arity:
    // every parameter type must match its corresponding result type.
    bool implicit_else_matches_result{true};
    if(frame.type == block_type::if_)
    {
        auto const param_count{get_runtime_block_result_count(frame.params)};
        implicit_else_matches_result = param_count == expected_count;
        for(::std::size_t i{}; implicit_else_matches_result && i != expected_count; ++i)
        {
            implicit_else_matches_result = frame.params.begin[i] == frame.result.begin[i];
        }
    }
    if(frame.type == block_type::if_ && !implicit_else_matches_result) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.if_missing_else.expected_count = expected_count;
        err.err_selectable.if_missing_else.expected_type =
            expected_count == 1uz ? static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(*frame.result.begin) :
                                   ::uwvm2::parser::wasm::standard::wasm1::type::value_type{};
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
    if(expected_count != 0uz)
    {
        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
            auto const& actual_operand{operand_stack[stack_size - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
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

    // Core 1/2 validation restores the enclosing control frame at `end`.
    // Its unreachable flag is not a control-flow merge: even two terminating
    // if arms (including br 0, which reaches this end) cannot make a later
    // missing operand valid. See Core 2, appendix 7.3, pop_ctrl/end.
    is_polymorphic = frame.polymorphic_base;

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
               !finalize_runtime_local_func_llvm_jit_emit_state(llvm_jit_emit_state, *emitted_llvm_jit_ir_storage)) [[unlikely]]
            {
                disable_inline_llvm_jit_emission();
            }
            else if(tiered_loop_reentries_out != nullptr) { *tiered_loop_reentries_out = llvm_jit_emit_state.tiered_loop_reentries; }
        }

        return;
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_end(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
