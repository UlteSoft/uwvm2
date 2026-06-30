// WebAssembly 1.1 instruction support for the UWVM interpreter translator.
// Feature switches are consumed here, during translation, so runtime opfuncs stay branch-free.

case opcode_byte(wasm1p1_code::table_get):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::table_get),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.get")};
    check_table_index(op_begin, table_index);
    validate_i32_operands(op_begin, u8"table.get", 1uz);
    auto const table_type{get_table_value_type(table_index)};
    operand_stack_push(table_type);

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const table_ptr{resolve_runtime_table(table_index)};
    stacktop_flush_all_to_operand_stack(bytecode);
    emit_opfunc_to(bytecode, translate::get_uwvmint_table_get_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, table_ptr);
    stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, table_type);
    break;
}
case opcode_byte(wasm1p1_code::table_set):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::table_set),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.set")};
    check_table_index(op_begin, table_index);
    auto const table_type{get_table_value_type(table_index)};

    if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.set", 2uz); }

    auto const value{try_pop_concrete_operand()};
    if(!operand_type_matches(value, table_type)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.set";
        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const index{try_pop_concrete_operand()};
    if(!operand_type_matches(index, curr_operand_stack_value_type::i32)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.set";
        err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
        err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(index.type);
        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const table_ptr{resolve_runtime_table(table_index)};
    auto const module_ptr{::std::addressof(curr_module)};
    stacktop_flush_all_to_operand_stack(bytecode);
    emit_opfunc_to(bytecode, translate::get_uwvmint_table_set_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, table_ptr);
    emit_imm_to(bytecode, module_ptr);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case opcode_byte(wasm1p1_code::select_t):
{
    auto const op_begin{code_curr};
    ++code_curr;

    auto const result_type_count{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"select.result_types")};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const emit_select_for_type{[&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
                                    {
                                        switch(vt)
                                        {
                                            case curr_operand_stack_value_type::i32:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_i32_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                 interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::i64:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_i64_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                 interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::f32:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_f32_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                 interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::f64:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_f64_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                 interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::v128:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_v128_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                  interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::funcref:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_funcref_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                    interpreter_tuple));
                                                break;
                                            case curr_operand_stack_value_type::externref:
                                                emit_opfunc_to(bytecode,
                                                               translate::get_uwvmint_select_externref_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                                      interpreter_tuple));
                                                break;
                                            [[unlikely]] default:
                                                ::fast_io::fast_terminate();
                                        }
                                    }};

    if(result_type_count == 0u)
    {
        if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

        auto const cond{try_pop_concrete_operand()};
        if(!operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
            err.err_code = code_validation_error_code::select_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        auto const v2{try_pop_concrete_operand()};
        auto const v1{try_peek_concrete_operand()};

        if(v1.from_stack && v2.from_stack && !v1.is_unknown && !v2.is_unknown && v1.type != v2.type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(v1.type);
            err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2.type);
            err.err_code = code_validation_error_code::select_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        bool const have_known_select_value_type{(v1.from_stack && !v1.is_unknown) || (v2.from_stack && !v2.is_unknown)};
        auto const select_value_type{(v1.from_stack && !v1.is_unknown) ? v1.type : v2.type};
        if(have_known_select_value_type && !is_untyped_select_value_type(select_value_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(select_value_type);
            err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(select_value_type);
            err.err_code = code_validation_error_code::select_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        if(v1.from_stack && !v1.is_unknown)
        {
            emit_select_for_type(v1.type);
            stacktop_after_pop_n_if_reachable(bytecode, 2uz);
        }
        else if(!v1.from_stack)
        {
            if(v2.from_stack)
            {
                if(v2.is_unknown) { push_unknown_operand(); }
                else { operand_stack_push(v2.type); }
            }
            else if(is_polymorphic)
            {
                push_unknown_operand();
            }
        }

        break;
    }

    if(result_type_count != 1u) [[unlikely]] { fail_invalid_immediate(op_begin, u8"select.result_types"); }

    auto const result_type_byte{read_u8_immediate(code_curr, code_end, op_begin, u8"select.result_type")};
    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
    ensure_wasm1p1_value_type_enabled(op_begin, result_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);

    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

    auto const cond{try_pop_concrete_operand()};
    if(!operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
        err.err_code = code_validation_error_code::select_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const v2{try_pop_concrete_operand()};
    if(!operand_type_matches(v2, result_type)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(result_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2.type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const v1{try_peek_concrete_operand()};
    if(!operand_type_matches(v1, result_type)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(result_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v1.type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(v1.from_stack)
    {
        if(!v1.is_unknown) { emit_select_for_type(result_type); }
        operand_stack.back_unchecked() = {result_type};
        stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    }
    else
    {
        operand_stack_push(result_type);
    }

    break;
}
case opcode_byte(wasm1p1_code::i32_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
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
case opcode_byte(wasm1p1_code::i32_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
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
case opcode_byte(wasm1p1_code::i64_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
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
case opcode_byte(wasm1p1_code::i64_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
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
case opcode_byte(wasm1p1_code::i64_extend32_s):
{
    auto const op_begin{code_curr};
    if(!wasm1p1_para.enable_sign_extension) [[unlikely]]
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
case opcode_byte(wasm1p1_code::ref_null):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
    }

    auto const rt_byte{read_u8_immediate(code_curr, code_end, op_begin, u8"ref.null")};
    auto const rt{static_cast<::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type>(rt_byte)};
    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
    if(rt != reference_type::funcref && rt != reference_type::externref) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = rt_byte;
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const vt{static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(rt))};
    ensure_wasm1p1_value_type_enabled(op_begin, vt, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    operand_stack_push(vt);

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    stacktop_prepare_push1_if_reachable(bytecode, vt);
    if(vt == curr_operand_stack_value_type::funcref)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_ref_null_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_ref_null_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
    }
    stacktop_commit_push1_typed_if_reachable(vt);
    break;
}
case opcode_byte(wasm1p1_code::ref_is_null):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_is_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    }

    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }

    auto const ref_value{try_pop_concrete_operand()};
    if(ref_value.from_stack && !ref_value.is_unknown && !is_reference_value_type(ref_value.type)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(ref_value.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    operand_stack_push(curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32)) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
    if(!ref_value.from_stack || ref_value.type == curr_operand_stack_value_type::funcref)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_ref_is_null_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
    }
    stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    break;
}
case opcode_byte(wasm1p1_code::ref_func):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_reference_types) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::ref_func),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
    }

    auto const func_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"ref.func")};
    check_ref_func_index(op_begin, func_index);
    operand_stack_push(curr_operand_stack_value_type::funcref);

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::funcref);
    emit_opfunc_to(bytecode, translate::get_uwvmint_ref_func_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, ::std::addressof(curr_module));
    emit_imm_to(bytecode, func_index);
    stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::funcref);
    break;
}
case opcode_byte(wasm1p1_code::simd_prefix):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm1p1_para.enable_simd) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::simd_prefix),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
    }

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"simd")};
    auto const simd_code{static_cast<wasm1p1_simd_code>(subopcode)};

    switch(simd_code)
    {
        case wasm1p1_simd_code::v128_const:
        {
            wasm_v128_t imm{};
            if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(imm)) [[unlikely]] { fail_invalid_immediate(op_begin, u8"v128.const"); }
            ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));
            code_curr += sizeof(imm);

            operand_stack_push(curr_operand_stack_value_type::v128);
            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::v128);
            emit_opfunc_to(bytecode, translate::get_uwvmint_v128_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, imm);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::v128);
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    break;
}
case opcode_byte(wasm1p1_code::numeric_prefix):
{
    auto const op_begin{code_curr};
    ++code_curr;

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"numeric_prefix")};
    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

    auto const emit_trunc_sat{
        [&](::uwvm2::utils::container::u8string_view op_name,
            curr_operand_stack_value_type in_type,
            curr_operand_stack_value_type out_type,
            auto fptr) constexpr UWVM_THROWS
        {
            if(!wasm1p1_para.enable_nontrapping_float_to_int) [[unlikely]]
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
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"memory.init.dataidx")};
            check_data_index(op_begin, data_index);
            auto const memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.init.memidx")};
            check_memory_index_zero(op_begin, memory_index, u8"memory.init");
            validate_i32_operands(op_begin, u8"memory.init", 3uz);
            ensure_memory0_resolved();
            auto data_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t*>(
                ::std::addressof(curr_module.local_defined_data_vec_storage.index_unchecked(data_index)))};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_memory_init_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, data_ptr);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::data_drop:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"data.drop")};
            check_data_index(op_begin, data_index);
            auto data_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t*>(
                ::std::addressof(curr_module.local_defined_data_vec_storage.index_unchecked(data_index)))};
            emit_opfunc_to(bytecode, translate::get_uwvmint_data_drop_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, data_ptr);
            break;
        }
        case wasm1p1_numeric_code::memory_copy:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const dst_memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.copy.dst")};
            check_memory_index_zero(op_begin, dst_memory_index, u8"memory.copy");
            auto const src_memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.copy.src")};
            check_memory_index_zero(op_begin, src_memory_index, u8"memory.copy");
            validate_i32_operands(op_begin, u8"memory.copy", 3uz);
            ensure_memory0_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_memory_copy_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::memory_fill:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.fill")};
            check_memory_index_zero(op_begin, memory_index, u8"memory.fill");
            validate_i32_operands(op_begin, u8"memory.fill", 3uz);
            ensure_memory0_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_memory_fill_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::table_init:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
            }
            auto const element_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.init.elemidx")};
            check_element_index(op_begin, element_index);
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.init.tableidx")};
            check_table_index(op_begin, table_index);

            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(
                    curr_module.local_defined_element_vec_storage.index_unchecked(element_index).element_type_ptr->storage.segment.reftype))};
            auto const table_value_type{get_table_value_type(table_index)};
            if(element_value_type != table_value_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_value_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(element_value_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            validate_i32_operands(op_begin, u8"table.init", 3uz);
            auto const table_ptr{resolve_runtime_table(table_index)};
            auto element_ptr{const_cast<::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t*>(
                ::std::addressof(curr_module.local_defined_element_vec_storage.index_unchecked(element_index)))};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_table_init_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, table_ptr);
            emit_imm_to(bytecode, element_ptr);
            emit_imm_to(bytecode, ::std::addressof(curr_module));
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::elem_drop:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
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
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const dst_table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.dst")};
            check_table_index(op_begin, dst_table_index);
            auto const src_table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.src")};
            check_table_index(op_begin, src_table_index);

            auto const dst_type{get_table_value_type(dst_table_index)};
            auto const src_type{get_table_value_type(src_table_index)};
            if(dst_type != src_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(dst_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(src_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            validate_i32_operands(op_begin, u8"table.copy", 3uz);
            auto const dst_table_ptr{resolve_runtime_table(dst_table_index)};
            auto const src_table_ptr{resolve_runtime_table(src_table_index)};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_table_copy_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, dst_table_ptr);
            emit_imm_to(bytecode, src_table_ptr);
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        case wasm1p1_numeric_code::table_grow:
        {
            if(!wasm1p1_para.enable_reference_types) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.grow")};
            check_table_index(op_begin, table_index);
            auto const table_type{get_table_value_type(table_index)};

            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.grow", 2uz); }

            auto const delta{try_pop_concrete_operand()};
            if(!operand_type_matches(delta, curr_operand_stack_value_type::i32)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(delta.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const value{try_pop_concrete_operand()};
            if(!operand_type_matches(value, table_type)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            operand_stack_push(curr_operand_stack_value_type::i32);
            auto const table_ptr{resolve_runtime_table(table_index)};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_table_grow_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, table_ptr);
            emit_imm_to(bytecode, ::std::addressof(curr_module));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
            break;
        }
        case wasm1p1_numeric_code::table_size:
        {
            if(!wasm1p1_para.enable_reference_types) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.size")};
            check_table_index(op_begin, table_index);
            operand_stack_push(curr_operand_stack_value_type::i32);
            auto const table_ptr{resolve_runtime_table(table_index)};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_table_size_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, table_ptr);
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 0uz, curr_operand_stack_value_type::i32);
            break;
        }
        case wasm1p1_numeric_code::table_fill:
        {
            if(!wasm1p1_para.enable_bulk_memory) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const table_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"table.fill")};
            check_table_index(op_begin, table_index);
            auto const table_type{get_table_value_type(table_index)};

            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.fill", 3uz); }

            auto const len{try_pop_concrete_operand()};
            if(!operand_type_matches(len, curr_operand_stack_value_type::i32)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(len.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const value{try_pop_concrete_operand()};
            if(!operand_type_matches(value, table_type)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const index{try_pop_concrete_operand()};
            if(!operand_type_matches(index, curr_operand_stack_value_type::i32)) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_value_type(index.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            auto const table_ptr{resolve_runtime_table(table_index)};
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_table_fill_funcref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, table_ptr);
            emit_imm_to(bytecode, ::std::addressof(curr_module));
            stacktop_after_pop_n_if_reachable(bytecode, 3uz);
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    break;
}
