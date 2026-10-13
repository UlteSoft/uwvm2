    // WebAssembly 1.1 instruction support for the UWVM interpreter translator.
    // Feature switches are consumed here, during translation, so runtime opfuncs stay branch-free.

case static_cast<wasm_byte>(wasm1p1_code::table_get):
{
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end); dispatch proved the opcode.
    // [select opcode] result vector ... code_end
    // [safe         ] dispatch proved the opcode readable; op_begin borrows that address.
    auto const op_begin{code_curr};
    ++code_curr;
    // [select opcode] result vector ... code_end
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr after the proven one-byte opcode.
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end)
    //                ^^ code_curr after the proven one-byte opcode.

    if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_u32(wasm1p1_code::table_get),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    // [consumed opcode] table immediate ... code_end

    // [safe           ] unsafe (could be code_end); bounded decoder commits

    // code_curr only after the complete u32 field, never beyond code_end.

    auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.get")};
    check_table_index(op_begin, table_index, opcode_u32(wasm1p1_code::table_get));
    auto const event{validate_table_access.template operator()<0x25u>(op_begin, table_index, u8"table.get")};
    // Use the SAME first-typed carrier for the untouched ring/opfunc emitter.
    auto const table_type{static_cast<curr_operand_stack_value_type>(event.element_carrier)};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const table_ptr{resolve_runtime_table(table_index)};
    auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
        ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
    stacktop_flush_all_to_operand_stack(bytecode);
    if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
    { emit_table64_op.template operator()<table64_operation::get>(table_type, gc_table); }
    else if(gc_table)
    { emit_table64_op.template operator()<table64_operation::get, false>(table_type, true); }
    else if(table_type == static_cast<curr_operand_stack_value_type>(0x69u))
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_get_exnref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    else if(table_type == curr_operand_stack_value_type::externref)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_get_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_get_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, table_ptr);
    if(table_type == static_cast<curr_operand_stack_value_type>(0x69u) || gc_table)
    {
        // The exnref handler retains a checked token in the caller module before publishing its result.
        emit_imm_to(bytecode, ::std::addressof(curr_module));
    }
    stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, table_type);
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::table_set):
{
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end); dispatch proved the opcode.
    auto const op_begin{code_curr};
    ++code_curr;
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end)
    //                ^^ code_curr after the proven one-byte opcode.

    if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_u32(wasm1p1_code::table_set),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    // [consumed opcode] table immediate ... code_end

    // [safe           ] unsafe (could be code_end); bounded decoder commits

    // code_curr only after the complete u32 field, never beyond code_end.

    auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.set")};
    check_table_index(op_begin, table_index, opcode_u32(wasm1p1_code::table_set));
    auto const event{validate_table_access.template operator()<0x26u>(op_begin, table_index, u8"table.set")};
    // Use the SAME first-typed carrier for the untouched ring/opfunc emitter.
    auto const table_type{static_cast<curr_operand_stack_value_type>(event.element_carrier)};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const table_ptr{resolve_runtime_table(table_index)};
    auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
        ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
    stacktop_flush_all_to_operand_stack(bytecode);
    if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
    { emit_table64_op.template operator()<table64_operation::set>(table_type, gc_table); }
    else if(gc_table)
    { emit_table64_op.template operator()<table64_operation::set, false>(table_type, true); }
    else if(table_type == static_cast<curr_operand_stack_value_type>(0x69u))
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_set_exnref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    else if(table_type == curr_operand_stack_value_type::externref)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_set_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_table_set_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, table_ptr);
    if(::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
       ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::function)
    { emit_imm_to(bytecode, ::std::addressof(curr_module)); }
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::select_t):
{
    // [select opcode] result-type vector ... code_end
    // [safe         ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved the opcode byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [select opcode] result-type vector ... code_end
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr after the checked opcode.

    if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::select_t),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    auto const result_type_count{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"select.result_types")};

    // select_t result_type_count result_type ...
    // [           safe         ] unsafe (could be the section_end)
    //                            ^^ code_curr

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const emit_select_for_type{
        [&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
        {
            // Core 3 exnref is an ABI carrier outside the named legacy value-type enum.
            // Handle it before the exhaustive switch so -Wswitch keeps auditing legacy cases.
            if(static_cast<wasm_byte>(vt) == 0x69u)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                return;
            }
            switch(vt)
            {
                case curr_operand_stack_value_type::i32:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::i64:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::f32:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::f64:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::v128:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_v128_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::funcref:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                case curr_operand_stack_value_type::externref:
                    emit_opfunc_to(bytecode, translate::get_uwvmint_select_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    break;
                [[unlikely]] default:
                    ::fast_io::fast_terminate();
            }
        }};

    if(result_type_count != 1u) [[unlikely]] { fail_invalid_immediate(op_begin, u8"select.result_types"); }

    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_core_type{};
    ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
    // The initializer retains a validated type context for mixed function/aggregate tables.
    // A legacy function-only table has no context; the bounded scanner handles that fallback.
    auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
    auto const& context{retained_context == nullptr ? empty_context : *retained_context};
    auto const result_type_byte{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
        code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
        runtime_type_count, ::std::addressof(result_core_type), context,
        !wasm1p1_para.disable_gc, !wasm1p1_para.disable_exceptions)};

    // select_t result_type_count result_type ...
    // [                 safe               ] unsafe (could be the section_end)
    //                                        ^^ code_curr

    // Field commits are transactional: a count-LEB decode failure leaves the cursor after the opcode; a decoded-count
    // arity rejection or result-type decode failure leaves it after the count; a type-policy rejection follows the complete value encoding.
    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
    // The bounded Core 3 decoder checked exn/noexn and the exceptions gate; this
    // opcode already checked reference-types. The old wasm1p1 enum has no 0x69.
    if(result_type_byte != 0x69u)
    { ensure_wasm1p1_value_type_enabled(op_begin, result_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }

    auto const event{validate_typed_select(op_begin, result_type, result_core_type)};
    // SAME owned third-pop metadata preserves the original physical ring decision.
    // Common typing already replaced the logical three inputs by declared result.
    if(event.left_concrete)
    {
        if(!event.left_unknown) { emit_select_for_type(result_type); }
        stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    }
    if constexpr(FusedI32Sink::receives_fused_i32_operations)
    { fused_i32_transaction.typed_select(event); }

    break;
}

case static_cast<wasm_byte>(wasm1p1_code::i32_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::i32_extend8_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    validate_numeric_unary(u8"i32.extend8_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_extend8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::i32_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::i32_extend16_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    validate_numeric_unary(u8"i32.extend16_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_extend16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::i64_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::i64_extend8_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    validate_numeric_unary(u8"i64.extend8_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::i64_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::i64_extend16_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    validate_numeric_unary(u8"i64.extend16_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::i64_extend32_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::i64_extend32_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    validate_numeric_unary(u8"i64.extend32_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::ref_null):
{
    // [ref.null] heap ... code_end
    // [safe    ] unsafe (could be code_end)
    // ^^ code_curr / op_begin: outer dispatch proved this opcode byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.null] heap ... code_end
    // [safe    ] unsafe (could be code_end)
    //            ^^ code_curr: now checked by the shared bounded heap decoder.

    if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
    }

    // The complete Core 3 context classifies defined heaps. Abstract GC heaps
    // remain valid with an empty context even when legacy function types exist.
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    t3::heap_type decoded_heap{};
    unsigned rt_byte{};
    bool exact_function_heap{};
    auto const core3_context{curr_module.type_section_storage.core3_context_ptr};
    auto const type_count{get_runtime_type_section_count(curr_module)};
    if(core3_context != nullptr || !wasm1p1_para.disable_gc)
    {
        ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
        auto const& context{core3_context == nullptr ? empty_context : *core3_context};
        auto const decoded{::uwvm2::validation::standard::wasm3::scan_core3_ref_null_heap(
            code_curr, code_end, context, !wasm1p1_para.disable_gc,
            !wasm1p1_para.disable_function_references, !wasm1p1_para.disable_exceptions)};
        // [ref.null][checked signed-33 heap] next ... code_end
        // [safe                            ] unsafe (could be code_end)
        //                                  ^^ code_curr: scanner commits only a complete, feature-enabled heap.
        using error = ::uwvm2::validation::standard::wasm3::core3_ref_null_error;
        if(decoded.error == error::gc_disabled) [[unlikely]]
        { fail_wasm1p1_feature_required(op_begin, opcode_u32(wasm1p1_code::ref_null),
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
        if(decoded.error == error::function_references_disabled) [[unlikely]]
        { fail_wasm1p1_feature_required(op_begin, opcode_u32(wasm1p1_code::ref_null),
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
        if(decoded.error == error::exceptions_disabled) [[unlikely]]
        { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(false, 0xd0u, op_begin, err); }
        if(decoded.error != error::ok) [[unlikely]] { fail_invalid_immediate(op_begin, u8"ref.null"); }
        decoded_heap = decoded.heap;
        rt_byte = decoded.carrier;
        if(decoded.heap.is_defined())
        {
            // contains() was proved by the decoder before this borrowed type-kind read.
            exact_function_heap = context.records.index_unchecked(static_cast<::std::size_t>(decoded.heap.code)).kind ==
                t3::composite_kind::function;
        }
    }
    else
    {
        // [ref.null] heap ... code_end
        // [safe    ] unsafe (could be code_end)
        //            ^^ code_curr: the bounded scanner commits only a complete valid heap.
        auto const decoded{::uwvm2::validation::standard::wasm3::scan_function_ref_null_heap(
            code_curr, code_end, !wasm1p1_para.disable_function_references, type_count, true, !wasm1p1_para.disable_gc)};
        // [ref.null][complete signed heap immediate] next ... code_end on success.
        // [safe                                   ] unsafe (could be code_end)
        //                                           ^^ code_curr: unchanged on every scanner failure.
        if(decoded.error != ::uwvm2::validation::standard::wasm3::function_heap_immediate_error::ok) [[unlikely]]
        {
            // A failed bounded scan did not commit code_curr. Only this cold error path
            // repeats the immutable immediate to preserve the shared diagnostic policy.
            // [ref.null] [failed heap ... code_end)
            // [safe    ] unsafe (could be code_end)
            //            ^^ code_curr: the wrapper retains it on failure, then throws.
            static_cast<void>(::uwvm2::validation::standard::wasm3::read_function_ref_null_carrier(
                code_curr, code_end, !wasm1p1_para.disable_function_references, type_count,
                op_begin, err, true, !wasm1p1_para.disable_gc));
            ::fast_io::fast_terminate(); // The same immutable immediate cannot succeed after its failed scan.
        }
        rt_byte = decoded.carrier;
        decoded_heap = decoded.heap;
        exact_function_heap = decoded.heap.is_defined();
    }
    if(rt_byte == 0x69u)
    {
        ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(
            !wasm1p1_para.disable_exceptions, 0xd0u, op_begin, err);
    }
    auto const rt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(rt_byte)};
    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
    if(rt != reference_type::funcref && rt != reference_type::externref && rt_byte != 0x69u) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = rt_byte;
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const vt{rt_byte == 0x69u ? static_cast<curr_operand_stack_value_type>(0x69u) :
        static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(rt))};
    // For 0x69, ref.null checked reference-types before decoding and checked
    // exceptions above. Preserve the legacy gate for every other carrier.
    if(rt_byte != 0x69u)
    { ensure_wasm1p1_value_type_enabled(op_begin, vt, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type); }
    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_null_event(
        decoded_heap, rt_byte, exact_function_heap)};
    operand_stack_push(vt);
    // ref.null preserves the exact decoded heap for every ABI carrier. In
    // particular, noextern must remain the external bottom heap, not extern.
    {
        // [validation-owned operand stack ... last pushed element] one-past
        // [safe                                                 ] push above proved this element exists.
        auto& top{operand_stack.back_unchecked()};
        top.core_type = event.result_type;
        top.has_core_type = true;
        top.exact_function_type_index = event.exact_function_type_index;
    }

    // [ref.null][checked exn/noexn heap][next opcode or code_end]
    // [safe                               ] unsafe (possibly code_end)
    //                                     ^^ code_curr: inspect only after the endpoint proof.
    // Keep the adjacent null-throw fastpath and the event's exact semantic
    // heap. This branch emits no carrier/opfunc or register-ring spill.
    if(event.carrier == 0x69u && code_curr != code_end && ::std::to_integer<unsigned>(*code_curr) == 0x0au)
    {
        static_null_exception_pending = true;
        break;
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    stacktop_prepare_push1_if_reachable(bytecode, vt);
    if(vt == curr_operand_stack_value_type::funcref)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_ref_null_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_ref_null_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
    }
    stacktop_commit_push1_typed_if_reachable(vt);
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::ref_is_null):
{
    // [ref.is_null] next opcode ... code_end
    // [safe       ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved this opcode exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.is_null] next opcode ... code_end
    // [safe       ] unsafe (could be code_end)
    //               ^^ code_curr; this opcode has no immediate.

    if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_is_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    }

    namespace reference_semantics = ::uwvm2::validation::standard::wasm3;
    decltype(try_pop_concrete_operand()) ref_value{};
    auto const consume_reference{[&]() constexpr noexcept
    {
        // Count > 0 in the common transition proves the actual top entry.
        // The native pop copies its rich type before retiring that stack slot.
        ref_value = try_pop_concrete_operand();
        return reference_semantics::core3_reference_operand{
            reference_semantics::core3_operand_effective_type(ref_value),
            !ref_value.from_stack || ref_value.is_unknown, static_cast<unsigned>(ref_value.type)};
    }};
    auto const transition{reference_semantics::apply_core3_ref_is_null_typed_transition(
        is_polymorphic, concrete_operand_count, consume_reference)};
    if(transition.error == reference_semantics::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }
    if(transition.error != reference_semantics::typed_stack_error::ok) [[unlikely]]
    {
        // [caller-saved checked opcode] next ... | code_end
        // [safe                       ]         | one-past is never read.
        // ^^ op_begin -> err.err_curr: copy only; dispatch owns the range proof.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(ref_value.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const& event{transition.event};
    operand_stack_push(curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    if(!event.concrete_input || event.input_carrier == 0x70u)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
    }
    stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    break;
}


case static_cast<wasm_byte>(0xd3u): // Core 3 ref.eq
{
    // [ref.eq] next ... code_end
    // [safe  ] unsafe (could be code_end)
    // ^^ op_begin: the outer dispatcher proved this complete opcode byte.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.eq] next ... code_end
    // [safe  ] unsafe (possibly one-past)
    //          ^^ code_curr: ref.eq has no immediate.
    if(wasm1p1_para.disable_gc) [[unlikely]]
    { fail_wasm1p1_feature_required(op_begin, 0xd3u,
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
    if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.eq", 2uz); }
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    t3::core_value_type const expected{t3::value_kind::reference,
        {static_cast<::std::int_least64_t>(t3::abstract_heap_type::eq)}, true};
    auto const signatures{::uwvm2::validation::standard::wasm3::core3_signature_view<
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{
            rich_owned_begin, rich_owned_available ? runtime_type_count : 0uz}};
    for(unsigned index{}; index != 2u; ++index)
    {
        auto const operand{try_pop_concrete_operand()};
        if(operand.from_stack && !operand.is_unknown &&
           !runtime_core3_value_type_matches(
               ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(operand),
               expected, signatures)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Borrowed checked opcode; no pointer movement.
            err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(operand.type);
            err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    operand_stack_push(curr_operand_stack_value_type::i32);
    // Materialize both reference carriers before the handler reads the contiguous operand stack.
    if(!is_polymorphic) { stacktop_flush_all_to_operand_stack(bytecode); }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_gc_ref_eq_fptr_from_tuple<CompileOption>(interpreter_tuple));
    stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    break;
}

case static_cast<wasm_byte>(0xd5u):
case static_cast<wasm_byte>(0xd6u):
{
    // [br_on_null / br_on_non_null] labelidx ... next opcode ... code_end
    // [safe                      ] unsafe (could be code_end)
    // ^^ code_curr: outer dispatch checked this opcode byte.
    auto const op_begin{code_curr};
    auto const branch_non_null{static_cast<unsigned>(*code_curr) == 0xd6u};
    auto const op_name{branch_non_null ? ::uwvm2::utils::container::u8string_view{u8"br_on_non_null"} : ::uwvm2::utils::container::u8string_view{u8"br_on_null"}};
    ++code_curr;
    // [branch opcode] labelidx ... next opcode ... code_end
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr: bounded decoder must check the complete u32 immediate.
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, branch_non_null ? 0xd6u : 0xd5u, op_begin, err);
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;
    auto const [label_next, label_error]{::fast_io::parse_by_scan(
        reinterpret_cast<char8_t const*>(code_curr), reinterpret_cast<char8_t const*>(code_end), ::fast_io::mnp::leb128_get(label_index))};
    if(label_error != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Borrow the checked opcode for diagnostics; no dereference.
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_error);
    }
    // [branch opcode][complete labelidx] next opcode ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                 ^^ code_curr; label_next is proven within [code_curr, code_end].
    code_curr = reinterpret_cast<::std::byte const*>(label_next);
    // [branch opcode][complete labelidx] next opcode ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                                   ^^ code_curr
    auto const label_count{control_flow_stack.size()};
    if(static_cast<::std::uint_least64_t>(label_index) >= label_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Diagnostic borrow within the checked instruction.
        err.err_selectable.illegal_label_index = {.label_index = label_index,
            .all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(label_count)};
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto& target_frame{control_flow_stack.index_unchecked(label_count - 1uz - static_cast<::std::size_t>(label_index))};
    auto const target_types{target_frame.label};
    auto const target_arity{target_types.begin == target_types.end ? 0uz : static_cast<::std::size_t>(target_types.end - target_types.begin)};
    if(branch_non_null && (target_arity == 0uz ||
       !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(target_types.begin[target_arity - 1uz]))) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Diagnostic borrow; the short-circuit above protects the last-type access.
        err.err_selectable.wasm1p1_invalid_reference_type.value = target_arity == 0uz ? 0x40u : static_cast<unsigned>(target_types.begin[target_arity - 1uz]);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const prefix_arity{target_arity - static_cast<::std::size_t>(branch_non_null)};
    if(!is_polymorphic && concrete_operand_count() <= prefix_arity) [[unlikely]]
    { report_operand_stack_underflow(op_begin, op_name, prefix_arity + 1uz); }
    auto const reference{try_pop_concrete_operand()};
    if(reference.from_stack && !reference.is_unknown &&
       !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(reference.type)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Borrow only, no input-pointer read.
        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<unsigned>(reference.type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const bottom{!reference.from_stack || reference.is_unknown || reference.is_reference_bottom};
    auto const carrier{bottom ? curr_operand_stack_value_type::funcref : reference.type};
    auto const require_match{[&](auto const& actual, ::std::size_t type_index) constexpr UWVM_THROWS
    {
        // [target_types.begin, target_types.end) is the validated label signature.
        // [safe                                     ] unsafe (end)
        //          ^^ begin[type_index], proven by type_index < target_arity.
        // Equal funcref carriers do not imply equal concrete function heaps.
        if(block_value_matches(actual, target_types, target_frame.signature_type_index,
            target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
            target_frame.singleton_result_core_type, type_index)) { return; }
        auto const expected{target_types.begin[type_index]};
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Diagnostic borrow, safe after the bounded decode above.
        err.err_selectable.br_value_type_mismatch = {.op_code_name = op_name,
            .expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected),
            .actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual.type)};
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }};
    if(branch_non_null && reference.from_stack)
    {
        // Only the taken edge carries the reference, narrowed to non-null.
        auto narrowed{operand_stack_storage_t{.type = reference.type,
            .is_unknown = reference.is_unknown, .is_reference_bottom = reference.is_reference_bottom,
            .exact_function_type_index = reference.exact_function_type_index,
            .core_type = ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(reference),
            .has_core_type = true}};
        narrowed.core_type.nullable = false;
        require_match(narrowed, target_arity - 1uz);
    }
    auto const concrete_count{concrete_operand_count() < prefix_arity ? concrete_operand_count() : prefix_arity};
    for(::std::size_t i{}; i != concrete_count; ++i)
    { require_match(operand_stack.index_unchecked(operand_stack.size() - 1uz - i), prefix_arity - 1uz - i); }
    // Reify the declared label tuple on every fallthrough, preserving its exact
    // Core 3 heap witnesses. For br_on_non_null the final label operand is
    // branch-only, so remove it after constructing the full rich tuple.
    pop_available_concrete_operands(concrete_count);
    block_push_types(target_types, target_frame.signature_type_index,
        target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
        target_frame.singleton_result_core_type);
    if(branch_non_null)
    {
        // The preceding target_arity > 0 check proves this pushed operand exists.
        operand_stack_pop_unchecked();
    }
    if(!branch_non_null)
    {
        // The fallthrough value is reference-only bottom after a polymorphic pop; it never matches numbers.
        operand_stack_push(carrier);
        operand_stack.back_unchecked().is_reference_bottom = bottom;
        namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
        operand_stack.back_unchecked().core_type = bottom ?
            t3::core_value_type{t3::value_kind::reference, {t3::heap_type::bottom_code}, false} :
            ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(reference);
        operand_stack.back_unchecked().core_type.nullable = false;
        operand_stack.back_unchecked().has_core_type = true;
    }
    // References never occupy numeric register-ring slots. Check only the tag in the live stack
    // slot; repair only the taken edge. Neither operation introduces a memory access guard.
    if(!is_polymorphic && codegen_reachable)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        auto const target_label_id{get_branch_target_label_id(target_frame)};
        auto branch_label_id{target_label_id};
        auto const taken_size{operand_stack.size() + (branch_non_null ? 1uz : 0uz) - (branch_non_null ? 0uz : 1uz)};
        auto const target_base{target_frame.operand_stack_base};
        auto const need_repair{taken_size != target_base + target_arity};
        // The entry contract and ring fills may require a taken-edge thunk. The byref/no-ring
        // exact-shape case can jump directly, avoiding a redundant interpreter dispatch.
        if(need_repair || stacktop_enabled)
        {
            auto const thunk_start{thunks.size()};
            branch_label_id = new_label(true);
            set_label_offset(branch_label_id, thunk_start);
            auto const saved_curr_stacktop{curr_stacktop};
            auto const saved_memory_count{stacktop_memory_count};
            auto const saved_cache_count{stacktop_cache_count};
            auto const saved_cache_i32_count{stacktop_cache_i32_count};
            auto const saved_cache_i64_count{stacktop_cache_i64_count};
            auto const saved_cache_f32_count{stacktop_cache_f32_count};
            auto const saved_cache_f64_count{stacktop_cache_f64_count};
            auto const saved_codegen_operand_stack{codegen_operand_stack};
            if(!branch_non_null) { stacktop_after_pop_n_if_reachable(thunks, 1uz); }
            if(need_repair)
            {
                if(target_arity == 0uz)
                {
                    emit_drop_to_stack_size_no_fill(thunks, target_base);
                    if constexpr(stacktop_enabled && !strict_cf_entry_like_call) { stacktop_fill_to_canonical(thunks); }
                }
                else { emit_preserve_top_values_drop_to_base_restore(thunks, target_types, target_base, taken_size, true); }
            }
            if constexpr(stacktop_enabled && strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(thunks); }
            // If modeling this edge emitted no repair/fill, bypass the empty thunk. A live
            // loop register transform still needs its handler; all other edges jump directly.
            bool const loop_transform{stacktop_enabled && target_frame.type == block_type::loop &&
                stacktop_regtransform_cf_entry && stacktop_cache_count != 0uz};
            if(thunks.size() == thunk_start && !loop_transform) { branch_label_id = target_label_id; }
            else if(loop_transform) { emit_br_to_with_stacktop_transform(thunks, target_label_id, true); }
            else { emit_br_to(thunks, target_label_id, true); }
            if constexpr(stacktop_enabled)
            {
                if constexpr(!strict_cf_entry_like_call)
                {
                    if(target_label_id == target_frame.end_label_id)
                    {
                        target_frame.stacktop_has_end_state = true;
                        target_frame.stacktop_currpos_at_end = curr_stacktop;
                        target_frame.stacktop_memory_count_at_end = stacktop_memory_count;
                        target_frame.stacktop_cache_count_at_end = stacktop_cache_count;
                        target_frame.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                        target_frame.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                        target_frame.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                        target_frame.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                        target_frame.codegen_operand_stack_at_end = codegen_operand_stack;
                    }
                }
            }
            curr_stacktop = saved_curr_stacktop;
            stacktop_memory_count = saved_memory_count;
            stacktop_cache_count = saved_cache_count;
            stacktop_cache_i32_count = saved_cache_i32_count;
            stacktop_cache_i64_count = saved_cache_i64_count;
            stacktop_cache_f32_count = saved_cache_f32_count;
            stacktop_cache_f64_count = saved_cache_f64_count;
            codegen_operand_stack = saved_codegen_operand_stack;
        }
        auto const emit_branch{[&]<typename RefT>() constexpr UWVM_THROWS
        {
            if(branch_non_null)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_br_on_reference_typed_fptr_from_tuple<CompileOption, RefT, true>(curr_stacktop, interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_br_on_reference_typed_fptr_from_tuple<CompileOption, RefT, false>(curr_stacktop, interpreter_tuple)); }
        }};
        if(carrier == curr_operand_stack_value_type::funcref) { emit_branch.template operator()<wasm_funcref_t>(); }
        else { emit_branch.template operator()<wasm_externref_t>(); }
        emit_ptr_label_placeholder(branch_label_id, false);
        if(branch_non_null) { stacktop_after_pop_n_if_reachable(bytecode, 1uz); }
    }
    break;
}

case static_cast<wasm_byte>(0xd4u):
{
    // [ref.as_non_null] next opcode ... (code_end)
    // [safe           ] unsafe (could be code_end)
    // ^^ code_curr / op_begin; the outer dispatch proved this opcode exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.as_non_null] next opcode ... (code_end)
    // [safe           ] unsafe (could be code_end)
    //                   ^^ code_curr; no immediate belongs to this opcode.
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, 0xd4u, op_begin, err);
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    decltype(try_pop_concrete_operand()) reference{};
    auto const consume{[&]() constexpr noexcept
    {
        // Shared count > 0 proves a concrete top; copy before its actual stack retirement.
        reference = try_pop_concrete_operand();
        return v3::core3_operand{v3::core3_operand_effective_type(reference), !reference.from_stack || reference.is_unknown};
    }};
    v3::core3_operand normalized{};
    auto const pop_error{v3::pop_core3_typed_operand(is_polymorphic, concrete_operand_count, consume, normalized)};
    if(pop_error == v3::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.as_non_null", 1uz); }
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    t3::core_value_type narrowed_core{};
    if(v3::narrow_core3_non_null_reference(normalized, narrowed_core) != v3::typed_stack_error::ok) [[unlikely]]
    {
        // [checked ref.as_non_null opcode] ... | code_end
        // [safe opcode byte             ] unsafe (at code_end); no guest byte is read here.
        // ^^ err.err_curr borrows the dispatch-checked opcode.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(reference.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const reference_bottom{narrowed_core.heap.code == t3::heap_type::bottom_code};
    auto const carrier{reference_bottom ? curr_operand_stack_value_type::funcref : reference.type};
    // Reify the shared exact reference-only result. Unknown Bot cannot become numeric Bot.
    operand_stack_push(carrier);
    auto& top{operand_stack.back_unchecked()}; // The append proves this owned entry exists.
    top.is_reference_bottom = reference_bottom;
    top.exact_function_type_index = reference.exact_function_type_index;
    top.core_type = narrowed_core;
    top.has_core_type = true;
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if(codegen_reachable)
    {
        if(carrier == curr_operand_stack_value_type::funcref)
        { emit_opfunc_to(bytecode, translate::get_uwvmint_ref_as_non_null_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple)); }
        else
        { emit_opfunc_to(bytecode, translate::get_uwvmint_ref_as_non_null_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple)); }
    }
    // References occupy the operand stack, outside the numeric register ring. The live value
    // stays in place; its stack height and all numeric ring positions are unchanged.
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::ref_func):
{
    // [ref.func] function-index LEB ... code_end
    // [safe    ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved this opcode exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.func] function-index LEB ... code_end
    // [safe    ] unsafe (could be code_end)
    //            ^^ code_curr; bounded LEB decoding checks the immediate.

    if(!wasm2_feature_enabled(wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_func),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
    }

    auto const func_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"ref.func")};
    // [ref.func][checked u32 immediate] next ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                                   ^^ code_curr: sole bounded decoder committed the complete index.
    check_ref_func_index(op_begin, func_index);
    operand_stack_push(curr_operand_stack_value_type::funcref);

    // Imported and local ref.func values retain their declaration's exact heap type.
    // Search by pointer identity; no subtraction or ordering is done on unrelated storage.
    auto const ordinal{static_cast<::std::size_t>(func_index)};
    auto const declaration{ordinal < import_func_count ?
        curr_module.imported_function_vec_storage.index_unchecked(ordinal).import_type_ptr->imports.storage.function :
        curr_module.local_defined_function_vec_storage.index_unchecked(ordinal - import_func_count).function_type_ptr};
    auto const types_begin{curr_module.type_section_storage.type_section_begin};
    auto const types_end{curr_module.type_section_storage.type_section_end};
    ::std::size_t exact_type{SIZE_MAX};
    for(auto type_cursor{types_begin}; type_cursor != types_end; ++type_cursor)
    {
        // [type records ...] types_end
        // [safe            ] declaration and cursor borrow initialized module metadata.
        // ^^ type_cursor advances only while strictly before the end of this allocation.
        if(type_cursor == declaration) { exact_type = static_cast<::std::size_t>(type_cursor - types_begin); break; }
    }
    if(exact_type == SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_func_event(func_index, exact_type)};
    operand_stack.back_unchecked().exact_function_type_index = event.exact_function_type_index;
    operand_stack.back_unchecked().core_type = event.result_type;
    operand_stack.back_unchecked().has_core_type = true;

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::funcref);
    emit_opfunc_to(bytecode, translate::get_uwvmint_ref_func_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, ::std::addressof(curr_module));
    emit_imm_to(bytecode, event.function_index);
    stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::funcref);
    break;
}

case static_cast<wasm_byte>(wasm1p1_code::simd_prefix):
{
    auto const op_begin{code_curr};
    // 0xfd ...
    // [safe] unsafe (could be code_end)
    // ^^ code_curr: outer dispatch proved one byte readable.
    ++code_curr;
    // [safe] unsafe (could be code_end)
    //        ^^ code_curr: only the consumed prefix is behind the cursor.

    if(!wasm2_feature_enabled(wasm2_feature_kind::simd)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::simd_prefix),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
    }

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"simd")};
    // 0xfd subopcode ...
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr: bounded read_leb128 consumed the complete u32.
    if(::uwvm2::validation::standard::wasm3::relaxed_simd_operand_count(subopcode) != 0u && wasm1p1_para.disable_relaxed_simd)
    {
        fail_wasm1p1_feature_required(op_begin, subopcode,
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::relaxed_simd,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    auto const simd_code{static_cast<wasm1p1_simd_code>(
        ::uwvm2::validation::standard::wasm3::relaxed_simd_canonical_opcode(subopcode))};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    namespace simd_opt = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1p1_simd_details;

    static constexpr value_type_enum v128_v128_operands[2u]{value_type_enum::v128, value_type_enum::v128};
    [[maybe_unused]] static constexpr value_type_enum i32_v128_operands[2u]{value_type_enum::i32, value_type_enum::v128};
    static constexpr value_type_enum v128_i32_operands[2u]{value_type_enum::v128, value_type_enum::i32};
    static constexpr value_type_enum v128_i64_operands[2u]{value_type_enum::v128, value_type_enum::i64};
    static constexpr value_type_enum v128_f32_operands[2u]{value_type_enum::v128, value_type_enum::f32};
    static constexpr value_type_enum v128_f64_operands[2u]{value_type_enum::v128, value_type_enum::f64};
    static constexpr value_type_enum v128_v128_v128_operands[3u]{value_type_enum::v128, value_type_enum::v128, value_type_enum::v128};

    [[maybe_unused]] auto const read_memarg_immediate{[&](code_validation_error_code ec) constexpr UWVM_THROWS -> wasm_u32
                                     {
                                         using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                         wasm_u32 value{};  // init
                                         auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                          ::fast_io::mnp::leb128_get(value))};
                                         if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                             // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                             // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                                             err.err_curr = op_begin;
                                             err.err_code = ec;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                                         }

                                         // Wasm1p1 LEB immediate ... code_end
                                         // [safe consumed bytes] unsafe (could be code_end)
                                         // ^^ next: preceding bounded scan/lookahead proved a position in this code slice.
                                         // Wasm1p1 immediate ... code_end
                                         // [safe consumed bytes] unsafe (could be code_end)
                                         // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
                                         code_curr = reinterpret_cast<::std::byte const*>(next);
                                         // Wasm1p1 LEB immediate ... code_end
                                         // [safe consumed bytes] unsafe (could be code_end)
                                         //                       ^^ code_curr may be one-past; no read occurs here.
                                         return value;
                                     }};

    auto const validate_simd_memarg{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> ::std::uint64_t
                                    {
                                        // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                                        auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                                            code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para), all_memory_count, memory_address_type_at, max_align, op_name, err)};
                                        // op_name [validated memarg] ...
                                        // [safe                   ] unsafe (could be code_end)
                                        //                           ^^ code_curr
                                        current_memory_index = memarg.immediate.memory_index;
                                        current_memory_address64 = memarg.address_type == ::uwvm2::validation::standard::wasm3::storage_address_type::i64;
                                        auto const offset{memarg.immediate.offset};
                                        return offset;
                                    }};

    auto const validate_simd_load{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> ::std::uint64_t
                                  {
                                      auto const offset{validate_simd_memarg(op_name, max_align)};
                                      curr_operand_stack_value_type const expected[]{current_memory_address64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
                                      // [one owned address type] end: bounded one-element signature.
                                      pop_expected_operands(op_begin, op_name, {expected, expected + 1uz});
                                      operand_stack_push(curr_operand_stack_value_type::v128);
                                      return offset;
                                  }};

    auto const validate_simd_store{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> ::std::uint64_t
                                   {
                                       auto const offset{validate_simd_memarg(op_name, max_align)};
                                       curr_operand_stack_value_type const expected[]{current_memory_address64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32,
                                           curr_operand_stack_value_type::v128};
                                       // [address type, v128] end: exactly two owned signature entries.
                                       pop_expected_operands(op_begin, op_name, {expected, expected + 2uz});
                                       return offset;
                                   }};

    auto const emit_memory64_simd_if_selected{[&]<simd_opt::simd_code Op>(::std::uint64_t offset, wasm_byte lane = {}) constexpr UWVM_THROWS -> bool
    {
        if(!current_memory_address64) { return false; }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        ensure_memory_resolved();
        namespace wide = ::uwvm2::runtime::compiler::uwvm_int::optable::details::memory64_simd;
        if constexpr(!wide::consumes_vector<Op> && stacktop_enabled_for_vt(curr_operand_stack_value_type::v128))
        {
            // Whole-translator v128/FP and i64 rings are disjoint. Reserve a
            // vector slot before the load writes it, preserving older vectors.
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::v128);
        }
        if constexpr(!stacktop_v128_enabled)
        {
            // An uncached v128 above an i64 can force that address into operand
            // memory. Materialize the complete stack and select a matching
            // memory-only handler; the tail-call parameter ABI is unchanged.
            stacktop_flush_all_to_operand_stack(bytecode);
            constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t memory_option{
                .is_tail_call = CompileOption.is_tail_call};
            emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_simd_fptr_from_tuple<Op, memory_option>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_simd_fptr_from_tuple<Op, CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple)); }
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_imm_to(bytecode, offset);
        if constexpr(wide::lane_load<Op> || wide::lane_store<Op>) { emit_imm_to(bytecode, lane); }
        if constexpr(wide::store<Op>) { stacktop_after_pop_n_if_reachable(bytecode, 2uz); }
        else if constexpr(!stacktop_v128_enabled)
        { stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, wide::consumes_vector<Op> ? 2uz : 1uz, curr_operand_stack_value_type::v128); }
        else { stacktop_after_pop_n_push1_typed_if_reachable(bytecode, wide::consumes_vector<Op> ? 2uz : 1uz, curr_operand_stack_value_type::v128); }
        return true;
    }};

    auto const emit_v128_load{[&](::std::uint64_t full_offset) constexpr UWVM_THROWS
                              {
                                  if(emit_memory64_simd_if_selected.template operator()<simd_opt::simd_code::v128_load>(full_offset)) { return; }
                                  // Memory32 decoding established the u32 offset bound.
                                  auto const offset{static_cast<wasm_u32>(full_offset)};
                                  ensure_memory_resolved();
                                  stacktop_flush_all_to_operand_stack(bytecode);
                                  emit_opfunc_to(bytecode,
                                                 translate::get_uwvmint_simd_v128_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
                                  emit_imm_to(bytecode, resolved_memory.memory_p);
                                  emit_imm_to(bytecode, offset);
                                  stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
                              }};

    auto const emit_v128_store{[&](::std::uint64_t full_offset) constexpr UWVM_THROWS
                               {
                                   if(emit_memory64_simd_if_selected.template operator()<simd_opt::simd_code::v128_store>(full_offset)) { return; }
                                  // Memory32 decoding established the u32 offset bound.
                                  auto const offset{static_cast<wasm_u32>(full_offset)};
                                  ensure_memory_resolved();
                                   stacktop_flush_all_to_operand_stack(bytecode);
                                   emit_opfunc_to(bytecode,
                                                  translate::get_uwvmint_simd_v128_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
                                   emit_imm_to(bytecode, resolved_memory.memory_p);
                                   emit_imm_to(bytecode, offset);
                                   stacktop_after_pop_n_if_reachable(bytecode, 2uz);
                               }};

    auto const validate_v128_unary{
        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
        { validate_numeric_unary_stack_effect(op_begin, op_name, curr_operand_stack_value_type::v128, curr_operand_stack_value_type::v128); }};

    auto const validate_v128_binary{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                    {
                                        pop_expected_operands(op_begin, op_name, {v128_v128_operands, v128_v128_operands + 2u});
                                        operand_stack_push(curr_operand_stack_value_type::v128);
                                    }};

    auto const validate_v128_test{
        [&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
        { validate_numeric_unary_stack_effect(op_begin, op_name, curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32); }};

    auto const emit_v128_unary{[&]<simd_opt::v128_unop Op>() constexpr UWVM_THROWS
                               {
                                   stacktop_flush_all_to_operand_stack(bytecode);
                                   emit_opfunc_to(bytecode,
                                                  translate::get_uwvmint_simd_v128_unop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                   stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
                               }};

    auto const emit_v128_binary{[&]<simd_opt::v128_binop Op>() constexpr UWVM_THROWS
                                {
                                    stacktop_flush_all_to_operand_stack(bytecode);
                                    emit_opfunc_to(bytecode,
                                                   translate::get_uwvmint_simd_v128_binop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                    stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
                                }};

    auto const emit_v128_test{[&]<simd_opt::v128_testop Op>() constexpr UWVM_THROWS
                              {
                                  stacktop_flush_all_to_operand_stack(bytecode);
                                  emit_opfunc_to(bytecode,
                                                 translate::get_uwvmint_simd_v128_testop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                  stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
                              }};

    auto const read_lane_immediate{[&](::uwvm2::utils::container::u8string_view op_name, wasm_byte lane_count) constexpr UWVM_THROWS -> wasm_byte
                                   {
                                       auto const lane{read_u8_immediate(code_curr, code_end, op_begin, op_name)};
                                       if(lane >= lane_count) [[unlikely]] { fail_invalid_immediate(op_begin, op_name); }
                                       return lane;
                                   }};

    auto const validate_simd_splat{[&](::uwvm2::utils::container::u8string_view op_name, curr_operand_stack_value_type scalar_type) constexpr UWVM_THROWS
                                   { validate_numeric_unary_stack_effect(op_begin, op_name, scalar_type, curr_operand_stack_value_type::v128); }};

    auto const validate_v128_ternary{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                     {
                                         pop_expected_operands(op_begin, op_name, {v128_v128_v128_operands, v128_v128_v128_operands + 3u});
                                         operand_stack_push(curr_operand_stack_value_type::v128);
                                     }};

    auto const validate_v128_shift{[&](::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
                                   {
                                       pop_expected_operands(op_begin, op_name, {v128_i32_operands, v128_i32_operands + 2u});
                                       operand_stack_push(curr_operand_stack_value_type::v128);
                                   }};

    auto const emit_full_unop{[&]<simd_opt::simd_code Op>() constexpr UWVM_THROWS
                              {
                                  stacktop_flush_all_to_operand_stack(bytecode);
                                  emit_opfunc_to(bytecode,
                                                 translate::get_uwvmint_simd_full_unop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                  stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
                              }};

    auto const emit_full_binop{[&]<simd_opt::simd_code Op>() constexpr UWVM_THROWS
                               {
                                   stacktop_flush_all_to_operand_stack(bytecode);
                                   emit_opfunc_to(bytecode,
                                                  translate::get_uwvmint_simd_full_binop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                   stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
                               }};

    auto const emit_full_ternop{[&]<simd_opt::simd_code Op>() constexpr UWVM_THROWS
    {
        stacktop_flush_all_to_operand_stack(bytecode);
        emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_ternop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
        stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 3uz, curr_operand_stack_value_type::v128);
    }};

    auto const emit_full_bitselect{
        [&]() constexpr UWVM_THROWS
        {
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_bitselect_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 3uz, curr_operand_stack_value_type::v128);
        }};

    auto const emit_full_test{[&]<simd_opt::simd_code Op>() constexpr UWVM_THROWS
                              {
                                  stacktop_flush_all_to_operand_stack(bytecode);
                                  emit_opfunc_to(bytecode,
                                                 translate::get_uwvmint_simd_full_testop_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                  stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
                              }};

    auto const emit_full_shift{[&]<simd_opt::simd_code Op>() constexpr UWVM_THROWS
                               {
                                   stacktop_flush_all_to_operand_stack(bytecode);
                                   emit_opfunc_to(bytecode,
                                                  translate::get_uwvmint_simd_full_shift_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
                                   stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
                               }};

    auto const emit_full_splat{
        [&]<simd_opt::simd_code Op, typename ScalarT>() constexpr UWVM_THROWS
        {
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_splat_fptr_from_tuple<CompileOption, Op, ScalarT>(curr_stacktop, interpreter_tuple));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
        }};

    auto const emit_full_extract_lane{
        [&]<simd_opt::simd_code Op, typename ScalarT>(wasm_byte lane) constexpr UWVM_THROWS
        {
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_simd_full_extract_lane_fptr_from_tuple<CompileOption, Op, ScalarT>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, lane);
            curr_operand_stack_value_type out_type;
            if constexpr(::std::same_as<ScalarT, wasm_i32>) { out_type = curr_operand_stack_value_type::i32; }
            else if constexpr(::std::same_as<ScalarT, wasm_i64>) { out_type = curr_operand_stack_value_type::i64; }
            else if constexpr(::std::same_as<ScalarT, wasm_f32>) { out_type = curr_operand_stack_value_type::f32; }
            else
            {
                out_type = curr_operand_stack_value_type::f64;
            }
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, out_type);
        }};

    auto const emit_full_replace_lane{
        [&]<simd_opt::simd_code Op, typename ScalarT>(wasm_byte lane) constexpr UWVM_THROWS
        {
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_simd_full_replace_lane_fptr_from_tuple<CompileOption, Op, ScalarT>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, lane);
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
        }};

    auto const emit_full_shuffle{[&](wasm_byte const(&lanes)[16]) constexpr UWVM_THROWS
                                 {
                                     stacktop_flush_all_to_operand_stack(bytecode);
                                     emit_opfunc_to(bytecode,
                                                    translate::get_uwvmint_simd_full_shuffle_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                     auto const controls{simd_opt::make_shuffle_controls(lanes)};
                                     for(auto lane: controls.lhs) { emit_imm_to(bytecode, lane); }
                                     for(auto lane: controls.rhs) { emit_imm_to(bytecode, lane); }
                                     stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
                                 }};

    auto const emit_full_mem_load{
        [&]<simd_opt::simd_code Op>(::std::uint64_t full_offset, wasm_byte lane = wasm_byte{}) constexpr UWVM_THROWS
        {
            if(emit_memory64_simd_if_selected.template operator()<Op>(full_offset, lane)) { return; }
            // Only the memory32 path narrows a validated offset to its original immediate ABI.
            auto const offset{static_cast<wasm_u32>(full_offset)};
            ensure_memory_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_mem_load_fptr_from_tuple<CompileOption, Op>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            if constexpr(Op == simd_opt::simd_code::v128_load8_lane || Op == simd_opt::simd_code::v128_load16_lane ||
                         Op == simd_opt::simd_code::v128_load32_lane || Op == simd_opt::simd_code::v128_load64_lane)
            {
                emit_imm_to(bytecode, lane);
                stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::v128);
            }
            else
            {
                stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
            }
        }};

    auto const emit_full_mem_store{
        [&]<simd_opt::simd_code Op>(::std::uint64_t full_offset, wasm_byte lane = wasm_byte{}) constexpr UWVM_THROWS
        {
            if(emit_memory64_simd_if_selected.template operator()<Op>(full_offset, lane)) { return; }
            // Only the memory32 path narrows a validated offset to its original immediate ABI.
            auto const offset{static_cast<wasm_u32>(full_offset)};
            ensure_memory_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_mem_store_fptr_from_tuple<CompileOption, Op>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            if constexpr(Op != simd_opt::simd_code::v128_store) { emit_imm_to(bytecode, lane); }
            stacktop_after_pop_n_if_reachable(bytecode, 2uz);
        }};

    switch(simd_code)
    {
        case wasm1p1_simd_code::v128_load:
        {
            auto const offset{validate_simd_load(u8"v128.load", 4u)};
            emit_v128_load(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load8x8_s:
        {
            auto const offset{validate_simd_load(u8"v128.load8x8_s", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load8x8_s>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load8x8_u:
        {
            auto const offset{validate_simd_load(u8"v128.load8x8_u", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load8x8_u>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load16x4_s:
        {
            auto const offset{validate_simd_load(u8"v128.load16x4_s", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load16x4_s>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load16x4_u:
        {
            auto const offset{validate_simd_load(u8"v128.load16x4_u", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load16x4_u>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load32x2_s:
        {
            auto const offset{validate_simd_load(u8"v128.load32x2_s", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load32x2_s>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load32x2_u:
        {
            auto const offset{validate_simd_load(u8"v128.load32x2_u", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load32x2_u>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load8_splat:
        {
            auto const offset{validate_simd_load(u8"v128.load8_splat", 0u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load8_splat>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load16_splat:
        {
            auto const offset{validate_simd_load(u8"v128.load16_splat", 1u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load16_splat>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load32_splat:
        {
            auto const offset{validate_simd_load(u8"v128.load32_splat", 2u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load32_splat>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load64_splat:
        {
            auto const offset{validate_simd_load(u8"v128.load64_splat", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load64_splat>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_store:
        {
            auto const offset{validate_simd_store(u8"v128.store", 4u)};
            emit_v128_store(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load8_lane:
        {
            auto const offset{validate_simd_store(u8"v128.load8_lane", 0u)};
            operand_stack_push(curr_operand_stack_value_type::v128);
            auto const lane{read_lane_immediate(u8"v128.load8_lane", 16u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load8_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_load16_lane:
        {
            auto const offset{validate_simd_store(u8"v128.load16_lane", 1u)};
            operand_stack_push(curr_operand_stack_value_type::v128);
            auto const lane{read_lane_immediate(u8"v128.load16_lane", 8u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load16_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_load32_lane:
        {
            auto const offset{validate_simd_store(u8"v128.load32_lane", 2u)};
            operand_stack_push(curr_operand_stack_value_type::v128);
            auto const lane{read_lane_immediate(u8"v128.load32_lane", 4u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load32_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_load64_lane:
        {
            auto const offset{validate_simd_store(u8"v128.load64_lane", 3u)};
            operand_stack_push(curr_operand_stack_value_type::v128);
            auto const lane{read_lane_immediate(u8"v128.load64_lane", 2u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load64_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_store8_lane:
        {
            auto const offset{validate_simd_store(u8"v128.store8_lane", 0u)};
            auto const lane{read_lane_immediate(u8"v128.store8_lane", 16u)};
            emit_full_mem_store.template operator()<simd_opt::simd_code::v128_store8_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_store16_lane:
        {
            auto const offset{validate_simd_store(u8"v128.store16_lane", 1u)};
            auto const lane{read_lane_immediate(u8"v128.store16_lane", 8u)};
            emit_full_mem_store.template operator()<simd_opt::simd_code::v128_store16_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_store32_lane:
        {
            auto const offset{validate_simd_store(u8"v128.store32_lane", 2u)};
            auto const lane{read_lane_immediate(u8"v128.store32_lane", 4u)};
            emit_full_mem_store.template operator()<simd_opt::simd_code::v128_store32_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_store64_lane:
        {
            auto const offset{validate_simd_store(u8"v128.store64_lane", 3u)};
            auto const lane{read_lane_immediate(u8"v128.store64_lane", 2u)};
            emit_full_mem_store.template operator()<simd_opt::simd_code::v128_store64_lane>(offset, lane);
            break;
        }
        case wasm1p1_simd_code::v128_load32_zero:
        {
            auto const offset{validate_simd_load(u8"v128.load32_zero", 2u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load32_zero>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_load64_zero:
        {
            auto const offset{validate_simd_load(u8"v128.load64_zero", 3u)};
            emit_full_mem_load.template operator()<simd_opt::simd_code::v128_load64_zero>(offset);
            break;
        }
        case wasm1p1_simd_code::v128_const:
        {
            wasm_v128_t imm{};
            if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(imm)) [[unlikely]] { fail_invalid_immediate(op_begin, u8"v128.const"); }
            ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));
            // [v128.const immediate (16 bytes)] next opcode ... code_end
            // [safe                            ] unsafe (could be code_end)
            // ^^ code_curr; the preceding remaining-length check proves the complete immediate.
            code_curr += sizeof(imm);
            // [v128.const immediate (16 bytes)] next opcode ... code_end
            // [safe                            ] unsafe (could be code_end)
            //                                  ^^ code_curr; it may equal code_end.

            operand_stack_push(curr_operand_stack_value_type::v128);
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::v128);
            emit_opfunc_to(bytecode, translate::get_uwvmint_v128_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, imm);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::v128);
            break;
        }
        case wasm1p1_simd_code::i8x16_shuffle:
        {
            wasm_byte lanes[16]{};  // init
            for(::std::size_t i{}; i != 16uz; ++i) { lanes[i] = read_lane_immediate(u8"i8x16.shuffle", 32u); }
            validate_v128_binary(u8"i8x16.shuffle");
            emit_full_shuffle(lanes);
            break;
        }
        case wasm1p1_simd_code::i8x16_swizzle:
        {
            validate_v128_binary(u8"i8x16.swizzle");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_swizzle>();
            break;
        }
        case wasm1p1_simd_code::i8x16_splat:
        {
            validate_simd_splat(u8"i8x16.splat", curr_operand_stack_value_type::i32);
            emit_full_splat.template operator()<simd_opt::simd_code::i8x16_splat, wasm_i32>();
            break;
        }
        case wasm1p1_simd_code::i16x8_splat:
        {
            validate_simd_splat(u8"i16x8.splat", curr_operand_stack_value_type::i32);
            emit_full_splat.template operator()<simd_opt::simd_code::i16x8_splat, wasm_i32>();
            break;
        }
        case wasm1p1_simd_code::i32x4_splat:
        {
            validate_numeric_unary_stack_effect(op_begin, u8"i32x4.splat", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::v128);
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_simd_i32x4_splat_fptr_from_tuple<CompileOption, simd_opt::v128_splatop::i32x4>(curr_stacktop, interpreter_tuple));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
            break;
        }
        case wasm1p1_simd_code::i64x2_splat:
        {
            validate_simd_splat(u8"i64x2.splat", curr_operand_stack_value_type::i64);
            emit_full_splat.template operator()<simd_opt::simd_code::i64x2_splat, wasm_i64>();
            break;
        }
        case wasm1p1_simd_code::f32x4_splat:
        {
            validate_numeric_unary_stack_effect(op_begin, u8"f32x4.splat", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::v128);
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_simd_f32x4_splat_fptr_from_tuple<CompileOption, simd_opt::v128_splatop::f32x4>(curr_stacktop, interpreter_tuple));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
            break;
        }
        case wasm1p1_simd_code::f64x2_splat:
        {
            validate_simd_splat(u8"f64x2.splat", curr_operand_stack_value_type::f64);
            emit_full_splat.template operator()<simd_opt::simd_code::f64x2_splat, wasm_f64>();
            break;
        }
        case wasm1p1_simd_code::i8x16_extract_lane_s:
        {
            auto const lane{read_lane_immediate(u8"i8x16.extract_lane_s", 16u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i8x16.extract_lane_s", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i8x16_extract_lane_s, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i8x16_extract_lane_u:
        {
            auto const lane{read_lane_immediate(u8"i8x16.extract_lane_u", 16u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i8x16.extract_lane_u", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i8x16_extract_lane_u, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i8x16_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"i8x16.replace_lane", 16u)};
            pop_expected_operands(op_begin, u8"i8x16.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::i8x16_replace_lane, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i16x8_extract_lane_s:
        {
            auto const lane{read_lane_immediate(u8"i16x8.extract_lane_s", 8u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i16x8.extract_lane_s", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i16x8_extract_lane_s, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i16x8_extract_lane_u:
        {
            auto const lane{read_lane_immediate(u8"i16x8.extract_lane_u", 8u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i16x8.extract_lane_u", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i16x8_extract_lane_u, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i16x8_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"i16x8.replace_lane", 8u)};
            pop_expected_operands(op_begin, u8"i16x8.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::i16x8_replace_lane, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i32x4_extract_lane:
        {
            auto const lane{read_lane_immediate(u8"i32x4.extract_lane", 4u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i32x4.extract_lane", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i32);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i32x4_extract_lane, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i32x4_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"i32x4.replace_lane", 4u)};
            pop_expected_operands(op_begin, u8"i32x4.replace_lane", {v128_i32_operands, v128_i32_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::i32x4_replace_lane, wasm_i32>(lane);
            break;
        }
        case wasm1p1_simd_code::i64x2_extract_lane:
        {
            auto const lane{read_lane_immediate(u8"i64x2.extract_lane", 2u)};
            validate_numeric_unary_stack_effect(op_begin, u8"i64x2.extract_lane", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::i64);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::i64x2_extract_lane, wasm_i64>(lane);
            break;
        }
        case wasm1p1_simd_code::i64x2_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"i64x2.replace_lane", 2u)};
            pop_expected_operands(op_begin, u8"i64x2.replace_lane", {v128_i64_operands, v128_i64_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::i64x2_replace_lane, wasm_i64>(lane);
            break;
        }
        case wasm1p1_simd_code::f32x4_extract_lane:
        {
            auto const lane{read_u8_immediate(code_curr, code_end, op_begin, u8"f32x4.extract_lane")};
            if(lane >= 4u) [[unlikely]] { fail_invalid_immediate(op_begin, u8"f32x4.extract_lane"); }
            validate_numeric_unary_stack_effect(op_begin, u8"f32x4.extract_lane", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::f32);
            stacktop_flush_all_to_operand_stack(bytecode);
            switch(lane)
            {
                case 0u:
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_simd_f32x4_extract_lane_fptr_from_tuple<CompileOption, 0uz>(curr_stacktop, interpreter_tuple));
                    break;
                case 1u:
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_simd_f32x4_extract_lane_fptr_from_tuple<CompileOption, 1uz>(curr_stacktop, interpreter_tuple));
                    break;
                case 2u:
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_simd_f32x4_extract_lane_fptr_from_tuple<CompileOption, 2uz>(curr_stacktop, interpreter_tuple));
                    break;
                case 3u:
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_simd_f32x4_extract_lane_fptr_from_tuple<CompileOption, 3uz>(curr_stacktop, interpreter_tuple));
                    break;
                [[unlikely]] default:
                    ::fast_io::fast_terminate();
            }
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
            break;
        }
        case wasm1p1_simd_code::f32x4_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"f32x4.replace_lane", 4u)};
            pop_expected_operands(op_begin, u8"f32x4.replace_lane", {v128_f32_operands, v128_f32_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::f32x4_replace_lane, wasm_f32>(lane);
            break;
        }
        case wasm1p1_simd_code::f64x2_extract_lane:
        {
            auto const lane{read_lane_immediate(u8"f64x2.extract_lane", 2u)};
            validate_numeric_unary_stack_effect(op_begin, u8"f64x2.extract_lane", curr_operand_stack_value_type::v128, curr_operand_stack_value_type::f64);
            emit_full_extract_lane.template operator()<simd_opt::simd_code::f64x2_extract_lane, wasm_f64>(lane);
            break;
        }
        case wasm1p1_simd_code::f64x2_replace_lane:
        {
            auto const lane{read_lane_immediate(u8"f64x2.replace_lane", 2u)};
            pop_expected_operands(op_begin, u8"f64x2.replace_lane", {v128_f64_operands, v128_f64_operands + 2u});
            operand_stack_push(curr_operand_stack_value_type::v128);
            emit_full_replace_lane.template operator()<simd_opt::simd_code::f64x2_replace_lane, wasm_f64>(lane);
            break;
        }
        case wasm1p1_simd_code::v128_not:
        {
            validate_v128_unary(u8"v128.not");
            emit_v128_unary.template operator()<simd_opt::v128_unop::not_>();
            break;
        }
        case wasm1p1_simd_code::v128_and:
        {
            validate_v128_binary(u8"v128.and");
            emit_v128_binary.template operator()<simd_opt::v128_binop::and_>();
            break;
        }
        case wasm1p1_simd_code::v128_andnot:
        {
            validate_v128_binary(u8"v128.andnot");
            emit_v128_binary.template operator()<simd_opt::v128_binop::andnot>();
            break;
        }
        case wasm1p1_simd_code::v128_or:
        {
            validate_v128_binary(u8"v128.or");
            emit_v128_binary.template operator()<simd_opt::v128_binop::or_>();
            break;
        }
        case wasm1p1_simd_code::v128_xor:
        {
            validate_v128_binary(u8"v128.xor");
            emit_v128_binary.template operator()<simd_opt::v128_binop::xor_>();
            break;
        }
        case wasm1p1_simd_code::v128_any_true:
        {
            validate_v128_test(u8"v128.any_true");
            emit_v128_test.template operator()<simd_opt::v128_testop::any_true>();
            break;
        }
        case wasm1p1_simd_code::f32x4_relaxed_madd:
        {
            validate_v128_ternary(u8"f32x4.relaxed_madd");
            emit_full_ternop.template operator()<simd_opt::simd_code::f32x4_relaxed_madd>();
            break;
        }
        case wasm1p1_simd_code::f32x4_relaxed_nmadd:
        {
            validate_v128_ternary(u8"f32x4.relaxed_nmadd");
            emit_full_ternop.template operator()<simd_opt::simd_code::f32x4_relaxed_nmadd>();
            break;
        }
        case wasm1p1_simd_code::f64x2_relaxed_madd:
        {
            validate_v128_ternary(u8"f64x2.relaxed_madd");
            emit_full_ternop.template operator()<simd_opt::simd_code::f64x2_relaxed_madd>();
            break;
        }
        case wasm1p1_simd_code::f64x2_relaxed_nmadd:
        {
            validate_v128_ternary(u8"f64x2.relaxed_nmadd");
            emit_full_ternop.template operator()<simd_opt::simd_code::f64x2_relaxed_nmadd>();
            break;
        }
        case wasm1p1_simd_code::i16x8_relaxed_dot_i8x16_i7x16_s:
        {
            validate_v128_binary(u8"i16x8.relaxed_dot_i8x16_i7x16_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_relaxed_dot_i8x16_i7x16_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_relaxed_dot_i8x16_i7x16_add_s:
        {
            validate_v128_ternary(u8"i32x4.relaxed_dot_i8x16_i7x16_add_s");
            emit_full_ternop.template operator()<simd_opt::simd_code::i32x4_relaxed_dot_i8x16_i7x16_add_s>();
            break;
        }
        case wasm1p1_simd_code::v128_bitselect:
        {
            validate_v128_ternary(u8"v128.bitselect");
            emit_full_bitselect();
            break;
        }
        case wasm1p1_simd_code::i8x16_eq:
        {
            validate_v128_binary(u8"i8x16.eq");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_eq>();
            break;
        }
        case wasm1p1_simd_code::i8x16_ne:
        {
            validate_v128_binary(u8"i8x16.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_ne>();
            break;
        }
        case wasm1p1_simd_code::i8x16_lt_s:
        {
            validate_v128_binary(u8"i8x16.lt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_lt_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_lt_u:
        {
            validate_v128_binary(u8"i8x16.lt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_lt_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_gt_s:
        {
            validate_v128_binary(u8"i8x16.gt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_gt_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_gt_u:
        {
            validate_v128_binary(u8"i8x16.gt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_gt_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_le_s:
        {
            validate_v128_binary(u8"i8x16.le_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_le_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_le_u:
        {
            validate_v128_binary(u8"i8x16.le_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_le_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_ge_s:
        {
            validate_v128_binary(u8"i8x16.ge_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_ge_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_ge_u:
        {
            validate_v128_binary(u8"i8x16.ge_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_ge_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_eq:
        {
            validate_v128_binary(u8"i16x8.eq");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_eq>();
            break;
        }
        case wasm1p1_simd_code::i16x8_ne:
        {
            validate_v128_binary(u8"i16x8.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_ne>();
            break;
        }
        case wasm1p1_simd_code::i16x8_lt_s:
        {
            validate_v128_binary(u8"i16x8.lt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_lt_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_lt_u:
        {
            validate_v128_binary(u8"i16x8.lt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_lt_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_gt_s:
        {
            validate_v128_binary(u8"i16x8.gt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_gt_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_gt_u:
        {
            validate_v128_binary(u8"i16x8.gt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_gt_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_le_s:
        {
            validate_v128_binary(u8"i16x8.le_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_le_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_le_u:
        {
            validate_v128_binary(u8"i16x8.le_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_le_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_ge_s:
        {
            validate_v128_binary(u8"i16x8.ge_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_ge_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_ge_u:
        {
            validate_v128_binary(u8"i16x8.ge_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_ge_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_eq:
        {
            validate_v128_binary(u8"i32x4.eq");
            emit_v128_binary.template operator()<simd_opt::v128_binop::i32x4_eq>();
            break;
        }
        case wasm1p1_simd_code::i32x4_ne:
        {
            validate_v128_binary(u8"i32x4.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_ne>();
            break;
        }
        case wasm1p1_simd_code::i32x4_lt_s:
        {
            validate_v128_binary(u8"i32x4.lt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_lt_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_lt_u:
        {
            validate_v128_binary(u8"i32x4.lt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_lt_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_gt_s:
        {
            validate_v128_binary(u8"i32x4.gt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_gt_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_gt_u:
        {
            validate_v128_binary(u8"i32x4.gt_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_gt_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_le_s:
        {
            validate_v128_binary(u8"i32x4.le_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_le_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_le_u:
        {
            validate_v128_binary(u8"i32x4.le_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_le_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_ge_s:
        {
            validate_v128_binary(u8"i32x4.ge_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_ge_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_ge_u:
        {
            validate_v128_binary(u8"i32x4.ge_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_ge_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_all_true:
        {
            validate_v128_test(u8"i32x4.all_true");
            emit_v128_test.template operator()<simd_opt::v128_testop::i32x4_all_true>();
            break;
        }
        case wasm1p1_simd_code::i8x16_all_true:
        {
            validate_v128_test(u8"i8x16.all_true");
            emit_full_test.template operator()<simd_opt::simd_code::i8x16_all_true>();
            break;
        }
        case wasm1p1_simd_code::i8x16_bitmask:
        {
            validate_v128_test(u8"i8x16.bitmask");
            emit_full_test.template operator()<simd_opt::simd_code::i8x16_bitmask>();
            break;
        }
        case wasm1p1_simd_code::i16x8_all_true:
        {
            validate_v128_test(u8"i16x8.all_true");
            emit_full_test.template operator()<simd_opt::simd_code::i16x8_all_true>();
            break;
        }
        case wasm1p1_simd_code::i16x8_bitmask:
        {
            validate_v128_test(u8"i16x8.bitmask");
            emit_full_test.template operator()<simd_opt::simd_code::i16x8_bitmask>();
            break;
        }
        case wasm1p1_simd_code::i32x4_bitmask:
        {
            validate_v128_test(u8"i32x4.bitmask");
            emit_full_test.template operator()<simd_opt::simd_code::i32x4_bitmask>();
            break;
        }
        case wasm1p1_simd_code::i64x2_all_true:
        {
            validate_v128_test(u8"i64x2.all_true");
            emit_full_test.template operator()<simd_opt::simd_code::i64x2_all_true>();
            break;
        }
        case wasm1p1_simd_code::i64x2_bitmask:
        {
            validate_v128_test(u8"i64x2.bitmask");
            emit_full_test.template operator()<simd_opt::simd_code::i64x2_bitmask>();
            break;
        }
        case wasm1p1_simd_code::i8x16_abs:
        {
            validate_v128_unary(u8"i8x16.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::i8x16_abs>();
            break;
        }
        case wasm1p1_simd_code::i8x16_neg:
        {
            validate_v128_unary(u8"i8x16.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::i8x16_neg>();
            break;
        }
        case wasm1p1_simd_code::i8x16_popcnt:
        {
            validate_v128_unary(u8"i8x16.popcnt");
            emit_full_unop.template operator()<simd_opt::simd_code::i8x16_popcnt>();
            break;
        }
        case wasm1p1_simd_code::i16x8_abs:
        {
            validate_v128_unary(u8"i16x8.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_abs>();
            break;
        }
        case wasm1p1_simd_code::i16x8_neg:
        {
            validate_v128_unary(u8"i16x8.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_neg>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extend_low_i8x16_s:
        {
            validate_v128_unary(u8"i16x8.extend_low_i8x16_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extend_low_i8x16_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extend_high_i8x16_s:
        {
            validate_v128_unary(u8"i16x8.extend_high_i8x16_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extend_high_i8x16_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extend_low_i8x16_u:
        {
            validate_v128_unary(u8"i16x8.extend_low_i8x16_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extend_low_i8x16_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extend_high_i8x16_u:
        {
            validate_v128_unary(u8"i16x8.extend_high_i8x16_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extend_high_i8x16_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_s:
        {
            validate_v128_unary(u8"i16x8.extadd_pairwise_i8x16_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extadd_pairwise_i8x16_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extadd_pairwise_i8x16_u:
        {
            validate_v128_unary(u8"i16x8.extadd_pairwise_i8x16_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i16x8_extadd_pairwise_i8x16_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_abs:
        {
            validate_v128_unary(u8"i32x4.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_abs>();
            break;
        }
        case wasm1p1_simd_code::i32x4_neg:
        {
            validate_v128_unary(u8"i32x4.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_neg>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extend_low_i16x8_s:
        {
            validate_v128_unary(u8"i32x4.extend_low_i16x8_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extend_low_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extend_high_i16x8_s:
        {
            validate_v128_unary(u8"i32x4.extend_high_i16x8_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extend_high_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extend_low_i16x8_u:
        {
            validate_v128_unary(u8"i32x4.extend_low_i16x8_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extend_low_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extend_high_i16x8_u:
        {
            validate_v128_unary(u8"i32x4.extend_high_i16x8_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extend_high_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_s:
        {
            validate_v128_unary(u8"i32x4.extadd_pairwise_i16x8_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extadd_pairwise_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extadd_pairwise_i16x8_u:
        {
            validate_v128_unary(u8"i32x4.extadd_pairwise_i16x8_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_extadd_pairwise_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i64x2_abs:
        {
            validate_v128_unary(u8"i64x2.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_abs>();
            break;
        }
        case wasm1p1_simd_code::i64x2_neg:
        {
            validate_v128_unary(u8"i64x2.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_neg>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extend_low_i32x4_s:
        {
            validate_v128_unary(u8"i64x2.extend_low_i32x4_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_extend_low_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extend_high_i32x4_s:
        {
            validate_v128_unary(u8"i64x2.extend_high_i32x4_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_extend_high_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extend_low_i32x4_u:
        {
            validate_v128_unary(u8"i64x2.extend_low_i32x4_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_extend_low_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extend_high_i32x4_u:
        {
            validate_v128_unary(u8"i64x2.extend_high_i32x4_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i64x2_extend_high_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_narrow_i16x8_s:
        {
            validate_v128_binary(u8"i8x16.narrow_i16x8_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_narrow_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_narrow_i16x8_u:
        {
            validate_v128_binary(u8"i8x16.narrow_i16x8_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_narrow_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_add:
        {
            validate_v128_binary(u8"i8x16.add");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_add>();
            break;
        }
        case wasm1p1_simd_code::i8x16_add_sat_s:
        {
            validate_v128_binary(u8"i8x16.add_sat_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_add_sat_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_add_sat_u:
        {
            validate_v128_binary(u8"i8x16.add_sat_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_add_sat_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_sub:
        {
            validate_v128_binary(u8"i8x16.sub");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_sub>();
            break;
        }
        case wasm1p1_simd_code::i8x16_sub_sat_s:
        {
            validate_v128_binary(u8"i8x16.sub_sat_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_sub_sat_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_sub_sat_u:
        {
            validate_v128_binary(u8"i8x16.sub_sat_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_sub_sat_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_min_s:
        {
            validate_v128_binary(u8"i8x16.min_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_min_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_min_u:
        {
            validate_v128_binary(u8"i8x16.min_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_min_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_max_s:
        {
            validate_v128_binary(u8"i8x16.max_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_max_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_max_u:
        {
            validate_v128_binary(u8"i8x16.max_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_max_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_avgr_u:
        {
            validate_v128_binary(u8"i8x16.avgr_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i8x16_avgr_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_q15mulr_sat_s:
        {
            validate_v128_binary(u8"i16x8.q15mulr_sat_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_q15mulr_sat_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_narrow_i32x4_s:
        {
            validate_v128_binary(u8"i16x8.narrow_i32x4_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_narrow_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_narrow_i32x4_u:
        {
            validate_v128_binary(u8"i16x8.narrow_i32x4_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_narrow_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_add:
        {
            validate_v128_binary(u8"i16x8.add");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_add>();
            break;
        }
        case wasm1p1_simd_code::i16x8_add_sat_s:
        {
            validate_v128_binary(u8"i16x8.add_sat_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_add_sat_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_add_sat_u:
        {
            validate_v128_binary(u8"i16x8.add_sat_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_add_sat_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_sub:
        {
            validate_v128_binary(u8"i16x8.sub");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_sub>();
            break;
        }
        case wasm1p1_simd_code::i16x8_sub_sat_s:
        {
            validate_v128_binary(u8"i16x8.sub_sat_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_sub_sat_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_sub_sat_u:
        {
            validate_v128_binary(u8"i16x8.sub_sat_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_sub_sat_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_mul:
        {
            validate_v128_binary(u8"i16x8.mul");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_mul>();
            break;
        }
        case wasm1p1_simd_code::i16x8_min_s:
        {
            validate_v128_binary(u8"i16x8.min_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_min_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_min_u:
        {
            validate_v128_binary(u8"i16x8.min_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_min_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_max_s:
        {
            validate_v128_binary(u8"i16x8.max_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_max_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_max_u:
        {
            validate_v128_binary(u8"i16x8.max_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_max_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_avgr_u:
        {
            validate_v128_binary(u8"i16x8.avgr_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_avgr_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extmul_low_i8x16_s:
        {
            validate_v128_binary(u8"i16x8.extmul_low_i8x16_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_extmul_low_i8x16_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extmul_high_i8x16_s:
        {
            validate_v128_binary(u8"i16x8.extmul_high_i8x16_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_extmul_high_i8x16_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extmul_low_i8x16_u:
        {
            validate_v128_binary(u8"i16x8.extmul_low_i8x16_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_extmul_low_i8x16_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_extmul_high_i8x16_u:
        {
            validate_v128_binary(u8"i16x8.extmul_high_i8x16_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i16x8_extmul_high_i8x16_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_add:
        {
            validate_v128_binary(u8"i32x4.add");
            emit_v128_binary.template operator()<simd_opt::v128_binop::i32x4_add>();
            break;
        }
        case wasm1p1_simd_code::i32x4_sub:
        {
            validate_v128_binary(u8"i32x4.sub");
            emit_v128_binary.template operator()<simd_opt::v128_binop::i32x4_sub>();
            break;
        }
        case wasm1p1_simd_code::i32x4_mul:
        {
            validate_v128_binary(u8"i32x4.mul");
            emit_v128_binary.template operator()<simd_opt::v128_binop::i32x4_mul>();
            break;
        }
        case wasm1p1_simd_code::i32x4_min_s:
        {
            validate_v128_binary(u8"i32x4.min_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_min_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_min_u:
        {
            validate_v128_binary(u8"i32x4.min_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_min_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_max_s:
        {
            validate_v128_binary(u8"i32x4.max_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_max_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_max_u:
        {
            validate_v128_binary(u8"i32x4.max_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_max_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_dot_i16x8_s:
        {
            validate_v128_binary(u8"i32x4.dot_i16x8_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_dot_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extmul_low_i16x8_s:
        {
            validate_v128_binary(u8"i32x4.extmul_low_i16x8_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_extmul_low_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extmul_high_i16x8_s:
        {
            validate_v128_binary(u8"i32x4.extmul_high_i16x8_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_extmul_high_i16x8_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extmul_low_i16x8_u:
        {
            validate_v128_binary(u8"i32x4.extmul_low_i16x8_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_extmul_low_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_extmul_high_i16x8_u:
        {
            validate_v128_binary(u8"i32x4.extmul_high_i16x8_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i32x4_extmul_high_i16x8_u>();
            break;
        }
        case wasm1p1_simd_code::i64x2_add:
        {
            validate_v128_binary(u8"i64x2.add");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_add>();
            break;
        }
        case wasm1p1_simd_code::i64x2_sub:
        {
            validate_v128_binary(u8"i64x2.sub");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_sub>();
            break;
        }
        case wasm1p1_simd_code::i64x2_mul:
        {
            validate_v128_binary(u8"i64x2.mul");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_mul>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extmul_low_i32x4_s:
        {
            validate_v128_binary(u8"i64x2.extmul_low_i32x4_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_extmul_low_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extmul_high_i32x4_s:
        {
            validate_v128_binary(u8"i64x2.extmul_high_i32x4_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_extmul_high_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extmul_low_i32x4_u:
        {
            validate_v128_binary(u8"i64x2.extmul_low_i32x4_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_extmul_low_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i64x2_extmul_high_i32x4_u:
        {
            validate_v128_binary(u8"i64x2.extmul_high_i32x4_u");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_extmul_high_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::f32x4_ceil:
        {
            validate_v128_unary(u8"f32x4.ceil");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_ceil>();
            break;
        }
        case wasm1p1_simd_code::f32x4_floor:
        {
            validate_v128_unary(u8"f32x4.floor");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_floor>();
            break;
        }
        case wasm1p1_simd_code::f32x4_trunc:
        {
            validate_v128_unary(u8"f32x4.trunc");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_trunc>();
            break;
        }
        case wasm1p1_simd_code::f32x4_nearest:
        {
            validate_v128_unary(u8"f32x4.nearest");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_nearest>();
            break;
        }
        case wasm1p1_simd_code::f64x2_ceil:
        {
            validate_v128_unary(u8"f64x2.ceil");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_ceil>();
            break;
        }
        case wasm1p1_simd_code::f64x2_floor:
        {
            validate_v128_unary(u8"f64x2.floor");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_floor>();
            break;
        }
        case wasm1p1_simd_code::f64x2_trunc:
        {
            validate_v128_unary(u8"f64x2.trunc");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_trunc>();
            break;
        }
        case wasm1p1_simd_code::f64x2_nearest:
        {
            validate_v128_unary(u8"f64x2.nearest");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_nearest>();
            break;
        }
        case wasm1p1_simd_code::f32x4_abs:
        {
            validate_v128_unary(u8"f32x4.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_abs>();
            break;
        }
        case wasm1p1_simd_code::f32x4_neg:
        {
            validate_v128_unary(u8"f32x4.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_neg>();
            break;
        }
        case wasm1p1_simd_code::f32x4_sqrt:
        {
            validate_v128_unary(u8"f32x4.sqrt");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_sqrt>();
            break;
        }
        case wasm1p1_simd_code::f64x2_abs:
        {
            validate_v128_unary(u8"f64x2.abs");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_abs>();
            break;
        }
        case wasm1p1_simd_code::f64x2_neg:
        {
            validate_v128_unary(u8"f64x2.neg");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_neg>();
            break;
        }
        case wasm1p1_simd_code::f64x2_sqrt:
        {
            validate_v128_unary(u8"f64x2.sqrt");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_sqrt>();
            break;
        }
        case wasm1p1_simd_code::f32x4_demote_f64x2_zero:
        {
            validate_v128_unary(u8"f32x4.demote_f64x2_zero");
            emit_full_unop.template operator()<simd_opt::simd_code::f32x4_demote_f64x2_zero>();
            break;
        }
        case wasm1p1_simd_code::f64x2_promote_low_f32x4:
        {
            validate_v128_unary(u8"f64x2.promote_low_f32x4");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_promote_low_f32x4>();
            break;
        }
        case wasm1p1_simd_code::f32x4_eq:
        {
            validate_v128_binary(u8"f32x4.eq");
            emit_v128_binary.template operator()<simd_opt::v128_binop::f32x4_eq>();
            break;
        }
        case wasm1p1_simd_code::f32x4_ne:
        {
            validate_v128_binary(u8"f32x4.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_ne>();
            break;
        }
        case wasm1p1_simd_code::f32x4_lt:
        {
            validate_v128_binary(u8"f32x4.lt");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_lt>();
            break;
        }
        case wasm1p1_simd_code::f32x4_gt:
        {
            validate_v128_binary(u8"f32x4.gt");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_gt>();
            break;
        }
        case wasm1p1_simd_code::f32x4_le:
        {
            validate_v128_binary(u8"f32x4.le");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_le>();
            break;
        }
        case wasm1p1_simd_code::f32x4_ge:
        {
            validate_v128_binary(u8"f32x4.ge");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_ge>();
            break;
        }
        case wasm1p1_simd_code::f64x2_eq:
        {
            validate_v128_binary(u8"f64x2.eq");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_eq>();
            break;
        }
        case wasm1p1_simd_code::f64x2_ne:
        {
            validate_v128_binary(u8"f64x2.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_ne>();
            break;
        }
        case wasm1p1_simd_code::f64x2_lt:
        {
            validate_v128_binary(u8"f64x2.lt");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_lt>();
            break;
        }
        case wasm1p1_simd_code::f64x2_gt:
        {
            validate_v128_binary(u8"f64x2.gt");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_gt>();
            break;
        }
        case wasm1p1_simd_code::f64x2_le:
        {
            validate_v128_binary(u8"f64x2.le");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_le>();
            break;
        }
        case wasm1p1_simd_code::f64x2_ge:
        {
            validate_v128_binary(u8"f64x2.ge");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_ge>();
            break;
        }
        case wasm1p1_simd_code::i64x2_eq:
        {
            validate_v128_binary(u8"i64x2.eq");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_eq>();
            break;
        }
        case wasm1p1_simd_code::i64x2_ne:
        {
            validate_v128_binary(u8"i64x2.ne");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_ne>();
            break;
        }
        case wasm1p1_simd_code::i64x2_lt_s:
        {
            validate_v128_binary(u8"i64x2.lt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_lt_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_gt_s:
        {
            validate_v128_binary(u8"i64x2.gt_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_gt_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_le_s:
        {
            validate_v128_binary(u8"i64x2.le_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_le_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_ge_s:
        {
            validate_v128_binary(u8"i64x2.ge_s");
            emit_full_binop.template operator()<simd_opt::simd_code::i64x2_ge_s>();
            break;
        }
        case wasm1p1_simd_code::f32x4_add:
        {
            validate_v128_binary(u8"f32x4.add");
            emit_v128_binary.template operator()<simd_opt::v128_binop::f32x4_add>();
            break;
        }
        case wasm1p1_simd_code::f32x4_sub:
        {
            validate_v128_binary(u8"f32x4.sub");
            emit_v128_binary.template operator()<simd_opt::v128_binop::f32x4_sub>();
            break;
        }
        case wasm1p1_simd_code::f32x4_mul:
        {
            validate_v128_binary(u8"f32x4.mul");
            emit_v128_binary.template operator()<simd_opt::v128_binop::f32x4_mul>();
            break;
        }
        case wasm1p1_simd_code::f32x4_div:
        {
            validate_v128_binary(u8"f32x4.div");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_div>();
            break;
        }
        case wasm1p1_simd_code::f32x4_min:
        {
            validate_v128_binary(u8"f32x4.min");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_min>();
            break;
        }
        case wasm1p1_simd_code::f32x4_max:
        {
            validate_v128_binary(u8"f32x4.max");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_max>();
            break;
        }
        case wasm1p1_simd_code::f32x4_pmin:
        {
            validate_v128_binary(u8"f32x4.pmin");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_pmin>();
            break;
        }
        case wasm1p1_simd_code::f32x4_pmax:
        {
            validate_v128_binary(u8"f32x4.pmax");
            emit_full_binop.template operator()<simd_opt::simd_code::f32x4_pmax>();
            break;
        }
        case wasm1p1_simd_code::f64x2_add:
        {
            validate_v128_binary(u8"f64x2.add");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_add>();
            break;
        }
        case wasm1p1_simd_code::f64x2_sub:
        {
            validate_v128_binary(u8"f64x2.sub");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_sub>();
            break;
        }
        case wasm1p1_simd_code::f64x2_mul:
        {
            validate_v128_binary(u8"f64x2.mul");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_mul>();
            break;
        }
        case wasm1p1_simd_code::f64x2_div:
        {
            validate_v128_binary(u8"f64x2.div");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_div>();
            break;
        }
        case wasm1p1_simd_code::f64x2_min:
        {
            validate_v128_binary(u8"f64x2.min");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_min>();
            break;
        }
        case wasm1p1_simd_code::f64x2_max:
        {
            validate_v128_binary(u8"f64x2.max");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_max>();
            break;
        }
        case wasm1p1_simd_code::f64x2_pmin:
        {
            validate_v128_binary(u8"f64x2.pmin");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_pmin>();
            break;
        }
        case wasm1p1_simd_code::f64x2_pmax:
        {
            validate_v128_binary(u8"f64x2.pmax");
            emit_full_binop.template operator()<simd_opt::simd_code::f64x2_pmax>();
            break;
        }
        case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_s:
        {
            validate_v128_unary(u8"i32x4.trunc_sat_f32x4_s");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_trunc_sat_f32x4_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_trunc_sat_f32x4_u:
        {
            validate_v128_unary(u8"i32x4.trunc_sat_f32x4_u");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_trunc_sat_f32x4_u>();
            break;
        }
        case wasm1p1_simd_code::f32x4_convert_i32x4_s:
        {
            validate_v128_unary(u8"f32x4.convert_i32x4_s");
            emit_v128_unary.template operator()<simd_opt::v128_unop::f32x4_convert_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::f32x4_convert_i32x4_u:
        {
            validate_v128_unary(u8"f32x4.convert_i32x4_u");
            emit_v128_unary.template operator()<simd_opt::v128_unop::f32x4_convert_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_s_zero:
        {
            validate_v128_unary(u8"i32x4.trunc_sat_f64x2_s_zero");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_trunc_sat_f64x2_s_zero>();
            break;
        }
        case wasm1p1_simd_code::i32x4_trunc_sat_f64x2_u_zero:
        {
            validate_v128_unary(u8"i32x4.trunc_sat_f64x2_u_zero");
            emit_full_unop.template operator()<simd_opt::simd_code::i32x4_trunc_sat_f64x2_u_zero>();
            break;
        }
        case wasm1p1_simd_code::f64x2_convert_low_i32x4_s:
        {
            validate_v128_unary(u8"f64x2.convert_low_i32x4_s");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_convert_low_i32x4_s>();
            break;
        }
        case wasm1p1_simd_code::f64x2_convert_low_i32x4_u:
        {
            validate_v128_unary(u8"f64x2.convert_low_i32x4_u");
            emit_full_unop.template operator()<simd_opt::simd_code::f64x2_convert_low_i32x4_u>();
            break;
        }
        case wasm1p1_simd_code::i8x16_shl:
        {
            validate_v128_shift(u8"i8x16.shl");
            emit_full_shift.template operator()<simd_opt::simd_code::i8x16_shl>();
            break;
        }
        case wasm1p1_simd_code::i8x16_shr_s:
        {
            validate_v128_shift(u8"i8x16.shr_s");
            emit_full_shift.template operator()<simd_opt::simd_code::i8x16_shr_s>();
            break;
        }
        case wasm1p1_simd_code::i8x16_shr_u:
        {
            validate_v128_shift(u8"i8x16.shr_u");
            emit_full_shift.template operator()<simd_opt::simd_code::i8x16_shr_u>();
            break;
        }
        case wasm1p1_simd_code::i16x8_shl:
        {
            validate_v128_shift(u8"i16x8.shl");
            emit_full_shift.template operator()<simd_opt::simd_code::i16x8_shl>();
            break;
        }
        case wasm1p1_simd_code::i16x8_shr_s:
        {
            validate_v128_shift(u8"i16x8.shr_s");
            emit_full_shift.template operator()<simd_opt::simd_code::i16x8_shr_s>();
            break;
        }
        case wasm1p1_simd_code::i16x8_shr_u:
        {
            validate_v128_shift(u8"i16x8.shr_u");
            emit_full_shift.template operator()<simd_opt::simd_code::i16x8_shr_u>();
            break;
        }
        case wasm1p1_simd_code::i32x4_shl:
        {
            validate_v128_shift(u8"i32x4.shl");
            emit_full_shift.template operator()<simd_opt::simd_code::i32x4_shl>();
            break;
        }
        case wasm1p1_simd_code::i32x4_shr_s:
        {
            validate_v128_shift(u8"i32x4.shr_s");
            emit_full_shift.template operator()<simd_opt::simd_code::i32x4_shr_s>();
            break;
        }
        case wasm1p1_simd_code::i32x4_shr_u:
        {
            validate_v128_shift(u8"i32x4.shr_u");
            emit_full_shift.template operator()<simd_opt::simd_code::i32x4_shr_u>();
            break;
        }
        case wasm1p1_simd_code::i64x2_shl:
        {
            validate_v128_shift(u8"i64x2.shl");
            emit_full_shift.template operator()<simd_opt::simd_code::i64x2_shl>();
            break;
        }
        case wasm1p1_simd_code::i64x2_shr_s:
        {
            validate_v128_shift(u8"i64x2.shr_s");
            emit_full_shift.template operator()<simd_opt::simd_code::i64x2_shr_s>();
            break;
        }
        case wasm1p1_simd_code::i64x2_shr_u:
        {
            validate_v128_shift(u8"i64x2.shr_u");
            emit_full_shift.template operator()<simd_opt::simd_code::i64x2_shr_u>();
            break;
        }
        [[unlikely]] default:
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    break;
}

case static_cast<wasm_byte>(wasm1p1_code::numeric_prefix):
{
    // [numeric prefix] subopcode LEB ... code_end
    // [safe         ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved the prefix byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [numeric prefix] subopcode LEB ... code_end
    // [safe         ] unsafe (could be code_end)
    //                  ^^ code_curr; bounded LEB decoding checks the subopcode.

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"numeric_prefix")};
    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

    auto const emit_trunc_sat{[&](::uwvm2::utils::container::u8string_view op_name,
                                  curr_operand_stack_value_type in_type,
                                  curr_operand_stack_value_type out_type,
                                  auto fptr) constexpr UWVM_THROWS
                              {
                                  if(!wasm2_feature_enabled(wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
                                  {
                                      fail_wasm1p1_feature_required(op_begin,
                                                                    subopcode,
                                                                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                                                    ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
                                  }

                                  validate_numeric_unary_stack_effect(op_begin, op_name, in_type, out_type);
                                  if constexpr(stacktop_enabled)
                                  {
                                      if(stacktop_enabled_for_vt(out_type) && !stacktop_ranges_merged_for(in_type, out_type))
                                      {
                                          stacktop_prepare_push1_if_reachable(bytecode, out_type);
                                      }
                                  }

                                  emit_opfunc_to(bytecode, fptr);
                                  if(stacktop_ranges_merged_for(in_type, out_type))
                                  {
                                      if constexpr(stacktop_enabled)
                                      {
                                          if(!is_polymorphic) { codegen_stack_set_top(out_type); }
                                      }
                                  }
                                  else
                                  {
                                      stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, out_type);
                                  }
                              }};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const validate_checked_bulk_memory{[&](::uwvm2::utils::container::u8string_view name,
        ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const& decoded) constexpr UWVM_THROWS
    {
        namespace bulk = ::uwvm2::validation::standard::wasm3;
        decltype(try_pop_concrete_operand()) actual_bulk_operand{};
        auto const consume_bulk_operand{[&]() constexpr noexcept
        {
            // The common sequence preflight and single-pop proof bound this owned
            // current-frame operand removal. No source or guest pointer advances.
            actual_bulk_operand = try_pop_concrete_operand();
            return bulk::core3_operand{bulk::core3_operand_effective_type(actual_bulk_operand),
                !actual_bulk_operand.from_stack || actual_bulk_operand.is_unknown};
        }};
        auto const failure{bulk::validate_bulk_memory_operand_sequence(
            decoded, is_polymorphic, concrete_operand_count, consume_bulk_operand)};
        if(failure.error == bulk::typed_stack_error::stack_underflow) [[unlikely]]
        { report_operand_stack_underflow(op_begin, name, bulk::bulk_memory_operand_count(decoded)); }
        if(failure.error != bulk::typed_stack_error::ok) [[unlikely]]
        {
            // [dispatch-checked FC][checked data/memory immediates] | code_end
            // [safe opcode        ][safe                         ] | one-past not read
            // ^^ op_begin -> err.err_curr: copy the original diagnostic span only.
            err.err_curr = op_begin;
            auto const core{bulk::bulk_memory_expected_operand_type(decoded, failure.failed_pop_index)};
            auto const expected{core.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
            err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = name,
                .expected_type = to_wasm1_value_type(expected), .actual_type = to_wasm1_value_type(actual_bulk_operand.type)};
            err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }};

    switch(numeric_code)
    {
        case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
            emit_trunc_sat(u8"i32.trunc_sat_f32_s",
                           curr_operand_stack_value_type::f32,
                           curr_operand_stack_value_type::i32,
                           translate::get_uwvmint_i32_trunc_sat_f32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
            emit_trunc_sat(u8"i32.trunc_sat_f32_u",
                           curr_operand_stack_value_type::f32,
                           curr_operand_stack_value_type::i32,
                           translate::get_uwvmint_i32_trunc_sat_f32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
            emit_trunc_sat(u8"i32.trunc_sat_f64_s",
                           curr_operand_stack_value_type::f64,
                           curr_operand_stack_value_type::i32,
                           translate::get_uwvmint_i32_trunc_sat_f64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
            emit_trunc_sat(u8"i32.trunc_sat_f64_u",
                           curr_operand_stack_value_type::f64,
                           curr_operand_stack_value_type::i32,
                           translate::get_uwvmint_i32_trunc_sat_f64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
            emit_trunc_sat(u8"i64.trunc_sat_f32_s",
                           curr_operand_stack_value_type::f32,
                           curr_operand_stack_value_type::i64,
                           translate::get_uwvmint_i64_trunc_sat_f32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
            emit_trunc_sat(u8"i64.trunc_sat_f32_u",
                           curr_operand_stack_value_type::f32,
                           curr_operand_stack_value_type::i64,
                           translate::get_uwvmint_i64_trunc_sat_f32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
            emit_trunc_sat(u8"i64.trunc_sat_f64_s",
                           curr_operand_stack_value_type::f64,
                           curr_operand_stack_value_type::i64,
                           translate::get_uwvmint_i64_trunc_sat_f64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
            emit_trunc_sat(u8"i64.trunc_sat_f64_u",
                           curr_operand_stack_value_type::f64,
                           curr_operand_stack_value_type::i64,
                           translate::get_uwvmint_i64_trunc_sat_f64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        case wasm1p1_numeric_code::memory_init:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"memory.init.dataidx")};
            check_data_index(op_begin, data_index);
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(op_begin, memory_index, u8"memory.init");
            auto const address_type{memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::init,
                .data_index = data_index, .destination_memory_index = memory_index,
                .destination_address = memory_address_type_at(memory_index)};
            validate_checked_bulk_memory(u8"memory.init", decoded_bulk);
            current_memory_index = memory_index;
            ensure_memory_resolved();
            auto data_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t*>(
                ::std::addressof(curr_module.local_defined_data_vec_storage.index_unchecked(data_index)))};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(address_type == curr_operand_stack_value_type::i64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_init_fptr_from_tuple<CompileOption>(interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory_init_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, data_ptr);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::data_drop:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"data.drop")};
            check_data_index(op_begin, data_index);
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::data_drop, .data_index = data_index};
            validate_checked_bulk_memory(u8"data.drop", decoded_bulk);
            auto data_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t*>(
                ::std::addressof(curr_module.local_defined_data_vec_storage.index_unchecked(data_index)))};
            emit_opfunc_to(bytecode, translate::get_uwvmint_data_drop_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, data_ptr);
            break;
        }
        case wasm1p1_numeric_code::memory_copy:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const dst_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(op_begin, dst_memory_index, u8"memory.copy");
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const src_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(op_begin, src_memory_index, u8"memory.copy");
            auto const destination64{memory_address_type_at(dst_memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64};
            auto const source64{memory_address_type_at(src_memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64};
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::copy,
                .destination_memory_index = dst_memory_index, .source_memory_index = src_memory_index,
                .destination_address = memory_address_type_at(dst_memory_index), .source_address = memory_address_type_at(src_memory_index)};
            validate_checked_bulk_memory(u8"memory.copy", decoded_bulk);
            current_memory_index = src_memory_index;
            ensure_memory_resolved();
            // Source memory is a validated module-owned object; retain it before resolving the destination.
            auto const source_memory{resolved_memory.memory_p};
            current_memory_index = dst_memory_index;
            ensure_memory_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            if(destination64 && source64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_bulk_fptr_from_tuple<true, true, true, CompileOption>(interpreter_tuple)); }
            else if(destination64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_bulk_fptr_from_tuple<true, true, false, CompileOption>(interpreter_tuple)); }
            else if(source64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_bulk_fptr_from_tuple<true, false, true, CompileOption>(interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory_copy_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, source_memory);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::memory_fill:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(op_begin, memory_index, u8"memory.fill");
            auto const address_type{memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::fill,
                .destination_memory_index = memory_index, .destination_address = memory_address_type_at(memory_index)};
            validate_checked_bulk_memory(u8"memory.fill", decoded_bulk);
            current_memory_index = memory_index;
            ensure_memory_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            if(address_type == curr_operand_stack_value_type::i64)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_bulk_fptr_from_tuple<false, true, false, CompileOption>(interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_memory_fill_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, resolved_memory.memory_p);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::table_init:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
            }
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const element_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.init.elemidx")};
            check_element_index(op_begin, element_index);
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.init.tableidx")};
            check_table_index(op_begin, table_index, subopcode);

            // [runtime element records ... element_index ... end) remains owned during compilation.
            // [safe                                                 ] check_element_index proved the slot.
            //                           ^^ element_declaration borrows a retained parser declaration; code_curr stays put.
            auto const* element_declaration{curr_module.local_defined_element_vec_storage.index_unchecked(element_index).element_type_ptr};
            if(element_declaration == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
            auto const& element{element_declaration->storage.segment};
            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element.reftype))};
            auto const table_value_type{get_table_value_type(table_index)};
            auto const element_core{element.has_core_type ? element.core_type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(element_value_type)};
            auto const matches{runtime_core3_value_type_matches(
                element_core, get_table_core_type(table_index),
                ::uwvm2::validation::standard::wasm3::core3_signature_view<
                    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin,
                        rich_owned_available ? runtime_type_count : 0uz})};
            if(!matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_value_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(element_value_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            validate_i32_operands(op_begin, u8"table.init", 2uz);
            validate_table_operand(op_begin, u8"table.init", table_operand_type(table_index));
            auto const table_ptr{resolve_runtime_table(table_index)};
            auto element_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t*>(
                ::std::addressof(curr_module.local_defined_element_vec_storage.index_unchecked(element_index)))};
            auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
                ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
            { emit_table64_op.template operator()<table64_operation::init>(table_value_type, gc_table); }
            else if(gc_table)
            { emit_table64_op.template operator()<table64_operation::init, false>(table_value_type, true); }
            else if(table_value_type == static_cast<curr_operand_stack_value_type>(0x69u))
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_init_exnref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else if(table_value_type == curr_operand_stack_value_type::externref)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_init_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_init_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            emit_imm_to(bytecode, table_ptr);
            emit_imm_to(bytecode, element_ptr);
            if(::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
               ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::function)
            { emit_imm_to(bytecode, ::std::addressof(curr_module)); }
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::elem_drop:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
            }
            auto const element_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"elem.drop")};
            check_element_index(op_begin, element_index);
            auto element_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t*>(
                ::std::addressof(curr_module.local_defined_element_vec_storage.index_unchecked(element_index)))};
            emit_opfunc_to(bytecode, translate::get_uwvmint_elem_drop_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, element_ptr);
            break;
        }
        case wasm1p1_numeric_code::table_copy:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const dst_table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.dst")};
            check_table_index(op_begin, dst_table_index, subopcode);
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const src_table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.src")};
            check_table_index(op_begin, src_table_index, subopcode);

            auto const dst_type{get_table_value_type(dst_table_index)};
            auto const src_type{get_table_value_type(src_table_index)};
            auto const copy_matches{rich_owned_available ?
                runtime_core3_value_type_matches(
                    get_table_core_type(src_table_index),
                    get_table_core_type(dst_table_index),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                dst_type == src_type};
            if(!copy_matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(dst_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(src_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const destination_type{table_operand_type(dst_table_index)};
            auto const source_type{table_operand_type(src_table_index)};
            auto const length_type{destination_type == curr_operand_stack_value_type::i64 && source_type == curr_operand_stack_value_type::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
            validate_table_operand(op_begin, u8"table.copy", length_type);
            validate_table_operand(op_begin, u8"table.copy", source_type);
            validate_table_operand(op_begin, u8"table.copy", destination_type);
            auto const dst_table_ptr{resolve_runtime_table(dst_table_index)};
            auto const src_table_ptr{resolve_runtime_table(src_table_index)};
            auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*dst_table_ptr) ==
                ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(destination_type == curr_operand_stack_value_type::i64)
            {
                if(source_type == curr_operand_stack_value_type::i64)
                { emit_table64_op.template operator()<table64_operation::copy, true, true>(dst_type, gc_table); }
                else { emit_table64_op.template operator()<table64_operation::copy, true, false>(dst_type, gc_table); }
            }
            else if(source_type == curr_operand_stack_value_type::i64)
            { emit_table64_op.template operator()<table64_operation::copy, false, true>(dst_type, gc_table); }
            else if(gc_table)
            { emit_table64_op.template operator()<table64_operation::copy, false, false>(dst_type, true); }
            else { emit_opfunc_to(bytecode, translate::get_uwvmint_table_copy_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, dst_table_ptr);
            emit_imm_to(bytecode, src_table_ptr);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::table_grow:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
            {
                fail_wasm2_feature_required(op_begin,
                                            subopcode,
                                            ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
            }
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.grow")};
            check_table_index(op_begin, table_index, subopcode);
            auto const table_type{get_table_value_type(table_index)};

            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.grow", 2uz); }

            auto const delta{try_pop_concrete_operand()};
            if(!operand_type_matches(delta, table_operand_type(table_index))) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(delta.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const value{try_pop_concrete_operand()};
            auto const value_matches{rich_owned_available && value.from_stack && !value.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                    get_table_core_type(table_index),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                operand_type_matches(value, table_type)};
            if(!value_matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            operand_stack_push(table_operand_type(table_index));
            auto const table_ptr{resolve_runtime_table(table_index)};
            auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
                ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
            { emit_table64_op.template operator()<table64_operation::grow>(table_type, gc_table); }
            else if(gc_table)
            { emit_table64_op.template operator()<table64_operation::grow, false>(table_type, true); }
            else if(table_type == static_cast<curr_operand_stack_value_type>(0x69u))
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_grow_exnref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else if(table_type == curr_operand_stack_value_type::externref)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_grow_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_grow_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            emit_imm_to(bytecode, table_ptr);
            if(::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
               ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::function)
            { emit_imm_to(bytecode, ::std::addressof(curr_module)); }
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, table_operand_type(table_index));
            break;
        }
        case wasm1p1_numeric_code::table_size:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::table_instructions)) [[unlikely]]
            {
                fail_wasm2_feature_required(op_begin,
                                            subopcode,
                                            ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
            }
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.size")};
            check_table_index(op_begin, table_index, subopcode);
            operand_stack_push(table_operand_type(table_index));
            auto const table_ptr{resolve_runtime_table(table_index)};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
            { emit_table64_op.template operator()<table64_operation::size>(get_table_value_type(table_index)); }
            else { emit_opfunc_to(bytecode, translate::get_uwvmint_table_size_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, table_ptr);
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 0uz, table_operand_type(table_index));
            break;
        }
        case wasm1p1_numeric_code::table_fill:
        {
            if(!wasm2_feature_enabled(wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            // [consumed opcode] table immediate ... code_end
            // [safe           ] unsafe (could be code_end); bounded decoder commits
            // code_curr only after the complete u32 field, never beyond code_end.
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.fill")};
            check_table_index(op_begin, table_index, subopcode);
            auto const table_type{get_table_value_type(table_index)};

            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.fill", 3uz); }

            auto const len{try_pop_concrete_operand()};
            if(!operand_type_matches(len, table_operand_type(table_index))) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(len.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const value{try_pop_concrete_operand()};
            auto const value_matches{rich_owned_available && value.from_stack && !value.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(value),
                    get_table_core_type(table_index),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                operand_type_matches(value, table_type)};
            if(!value_matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const index{try_pop_concrete_operand()};
            if(!operand_type_matches(index, table_operand_type(table_index))) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(index.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const table_ptr{resolve_runtime_table(table_index)};
            auto const gc_table{::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
                ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc};
            stacktop_flush_all_to_operand_stack(bytecode);
            if(table_operand_type(table_index) == curr_operand_stack_value_type::i64)
            { emit_table64_op.template operator()<table64_operation::fill>(table_type, gc_table); }
            else if(gc_table)
            { emit_table64_op.template operator()<table64_operation::fill, false>(table_type, true); }
            else if(table_type == static_cast<curr_operand_stack_value_type>(0x69u))
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_fill_exnref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else if(table_type == curr_operand_stack_value_type::externref)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_fill_externref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_table_fill_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            emit_imm_to(bytecode, table_ptr);
            if(::uwvm2::uwvm::runtime::storage::runtime_table_family(*table_ptr) ==
               ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::function)
            { emit_imm_to(bytecode, ::std::addressof(curr_module)); }
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        [[unlikely]] default:
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    break;
}
