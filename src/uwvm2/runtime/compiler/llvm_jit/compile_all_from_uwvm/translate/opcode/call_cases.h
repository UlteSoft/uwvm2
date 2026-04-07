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
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        callee_type_ptr = typesec.types.cbegin() + funcsec.funcs.index_unchecked(local_idx);
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};

    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

    if(!is_polymorphic && operand_stack.size() < param_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.operand_stack_underflow.op_code_name = u8"call";
        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
        err.err_selectable.operand_stack_underflow.stack_size_required = param_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const stack_size{operand_stack.size()};

    // Type-check arguments when the stack is non-polymorphic.
    if(!is_polymorphic && param_count != 0uz && stack_size >= param_count)
    {
        for(::std::size_t i{}; i != param_count; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_type{operand_stack[stack_size - 1uz - i].type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Consume parameters if present.
    if(param_count != 0uz) { operand_stack_pop_n(param_count); }

    // Push results.
    if(result_count != 0uz)
    {
        for(::std::size_t i{}; i != result_count; ++i) { operand_stack_push(callee_type.result.begin[i]); }
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

    auto const all_type_count_uz{typesec.types.size()};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_type_count_uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 table_index;  // No initialization necessary
    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(table_index))};
    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
    }

    // call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                          ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(table_next);

    // call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                                      ^^ code_curr

    if(table_index >= all_table_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_table_index.table_index = table_index;
        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Resolve the function signature by type index.
    auto const& callee_type{typesec.types.index_unchecked(static_cast<::std::size_t>(type_index))};
    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

    // Stack effect: (args..., i32 func_index) -> (results...)
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_table_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_table_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};

    if(!is_polymorphic && (param_count_plus_table_index_overflows || operand_stack.size() < required_stack_size)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.operand_stack_underflow.op_code_name = u8"call_indirect";
        err.err_selectable.operand_stack_underflow.stack_size_actual = operand_stack.size();
        err.err_selectable.operand_stack_underflow.stack_size_required = required_stack_size;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::operand_stack_underflow;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // function index operand (must be i32 if present)
    if(!operand_stack.empty())
    {
        auto const idx{operand_stack_pop_unchecked()};

        if(!is_polymorphic && idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"call_indirect";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(idx.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    auto const stack_size{operand_stack.size()};
    if(!is_polymorphic && param_count != 0uz && stack_size >= param_count)
    {
        for(::std::size_t i{}; i != param_count; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_type{operand_stack[stack_size - 1uz - i].type};
            if(actual_type != expected_type) [[unlikely]]
            {
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

    if(result_count != 0uz)
    {
        for(::std::size_t i{}; i != result_count; ++i) { operand_stack_push(callee_type.result.begin[i]); }
    }

    break;
}
