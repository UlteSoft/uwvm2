/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
case static_cast<wasm1_code>(wasm1p1_code::atomic_prefix):
{
    // FE subopcode reserved ... (code_end)
    // [safe] unsafe (could be code_end)
    // ^^ op_begin borrows the dispatch-checked opcode.
    auto const op_begin{code_curr};
    ++code_curr;
    // [FE] subopcode reserved ... (code_end)
    // [safe] unsafe (could be code_end)
    //        ^^ code_curr: the prefix was available; the helper checks the entire immediate.
    namespace atomic = ::uwvm2::validation::standard::wasm3;
    auto const decoded{atomic::read_atomic_instruction64(code_curr, code_end, op_begin,
        !wasm1p1_para.disable_threads, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para), all_memory_count, memory_address_type_at, err)};
    auto const& instruction{decoded.immediate};
    bool const address64{decoded.address_type == atomic::storage_address_type::i64};
    auto const address_type{address64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
    // [FE subopcode immediate] ... (code_end)
    // [safe                 ] unsafe (could be code_end)
    //                         ^^ code_curr: all immediates decoded before stack validation.
    auto const emit_checked_atomic{[&]() constexpr noexcept
    {
        if(emit_llvm_jit_active)
        {
            llvm_jit_instruction_emitted_inline = true;
            atomic::validated_atomic_event event{};
            // [original checked opcode ... code_curr] | code_end
            // [safe same expression allocation    ] | offsets only; no source reread.
            if(!atomic::complete_atomic_event(event, decoded,
                   static_cast<::std::size_t>(op_begin - code_begin),
                   static_cast<::std::size_t>(code_curr - op_begin), control_flow_stack.size(), !is_polymorphic) ||
               !try_emit_runtime_local_func_llvm_jit_atomic(llvm_jit_emit_state, event)) [[unlikely]]
            { disable_inline_llvm_jit_emission(); }
        }
    }};
    if(instruction.descriptor.kind == atomic::atomic_instruction_kind::fence)
    {
        emit_checked_atomic();
        break;
    }

    bool const is_store{instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::store};
    bool const is_wait{instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::wait32 ||
                       instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::wait64};
    ::uwvm2::utils::container::u8string_view const name{is_wait ?
        (instruction.descriptor.value_i64 ? ::uwvm2::utils::container::u8string_view{u8"memory.atomic.wait64"} :
                                           ::uwvm2::utils::container::u8string_view{u8"memory.atomic.wait32"}) :
        instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::notify ?
            ::uwvm2::utils::container::u8string_view{u8"memory.atomic.notify"} : is_store ?
            ::uwvm2::utils::container::u8string_view{u8"atomic.store"} :
        instruction.descriptor.kind == ::uwvm2::validation::standard::wasm3::atomic_instruction_kind::load ?
            ::uwvm2::utils::container::u8string_view{u8"atomic.load"} : ::uwvm2::utils::container::u8string_view{u8"atomic.rmw"}};
    auto const count{static_cast<::std::size_t>(instruction.descriptor.operand_count)};
    decltype(try_pop_concrete_operand()) actual_atomic_operand{};
    auto const consume_atomic_operand{[&]() constexpr noexcept
    {
        // The common kernel proved one owned operand exists above this frame's
        // base before this single pop; no source cursor or guest pointer moves.
        actual_atomic_operand = try_pop_concrete_operand();
        return atomic::core3_operand{atomic::core3_operand_effective_type(actual_atomic_operand),
            !actual_atomic_operand.from_stack || actual_atomic_operand.is_unknown};
    }};
    auto const stack_failure{atomic::validate_atomic_operand_sequence(
        decoded, is_polymorphic, concrete_operand_count, consume_atomic_operand)};
    if(stack_failure.error == atomic::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, name, count); }
    if(stack_failure.error != atomic::typed_stack_error::ok) [[unlikely]]
    {
        // [FE] checked subopcode/immediates ... | code_end
        // [safe dispatch-checked opcode       ] | one-past is never dereferenced
        // ^^ op_begin -> err.err_curr: borrow the original opcode only for diagnostics.
        err.err_curr = op_begin;
        auto const expected_core{atomic::atomic_expected_operand_type(decoded, stack_failure.failed_pop_index)};
        auto const expected{expected_core.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ?
            curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
        if(stack_failure.failed_pop_index + 1u == instruction.descriptor.operand_count)
        {
            if(address64)
            {
                err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = name,
                    .expected_type = to_wasm1_diagnostic_value_type(address_type), .actual_type = to_wasm1_diagnostic_value_type(actual_atomic_operand.type)};
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
            }
            else
            {
                err.err_selectable.memarg_address_type_not_i32.op_code_name = name;
                err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_diagnostic_value_type(actual_atomic_operand.type);
                err.err_code = code_validation_error_code::memarg_address_type_not_i32;
            }
        }
        else
        {
            err.err_selectable.store_value_type_mismatch.op_code_name = name;
            err.err_selectable.store_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(expected);
            err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual_atomic_operand.type);
            err.err_code = code_validation_error_code::store_value_type_mismatch;
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    if(!is_store) { operand_stack_push(instruction.descriptor.result_i64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32); }
    emit_checked_atomic();
    break;
}
