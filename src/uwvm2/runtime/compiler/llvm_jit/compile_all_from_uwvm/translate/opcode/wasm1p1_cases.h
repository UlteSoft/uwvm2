    // WebAssembly 1.1 opcode validation/emission for the LLVM JIT path. Keep feature gates synchronized with
    // validation/standard/wasm2/validator.h. The typed ABI carries funcref/externref and v128 as exact-width opaque
    // integers and represents multi-value signatures as Wasm-order literal structs.

case static_cast<wasm1_code>(wasm1p1_code::select_t):
{
    auto const op_begin{code_curr};
    ++code_curr;

    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::select_t),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    auto const result_type_count{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"select.result_types")};

    // select_t result_type_count result_type ...
    // [           safe         ] unsafe (could be the section_end)
    //                            ^^ code_curr

    // The typed-select opcode (0x1c) encodes a vector whose length is exactly one.
    if(result_type_count != 1u) [[unlikely]] { fail_invalid_immediate(op_begin, u8"select.result_types"); }

    auto const validate_select_condition{[&](concrete_operand_t cond) constexpr UWVM_THROWS
                                         {
                                             if(cond.from_stack && !cond.is_unknown && cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
                                             {
                                                 err.err_curr = op_begin;
                                                 err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_diagnostic_value_type(cond.type);
                                                 err.err_code = code_validation_error_code::select_cond_type_not_i32;
                                                 ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                             }
                                         }};

    auto const result_type_byte{read_u8_immediate(code_curr, code_end, op_begin, u8"select.result_type")};

    // select_t result_type_count result_type ...
    // [                 safe               ] unsafe (could be the section_end)
    //                                        ^^ code_curr

    // Field commits are transactional: a count-LEB decode failure leaves the cursor after the opcode; a decoded-count
    // arity rejection or result-type decode failure leaves it after the count; a type-policy rejection follows the type byte.
    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
    ensure_wasm1p1_value_type_enabled(op_begin, result_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);

    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

    auto const cond{try_pop_concrete_operand()};
    validate_select_condition(cond);

    auto const v2{try_pop_concrete_operand()};
    if(v2.from_stack && !v2.is_unknown && v2.type != result_type) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_diagnostic_value_type(result_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_diagnostic_value_type(v2.type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const v1{try_pop_concrete_operand()};
    if(v1.from_stack && !v1.is_unknown && v1.type != result_type) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_diagnostic_value_type(result_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_diagnostic_value_type(v1.type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    operand_stack_push(result_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(is_runtime_wasm_value_type_llvm_storage_supported(static_cast<runtime_operand_stack_value_type>(result_type)))
        {
            if(!try_emit_runtime_local_func_llvm_jit_select(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
        }
        else
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::table_get):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_byte(wasm1p1_code::table_get),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    auto const table_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.get")};
    check_table_index(op_begin, table_index, opcode_byte(wasm1p1_code::table_get));
    if(!is_polymorphic && concrete_operand_count() < 1uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.get", 1uz); }
    auto const index{try_pop_concrete_operand()};
    if(index.from_stack && !index.is_unknown && index.type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.get";
        err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32);
        err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(index.type);
        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    operand_stack_push(get_table_value_type(table_index));

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::table_set):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_byte(wasm1p1_code::table_set),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    auto const table_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.set")};
    check_table_index(op_begin, table_index, opcode_byte(wasm1p1_code::table_set));
    auto const table_type{get_table_value_type(table_index)};
    if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.set", 2uz); }
    auto const value{try_pop_concrete_operand()};
    if(value.from_stack && !value.is_unknown && value.type != table_type) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.set";
        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const index{try_pop_concrete_operand()};
    if(index.from_stack && !index.is_unknown && index.type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.set";
        err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32);
        err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(index.type);
        err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_null):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
    }
    auto const reference_type_byte{read_u8_immediate(code_curr, code_end, op_begin, u8"ref.null")};
    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
    auto const reference_type_value{static_cast<reference_type>(reference_type_byte)};
    if(reference_type_value != reference_type::funcref && reference_type_value != reference_type::externref) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = reference_type_byte;
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const value_type{static_cast<curr_operand_stack_value_type>(
        ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(reference_type_value))};
    ensure_wasm1p1_value_type_enabled(op_begin, value_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    operand_stack_push(value_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_is_null):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_is_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    }
    if(!is_polymorphic && concrete_operand_count() < 1uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }
    auto const reference{try_pop_concrete_operand()};
    if(reference.from_stack && !reference.is_unknown && reference.type != curr_operand_stack_value_type::funcref &&
       reference.type != curr_operand_stack_value_type::externref) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    operand_stack_push(curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_func):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_func),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
    }
    auto const function_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"ref.func")};
    check_ref_func_index(op_begin, function_index);
    operand_stack_push(curr_operand_stack_value_type::funcref);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::simd_prefix):
{
    auto const op_begin{code_curr};
    ++code_curr;
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::simd)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::simd_prefix),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_v128_const);
    }

    using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;
    namespace shared_simd = ::uwvm2::runtime::compiler::shared;
    auto const subopcode{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"simd")};

    auto const fail_simd_operand_type{[&](curr_operand_stack_value_type expected,
                                          curr_operand_stack_value_type actual) constexpr UWVM_THROWS
                                      {
                                          err.err_curr = op_begin;
                                          err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"simd";
                                          err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(expected);
                                          err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual);
                                          err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                          ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                      }};
    auto const pop_simd_operand{[&](curr_operand_stack_value_type expected) constexpr UWVM_THROWS
                                {
                                    auto const operand{try_pop_concrete_operand()};
                                    if(operand.from_stack && !operand.is_unknown && operand.type != expected) [[unlikely]]
                                    {
                                        fail_simd_operand_type(expected, operand.type);
                                    }
                                }};
    auto const require_simd_operands{[&](::std::size_t count) constexpr UWVM_THROWS
                                     {
                                         if(!is_polymorphic && concrete_operand_count() < count) [[unlikely]]
                                         {
                                             report_operand_stack_underflow(op_begin, u8"simd", count);
                                         }
                                     }};
    auto const scalar_value_type{
        []<shared_simd::wasm1p1_simd_scalar_kind ScalarKind>() constexpr noexcept -> curr_operand_stack_value_type
        {
            if constexpr(ScalarKind == shared_simd::wasm1p1_simd_scalar_kind::i32) { return curr_operand_stack_value_type::i32; }
            else if constexpr(ScalarKind == shared_simd::wasm1p1_simd_scalar_kind::i64) { return curr_operand_stack_value_type::i64; }
            else if constexpr(ScalarKind == shared_simd::wasm1p1_simd_scalar_kind::f32) { return curr_operand_stack_value_type::f32; }
            else if constexpr(ScalarKind == shared_simd::wasm1p1_simd_scalar_kind::f64) { return curr_operand_stack_value_type::f64; }
            else { return curr_operand_stack_value_type::v128; }
        }};

    auto const valid_simd_opcode{shared_simd::visit_wasm1p1_simd_instruction(
        static_cast<wasm1p1_simd_code>(subopcode),
        [&]<shared_simd::wasm1p1_simd_details::simd_code Op,
            shared_simd::wasm1p1_simd_instruction_kind Kind,
            shared_simd::wasm1p1_simd_scalar_kind ScalarKind,
            ::std::size_t LaneCount,
            ::std::uint_least32_t MaxAlign>() constexpr UWVM_THROWS -> bool
        {
            static_cast<void>(Op);
            if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load ||
                         Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
            {
                auto const align{
                    read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"simd.memarg.align")};
                auto const offset{
                    read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"simd.memarg.offset")};
                if(all_memory_count == 0u) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.no_memory.op_code_name = u8"simd.memory";
                    err.err_selectable.no_memory.align = align;
                    err.err_selectable.no_memory.offset = offset;
                    err.err_code = code_validation_error_code::no_memory;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                if(align > MaxAlign) [[unlikely]]
                {
                    err.err_curr = op_begin;
                    err.err_selectable.illegal_memarg_alignment.op_code_name = u8"simd.memory";
                    err.err_selectable.illegal_memarg_alignment.align = align;
                    err.err_selectable.illegal_memarg_alignment.max_align = MaxAlign;
                    err.err_code = code_validation_error_code::illegal_memarg_alignment;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                if constexpr(LaneCount != 0uz)
                {
                    auto const lane{read_u8_immediate(code_curr, code_end, op_begin, u8"simd.memory.lane")};
                    if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { fail_invalid_immediate(op_begin, u8"simd.memory.lane"); }
                }
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::constant ||
                              Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
            {
                if(static_cast<::std::size_t>(code_end - code_curr) < 16uz) [[unlikely]]
                {
                    fail_invalid_immediate(op_begin, u8"simd.v128.immediate", ::fast_io::parse_code::end_of_file);
                }
                if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
                {
                    for(::std::size_t i{}; i != 16uz; ++i)
                    {
                        if(::std::to_integer<::std::uint_least8_t>(code_curr[i]) >= 32u) [[unlikely]]
                        {
                            fail_invalid_immediate(op_begin, u8"i8x16.shuffle");
                        }
                    }
                }
                code_curr += 16uz;
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::extract_lane ||
                              Kind == shared_simd::wasm1p1_simd_instruction_kind::replace_lane)
            {
                auto const lane{read_u8_immediate(code_curr, code_end, op_begin, u8"simd.lane")};
                if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { fail_invalid_immediate(op_begin, u8"simd.lane"); }
            }

            if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::constant)
            {
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::unary)
            {
                require_simd_operands(1uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::test)
            {
                require_simd_operands(1uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::i32);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::binary ||
                              Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
            {
                require_simd_operands(2uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::ternary)
            {
                require_simd_operands(3uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::shift)
            {
                require_simd_operands(2uz);
                pop_simd_operand(curr_operand_stack_value_type::i32);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::splat)
            {
                require_simd_operands(1uz);
                pop_simd_operand(scalar_value_type.template operator()<ScalarKind>());
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::extract_lane)
            {
                require_simd_operands(1uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(scalar_value_type.template operator()<ScalarKind>());
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::replace_lane)
            {
                require_simd_operands(2uz);
                pop_simd_operand(scalar_value_type.template operator()<ScalarKind>());
                pop_simd_operand(curr_operand_stack_value_type::v128);
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load)
            {
                if constexpr(LaneCount == 0uz)
                {
                    require_simd_operands(1uz);
                    pop_simd_operand(curr_operand_stack_value_type::i32);
                }
                else
                {
                    require_simd_operands(2uz);
                    pop_simd_operand(curr_operand_stack_value_type::v128);
                    pop_simd_operand(curr_operand_stack_value_type::i32);
                }
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
            {
                require_simd_operands(2uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                pop_simd_operand(curr_operand_stack_value_type::i32);
            }
            return true;
        })};

    if(!valid_simd_opcode) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
        err.err_code = code_validation_error_code::illegal_opbase;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::i32_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::i32_extend8_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    validate_numeric_unary(u8"i32.extend8_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto i8_type{::llvm::Type::getInt8Ty(ir_builder.getContext())};
                   auto i32_type{::llvm::Type::getInt32Ty(ir_builder.getContext())};
                   return ir_builder.CreateSExt(ir_builder.CreateTrunc(operand.value, i8_type), i32_type);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::i32_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::i32_extend16_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    validate_numeric_unary(u8"i32.extend16_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto i16_type{::llvm::Type::getInt16Ty(ir_builder.getContext())};
                   auto i32_type{::llvm::Type::getInt32Ty(ir_builder.getContext())};
                   return ir_builder.CreateSExt(ir_builder.CreateTrunc(operand.value, i16_type), i32_type);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::i64_extend8_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::i64_extend8_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    validate_numeric_unary(u8"i64.extend8_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto i8_type{::llvm::Type::getInt8Ty(ir_builder.getContext())};
                   auto i64_type{::llvm::Type::getInt64Ty(ir_builder.getContext())};
                   return ir_builder.CreateSExt(ir_builder.CreateTrunc(operand.value, i8_type), i64_type);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::i64_extend16_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::i64_extend16_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    validate_numeric_unary(u8"i64.extend16_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto i16_type{::llvm::Type::getInt16Ty(ir_builder.getContext())};
                   auto i64_type{::llvm::Type::getInt64Ty(ir_builder.getContext())};
                   return ir_builder.CreateSExt(ir_builder.CreateTrunc(operand.value, i16_type), i64_type);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::i64_extend32_s):
{
    auto const op_begin{code_curr};
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::sign_extension)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::i64_extend32_s),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::sign_extension,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    validate_numeric_unary(u8"i64.extend32_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto i32_type{::llvm::Type::getInt32Ty(ir_builder.getContext())};
                   auto i64_type{::llvm::Type::getInt64Ty(ir_builder.getContext())};
                   return ir_builder.CreateSExt(ir_builder.CreateTrunc(operand.value, i32_type), i64_type);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

case static_cast<wasm1_code>(wasm1p1_code::numeric_prefix):
{
    auto const op_begin{code_curr};
    ++code_curr;

    auto const subopcode{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"numeric_prefix")};
    auto const numeric_code{static_cast<wasm1p1_numeric_code>(subopcode)};

    auto const validate_nontrapping_float_to_int{
        [&](::uwvm2::utils::container::u8string_view op_name,
            curr_operand_stack_value_type operand_type,
            curr_operand_stack_value_type result_type) constexpr UWVM_THROWS
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::nontrapping_float_to_int)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::nontrapping_float_to_int,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            validate_numeric_unary_stack_effect(op_begin, op_name, operand_type, result_type);
        }};

    auto const emit_nontrapping_float_to_int{
        [&](runtime_operand_stack_value_type operand_type,
            runtime_operand_stack_value_type result_type,
            bool is_signed,
            auto min_bounds,
            auto max_bounds,
            ::std::uint_least64_t min_result,
            ::std::uint_least64_t max_result) constexpr noexcept
        {
            if(emit_llvm_jit_active)
            {
                llvm_jit_instruction_emitted_inline = true;
                if(!try_emit_runtime_local_func_llvm_jit_unary(
                       llvm_jit_emit_state,
                       operand_type,
                       result_type,
                       [=](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept -> ::llvm::Value*
                       {
                           auto dest_type{result_type == runtime_operand_stack_value_type::i32
                                              ? ::llvm::Type::getInt32Ty(ir_builder.getContext())
                                              : ::llvm::Type::getInt64Ty(ir_builder.getContext())};
                           return emit_llvm_trunc_sat_float_to_int(
                               ir_builder, dest_type, is_signed, min_bounds, max_bounds, min_result, max_result, operand.value);
                       })) [[unlikely]]
                {
                    disable_inline_llvm_jit_emission();
                }
            }
        }};

    auto const check_memory_index_zero{
        [&](validation_module_traits_t::wasm_u32 memory_index, ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
        {
            if(all_memory_count == 0u) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.no_memory.op_code_name = op_name;
                err.err_selectable.no_memory.align = 0u;
                err.err_selectable.no_memory.offset = 0u;
                err.err_code = code_validation_error_code::no_memory;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            if(memory_index != 0u) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.illegal_memory_index.memory_index = memory_index;
                err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
                err.err_code = code_validation_error_code::illegal_memory_index;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

    auto const validate_i32_operands{[&](::uwvm2::utils::container::u8string_view op_name, ::std::size_t operand_count) constexpr UWVM_THROWS
                                     {
                                         if(!is_polymorphic && concrete_operand_count() < operand_count) [[unlikely]]
                                         {
                                             report_operand_stack_underflow(op_begin, op_name, operand_count);
                                         }

                                         for(::std::size_t i{}; i != operand_count; ++i)
                                         {
                                             auto const operand{try_pop_concrete_operand()};
                                             if(operand.from_stack && !operand.is_unknown && operand.type != curr_operand_stack_value_type::i32) [[unlikely]]
                                             {
                                                 err.err_curr = op_begin;
                                                 err.err_selectable.numeric_operand_type_mismatch.op_code_name = op_name;
                                                 err.err_selectable.numeric_operand_type_mismatch.expected_type =
                                                     static_cast<wasm_value_type>(curr_operand_stack_value_type::i32);
                                                 err.err_selectable.numeric_operand_type_mismatch.actual_type =
                                                     to_wasm1_diagnostic_value_type(operand.type);
                                                 err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                                                 ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                             }
                                         }
                                     }};

    auto const emit_validated_prefixed_instruction{[&]() constexpr noexcept
                                                   {
                                                       if(emit_llvm_jit_active)
                                                       {
                                                           llvm_jit_instruction_emitted_inline = true;
                                                           if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr))
                                                               [[unlikely]]
                                                           {
                                                               disable_inline_llvm_jit_emission();
                                                           }
                                                       }
                                                   }};

    switch(numeric_code)
    {
        case wasm1p1_numeric_code::i32_trunc_sat_f32_s:
            validate_nontrapping_float_to_int(u8"i32.trunc_sat_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f32,
                                          runtime_operand_stack_value_type::i32,
                                          true,
                                          -2147483648.0f,
                                          2147483648.0f,
                                          0x80000000ull,
                                          0x7fffffffull);
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f32_u:
            validate_nontrapping_float_to_int(u8"i32.trunc_sat_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f32,
                                          runtime_operand_stack_value_type::i32,
                                          false,
                                          0.0f,
                                          4294967296.0f,
                                          0u,
                                          0xffffffffull);
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f64_s:
            validate_nontrapping_float_to_int(u8"i32.trunc_sat_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f64,
                                          runtime_operand_stack_value_type::i32,
                                          true,
                                          -2147483648.0,
                                          2147483648.0,
                                          0x80000000ull,
                                          0x7fffffffull);
            break;
        case wasm1p1_numeric_code::i32_trunc_sat_f64_u:
            validate_nontrapping_float_to_int(u8"i32.trunc_sat_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f64,
                                          runtime_operand_stack_value_type::i32,
                                          false,
                                          0.0,
                                          4294967296.0,
                                          0u,
                                          0xffffffffull);
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f32_s:
            validate_nontrapping_float_to_int(u8"i64.trunc_sat_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f32,
                                          runtime_operand_stack_value_type::i64,
                                          true,
                                          -9223372036854775808.0f,
                                          9223372036854775808.0f,
                                          0x8000000000000000ull,
                                          0x7fffffffffffffffull);
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f32_u:
            validate_nontrapping_float_to_int(u8"i64.trunc_sat_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f32,
                                          runtime_operand_stack_value_type::i64,
                                          false,
                                          0.0f,
                                          18446744073709551616.0f,
                                          0u,
                                          0xffffffffffffffffull);
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f64_s:
            validate_nontrapping_float_to_int(u8"i64.trunc_sat_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f64,
                                          runtime_operand_stack_value_type::i64,
                                          true,
                                          -9223372036854775808.0,
                                          9223372036854775808.0,
                                          0x8000000000000000ull,
                                          0x7fffffffffffffffull);
            break;
        case wasm1p1_numeric_code::i64_trunc_sat_f64_u:
            validate_nontrapping_float_to_int(u8"i64.trunc_sat_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
            emit_nontrapping_float_to_int(runtime_operand_stack_value_type::f64,
                                          runtime_operand_stack_value_type::i64,
                                          false,
                                          0.0,
                                          18446744073709551616.0,
                                          0u,
                                          0xffffffffffffffffull);
            break;
        case wasm1p1_numeric_code::memory_init:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"memory.init.dataidx")};
            check_data_index(op_begin, data_index);
            auto const memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.init.memidx")};
            check_memory_index_zero(memory_index, u8"memory.init");
            validate_i32_operands(u8"memory.init", 3uz);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::data_drop:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::data_segment);
            }
            auto const data_index{read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"data.drop")};
            check_data_index(op_begin, data_index);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::memory_copy:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const dst_memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.copy.dst")};
            check_memory_index_zero(dst_memory_index, u8"memory.copy");
            auto const src_memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.copy.src")};
            check_memory_index_zero(src_memory_index, u8"memory.copy");
            validate_i32_operands(u8"memory.copy", 3uz);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::memory_fill:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const memory_index{read_u8_immediate(code_curr, code_end, op_begin, u8"memory.fill")};
            check_memory_index_zero(memory_index, u8"memory.fill");
            validate_i32_operands(u8"memory.fill", 3uz);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::table_init:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
            }
            auto const element_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.init.elemidx")};
            check_element_index(op_begin, element_index);
            auto const table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.init.tableidx")};
            check_table_index(op_begin, table_index, subopcode);
            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(
                    elemsec.elems.index_unchecked(element_index).storage.segment.reftype))};
            auto const table_value_type{get_table_value_type(table_index)};
            if(element_value_type != table_value_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_value_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(element_value_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            validate_i32_operands(u8"table.init", 3uz);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::elem_drop:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::element_segment);
            }
            auto const element_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"elem.drop")};
            check_element_index(op_begin, element_index);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::table_copy:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const dst_table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.dst")};
            check_table_index(op_begin, dst_table_index, subopcode);
            auto const src_table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.copy.src")};
            check_table_index(op_begin, src_table_index, subopcode);
            auto const dst_type{get_table_value_type(dst_table_index)};
            auto const src_type{get_table_value_type(src_table_index)};
            if(dst_type != src_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(dst_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(src_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            validate_i32_operands(u8"table.copy", 3uz);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::table_grow:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
            {
                fail_wasm2_feature_required(op_begin,
                                            subopcode,
                                            ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
            }
            auto const table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.grow")};
            check_table_index(op_begin, table_index, subopcode);
            auto const table_type{get_table_value_type(table_index)};
            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.grow", 2uz); }
            auto const delta{try_pop_concrete_operand()};
            if(delta.from_stack && !delta.is_unknown && delta.type != curr_operand_stack_value_type::i32) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(delta.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const value{try_pop_concrete_operand()};
            if(value.from_stack && !value.is_unknown && value.type != table_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            operand_stack_push(curr_operand_stack_value_type::i32);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::table_size:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
            {
                fail_wasm2_feature_required(op_begin,
                                            subopcode,
                                            ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                            ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
            }
            auto const table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.size")};
            check_table_index(op_begin, table_index, subopcode);
            operand_stack_push(curr_operand_stack_value_type::i32);
            emit_validated_prefixed_instruction();
            break;
        }
        case wasm1p1_numeric_code::table_fill:
        {
            if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::bulk_memory)) [[unlikely]]
            {
                fail_wasm1p1_feature_required(op_begin,
                                              subopcode,
                                              ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::bulk_memory,
                                              ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
            }
            auto const table_index{
                read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.fill")};
            check_table_index(op_begin, table_index, subopcode);
            auto const table_type{get_table_value_type(table_index)};
            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.fill", 3uz); }
            auto const len{try_pop_concrete_operand()};
            if(len.from_stack && !len.is_unknown && len.type != curr_operand_stack_value_type::i32) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(len.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const value{try_pop_concrete_operand()};
            if(value.from_stack && !value.is_unknown && value.type != table_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const index{try_pop_concrete_operand()};
            if(index.from_stack && !index.is_unknown && index.type != curr_operand_stack_value_type::i32) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32);
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(index.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            emit_validated_prefixed_instruction();
            break;
        }
        [[unlikely]] default:
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    break;
}
