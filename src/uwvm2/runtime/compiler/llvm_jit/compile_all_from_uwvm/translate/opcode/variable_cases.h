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

    if(operand_stack.empty()) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so drop becomes a no-op on the concrete stack.
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"drop";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
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
    // In polymorphic mode, operand-stack underflow is allowed, but concrete operands (if present) are still type-checked.

    if(!is_polymorphic && operand_stack.size() < 3uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.operand_stack_underflow.op_code_name = u8"select";
        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
        err.err_selectable.operand_stack_underflow.stack_size_required = 3uz;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // cond (must be i32 if it exists on the concrete stack)
    bool cond_from_stack{};
    curr_operand_stack_value_type cond_type{};
    if(!operand_stack.empty())
    {
        auto const cond{operand_stack_pop_unchecked()};
        cond_from_stack = true;
        cond_type = cond.type;
    }

    if(cond_from_stack && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_cond_type_not_i32.cond_type = cond_type;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // v2
    bool v2_from_stack{};
    curr_operand_stack_value_type v2_type{};
    if(!operand_stack.empty())
    {
        auto const v2{operand_stack_pop_unchecked()};
        v2_from_stack = true;
        v2_type = v2.type;
    }

    // v1
    bool v1_from_stack{};
    curr_operand_stack_value_type v1_type{};
    if(!operand_stack.empty())
    {
        auto const v1{operand_stack_pop_unchecked()};
        v1_from_stack = true;
        v1_type = v1.type;
    }

    if(v1_from_stack && v2_from_stack && v1_type != v2_type) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = v1_type;
        err.err_selectable.select_type_mismatch.type_v2 = v2_type;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // `select` consumes its concrete inputs and materializes a fresh result register.
    if(v1_from_stack) { operand_stack_push(v1_type); }
    else if(v2_from_stack) { operand_stack_push(v2_type); }

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
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // `local.get` materializes a fresh transient operand register from the stable
    // local register slot, even when the surrounding type state is polymorphic.
    operand_stack_push(local_type_from_index(local_index));

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_get(llvm_jit_emit_state, local_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
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
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    // `local.set` consumes one transient operand register and conceptually copies
    // that value into the stable register assigned to the target local.
    if(operand_stack.empty()) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so local.set becomes a no-op on the concrete stack.
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.set";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    else
    {
        auto const value{operand_stack.back()};
        if(value.type != curr_local_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
            err.err_selectable.local_variable_type_mismatch.actual_type = value.type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        static_cast<void>(operand_stack_pop_unchecked());
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_set(llvm_jit_emit_state, local_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
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
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    // `local.tee` is the register-form equivalent of "copy into the local register
    // slot, but keep the operand register live on the stack for subsequent uses".
    if(operand_stack.empty()) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed.
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.tee";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        else
        {
            // In polymorphic mode, `local.tee` still produces a value of the local's type.
            // pop t (dismiss), push t (here)
            operand_stack_push(curr_local_type);
        }
    }
    else
    {
        auto const value{operand_stack.back()};
        if(value.type != curr_local_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
            err.err_selectable.local_variable_type_mismatch.actual_type = value.type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::local_tee_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_local_tee(llvm_jit_emit_state, local_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
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
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    curr_operand_stack_value_type curr_global_type{};
    if(global_index < imported_global_count)
    {
        auto const imported_global_ptr{imported_globals.index_unchecked(global_index)};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        curr_global_type = imported_global_ptr->imports.storage.global.type;
    }
    else
    {
        auto const local_global_index{global_index - imported_global_count};
        curr_global_type = globalsec.local_globals.index_unchecked(local_global_index).global.type;
    }

    // global.get always pushes one value of the global's type (even in polymorphic mode)
    operand_stack_push(curr_global_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_global_get(llvm_jit_emit_state, global_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
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
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve the global's value type and mutability for global.set
    curr_operand_stack_value_type curr_global_type{};

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
    }
    else
    {
        auto const local_global_index{global_index - imported_global_count};
        auto const& local_global{globalsec.local_globals.index_unchecked(local_global_index).global};
        curr_global_type = local_global.type;
        curr_global_mutable = local_global.is_mutable;
    }

    // global.set requires the target global to be mutable (immutable globals cannot be written)
    if(!curr_global_mutable) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.immutable_global_set.global_index = global_index;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::immutable_global_set;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Stack effect: (value) -> () where value must match global's value type
    if(operand_stack.empty()) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so global.set becomes a no-op on the concrete stack.
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"global.set";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    else
    {
        auto const value{operand_stack_pop_unchecked()};

        if(value.type != curr_global_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.global_variable_type_mismatch.global_index = global_index;
            err.err_selectable.global_variable_type_mismatch.expected_type = curr_global_type;
            err.err_selectable.global_variable_type_mismatch.actual_type = value.type;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::global_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_global_set(llvm_jit_emit_state, global_index)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
