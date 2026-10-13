/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
case static_cast<wasm1_code>(0xfbu):
{
    // [FB] subopcode ... code_end
    // [safe] unsafe (could be code_end)
    // ^^ op_begin borrows the checked primary opcode.
    auto const op_begin{code_curr};
    ++code_curr;
    // [FB] subopcode ... code_end
    // [safe] unsafe (could be code_end)
    //        ^^ code_curr: only the checked prefix has been consumed.
    if(wasm1p1_para.disable_gc) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin, 0xfbu,
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    auto const decoded{::uwvm2::validation::standard::wasm3::scan_gc_instruction(code_curr, code_end)};
    if(decoded.error != ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok) [[unlikely]]
    { fail_invalid_immediate(op_begin, u8"gc"); }
    // [FB][checked complete immediate] next ... code_end
    // [safe                          ] unsafe (could be code_end)
    //                                  ^^ code_curr: scanner bounded the only pointer movement.
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    // The decoded cast's exn/noexn heaps require the exception feature even
    // when its input stack is polymorphic. Apply the same Core 3 policy as
    // the standalone validator before consuming a value or emitting IR.
    // [FB][bounded cast immediate] next ... code_end
    // [safe                       ] unsafe (could be code_end)
    // ^^ op_begin is the checked prefix borrowed for diagnostics; no cursor moves.
    v3::require_gc_cast_exception_policy(!wasm1p1_para.disable_exceptions,
        decoded.opcode, decoded.from, decoded.to, op_begin, err);
    using core_type = t3::core_value_type;
    switch(decoded.opcode)
    {
        case 0u: case 1u: case 2u: case 3u: case 4u: case 5u:
        case 6u: case 7u: case 8u: case 9u: case 10u: case 11u:
        case 12u: case 13u: case 14u: case 15u: case 16u: case 17u:
        case 18u: case 19u:
        {
            auto const* const recursive_types{curr_module.type_section_storage.core3_recursive_types_ptr};
            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
            auto const& context{retained_context == nullptr ? typesec.core3_context : *retained_context};
            if(decoded.opcode != 15u && (recursive_types == nullptr || retained_context == nullptr)) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc type index"); }
            struct gc_stack_adapter
            {
                decltype(concrete_operand_count) const& count;
                decltype(try_pop_concrete_operand) const& consume;
                decltype(operand_stack_push) const& append;
                decltype(operand_core_type) const& effective;
                bool const& polymorphic;
                [[nodiscard]] inline v3::core3_reference_error pop_expected(
                    core_type expected, v3::recursive_type_context const& context) noexcept
                {
                    auto const owned_consume{[&]() noexcept
                    {
                        // Shared kernel count > 0 proves the actual adapter top is live;
                        // copy its normalized rich type before retiring the stack entry.
                        auto const operand{consume()};
                        return v3::core3_operand{effective(operand), !operand.from_stack || operand.is_unknown};
                    }};
                    auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                    auto const error{v3::core3_reference_error_from_typed_stack(
                        v3::pop_core3_expected_operand(polymorphic, count, owned_consume, expected, matches))};

                    return error;
                }
                [[nodiscard]] inline v3::core3_reference_error pop_repeated(
                    core_type expected, ::std::uint_least32_t requested, v3::recursive_type_context const& context) noexcept
                {
                    auto const owned_consume{[&]() noexcept
                    {
                        // Bounded shared repetition proves each top exists above the actual frame.
                        auto const operand{consume()};
                        return v3::core3_operand{effective(operand), !operand.from_stack || operand.is_unknown};
                    }};
                    auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                    auto const error{v3::core3_reference_error_from_typed_stack(
                        v3::pop_core3_repeated_operands(polymorphic, count, owned_consume, expected, requested, matches))};

                    return error;
                }
                inline void push(core_type value) noexcept
                {
                    unsigned carrier{};
                    switch(value.kind)
                    {
                        case t3::value_kind::i32: carrier = 0x7fu; break;
                        case t3::value_kind::i64: carrier = 0x7eu; break;
                        case t3::value_kind::f32: carrier = 0x7du; break;
                        case t3::value_kind::f64: carrier = 0x7cu; break;
                        case t3::value_kind::v128: carrier = 0x7bu; break;
                        case t3::value_kind::reference:
                            carrier = value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                                value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern) ? 0x6fu :
                                value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                                value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn) ? 0x69u : 0x70u;
                            break;
                    }
                    append(static_cast<curr_operand_stack_value_type>(carrier), false,
                        (::std::numeric_limits<::std::size_t>::max)(), value, true);
                }
            };
            gc_stack_adapter stack{concrete_operand_count, try_pop_concrete_operand,
                operand_stack_push, operand_core_type, is_polymorphic};
            ::uwvm2::utils::container::vector<core_type> element_types{};
            if(decoded.opcode == 10u || decoded.opcode == 19u)
            {
                element_types.reserve(elemsec.elems.size());
                for(auto const& element : elemsec.elems)
                {
                    auto const& payload{element.storage.segment};
                    element_types.push_back_unchecked(payload.has_core_type ? payload.core_type :
                        v3::core3_legacy_carrier_type(payload.reftype));
                }
            }
            v3::core3_gc_environment const environment{{},
                {element_types.cbegin(), element_types.size()},
                datacountsec.present ? datacountsec.count : 0u, recursive_types};
            auto const failure{v3::validate_core3_gc_instruction(stack, decoded.opcode,
                decoded.first, decoded.second, environment, context, true)};
            if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"gc", 1uz); }
            if(failure != v3::core3_reference_error::ok) [[unlikely]]
            {
                auto name{::uwvm2::utils::container::u8string_view{u8"gc operand/type"}};
                if(failure == v3::core3_reference_error::unknown_type)
                { name = ::uwvm2::utils::container::u8string_view{u8"gc type index"}; }
                else if(failure == v3::core3_reference_error::unknown_field)
                { name = ::uwvm2::utils::container::u8string_view{u8"gc field index"}; }
                else if(failure == v3::core3_reference_error::immutable_field)
                { name = ::uwvm2::utils::container::u8string_view{u8"immutable gc field"}; }
                else if(failure == v3::core3_reference_error::packed_access_mismatch)
                { name = ::uwvm2::utils::container::u8string_view{u8"packed gc field access"}; }
                fail_invalid_immediate(op_begin, name);
            }
            if(emit_llvm_jit_active)
            {
                llvm_jit_instruction_emitted_inline = true;
                if(!try_emit_runtime_local_func_llvm_jit_decoded_gc_aggregate(llvm_jit_emit_state, decoded)) [[unlikely]]
                { disable_inline_llvm_jit_emission(); }
            }
            break;
        }
        case 20u: case 21u: case 22u: case 23u: // ref.test / ref.cast
        {
            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
            auto const& context{retained_context == nullptr ? typesec.core3_context : *retained_context};
            struct cast_stack_adapter
            {
                decltype(concrete_operand_count) const& count;
                decltype(try_pop_concrete_operand) const& consume;
                decltype(operand_stack_push) const& append;
                decltype(operand_core_type) const& effective;
                bool const& polymorphic;
                v3::recursive_type_context const& metadata;
                [[nodiscard]] inline v3::core3_reference_error pop_expected(
                    core_type expected, v3::recursive_type_context const& type_context) noexcept
                {
                    using e = v3::core3_reference_error;
                    if(count() == 0uz) { return polymorphic ? e::ok : e::stack_underflow; }
                    auto const operand{consume()};
                    return !operand.from_stack || operand.is_unknown ||
                        type_context.matches(effective(operand), expected) ? e::ok : e::type_mismatch;
                }
                inline void push(core_type value) noexcept
                {
                    if(value.kind == t3::value_kind::i32)
                    {
                        append(curr_operand_stack_value_type::i32);
                        return;
                    }
                    unsigned carrier{0x70u};
                    if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                       value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern))
                    { carrier = 0x6fu; }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn))
                    { carrier = 0x69u; }
                    constexpr auto no_witness{(::std::numeric_limits<::std::size_t>::max)()};
                    auto witness{no_witness};
                    if(value.heap.is_defined())
                    {
                        auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
                        // The shared cast validator checked every defined target before push().
                        // [records[0], records[size)) is the retained, immutable Core 3 type table.
                        // [safe                        ] contains(index) proves the kind read below.
                        if(metadata.contains(index) &&
                           metadata.records.index_unchecked(static_cast<::std::size_t>(index)).kind ==
                               t3::composite_kind::function)
                        { witness = static_cast<::std::size_t>(index); }
                    }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::nofunc))
                    { witness = no_witness - 1uz; }
                    append(static_cast<curr_operand_stack_value_type>(carrier), false, witness, value, true);
                }
            };
            cast_stack_adapter stack{concrete_operand_count, try_pop_concrete_operand,
                operand_stack_push, operand_core_type, is_polymorphic, context};
            auto const failure{v3::validate_core3_ref_cast(stack, decoded.to, context,
                decoded.opcode <= 21u, true)};
            if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"ref.test/ref.cast", 1uz); }
            if(failure != v3::core3_reference_error::ok) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"ref.test/ref.cast type"); }
            if(emit_llvm_jit_active)
            {
                llvm_jit_instruction_emitted_inline = true;
                if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
                { disable_inline_llvm_jit_emission(); }
            }
            break;
        }
        case 24u: case 25u: case 26u: case 27u: // cast branches / extern-any conversions
        {
            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
            auto const& context{retained_context == nullptr ? typesec.core3_context : *retained_context};
            struct reference_stack_adapter
            {
                decltype(concrete_operand_count) const& count;
                decltype(try_pop_concrete_operand) const& consume;
                decltype(operand_stack_push) const& append;
                decltype(operand_core_type) const& effective;
                bool const& polymorphic;
                v3::recursive_type_context const& metadata;
                [[nodiscard]] inline bool pop(v3::core3_operand& output) noexcept
                {
                    if(count() == 0uz)
                    {
                        if(!polymorphic) { return false; }
                        output = {{}, true}; return true;
                    }
                    auto const operand{consume()};
                    output = {effective(operand), !operand.from_stack || operand.is_unknown};
                    return true;
                }
                [[nodiscard]] inline v3::core3_reference_error pop_expected(
                    core_type expected, v3::recursive_type_context const& type_context) noexcept
                {
                    v3::core3_operand operand{};
                    if(!pop(operand)) { return v3::core3_reference_error::stack_underflow; }
                    return operand.unknown || type_context.matches(operand.type, expected) ?
                        v3::core3_reference_error::ok : v3::core3_reference_error::type_mismatch;
                }
                inline void push(core_type value) noexcept
                {
                    unsigned carrier{0x70u};
                    if(value.kind != t3::value_kind::reference)
                    {
                        carrier = value.kind == t3::value_kind::i32 ? 0x7fu :
                            value.kind == t3::value_kind::i64 ? 0x7eu :
                            value.kind == t3::value_kind::f32 ? 0x7du :
                            value.kind == t3::value_kind::f64 ? 0x7cu : 0x7bu;
                    }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern))
                    { carrier = 0x6fu; }
                    else if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                            value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn))
                    { carrier = 0x69u; }
                    constexpr auto no_witness{(::std::numeric_limits<::std::size_t>::max)()};
                    auto witness{no_witness};
                    if(value.kind == t3::value_kind::reference && value.heap.is_defined())
                    {
                        auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
                        // [metadata.records[0], records[size)) is the retained validated type table.
                        // [safe                               ] contains(index) proves this kind read.
                        if(metadata.contains(index) &&
                           metadata.records.index_unchecked(static_cast<::std::size_t>(index)).kind ==
                               t3::composite_kind::function)
                        { witness = static_cast<::std::size_t>(index); }
                    }
                    else if(value.kind == t3::value_kind::reference &&
                            value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::nofunc))
                    { witness = no_witness - 1uz; }
                    append(static_cast<curr_operand_stack_value_type>(carrier), false, witness, value, true);
                }
            };
            reference_stack_adapter stack{concrete_operand_count, try_pop_concrete_operand,
                operand_stack_push, operand_core_type, is_polymorphic, context};
            v3::core3_reference_error failure{};
            if(decoded.opcode <= 25u)
            {
                auto const label_count{control_flow_stack.size()};
                if(static_cast<::std::uint_least64_t>(decoded.first) >= label_count) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin; // Borrowed checked opcode; no pointer advancement.
                    err.err_selectable.illegal_label_index = {.label_index = decoded.first,
                        .all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(label_count)};
                    err.err_code = code_validation_error_code::illegal_label_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                // decoded.first < label_count proves the reverse frame index stays in range.
                auto const& target_frame{control_flow_stack.index_unchecked(
                    label_count - 1uz - static_cast<::std::size_t>(decoded.first))};
                auto const label_carriers{target_frame.type == block_type::loop ? target_frame.params : target_frame.result};
                // Both endpoints borrow one immutable signature; avoid null-pointer subtraction for the empty tuple.
                auto const arity{label_carriers.begin == label_carriers.end ? 0uz :
                    static_cast<::std::size_t>(label_carriers.end - label_carriers.begin)};
                ::uwvm2::utils::container::vector<core_type> label_types{};
                label_types.reserve(arity);
                for(::std::size_t i{}; i != arity; ++i)
                {
                    auto const rich{block_core_type_at(label_carriers, target_frame.signature_type_index,
                        target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                        target_frame.singleton_result_core_type, i)};
                    // [label_carriers.begin, label_carriers.end) contains arity entries.
                    // [safe                                    ] i < arity proves this carrier read.
                    label_types.push_back_unchecked(rich.has_type ? rich.type :
                        v3::core3_legacy_carrier_type(label_carriers.begin[i]));
                }
                failure = v3::validate_core3_branch_on_cast(stack, decoded.from, decoded.to,
                    {label_types.cbegin(), label_types.size()}, context, decoded.opcode == 25u, true);
            }
            else
            { failure = v3::validate_core3_convert_reference(stack, context, decoded.opcode == 26u, true); }
            if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"gc cast/convert", 1uz); }
            if(failure != v3::core3_reference_error::ok) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc cast/convert type"); }
            if(emit_llvm_jit_active)
            {
                llvm_jit_instruction_emitted_inline = true;
                if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
                { disable_inline_llvm_jit_emission(); }
            }
            break;
        }
        case 28u: // ref.i31
        case 29u: // i31.get_s
        case 30u: // i31.get_u
        {
            auto const name{decoded.opcode == 28u ? ::uwvm2::utils::container::u8string_view{u8"ref.i31"} :
                decoded.opcode == 29u ? ::uwvm2::utils::container::u8string_view{u8"i31.get_s"} :
                                       ::uwvm2::utils::container::u8string_view{u8"i31.get_u"}};
            decltype(try_pop_concrete_operand()) input{};
            auto const consume{[&]() constexpr noexcept
            {
                // Shared count > 0 proves a live top above this actual frame.
                // Copy the rich type before removing its owning stack entry.
                input = try_pop_concrete_operand();
                return v3::core3_operand{operand_core_type(input), !input.from_stack || input.is_unknown};
            }};
            auto const matches{[&](auto actual, auto expected) constexpr noexcept
            {
                // typesec owns the actual immutable signatures for this fused JIT
                // translation. Borrow that checked section, as the enclosing
                // matcher does; interpreter-only pointer aliases are not in scope.
                // [owned_signatures.begin, owned_signatures.end) remains retained.
                // [safe                                        ] no cursor advances.
                return runtime_core3_value_type_matches(actual, expected, typesec.owned_signatures);
            }};
            auto const transition{v3::apply_core3_i31_typed_transition(
                decoded.opcode, is_polymorphic, concrete_operand_count, consume, matches)};
            if(!transition.supported) [[unlikely]] { fail_invalid_immediate(op_begin, u8"gc i31"); }
            if(transition.error == v3::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, name, 1uz); }
            if(transition.error != v3::typed_stack_error::ok) [[unlikely]]
            {
                // Checked FB prefix is copied for a diagnostic only; there is
                // no immediate read, pointer arithmetic or cursor movement here.
                err.err_curr = op_begin;
                if(decoded.opcode == 28u)
                {
                    err.err_selectable.numeric_operand_type_mismatch = {
                        .op_code_name = u8"ref.i31",
                        .expected_type = to_wasm1_diagnostic_value_type(curr_operand_stack_value_type::i32),
                        .actual_type = to_wasm1_diagnostic_value_type(input.type)};
                    err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                }
                else
                {
                    err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<::std::uint_least8_t>(input.type);
                    err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                }
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            if(decoded.opcode == 28u)
            { operand_stack_push(curr_operand_stack_value_type::funcref, false,
                (::std::numeric_limits<::std::size_t>::max)(), transition.output, true); }
            else { operand_stack_push(curr_operand_stack_value_type::i32); }
            if(emit_llvm_jit_active)
            {
                llvm_jit_instruction_emitted_inline = true;
                if(!try_emit_runtime_local_func_llvm_jit_decoded_gc_i31(llvm_jit_emit_state, decoded)) [[unlikely]]
                { disable_inline_llvm_jit_emission(); }
            }
            break;
        }
        [[unlikely]] default:
        {
            // Never silently accept an aggregate operation before its exact lowering exists.
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Borrowed checked prefix; no pointer movement.
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(decoded.opcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    break;
}
