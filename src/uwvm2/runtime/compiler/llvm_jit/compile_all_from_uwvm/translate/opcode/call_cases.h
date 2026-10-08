    // Direct and indirect call validation cases for the WebAssembly primary opcode set. Validation and emission preserve
    // complete parameter/result tuples; typed LLVM calls return 0 results as void, 1 as a scalar, and multiple results as
    // a Wasm-order literal struct. Raw host/import boundaries use the corresponding tightly packed result buffer.

case static_cast<wasm1_code>(0x12u):
{
    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x12u, code_curr, err);
    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    //          ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(func_index))};
    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index_encoding;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
    }

    // return_call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //      ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(func_next);

    // return_call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //                ^^ code_curr

    // Validate function index range (imports + locals)
    auto const all_function_size{import_func_count + local_func_count};
    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve callee type
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* callee_type_ptr{};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        auto const& imported_funcs{importsec.importdesc.index_unchecked(0u)};
        auto const imported_func_ptr{imported_funcs.index_unchecked(static_cast<::std::size_t>(func_index))};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        // [validated import/type storage] end; function/type index checked above.
        // [safe                         ]; borrowed until module retirement.
        // ^^ callee_type_ptr receives the complete function-type record.
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        // [validated import/type storage] end; function/type index checked above.
        // [safe                         ]; borrowed until module retirement.
        // ^^ callee_type_ptr receives the complete function-type record.
        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};
    auto const callee_type_index{synthetic_function_type_index_from_pointer(callee_type_ptr)};

    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
    validate_rich_tail_results(op_begin, u8"return_call", callee_type, callee_type_index);

    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"return_call", param_count); }

    // Type-check arguments when the stack is non-polymorphic.
    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!callee_argument_matches(actual_operand, callee_type, callee_type_index, param_count - 1uz - i)) [[unlikely]]
            {
                // [tail opcode] immediates ... code_end
                // [safe       ] unsafe (could be code_end); no dereference.
                // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;
    // The sole typed walk already checked the complete callee tuple.
    // [owned output vector] indices only; no input pointer movement/read.
    retain_lazy_dependency({::uwvm2::validation::standard::wasm3::validated_call_dependency_kind::direct_function, static_cast<::std::size_t>(func_index)});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_return_call(llvm_jit_emit_state, func_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }
    break;
}
case wasm1_code::call:
{
    // call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    //          ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(func_index))};
    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index_encoding;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
    }

    // call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //      ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(func_next);

    // call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //                ^^ code_curr

    // Validate function index range (imports + locals)
    auto const all_function_size{import_func_count + local_func_count};
    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_function_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve callee type
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* callee_type_ptr{};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        auto const& imported_funcs{importsec.importdesc.index_unchecked(0u)};
        auto const imported_func_ptr{imported_funcs.index_unchecked(static_cast<::std::size_t>(func_index))};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        // callee type: [checked import's function type record] record end
        // [safe complete borrowed record                  ] unsafe (past record end)
        // ^^ callee_type_ptr takes the checked import's function type pointer for this call.
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        // callee type: typesec.types.begin [validated type index] typesec.types.end
        // [safe element within typesec.types                     ] unsafe (past end)
        // ^^ callee_type_ptr uses the parsed local function's validated type index to borrow that element.
        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};
    auto const callee_type_index{synthetic_function_type_index_from_pointer(callee_type_ptr)};

    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"call", param_count); }

    // Type-check arguments when the stack is non-polymorphic.
    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!callee_argument_matches(actual_operand, callee_type, callee_type_index, param_count - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
    if(native_eh_work) { native_eh_work->prepare_call(func_index, control_flow_stack); }
#endif
    // Consume parameters if present.
    if(param_count != 0uz) { operand_stack_pop_n(param_count); }

    // Push every result in Wasm source order. The emitter uses the same full tuple signature for direct typed calls and
    // raw-buffer bridge calls, so validation and LLVM lowering keep identical stack effects.
    if(result_count != 0uz)
    {
        operand_stack_push_function_results(callee_type, synthetic_function_type_index_from_pointer(callee_type_ptr));
    }

    // Preserve the actual checked tuple dependency even if this legal body's
    // earlier native lowering declined. Default full calls have no sink.
    // [owned output vector] indices only; no input pointer movement/read.
    retain_lazy_dependency({::uwvm2::validation::standard::wasm3::validated_call_dependency_kind::direct_function, static_cast<::std::size_t>(func_index)});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        llvm_jit_emit_state.checkpoint_call = {};
        // Imported Wasm forwarding is eligible only with the actual registered
        // defined-leaf witness. Host/dl/weak imports keep their prior metadata
        // and the original runtime foreign/nonreplayable admission guard.
        if(llvm_jit_emit_state.checkpoint_plan != nullptr &&
           (static_cast<::std::size_t>(func_index) >= import_func_count ||
            runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(llvm_jit_emit_state, func_index)))
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            ::std::vector<checkpoint::types::core_value_type> exact_after{}; exact_after.reserve(operand_stack.size());
            bool complete{!is_polymorphic && code_curr >= code_begin && code_curr < code_end};
            for(::std::size_t i{}; complete && i != operand_stack.size(); ++i)
            {
                // [same fused validator's complete post-call operand deque] end
                // [safe] i<N BEFORE preserving exact Core3 result/prefix types.
                auto const& operand{operand_stack[i]};
                if(operand.is_unknown) { complete=false; break; }
                exact_after.push_back(operand_core_type(operand)); // preserve rich/bottom/function witnesses from this same walk
            }
            // [checked original expression begin ... decoded next ...] end
            // [safe] complete same-allocation bounded cursor before difference;
            // no pointer advance/read or second immediate decoder is introduced.
            if(complete && !prepare_runtime_local_func_llvm_jit_checkpoint_direct_call(llvm_jit_emit_state,
                checkpoint_opcode_transaction, param_count, result_count,
                static_cast<::std::size_t>(code_curr-code_begin), exact_after))
            { disable_inline_llvm_jit_emission(); }
        }
        if(emit_llvm_jit_active && (!try_emit_runtime_local_func_llvm_jit_call(llvm_jit_emit_state, func_index) ||
            !finish_runtime_local_func_llvm_jit_checkpoint_direct_call(llvm_jit_emit_state)))
        [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case static_cast<wasm1_code>(0x13u):
{
    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x13u, code_curr, err);
    // return_call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // return_call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // return_call_indirect type_index table_index ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(type_index))};
    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [checked opcode] ... end; no dereference.
        // [safe          ] unsafe; ^^ err_curr receives op_begin.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
    }

    // return_call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //               ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(type_next);

    // return_call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //                          ^^ code_curr

    // Decode through a local scanner and commit code_curr only after the complete trailing field.
    // MVP likewise advances only after matching its literal 0x00, so every trailing-immediate decode
    // failure leaves code_curr at the table_index position shown above.
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index{};
    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end), ::fast_io::mnp::leb128_get(table_index))};
    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [checked opcode] ... end; no dereference.
        // [safe          ] unsafe; ^^ err_curr receives op_begin.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
    }
    // return_call_indirect type_index table_index ... code_end
    // [          decoded and safe          ] unsafe (could be code_end)
    //                          ^^ code_curr; table_next was checked before this commit.
    code_curr = reinterpret_cast<::std::byte const*>(table_next);

    // return_call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                                      ^^ code_curr

    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
    // type_index so the LLVM translator and standard validators agree on compound-invalid operands.
    auto const all_type_count_uz{typesec.types.size()};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        // [checked opcode] ... end; no dereference.
        // [safe          ] unsafe; ^^ err_curr receives op_begin.
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        ::std::addressof(typesec.core3_context), static_cast<::std::size_t>(type_index),
        op_begin, u8"return_call_indirect", err);
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::multiple_tables) && table_index != 0u)
        [[unlikely]]
    {
        fail_wasm2_feature_required(
            op_begin,
            static_cast<validation_module_traits_t::wasm_u32>(
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(static_cast<wasm1_code>(0x13u))),
            ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    if(table_index >= all_table_count) [[unlikely]]
    {
        // [checked opcode] ... end; no dereference.
        // [safe          ] unsafe; ^^ err_curr receives op_begin.
        err.err_curr = op_begin;
        err.err_selectable.illegal_table_index.table_index = table_index;
        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve the function signature by type index.
    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
    auto const callee_type_index{static_cast<::std::size_t>(type_index)};
    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
    validate_rich_tail_results(op_begin, u8"return_call_indirect", callee_type, callee_type_index);
    auto const table_core{get_table_core_type(table_index)};
    auto const table_exact_type{table_core.has_type ? table_core.type :
        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(get_table_value_type(table_index))};
    if(!::uwvm2::validation::standard::wasm3::core3_indirect_call_table_type_matches(
        table_exact_type, typesec)) [[unlikely]]
    {
        // [checked opcode] ... end; no dereference.
        // [safe          ] unsafe; ^^ err_curr receives op_begin.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
        err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(get_table_value_type(table_index));
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Tail effect consumes args and the table-typed selector, then terminates this frame.
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"return_call_indirect", required_stack_size);
    }

    // The selector uses the declared table address type, independently of memory64.
    validate_table_operand(op_begin, u8"return_call_indirect", table_operand_type(table_index));

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!callee_argument_matches(actual_operand, callee_type, callee_type_index, param_count - 1uz - i)) [[unlikely]]
            {
                // [checked opcode] ... end; no dereference.
                // [safe          ] unsafe; ^^ err_curr receives op_begin.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    // The sole typed walk already checked the complete callee tuple.
    // [owned output vector] indices only; no input pointer movement/read.
    retain_lazy_dependency({::uwvm2::validation::standard::wasm3::validated_call_dependency_kind::indirect_table, static_cast<::std::size_t>(table_index)});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_call_indirect<true>(llvm_jit_emit_state, type_index, table_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::call_indirect:
{
    // call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // call_indirect type_index table_index ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_index;  // No initialization necessary
    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(type_index))};
    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
    }

    // call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //               ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(type_next);

    // call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //                          ^^ code_curr

    // Decode through a local scanner and commit code_curr only after the complete trailing field.
    // MVP likewise advances only after matching its literal 0x00, so every trailing-immediate decode
    // failure leaves code_curr at the table_index position shown above.
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index{};
    if(!::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_call_indirect_reserved_byte(wasm1p1_para))
    {
        // Reference Types/Core 2.0 encode a real `tableidx ::= u32`; disabling
        // multiple tables constrains the decoded value and does not alter this grammar.
        auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                    ::fast_io::mnp::leb128_get(table_index))};
        if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
        }
        // call_indirect table index ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ table_next: preceding bounded scan/lookahead proved a position in this code slice.
        // call_indirect table immediate ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
        code_curr = reinterpret_cast<::std::byte const*>(table_next);
        // call_indirect table index ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        //                       ^^ code_curr may be one-past; no read occurs here.
    }
    else
    {
        // Core 1.0 section 5.4.1 uses one literal reserved byte in
        // `0x11 typeidx 0x00`; it is not a ULEB128 table index.
        if(code_curr == code_end || *code_curr != ::std::byte{}) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // call_indirect reserved table byte ... code_end
        // [safe byte                          ] unsafe (could be code_end)
        // ^^ code_curr: non-end and zero-byte checks proved it exists.
        ++code_curr;
        // [safe byte                          ] unsafe (could be code_end)
        //                                       ^^ code_curr may be one-past.
    }

    // call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                                      ^^ code_curr

    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
    // type_index so the LLVM translator and standard validators agree on compound-invalid operands.
    auto const all_type_count_uz{typesec.types.size()};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        ::std::addressof(typesec.core3_context), static_cast<::std::size_t>(type_index),
        op_begin, u8"call_indirect", err);
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::multiple_tables) && table_index != 0u)
        [[unlikely]]
    {
        fail_wasm2_feature_required(
            op_begin,
            static_cast<validation_module_traits_t::wasm_u32>(
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(wasm1_code::call_indirect)),
            ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    if(table_index >= all_table_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_table_index.table_index = table_index;
        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const table_core{get_table_core_type(table_index)};
    auto const table_exact_type{table_core.has_type ? table_core.type :
        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(get_table_value_type(table_index))};
    if(!::uwvm2::validation::standard::wasm3::core3_indirect_call_table_type_matches(
        table_exact_type, typesec)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] complete immediates ... | code_end
        // [dispatch-proven byte, where present          ] | one-past is not dereferenced.
        // ^^ op_begin -> err.err_curr: diagnostic borrow only; no input cursor moves.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
        err.err_selectable.br_value_type_mismatch.expected_type =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(get_table_value_type(table_index));
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve the function signature by type index.
    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
    auto const callee_type_index{static_cast<::std::size_t>(type_index)};
    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

    // Stack effect: (args..., table-address table_element_index) -> (results...)
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"call_indirect", required_stack_size);
    }

    // The selector uses the declared table address type, independently of memory64.
    validate_table_operand(op_begin, u8"call_indirect", table_operand_type(table_index));

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!callee_argument_matches(actual_operand, callee_type, callee_type_index, param_count - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    if(param_count != 0uz) { operand_stack_pop_n(param_count); }

    // Apply the same complete result tuple as direct call; call_indirect merges typed and raw-buffer paths with the
    // canonical scalar-or-struct LLVM result type before restoring individual Wasm stack values.
    if(result_count != 0uz)
    {
        operand_stack_push_function_results(callee_type, static_cast<::std::size_t>(type_index));
    }

    // The sole typed walk already checked the complete callee tuple.
    // [owned output vector] indices only; no input pointer movement/read.
    retain_lazy_dependency({::uwvm2::validation::standard::wasm3::validated_call_dependency_kind::indirect_table, static_cast<::std::size_t>(table_index)});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        llvm_jit_emit_state.checkpoint_call = {};
        if(runtime_local_func_llvm_jit_checkpoint_dynamic_call_selected(llvm_jit_emit_state))
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            ::std::vector<checkpoint::types::core_value_type> exact_after{};
            exact_after.reserve(operand_stack.size());
            bool complete{!is_polymorphic && code_curr >= code_begin && code_curr < code_end};
            for(::std::size_t i{}; complete && i != operand_stack.size(); ++i)
            {
                // [same fused validator's exact post-call operand tuple0..N] end
                // [safe] i<N BEFORE reading the rich declaration. The table
                // selector/reference and arguments were checked by this walk.
                auto const& operand{operand_stack[i]};
                if(operand.is_unknown) { complete=false; break; }
                exact_after.push_back(operand_core_type(operand));
            }
            // [original expression begin ... fully decoded next ...] end
            // [safe] same-allocation complete bounded cursor BEFORE subtraction;
            // no cursor change/read or second validation/body walk is performed.
            if(complete && !prepare_runtime_local_func_llvm_jit_checkpoint_dynamic_call(llvm_jit_emit_state,
                checkpoint_opcode_transaction, param_count, result_count,
                static_cast<::std::size_t>(code_curr-code_begin), exact_after))
            { disable_inline_llvm_jit_emission(); }
        }
        if(emit_llvm_jit_active && (!try_emit_runtime_local_func_llvm_jit_call_indirect(llvm_jit_emit_state, type_index, table_index) ||
            !finish_runtime_local_func_llvm_jit_checkpoint_direct_call(llvm_jit_emit_state)))
        [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}

case static_cast<wasm1_code>(0x14u):
case static_cast<wasm1_code>(0x15u):
{
    // call_ref / return_call_ref typeidx ... code_end
    // [safe                      ] unsafe (could be code_end)
    // ^^ code_curr: the outer opcode dispatch proved this byte exists.
    auto const op_begin{code_curr};
    auto const tail{*code_curr == ::std::byte{0x15u}};
    auto const op_name{tail ? ::uwvm2::utils::container::u8string_view{u8"return_call_ref"} :
                              ::uwvm2::utils::container::u8string_view{u8"call_ref"}};
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, tail ? 0x15u : 0x14u, op_begin, err);
    if(tail)
    { ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x15u, op_begin, err); }
    ++code_curr;
    // call_ref / return_call_ref typeidx ... code_end
    // [safe                      ] unsafe (could be code_end)
    //                             ^^ code_curr: skip only the dispatch-checked opcode.
    auto const type_index{read_leb128.template operator()<validation_module_traits_t::wasm_u32>(
        code_curr, code_end, op_begin, op_name)};
    // call_ref / return_call_ref [complete typeidx] ... code_end
    // [safe                                      ] unsafe (could be code_end)
    //                                           ^^ code_curr: bounded LEB decoding committed the whole immediate.
    if(static_cast<::std::size_t>(type_index) >= typesec.types.size()) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Borrow the checked opcode; no unbounded input read.
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count =
            static_cast<validation_module_traits_t::wasm_u32>(typesec.types.size());
        err.err_code = code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        ::std::addressof(typesec.core3_context), static_cast<::std::size_t>(type_index),
        op_begin, op_name, err);
    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
    auto const callee_type_index{static_cast<::std::size_t>(type_index)};
    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
    if(tail) { validate_rich_tail_results(op_begin, u8"return_call_ref", callee_type, callee_type_index); }

    constexpr auto max_size{(::std::numeric_limits<::std::size_t>::max)()};
    auto const required{param_count == max_size ? max_size : param_count + 1uz};
    if(!is_polymorphic && (param_count == max_size || concrete_operand_count() < required)) [[unlikely]]
    { report_operand_stack_underflow(op_begin, op_name, required); }

    // The reference is the last evaluated operand. An erased funcref is a supertype and
    // cannot statically prove `(ref null typeidx)`; ref.func/ref.null carry an exact witness.
    auto const reference{try_pop_concrete_operand()};
    auto const rich_reference_signature{!typesec.owned_signatures.empty() &&
        typesec.owned_signatures.size() == typesec.types.size()};
    auto const reference_matches{::uwvm2::validation::standard::wasm3::core3_call_ref_reference_matches(
        reference, static_cast<::std::size_t>(type_index), typesec.types.size(), rich_reference_signature,
        [&](auto actual, auto expected) constexpr noexcept
        { return runtime_core3_value_type_matches(actual, expected, typesec.owned_signatures); },
        [&](::std::size_t index) constexpr noexcept -> auto const&
        {
            // [retained carrier type records 0 ... index ... size) end
            // [safe] shared matcher proved index<size BEFORE this borrow.
            return typesec.types.index_unchecked(index);
        })};
    if(!reference_matches) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Checked opcode remains within the function body.
        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
        err.err_selectable.br_value_type_mismatch.expected_type =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(reference.type);
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const concrete_to_check{concrete_operand_count() < param_count ? concrete_operand_count() : param_count};
    for(::std::size_t i{}; i != concrete_to_check; ++i)
    {
        auto const expected{callee_type.parameter.begin[param_count - 1uz - i]};
        auto const& actual{operand_stack[operand_stack.size() - 1uz - i]};
        if(!callee_argument_matches(actual, callee_type, callee_type_index, param_count - 1uz - i)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Diagnostic borrow; no cursor movement.
            err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
            err.err_selectable.br_value_type_mismatch.expected_type =
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected);
            err.err_selectable.br_value_type_mismatch.actual_type =
                static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual.type);
            err.err_code = code_validation_error_code::br_value_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    operand_stack_pop_n(param_count);
    if(tail)
    {
        operand_stack_truncate_to(control_flow_stack.back_unchecked().operand_stack_base);
        is_polymorphic = true;
    }
    else
    { operand_stack_push_function_results(callee_type, static_cast<::std::size_t>(type_index)); }

    // The sole typed walk already checked the complete callee tuple.
    // [owned output vector] indices only; no input pointer movement/read.
    retain_lazy_dependency({::uwvm2::validation::standard::wasm3::validated_call_dependency_kind::reference_signature, static_cast<::std::size_t>(type_index)});

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        // A tail transfer retires this activation and has no waiting/after
        // parent of its own. Its existing native musttail edge stays intact.
        llvm_jit_emit_state.checkpoint_call = {};
        if(!tail && runtime_local_func_llvm_jit_checkpoint_dynamic_call_selected(llvm_jit_emit_state))
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            ::std::vector<checkpoint::types::core_value_type> exact_after{};
            exact_after.reserve(operand_stack.size());
            bool complete{!is_polymorphic && code_curr >= code_begin && code_curr < code_end};
            for(::std::size_t i{}; complete && i != operand_stack.size(); ++i)
            {
                // [same fused validator's exact post-call operand tuple0..N] end
                // [safe] i<N BEFORE reading the rich declaration. The table
                // selector/reference and arguments were checked by this walk.
                auto const& operand{operand_stack[i]};
                if(operand.is_unknown) { complete=false; break; }
                exact_after.push_back(operand_core_type(operand));
            }
            // [original expression begin ... fully decoded next ...] end
            // [safe] same-allocation complete bounded cursor BEFORE subtraction;
            // no cursor change/read or second validation/body walk is performed.
            if(complete && !prepare_runtime_local_func_llvm_jit_checkpoint_dynamic_call(llvm_jit_emit_state,
                checkpoint_opcode_transaction, param_count, result_count,
                static_cast<::std::size_t>(code_curr-code_begin), exact_after))
            { disable_inline_llvm_jit_emission(); }
        }
        if(emit_llvm_jit_active)
        {
            auto const emitted{tail ? try_emit_runtime_local_func_llvm_jit_call_ref<true>(llvm_jit_emit_state, type_index) :
                                      try_emit_runtime_local_func_llvm_jit_call_ref<false>(llvm_jit_emit_state, type_index)};
            if(!emitted || (!tail && !finish_runtime_local_func_llvm_jit_checkpoint_direct_call(llvm_jit_emit_state)))
            [[unlikely]] { disable_inline_llvm_jit_emission(); }
        }
    }
    break;
}
