// Parametric and variable validation over the shared runtime value tags.
// Primary select admits numeric/vector types; typed reference select is in wasm1p1_cases.h.

case wasm1_code::drop:
{
    // drop   ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // drop   ...
    // [safe] unsafe (could be the section_end)
    //        ^^ op_begin

    ++code_curr;

    // drop   ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(concrete_operand_count() == 0uz) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so drop becomes a no-op on the concrete stack.
        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"drop", 1uz); }
    }
    else
    {
        static_cast<void>(operand_stack_pop_unchecked());
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_drop(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::select:
{
    // select ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // select ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // select ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    // Stack effect: (v1 v2 i32) -> (v) where v is v1/v2 and v1,v2 must have the same type.
    // Untyped select admits the enabled numeric/vector types. Concrete references require typed select.
    // In polymorphic mode, operand-stack underflow is allowed, but concrete operands (if present) are still type-checked.

    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

    // cond (must be i32 if it exists on the concrete stack)
    bool cond_from_stack{};
    curr_operand_stack_value_type cond_type{};
    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack && !cond.is_unknown)
    {
        cond_from_stack = true;
        cond_type = cond.type;
    }

    if(cond_from_stack && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_diagnostic_value_type(cond_type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // v2
    bool v2_from_stack{};
    curr_operand_stack_value_type v2_type{};
    if(auto const v2{try_pop_concrete_operand()}; v2.from_stack && !v2.is_unknown)
    {
        v2_from_stack = true;
        v2_type = v2.type;
    }

    // v1
    bool v1_from_stack{};
    curr_operand_stack_value_type v1_type{};
    if(auto const v1{try_pop_concrete_operand()}; v1.from_stack && !v1.is_unknown)
    {
        v1_from_stack = true;
        v1_type = v1.type;
    }

    if(v1_from_stack && v2_from_stack && v1_type != v2_type) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_diagnostic_value_type(v1_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_diagnostic_value_type(v2_type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Untyped select accepts numeric/vector values, never concrete references.
    // A missing/Unknown input does not exempt the other, known input from this rule.
    auto const is_select_value_type{[](curr_operand_stack_value_type type) constexpr noexcept
        { return type == curr_operand_stack_value_type::i32 || type == curr_operand_stack_value_type::i64 ||
                 type == curr_operand_stack_value_type::f32 || type == curr_operand_stack_value_type::f64 ||
                 type == curr_operand_stack_value_type::v128; }};
    auto const selected_type{v1_from_stack ? v1_type : v2_type};
    if((v1_from_stack || v2_from_stack) && !is_select_value_type(selected_type)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_diagnostic_value_type(selected_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_diagnostic_value_type(selected_type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // `select` consumes three inputs and materializes one fresh result register.
    if(v1_from_stack) { operand_stack_push(v1_type); }
    else if(v2_from_stack) { operand_stack_push(v2_type); }
    // select produces a slot even when both inputs are Unknown. The slot is validation-only
    // in unreachable code: do not drop it or invent a concrete type for later consumers.
    else { operand_stack_push(curr_operand_stack_value_type{}, true); }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_select(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::local_get:
{
    // local.get ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.get ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.get local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};

    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.get local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.get local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    // check the local_index is valid
    if(local_index >= all_local_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& curr_local_register{local_virtual_register_from_index(local_index)};
    if(!initialized_locals.is_initialized(local_index, local_initially_initialized(local_index))) [[unlikely]]
    {
        // local.get local_index ...
        // [dispatch-checked opcode and decoded immediate] | code_end
        // ^^ op_begin -> err.err_curr: retain the checked opcode; no input read or cursor movement.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch = {
            .op_code_name = u8"local.get (unset non-null local)",
            .expected_type = to_wasm1_diagnostic_value_type(curr_local_register.type),
            .actual_type = to_wasm1_diagnostic_value_type(curr_local_register.type)};
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // `local.get` materializes a fresh transient operand register from the stable
    // local register slot, even when the surrounding type state is polymorphic.
    // Core 3 still requires initialization when the current operand stack is unreachable.
    operand_stack_push(curr_local_register.type, false, curr_local_register.exact_function_type_index,
        curr_local_register.core_type, curr_local_register.has_core_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_get(llvm_jit_emit_state, local_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::local_set:
{
    // local.set ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.set ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.set local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};

    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.set local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.set local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    // check the local_index is valid
    if(local_index >= all_local_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    // `local.set` consumes one transient operand register and conceptually copies
    // that value into the stable register assigned to the target local.
    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so local.set becomes a no-op on the concrete stack.
        report_operand_stack_underflow(op_begin, u8"local.set", 1uz);
    }
    else if(auto const value{try_pop_concrete_operand()}; value.from_stack && !value.is_unknown)
    {
        auto const& declaration{local_virtual_register_from_index(local_index)};
        if(!core3_value_matches(value, curr_local_type, declaration.core_type, declaration.has_core_type)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_local_type);
            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Record a validated assignment even in unreachable code, matching Core 3 set_local.
    initialized_locals.initialize(local_index, local_initially_initialized(local_index));

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_set(llvm_jit_emit_state, local_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::local_tee:
{
    // local.tee ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.tee ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.tee local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 local_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};

    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.tee local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.tee local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    // check the local_index is valid
    if(local_index >= all_local_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    // `local.tee` is the register-form equivalent of "copy into the local register
    // slot, but keep the operand register live on the stack for subsequent uses".
    if(concrete_operand_count() == 0uz) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed.
        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"local.tee", 1uz); }
        else
        {
            // In polymorphic mode, `local.tee` still produces a value of the local's type.
            // pop t (dismiss), push t (here)
            operand_stack_push(curr_local_type, false,
                local_virtual_register_from_index(local_index).exact_function_type_index,
                local_virtual_register_from_index(local_index).core_type,
                local_virtual_register_from_index(local_index).has_core_type);
        }
    }
    else if(auto const value{try_peek_concrete_operand()}; value.from_stack && !value.is_unknown)
    {
        auto const& declaration{local_virtual_register_from_index(local_index)};
        if(!core3_value_matches(value, curr_local_type, declaration.core_type, declaration.has_core_type)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_local_type);
            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_tee_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // local.tee refines an explicit Unknown to the declared local type (pop t; push t).
    operand_stack.back().type = curr_local_type;
    operand_stack.back().is_unknown = false;
    operand_stack.back().is_reference_bottom = false;
    // local.tee's result has the declared local type, including its Core 3 heap witness.
    operand_stack.back().exact_function_type_index =
        local_virtual_register_from_index(local_index).exact_function_type_index;
    operand_stack.back().core_type = local_virtual_register_from_index(local_index).core_type;
    operand_stack.back().has_core_type = local_virtual_register_from_index(local_index).has_core_type;

    // Record a validated assignment even in unreachable code, matching Core 3 set_local.
    initialized_locals.initialize(local_index, local_initially_initialized(local_index));

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_tee(llvm_jit_emit_state, local_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::global_get:
{
    // global.get ...
    // [  safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // global.get ...
    // [ safe   ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // global.get global_index ...
    // [ safe   ] unsafe (could be the section_end)
    //            ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(global_index))};

    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
    }

    // global.get global_index ...
    // [     safe            ] unsafe (could be the section_end)
    //            ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

    // global.get global_index ...
    // [      safe           ] unsafe (could be the section_end)
    //                         ^^ code_curr

    // check the global_index is valid
    if(global_index >= all_global_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    curr_operand_stack_value_type curr_global_type{};
    ::std::size_t curr_global_witness{(::std::numeric_limits<::std::size_t>::max)()};
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type curr_global_core_type{};
    bool curr_global_has_core_type{};
    if(global_index < imported_global_count)
    {
        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        auto const& imported_global{imported_global_ptr->imports.storage.global};
        curr_global_type = imported_global.type;
        if(imported_global.has_core_type)
        {
            curr_global_witness = exact_function_type_witness(imported_global.core_type);
            curr_global_core_type = imported_global.core_type;
            curr_global_has_core_type = true;
        }
    }
    else
    {
        auto const local_global_index{global_index - imported_global_count};
        auto const& local_global{globalsec.local_globals.index_unchecked(local_global_index).global};
        curr_global_type = local_global.type;
        if(local_global.has_core_type)
        {
            curr_global_witness = exact_function_type_witness(local_global.core_type);
            curr_global_core_type = local_global.core_type;
            curr_global_has_core_type = true;
        }
    }

    // global.get always pushes one value of the global's type (even in polymorphic mode)
    operand_stack_push(curr_global_type, false, curr_global_witness,
        curr_global_core_type, curr_global_has_core_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_global_get(llvm_jit_emit_state, global_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::global_set:
{
    // global.set ...
    // [  safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // global.set ...
    // [ safe   ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // global.set global_index ...
    // [ safe   ] unsafe (could be the section_end)
    //            ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 global_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(global_index))};

    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
    }

    // global.set global_index ...
    // [     safe            ] unsafe (could be the section_end)
    //            ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

    // global.set global_index ...
    // [      safe           ] unsafe (could be the section_end)
    //                         ^^ code_curr

    // Validate global_index range (imports + local globals)
    if(global_index >= all_global_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve the global's value type and mutability for global.set
    curr_operand_stack_value_type curr_global_type{};
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type curr_global_core_type{};
    bool curr_global_has_core_type{};

    bool curr_global_mutable{};
    if(global_index < imported_global_count)
    {
        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        auto const& imported_global{imported_global_ptr->imports.storage.global};
        curr_global_type = imported_global.type;
        curr_global_mutable = imported_global.is_mutable;
        curr_global_core_type = imported_global.core_type;
        curr_global_has_core_type = imported_global.has_core_type;
    }
    else
    {
        auto const local_global_index{global_index - imported_global_count};
        auto const& local_global{globalsec.local_globals.index_unchecked(local_global_index).global};
        curr_global_type = local_global.type;
        curr_global_mutable = local_global.is_mutable;
        curr_global_core_type = local_global.core_type;
        curr_global_has_core_type = local_global.has_core_type;
    }

    // global.set requires the target global to be mutable (immutable globals cannot be written)
    if(!curr_global_mutable) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.immutable_global_set.global_index = global_index;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::immutable_global_set;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: (value) -> () where value must match global's value type
    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so global.set becomes a no-op on the concrete stack.
        report_operand_stack_underflow(op_begin, u8"global.set", 1uz);
    }
    else if(auto const value{try_pop_concrete_operand()}; value.from_stack && !value.is_unknown)
    {
        if(!core3_value_matches(value, curr_global_type,
            curr_global_core_type, curr_global_has_core_type)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.global_variable_type_mismatch.global_index = global_index;
            err.err_selectable.global_variable_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_global_type);
            err.err_selectable.global_variable_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::global_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_global_set(llvm_jit_emit_state, global_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
