    // Branch opcode validation cases for the WebAssembly primary opcode set. Label signatures are complete Wasm tuples:
    // loops branch with their parameter tuple, while blocks, ifs, and the function label branch with their result tuple.

case static_cast<wasm1_code>(0x0au): // Core 3 throw_ref in a validated unreachable region
{
    // [throw_ref] next ... code_end
    // [safe     ] unsafe (could be code_end)
    // ^^ op_begin borrows the dispatch-checked opcode; throw_ref has no immediate.
    auto const op_begin{code_curr};
    ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x0au, op_begin, err);
    ++code_curr;
    // [throw_ref] next ... code_end
    // [safe     ] unsafe (could be code_end)
    //             ^^ code_curr: exactly one checked byte consumed, possibly one-past.
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    decltype(try_pop_concrete_operand()) operand{};
    auto const consume{[&]() constexpr noexcept
    {
        // Shared count > 0 proves one live current-frame top; preserve the owned rich type.
        operand = try_pop_concrete_operand();
        return v3::core3_operand{operand_core_type(operand), !operand.from_stack || operand.is_unknown};
    }};
    auto const matches{[&](auto actual, auto expected) constexpr noexcept
    { return runtime_core3_value_type_matches(actual, expected, typesec.owned_signatures); }};
    auto const failure{v3::pop_core3_expected_operand(is_polymorphic, concrete_operand_count, consume,
        v3::exception_validation_details::exception_reference(true), matches)};
    if(failure == v3::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"throw_ref", 1uz); }
    // Shared Core 3 expected-pop uses the complete heap/nullability descriptor,
    // including reference-only Bot, rather than legacy exn/funcref carriers.
    if(failure != v3::typed_stack_error::ok) [[unlikely]]
    {
        // [throw_ref] ... code_end; reporting borrows op_begin without advancing or reading it.
        // [safe     ]
        // ^^ err_curr
        err.err_curr = op_begin;
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
        err.err_selectable.br_value_type_mismatch = {.op_code_name = u8"throw_ref",
            .expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(0x69u),
            .actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(operand.type)};
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_throw_ref(llvm_jit_emit_state)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    v3::make_core3_frame_unreachable(is_polymorphic, [&]() constexpr noexcept
    {
        // The actual function/control frame owns this proven stack base; truncate its suffix only.
        operand_stack_truncate_to(control_flow_stack.back_unchecked().operand_stack_base);
    });
    break;
}
case static_cast<wasm1_code>(0x08u): // Core 3 locally caught throw
case wasm1_code::br:
{
    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    if(curr_opbase == static_cast<wasm1_code>(0x08u))
    { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x08u, op_begin, err); }
    // [br/throw] u32 index ... code_end
    // [safe    ] unsafe (could be code_end); dispatch proved the opcode byte before this increment.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

    if(curr_opbase == static_cast<wasm1_code>(0x08u))
    {
        namespace eh = ::uwvm2::runtime::compiler::shared::wasm_exception_control;
        auto const tag{eh::resolve_tag(curr_module, label_index, op_begin, 0x08u, err)};
        auto const payload_count{tag.parameters.size()};
        if(!is_polymorphic && concrete_operand_count() < payload_count) [[unlikely]]
        { report_operand_stack_underflow(op_begin, u8"throw", payload_count); }
        // A tag payload can contain exact GC/reference types whose flat ABI
        // carriers are all funcref. Use the retained Core 3 signature when it
        // exists, as the standalone validator does for the same throw.
        auto const rich_tag_payload{typesec.owned_signatures.size() == typesec.types.size() &&
            tag.type_index < typesec.owned_signatures.size()};
        if(rich_tag_payload && typesec.owned_signatures.index_unchecked(tag.type_index).parameters.size() != payload_count)
            [[unlikely]] { runtime_storage_bug(); }
        auto const available{concrete_operand_count()};
        auto const concrete{available < payload_count ? available : payload_count};
        for(::std::size_t i{}; i != concrete; ++i)
        {
            // [operand stack live entries] top ...; i < concrete <= available
            // [safe                      ] proves this indexed borrow is live.
            // [tag payload parameters] end; i < concrete <= payload_count
            // [safe                 ] proves the reverse parameter index is live.
            auto const& operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const parameter_index{payload_count - 1uz - i};
            auto const matches{rich_tag_payload ?
                core3_value_matches(operand, tag.parameters.begin[parameter_index],
                    typesec.owned_signatures.index_unchecked(tag.type_index).parameters.index_unchecked(parameter_index), true) :
                ::uwvm2::validation::standard::wasm3::reference_carrier_matches(operand, tag.parameters.begin[parameter_index])};
            if(!matches) [[unlikely]]
            { eh::unsupported(op_begin, 0x08u, err); }
        }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        if(native_eh_work)
        {
            bool numeric{native_eh_leaf_observer::numeric_range(tag.parameters)};
            if(rich_tag_payload)
            {
                for(auto const& value: typesec.owned_signatures.index_unchecked(tag.type_index).parameters)
                { numeric &= native_eh_leaf_observer::numeric_core(value); }
            }
            native_eh_work->prepare_throw(tag.identity, numeric, control_flow_stack);
        }
#endif
        auto const target{eh::find_handler(control_flow_stack, tag.identity)};
        if(target == SIZE_MAX)
        {
            // An unexecuted throw still validates its complete payload. A reachable
            // escaping throw uses the qualified native exception ABI, never a trap.
            // Native emission is opportunistic: its absence or failure cannot turn a
            // well-typed Core 3 throw into a validation error. The standard
            // validator and the fused pass must accept the same instruction.
            if(emit_llvm_jit_active && llvm_jit_emit_state.control_stack.back().is_reachable &&
               !try_emit_runtime_local_func_llvm_jit_throw_tuple(llvm_jit_emit_state,
                   label_index, {tag.parameters.begin, tag.parameters.end})) [[unlikely]]
            {
                disable_inline_llvm_jit_emission();
            }
            operand_stack_truncate_to(control_flow_stack.back_unchecked().operand_stack_base);
            is_polymorphic = true;
            if(emit_llvm_jit_active) { llvm_jit_instruction_emitted_inline = true; }
            break;
        }
        // The selected handler belongs to a live lexical ancestor; its outer target cannot have
        // expired. Runtime tuple repair below discards the payload for catch_all and preserves it
        // for catch. No exception object escapes, so allocation/unwind can be eliminated entirely.
        label_index = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(control_flow_stack.size() - 1uz - target);
    }

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
    if(label_index_uz >= all_label_count_uz) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            if(!block_value_matches(actual_operand, target_types, target_frame.signature_type_index,
                target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                target_frame.singleton_result_core_type, target_arity - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            if(!block_value_matches(actual_operand, target_types, target_frame.signature_type_index,
                target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                target_frame.singleton_result_core_type, target_arity - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual_type);
                err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        // Core 3 pop_vals/push_vals always reifies the complete label tuple on
        // fallthrough. This discards a narrower reference type even in reachable code.
        operand_stack_pop_n(concrete_to_check);
        operand_stack_push_types(target_types, target_frame.signature_type_index,
            target_frame.type != block_type::loop, target_frame.singleton_result_witness,
            target_frame.singleton_result_core_type, target_frame.has_singleton_result_core_type);
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                      // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                      // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                      // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                bool matches{};
                if(::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_validation_rules(wasm1p1_para))
                { matches = expected_label_types.begin[i] == actual_types.begin[i]; }
                else
                {
                    // Core 3 permits different target labels when the same live argument
                    // is a subtype of each. The selector remains at stack depth zero.
                    auto const depth_from_top{expected_arity - i};
                    if(concrete_operand_count() <= depth_from_top)
                    {
                        // Polymorphic bottom matches. Reachable underflow is reported
                        // after all immediates have been checked.
                        matches = true;
                    }
                    else
                    {
                        auto const& frame{control_flow_stack.index_unchecked(
                            all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
                        // [frame base ... argument ... selector] live operand deque
                        // [safe       | safe     | safe    ] unsafe (deque end)
                        //               ^^ operator[](size - 1 - depth_from_top)
                        // Neither code_curr nor the operand stack advances here.
                        auto const& argument{operand_stack[operand_stack.size() - 1uz - depth_from_top]};
                        matches = block_value_matches(argument, actual_types, frame.signature_type_index,
                            frame.type != block_type::loop, frame.has_singleton_result_core_type,
                            frame.singleton_result_core_type, i);
                    }
                }
                if(!matches)
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

                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(idx.type);
            err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(expected_arity != 0uz)
    {
        auto const& expected_frame{control_flow_stack.index_unchecked(
            all_label_count_uz - 1uz - static_cast<::std::size_t>(expected_label))};
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < expected_arity ? available_arg_count : expected_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{expected_label_types.begin[expected_arity - 1uz - i]};
            auto const& actual_operand{operand_stack[operand_stack.size() - 1uz - i]};
            auto const actual_type{actual_operand.type};
            if(!block_value_matches(actual_operand, expected_label_types, expected_frame.signature_type_index,
                expected_frame.type != block_type::loop, expected_frame.has_singleton_result_core_type,
                expected_frame.singleton_result_core_type, expected_arity - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
            if(!block_value_matches(actual_operand, func_frame.result, func_frame.signature_type_index,
                true, func_frame.has_singleton_result_core_type, func_frame.singleton_result_core_type,
                return_arity - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
