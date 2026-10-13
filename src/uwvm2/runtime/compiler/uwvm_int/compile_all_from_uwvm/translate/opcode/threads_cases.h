/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
case static_cast<wasm_byte>(wasm1p1_code::atomic_prefix):
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
    //                         ^^ code_curr: helper committed all bounded immediates.
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if(instruction.descriptor.kind == atomic::atomic_instruction_kind::fence)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_atomic_fence_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
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
                    .expected_type = to_wasm1_value_type(address_type), .actual_type = to_wasm1_value_type(actual_atomic_operand.type)};
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
            }
            else
            {
                err.err_selectable.memarg_address_type_not_i32.op_code_name = name;
                err.err_selectable.memarg_address_type_not_i32.addr_type = to_wasm1_value_type(actual_atomic_operand.type);
                err.err_code = code_validation_error_code::memarg_address_type_not_i32;
            }
        }
        else
        {
            err.err_selectable.store_value_type_mismatch.op_code_name = name;
            err.err_selectable.store_value_type_mismatch.expected_type = to_wasm1_value_type(expected);
            err.err_selectable.store_value_type_mismatch.actual_type = to_wasm1_value_type(actual_atomic_operand.type);
            err.err_code = code_validation_error_code::store_value_type_mismatch;
        }
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const result_type{instruction.descriptor.result_i64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
    if(!is_store) { operand_stack_push(result_type); }
    current_memory_index = instruction.memory.memory_index;
    current_memory_address64 = address64;
    // Memory32 keeps its four-byte immediate ABI; the typed decoder proved the
    // narrowing. Memory64 handlers consume the complete eight-byte offset.
    auto const emit_offset{[&]() constexpr
    {
        if(address64) { emit_imm_to(bytecode, instruction.memory.offset); }
        else { emit_imm_to(bytecode, static_cast<::std::uint32_t>(instruction.memory.offset)); }
    }};
    ensure_memory_resolved();
    if(is_wait || instruction.descriptor.kind == atomic::atomic_instruction_kind::notify)
    {
        // Blocking operations have the same register lifetime boundary as calls.
        // Materialize older cached operands as well as arguments, then restore
        // the canonical ring after the shared wait helper releases its locks.
        if constexpr(stacktop_enabled)
        { if(!is_polymorphic) { stacktop_flush_all_to_operand_stack(bytecode); } }
        auto emit{[&]<unsigned Operation>() constexpr
        {
            if(address64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_wait_notify_fptr_from_tuple<Operation, CompileOption>(interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_atomic_wait_notify_fptr_from_tuple<Operation, CompileOption>(interpreter_tuple)); }
        }};
        switch(instruction.opcode)
        {
            case 0u: emit.template operator()<0u>(); break;
            case 1u: emit.template operator()<1u>(); break;
            case 2u: emit.template operator()<2u>(); break;
            default: ::fast_io::fast_terminate();
        }
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_offset();
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic)
            {
                stacktop_commit_pop_n(count);
                codegen_stack_pop_n(count);
                auto const begin{stacktop_range_begin_pos(curr_operand_stack_value_type::i32)};
                auto const end{stacktop_range_end_pos(curr_operand_stack_value_type::i32)};
                auto const pos{stacktop_currpos_for_range(begin, end)};
                stacktop_set_currpos_for_range(begin, end, stacktop_ring_prev(pos, begin, end));
                ++stacktop_memory_count;
                codegen_stack_push(curr_operand_stack_value_type::i32);
                stacktop_cache_count = stacktop_cache_i32_count = stacktop_cache_i64_count = 0uz;
                stacktop_cache_f32_count = stacktop_cache_f64_count = 0uz;
                stacktop_fill_to_canonical(bytecode);
            }
        }
        break;
    }
    if(is_store)
    {
        auto emit_store{[&]<bool Wide, ::std::size_t Bytes>() constexpr
        {
            if(address64)
            {
                using value_type = ::std::conditional_t<Wide, wasm_i64, wasm_i32>;
                emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_atomic_fptr_from_tuple<value_type, Bytes, true, CompileOption>(
                    curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_atomic_store_fptr_from_tuple<Wide, Bytes, CompileOption>(
                    curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            }
        }};
        switch(instruction.opcode)
        {
            case 0x17u: emit_store.template operator()<false,4uz>(); break;
            case 0x18u: emit_store.template operator()<true,8uz>(); break;
            case 0x19u: emit_store.template operator()<false,1uz>(); break;
            case 0x1au: emit_store.template operator()<false,2uz>(); break;
            case 0x1bu: emit_store.template operator()<true,1uz>(); break;
            case 0x1cu: emit_store.template operator()<true,2uz>(); break;
            case 0x1du: emit_store.template operator()<true,4uz>(); break;
            default: ::fast_io::fast_terminate();
        }
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_offset();
        stacktop_after_pop_n_if_reachable(bytecode, 2uz);
        break;
    }
    if(instruction.descriptor.kind >= atomic::atomic_instruction_kind::add)
    {
        namespace shared_atomic = ::uwvm2::runtime::compiler::shared::wasm_threads;
        auto emit_family{[&]<shared_atomic::atomic_rmw_operation Operation>() constexpr
        {
            auto emit_width{[&]<bool Wide, ::std::size_t Bytes>() constexpr
            {
                if(address64)
                { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_rmw_fptr_from_tuple<Operation, Wide, Bytes, CompileOption>(
                    curr_stacktop, *resolved_memory.memory_p, interpreter_tuple)); }
                else
                { emit_opfunc_to(bytecode, translate::get_uwvmint_atomic_rmw_fptr_from_tuple<Operation, Wide, Bytes, CompileOption>(
                    curr_stacktop, *resolved_memory.memory_p, interpreter_tuple)); }
            }};
            switch((instruction.opcode - 0x1eu) % 7u)
            {
                case 0u: emit_width.template operator()<false,4uz>(); break;
                case 1u: emit_width.template operator()<true,8uz>(); break;
                case 2u: emit_width.template operator()<false,1uz>(); break;
                case 3u: emit_width.template operator()<false,2uz>(); break;
                case 4u: emit_width.template operator()<true,1uz>(); break;
                case 5u: emit_width.template operator()<true,2uz>(); break;
                case 6u: emit_width.template operator()<true,4uz>(); break;
                default: ::fast_io::fast_terminate();
            }
        }};
        switch(instruction.descriptor.kind)
        {
            case atomic::atomic_instruction_kind::add: emit_family.template operator()<shared_atomic::atomic_rmw_operation::add>(); break;
            case atomic::atomic_instruction_kind::sub: emit_family.template operator()<shared_atomic::atomic_rmw_operation::sub>(); break;
            case atomic::atomic_instruction_kind::and_: emit_family.template operator()<shared_atomic::atomic_rmw_operation::and_>(); break;
            case atomic::atomic_instruction_kind::or_: emit_family.template operator()<shared_atomic::atomic_rmw_operation::or_>(); break;
            case atomic::atomic_instruction_kind::xor_: emit_family.template operator()<shared_atomic::atomic_rmw_operation::xor_>(); break;
            case atomic::atomic_instruction_kind::exchange: emit_family.template operator()<shared_atomic::atomic_rmw_operation::exchange>(); break;
            case atomic::atomic_instruction_kind::compare_exchange: emit_family.template operator()<shared_atomic::atomic_rmw_operation::compare_exchange>(); break;
            default: ::fast_io::fast_terminate();
        }
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_offset();
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, count, result_type);
        break;
    }
    // FE is a fusion boundary. Reserve a result slot before emitting a load
    // into a different physical ring; the address ring supplies its own room.
    if(stacktop_enabled_for_vt(result_type) && !stacktop_ranges_merged_for(address_type, result_type))
    { stacktop_prepare_push1_if_reachable(bytecode, result_type); }
    auto emit_load{[&]<bool Wide, ::std::size_t Bytes>() constexpr
    {
        if(address64)
        {
            using value_type = ::std::conditional_t<Wide, wasm_i64, wasm_i32>;
            emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_atomic_fptr_from_tuple<value_type, Bytes, false, CompileOption>(
                curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_atomic_load_fptr_from_tuple<Wide, Bytes, CompileOption>(
                curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
    }};
    switch(instruction.opcode)
    {
        case 0x10u: emit_load.template operator()<false,4uz>(); break;
        case 0x11u: emit_load.template operator()<true,8uz>(); break;
        case 0x12u: emit_load.template operator()<false,1uz>(); break;
        case 0x13u: emit_load.template operator()<false,2uz>(); break;
        case 0x14u: emit_load.template operator()<true,1uz>(); break;
        case 0x15u: emit_load.template operator()<true,2uz>(); break;
        case 0x16u: emit_load.template operator()<true,4uz>(); break;
        default: ::fast_io::fast_terminate();
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_offset();
    if(address64) { stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, result_type); }
    else if constexpr(stacktop_enabled)
    {
        // Preserve the existing memory32 load's identity transition and fusion ABI.
        if(instruction.descriptor.result_i64)
        {
            if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
            { if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); } }
            else { stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64); }
        }
    }
    break;
}
