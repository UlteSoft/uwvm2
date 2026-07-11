    // WebAssembly 1.1 instruction support for the UWVM interpreter translator.
    // Feature switches are consumed here, during translation, so runtime opfuncs stay branch-free.

case static_cast<wasm_byte>(wasm1p1_code::table_get):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_reference_types) [[unlikely]]
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

case static_cast<wasm_byte>(wasm1p1_code::table_set):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_reference_types) [[unlikely]]
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

case static_cast<wasm_byte>(wasm1p1_code::select_t):
{
    auto const op_begin{code_curr};
    ++code_curr;

    auto const result_type_count{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"select.result_types")};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const emit_select_for_type{
        [&](curr_operand_stack_value_type vt) constexpr UWVM_THROWS
        {
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
                else
                {
                    operand_stack_push(v2.type);
                }
            }
            else if(is_polymorphic) { push_unknown_operand(); }
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

case static_cast<wasm_byte>(wasm1p1_code::i32_extend8_s):
{
    auto const op_begin{code_curr};
    if(wasm1p1_para.disable_sign_extension) [[unlikely]]
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
    if(wasm1p1_para.disable_sign_extension) [[unlikely]]
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
    if(wasm1p1_para.disable_sign_extension) [[unlikely]]
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
    if(wasm1p1_para.disable_sign_extension) [[unlikely]]
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
    if(wasm1p1_para.disable_sign_extension) [[unlikely]]
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
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_reference_types) [[unlikely]]
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
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_reference_types) [[unlikely]]
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
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    if(!ref_value.from_stack || ref_value.type == curr_operand_stack_value_type::funcref)
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

case static_cast<wasm_byte>(wasm1p1_code::ref_func):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_reference_types) [[unlikely]]
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

case static_cast<wasm_byte>(wasm1p1_code::simd_prefix):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(wasm1p1_para.disable_simd) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_u32(wasm1p1_code::simd_prefix),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
    }

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"simd")};
    auto const simd_code{static_cast<wasm1p1_simd_code>(subopcode)};

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    namespace simd_opt = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1p1_simd_details;

    static constexpr value_type_enum v128_v128_operands[2u]{value_type_enum::v128, value_type_enum::v128};
    static constexpr value_type_enum i32_v128_operands[2u]{value_type_enum::i32, value_type_enum::v128};
    static constexpr value_type_enum v128_i32_operands[2u]{value_type_enum::v128, value_type_enum::i32};
    static constexpr value_type_enum v128_i64_operands[2u]{value_type_enum::v128, value_type_enum::i64};
    static constexpr value_type_enum v128_f32_operands[2u]{value_type_enum::v128, value_type_enum::f32};
    static constexpr value_type_enum v128_f64_operands[2u]{value_type_enum::v128, value_type_enum::f64};
    static constexpr value_type_enum v128_v128_v128_operands[3u]{value_type_enum::v128, value_type_enum::v128, value_type_enum::v128};

    auto const read_memarg_immediate{[&](code_validation_error_code ec) constexpr UWVM_THROWS -> wasm_u32
                                     {
                                         using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                         wasm_u32 value{};  // init
                                         auto const [next, perr]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                                          reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                          ::fast_io::mnp::leb128_get(value))};
                                         if(perr != ::fast_io::parse_code::ok) [[unlikely]]
                                         {
                                             err.err_curr = op_begin;
                                             err.err_code = ec;
                                             ::uwvm2::parser::wasm::base::throw_wasm_parse_code(perr);
                                         }

                                         code_curr = reinterpret_cast<::std::byte const*>(next);
                                         return value;
                                     }};

    auto const validate_simd_memarg{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> wasm_u32
                                    {
                                        auto const align{read_memarg_immediate(code_validation_error_code::invalid_memarg_align)};
                                        auto const offset{read_memarg_immediate(code_validation_error_code::invalid_memarg_offset)};

                                        if(all_memory_count == 0u) [[unlikely]]
                                        {
                                            err.err_curr = op_begin;
                                            err.err_selectable.no_memory.op_code_name = op_name;
                                            err.err_selectable.no_memory.align = align;
                                            err.err_selectable.no_memory.offset = offset;
                                            err.err_code = code_validation_error_code::no_memory;
                                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                        }

                                        if(align > max_align) [[unlikely]]
                                        {
                                            err.err_curr = op_begin;
                                            err.err_selectable.illegal_memarg_alignment.op_code_name = op_name;
                                            err.err_selectable.illegal_memarg_alignment.align = align;
                                            err.err_selectable.illegal_memarg_alignment.max_align = max_align;
                                            err.err_code = code_validation_error_code::illegal_memarg_alignment;
                                            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                        }

                                        return offset;
                                    }};

    auto const validate_simd_load{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> wasm_u32
                                  {
                                      auto const offset{validate_simd_memarg(op_name, max_align)};
                                      validate_i32_operands(op_begin, op_name, 1uz);
                                      operand_stack_push(curr_operand_stack_value_type::v128);
                                      return offset;
                                  }};

    auto const validate_simd_store{[&](::uwvm2::utils::container::u8string_view op_name, wasm_u32 max_align) constexpr UWVM_THROWS -> wasm_u32
                                   {
                                       auto const offset{validate_simd_memarg(op_name, max_align)};
                                       pop_expected_operands(op_begin, op_name, {i32_v128_operands, i32_v128_operands + 2u});
                                       return offset;
                                   }};

    auto const emit_v128_load{[&](wasm_u32 offset) constexpr UWVM_THROWS
                              {
                                  ensure_memory0_resolved();
                                  stacktop_flush_all_to_operand_stack(bytecode);
                                  emit_opfunc_to(bytecode,
                                                 translate::get_uwvmint_simd_v128_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                  emit_imm_to(bytecode, resolved_memory0.memory_p);
                                  emit_imm_to(bytecode, offset);
                                  stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::v128);
                              }};

    auto const emit_v128_store{[&](wasm_u32 offset) constexpr UWVM_THROWS
                               {
                                   ensure_memory0_resolved();
                                   stacktop_flush_all_to_operand_stack(bytecode);
                                   emit_opfunc_to(bytecode,
                                                  translate::get_uwvmint_simd_v128_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                   emit_imm_to(bytecode, resolved_memory0.memory_p);
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
        [&]<simd_opt::simd_code Op>(wasm_u32 offset, wasm_byte lane = wasm_byte{}) constexpr UWVM_THROWS
        {
            ensure_memory0_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_mem_load_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory0.memory_p);
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
        [&]<simd_opt::simd_code Op>(wasm_u32 offset, wasm_byte lane = wasm_byte{}) constexpr UWVM_THROWS
        {
            ensure_memory0_resolved();
            stacktop_flush_all_to_operand_stack(bytecode);
            emit_opfunc_to(bytecode, translate::get_uwvmint_simd_full_mem_store_fptr_from_tuple<CompileOption, Op>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, resolved_memory0.memory_p);
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
            code_curr += sizeof(imm);

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
    auto const op_begin{code_curr};
    ++code_curr;

    auto const subopcode{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, u8"numeric_prefix")};
    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

    auto const emit_trunc_sat{[&](::uwvm2::utils::container::u8string_view op_name,
                                  curr_operand_stack_value_type in_type,
                                  curr_operand_stack_value_type out_type,
                                  auto fptr) constexpr UWVM_THROWS
                              {
                                  if(wasm1p1_para.disable_nontrapping_float_to_int) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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

            auto const element_value_type{static_cast<curr_operand_stack_value_type>(::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
            if(wasm1p1_para.disable_reference_types) [[unlikely]]
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
            if(wasm1p1_para.disable_reference_types) [[unlikely]]
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
            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
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
