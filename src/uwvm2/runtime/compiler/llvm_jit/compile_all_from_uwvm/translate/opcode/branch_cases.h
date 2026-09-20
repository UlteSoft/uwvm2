    // Branch opcode validation cases for the WebAssembly primary opcode set. Label signatures are complete Wasm tuples:
    // loops branch with their parameter tuple, while blocks, ifs, and the function label branch with their result tuple.

case wasm1_code::br:
{
    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(label_index))};
    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
    }

    // br     label_index ...
    // [     safe       ] unsafe (could be the section_end)
    //        ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(label_next);

    // br     label_index ...
    // [     safe       ] unsafe (could be the section_end)
    //                    ^^ code_curr

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
    if(label_index_uz >= all_label_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_label_index.label_index = label_index;
        err.err_selectable.illegal_label_index.all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

    auto const target_types{target_frame.type == block_type::loop ? target_frame.params : target_frame.result};
    auto const target_arity{get_runtime_block_result_count(target_types)};

    if(!is_polymorphic && concrete_operand_count() < target_arity) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"br", target_arity); }

    // type-check the branch arguments if present
    if(target_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < target_arity ? available_arg_count : target_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{target_types.begin[target_arity - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Consume branch arguments (if present) and make stack polymorphic (unreachable).
    if(target_arity != 0uz)
    {
        auto const concrete_to_consume{concrete_operand_count() < target_arity ? concrete_operand_count() : target_arity};
        operand_stack_pop_n(concrete_to_consume);
    }
    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after an unconditional branch).
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_br(llvm_jit_emit_state, label_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::br_if:
{
    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(label_index))};
    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
    }

    // br_if  label_index ...
    // [      safe      ] unsafe (could be the section_end)
    //        ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(label_next);

    // br_if  label_index ...
    // [      safe      ] unsafe (could be the section_end)
    //                    ^^ code_curr

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
    if(label_index_uz >= all_label_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_label_index.label_index = label_index;
        err.err_selectable.illegal_label_index.all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& target_frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - label_index_uz)};

    auto const target_types{target_frame.type == block_type::loop ? target_frame.params : target_frame.result};
    auto const target_arity{get_runtime_block_result_count(target_types)};

    // Need (labelargs..., i32 cond)
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const target_arity_plus_cond_overflows{target_arity == max_operand_stack_requirement};
    auto const required_stack_size{target_arity_plus_cond_overflows ? max_operand_stack_requirement : (target_arity + 1uz)};

    if(!is_polymorphic && (target_arity_plus_cond_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"br_if", required_stack_size);
    }

    // cond (must be i32 if present)
    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack && !cond.is_unknown)
    {
        if(cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_if";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(cond.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // type-check label arguments if present (they remain on stack for the fallthrough path)
    if(target_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < target_arity ? available_arg_count : target_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{target_types.begin[target_arity - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if(is_polymorphic)
        {
            // The fallthrough path retains the complete label tuple even if part of it was abstract in polymorphic code.
            operand_stack_pop_n(concrete_to_check);
            operand_stack_push_types(target_types);
        }
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_br_if(llvm_jit_emit_state, label_index)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
case wasm1_code::br_table:
{
    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    //           ^^ code_curr

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 target_count;  // No initialization necessary
    auto const [cnt_next, cnt_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(target_count))};
    if(cnt_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(cnt_err);
    }

    // br_table  target_count ...
    // [       safe         ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(cnt_next);

    // br_table  target_count ...
    // [       safe         ] unsafe (could be the section_end)
    //                       ^^ code_curr

    // Security hardening: each `br_table` label index (including the default label)
    // is an unsigned LEB128 value, so every entry consumes at least one byte.
    // Reject encodings that cannot possibly provide `target_count + 1` indices
    // before further validation work, which also blocks attacker-controlled
    // oversized counts from turning malformed inputs into resource-amplification paths.
    auto const remaining_bytes{static_cast<::std::size_t>(code_end - code_curr)};
    constexpr auto max_br_table_label_count{::std::numeric_limits<::std::size_t>::max()};
    bool target_count_exceeds_size_t{};
    ::std::size_t target_count_uz{};
    if constexpr(::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max() > max_br_table_label_count)
    {
        if(target_count > max_br_table_label_count) [[unlikely]] { target_count_exceeds_size_t = true; }
        else
        {
            target_count_uz = static_cast<::std::size_t>(target_count);
        }
    }
    else
    {
        target_count_uz = static_cast<::std::size_t>(target_count);
    }

    auto const target_count_plus_default_overflows{!target_count_exceeds_size_t && target_count_uz == max_br_table_label_count};
    if(target_count_exceeds_size_t || target_count_plus_default_overflows || remaining_bytes == 0uz || target_count_uz >= remaining_bytes) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.target_count = target_count;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.remaining_bytes = remaining_bytes;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.max_target_count = (remaining_bytes == 0uz ? 0uz : remaining_bytes - 1uz);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_table_target_count_exceeds_remaining_bytes;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::utils::container::vector<validation_module_traits_t::wasm_u32> llvm_jit_label_indices{};
    if(emit_llvm_jit_active) { llvm_jit_label_indices.reserve(target_count_uz); }

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const validate_label{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr UWVM_THROWS
                              {
                                  if(static_cast<::std::size_t>(li) >= all_label_count_uz) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.illegal_label_index.label_index = li;
                                      err.err_selectable.illegal_label_index.all_label_count =
                                          static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(all_label_count_uz);
                                      err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }
                              }};

    struct get_sig_result_t
    { runtime_block_result_type types{}; };

    auto const get_sig{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li) constexpr noexcept
                       {
                           auto const& frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};

                           return get_sig_result_t{frame.type == block_type::loop ? frame.params : frame.result};
                       }};

    bool have_expected_sig{};
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 expected_label{};
    runtime_block_result_type expected_label_types{};

    auto const check_br_table_sig{
        [&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li, runtime_block_result_type actual_types) constexpr UWVM_THROWS
        {
            if(!have_expected_sig)
            {
                have_expected_sig = true;
                expected_label = li;
                expected_label_types = actual_types;
                return;
            }

            auto const expected_arity{get_runtime_block_result_count(expected_label_types)};
            auto const actual_arity{get_runtime_block_result_count(actual_types)};
            bool mismatch{expected_arity != actual_arity};
            curr_operand_stack_value_type expected_type{};
            curr_operand_stack_value_type actual_type{};
            auto const comparable_count{expected_arity < actual_arity ? expected_arity : actual_arity};
            for(::std::size_t i{}; i != comparable_count; ++i)
            {
                // Core 1 requires identical label types; Core 2 also allows a common bottom
                // argument. An explicit MVP policy must retain the Core 1 rule in every backend.
                // The selector is still on top; only missing/unknown arguments may meet.
                auto const depth_from_top{expected_arity - i};
                auto const argument_is_bottom{!::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_validation_rules(wasm1p1_para) && is_polymorphic &&
                    (concrete_operand_count() <= depth_from_top ||
                     operand_stack[operand_stack.size() - 1uz - depth_from_top].is_unknown)};
                if(expected_label_types.begin[i] != actual_types.begin[i] && !argument_is_bottom)
                {
                    mismatch = true;
                    expected_type = expected_label_types.begin[i];
                    actual_type = actual_types.begin[i];
                    break;
                }
            }

            if(mismatch) [[unlikely]]
            {
                if(comparable_count == 0uz || expected_type == curr_operand_stack_value_type{})
                {
                    if(expected_arity != 0uz) { expected_type = expected_label_types.begin[0]; }
                    if(actual_arity != 0uz) { actual_type = actual_types.begin[0]; }
                }

                err.err_curr = op_begin;
                err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
                err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = li;
                err.err_selectable.br_table_target_type_mismatch.expected_arity =
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(expected_arity);
                err.err_selectable.br_table_target_type_mismatch.actual_arity =
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(actual_arity);
                err.err_selectable.br_table_target_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(expected_type);
                err.err_selectable.br_table_target_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_table_target_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }};

    for(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 i{}; i != target_count; ++i)
    {
        // ...    | curr_target ...
        // [safe] | unsafe (could be the section_end)
        //          ^^ code_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 li;  // No initialization necessary
        auto const [li_next, li_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(li))};
        if(li_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(li_err);
        }

        // ...   | curr_target ...
        // [safe | safe      ] unsafe (could be the section_end)
        //         ^^ code_curr

        code_curr = reinterpret_cast<::std::byte const*>(li_next);

        // ...   | curr_target ...
        // [safe | safe      ] unsafe (could be the section_end)
        //                     ^^ code_curr

        validate_label(li);

        check_br_table_sig(li, get_sig(li).types);

        if(emit_llvm_jit_active) { llvm_jit_label_indices.push_back(li); }
    }

    // ... last_target | default_label ...
    // [   safe      ]   unsafe (could be the section_end)
    //                   ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 default_label;  // No initialization necessary
    auto const [def_next, def_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(default_label))};
    if(def_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(def_err);
    }

    // ... last_target | default_label ...
    // [         safe  |      safe   ] unsafe (could be the section_end)
    //                   ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(def_next);

    // ... last_target | default_label ...
    // [         safe  |      safe   ] unsafe (could be the section_end)
    //                                 ^^ code_curr

    validate_label(default_label);

    check_br_table_sig(default_label, get_sig(default_label).types);

    // Stack effect: (labelargs..., i32 index) -> unreachable
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const expected_arity{get_runtime_block_result_count(expected_label_types)};
    auto const expected_arity_plus_index_overflows{expected_arity == max_operand_stack_requirement};
    auto const required_stack_size{expected_arity_plus_index_overflows ? max_operand_stack_requirement : (expected_arity + 1uz)};

    if(!is_polymorphic && (expected_arity_plus_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"br_table", required_stack_size);
    }

    if(auto const idx{try_pop_concrete_operand()}; idx.from_stack && !idx.is_unknown)
    {
        if(idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(idx.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(expected_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < expected_arity ? available_arg_count : expected_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{expected_label_types.begin[expected_arity - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_table";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Consume label args if present and make stack polymorphic.
    if(expected_arity != 0uz)
    {
        auto const concrete_to_consume{concrete_operand_count() < expected_arity ? concrete_operand_count() : expected_arity};
        operand_stack_pop_n(concrete_to_consume);
    }
    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after br_table).
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_br_table(llvm_jit_emit_state, llvm_jit_label_indices, default_label)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::return_:
{
    // return ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // return ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // return ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    // `return` exits the function immediately. It is equivalent to an unconditional branch to the
    // implicit outer function label (the bottom frame in control_flow_stack).
    auto const& func_frame{control_flow_stack.index_unchecked(0u)};

    ::std::size_t const return_arity{get_runtime_block_result_count(func_frame.result)};

    if(!is_polymorphic && concrete_operand_count() < return_arity) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"return", return_arity); }

    // Type-check the complete function-result tuple in source order.
    if(return_arity != 0uz)
    {
        auto const available_result_count{concrete_operand_count()};
        auto const concrete_to_check{available_result_count < return_arity ? available_result_count : return_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{func_frame.result.begin[return_arity - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!actual_operand.is_unknown && actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Consume return values (if present) and make stack polymorphic (unreachable).
    if(return_arity != 0uz)
    {
        auto const concrete_to_consume{concrete_operand_count() < return_arity ? concrete_operand_count() : return_arity};
        operand_stack_pop_n(concrete_to_consume);
    }

    // Avoid leaking concrete stack values into the polymorphic region (prevents false type errors after return).
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_return(llvm_jit_emit_state)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
    }

    break;
}
