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
case static_cast<wasm1_code>(0x1f): // Core 3 try_table
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    if(curr_opbase == static_cast<wasm1_code>(0x1f))
    { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x1fu, op_begin, err); }
    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);
    ::uwvm2::runtime::compiler::shared::wasm_exception_control::handlers exception_handlers{};
    if(curr_opbase == static_cast<wasm1_code>(0x1f))
    {
        exception_handlers = ::uwvm2::runtime::compiler::shared::wasm_exception_control::read_handlers(
            code_curr, code_end, op_begin, curr_module, control_flow_stack.size(),
            [&](::std::size_t frame_index) constexpr noexcept
            {
                auto const& frame{control_flow_stack.index_unchecked(frame_index)};
                return frame.type == block_type::loop ? frame.params : frame.result;
            },
            [&](::std::size_t frame_index, ::std::size_t value_index) noexcept
            {
                auto const& frame{control_flow_stack.index_unchecked(frame_index)};
                auto const carriers{frame.type == block_type::loop ? frame.params : frame.result};
                return block_core_type_at(carriers, frame.signature_type_index,
                    frame.type != block_type::loop, frame.has_singleton_result_core_type,
                    frame.singleton_result_core_type, value_index);
            }, err);
        // [try_table blocktype checked catch vector] next ... code_end
        // [safe                                   ] unsafe (could be code_end)
        //                                           ^^ code_curr: transactional decoder committed before publishing this frame.
    }


    enter_control_frame(op_begin, u8"block", block_type::block, block_signature);
    control_flow_stack.back_unchecked().exception_handlers = ::std::move(exception_handlers);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work)
    {
        bool const numeric{native_eh_leaf_observer::numeric_block(block_signature, typesec)};
        if(curr_opbase == static_cast<wasm1_code>(0x1fu))
        { native_eh_work->prepare_handlers(control_flow_stack.back_unchecked().exception_handlers, control_flow_stack.size() - 1uz, numeric); }
        else { native_eh_work->non_numeric_control(numeric); }
    }
#endif

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_block(llvm_jit_emit_state, block_signature)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
        else if(curr_opbase == static_cast<wasm1_code>(0x1f) &&
                !try_record_runtime_local_func_llvm_jit_exception_handlers(llvm_jit_emit_state,
                    control_flow_stack.back_unchecked().exception_handlers, control_flow_stack.size() - 1uz)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work) { native_eh_work->non_numeric_control(native_eh_leaf_observer::numeric_block(block_signature, typesec)); }
#endif

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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    runtime_block_signature_type block_signature{};
    parse_validation_block_signature(op_begin, block_signature);
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work) { native_eh_work->non_numeric_control(native_eh_leaf_observer::numeric_block(block_signature, typesec)); }
#endif

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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            if(!block_value_matches(actual_operand, if_frame.result, if_frame.signature_type_index, true,
                if_frame.has_singleton_result_core_type, if_frame.singleton_result_core_type,
                expected_count - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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


    // Core 3 pop_ctrl restores locals initialized only in the then arm before starting else.
    if(!initialized_locals.restore(if_frame.local_init_checkpoint)) [[unlikely]] { runtime_storage_bug(); }

    // Start else with the original block parameters above the outer stack height.
    operand_stack_truncate_to(if_frame.operand_stack_base);
    operand_stack_push_types(if_frame.params, if_frame.signature_type_index, false);
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            auto const start_rich{block_core_type_at(frame.params, frame.signature_type_index, false,
                false, {}, i)};
            auto const result_rich{block_core_type_at(frame.result, frame.signature_type_index, true,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
            auto const start_type{start_rich.has_type ? start_rich.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.params.begin[i])};
            auto const result_type{result_rich.has_type ? result_rich.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.result.begin[i])};
            implicit_else_matches_result = frame.params.begin[i] == frame.result.begin[i] &&
                runtime_core3_value_type_matches(
                    start_type, result_type, typesec.owned_signatures);
        }
    }
    if(frame.type == block_type::if_ && !implicit_else_matches_result) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            if(!block_value_matches(actual_operand, frame.result, frame.signature_type_index, true,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type,
                expected_count - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
    operand_stack_push_types(frame.result, frame.signature_type_index, true, frame.singleton_result_witness,
        frame.singleton_result_core_type, frame.has_singleton_result_core_type);

    // Core 1/2 validation restores the enclosing control frame at `end`.
    // Its unreachable flag is not a control-flow merge: even two terminating
    // if arms (including br 0, which reaches this end) cannot make a later
    // missing operand valid. See Core 2, appendix 7.3, pop_ctrl/end.
    is_polymorphic = frame.polymorphic_base;

    // Core 3 pop_ctrl does not export local.set/tee effects from any nested block, loop, if, or try_table.
    if(!initialized_locals.restore(frame.local_init_checkpoint)) [[unlikely]] { runtime_storage_bug(); }

    if(checkpoint_observer_controls.selected() &&
       !checkpoint_observer_controls.close(frame.checkpoint_scope,static_cast<::std::size_t>(op_begin-code_begin)))
    { runtime_storage_bug(); }
    // [code_begin ... actual checked end opcode ...] | code_end
    // [safe same original walk allocation           ] | one-past
    // Its scalar offset resolves private forward links; no pointer advances.
    // Pop the control frame.
    control_flow_stack.pop_back_unchecked();

    // The function body is a single expression terminated by `end`. When the function frame is closed,
    // validation of this function is complete and `end` must be the last opcode in the body.
    if(is_function_frame)
    {
        if(code_curr != code_end) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::trailing_code_after_end;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if(emit_llvm_jit_active)
        {
            llvm_jit_instruction_emitted_inline = true;
            if(!try_emit_runtime_local_func_llvm_jit_end(llvm_jit_emit_state) ||
               !checkpoint_observer_controls.complete() ||
               !checkpoint_opcode_transaction.commit_after_fused_opcode_validation() ||
               !finalize_runtime_local_func_llvm_jit_emit_state(llvm_jit_emit_state, *emitted_llvm_jit_ir_storage)) [[unlikely]]
            {
                disable_inline_llvm_jit_emission();
            }
            else if(tiered_loop_reentries_out != nullptr) { *tiered_loop_reentries_out = llvm_jit_emit_state.tiered_loop_reentries; }
        }

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        if(native_eh_work && emit_llvm_jit_active)
        {
            // [validated final end ... code_curr == code_end] one-past
            // [safe] Offset only after result/trailing checks and LLVM finalization.
            native_eh_work->commit(static_cast<::std::size_t>(code_curr - code_begin), true);
        }
#endif
        return;
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_end(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
