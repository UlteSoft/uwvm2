    // WebAssembly 1.1 opcode validation/emission for the LLVM JIT path. Keep feature gates synchronized with
    // validation/standard/wasm3/validator.h. The typed ABI carries funcref/externref and v128 as exact-width opaque
    // integers and represents multi-value signatures as Wasm-order literal structs.

case static_cast<wasm1_code>(wasm1p1_code::select_t):
{
    // [select opcode] result vector ... code_end
    // [safe         ] dispatch proved the opcode readable; op_begin borrows that address.
    auto const op_begin{code_curr};
    ++code_curr;
    // [select opcode] result vector ... code_end
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr after the proven one-byte opcode.

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

    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_core_type{};
    auto const result_type_byte{::uwvm2::validation::standard::wasm3::read_core3_value_carrier(
        code_curr, code_end, !wasm1p1_para.disable_function_references, op_begin, err,
        typesec.types.size(), ::std::addressof(result_core_type), typesec.core3_context,
        !wasm1p1_para.disable_gc, !wasm1p1_para.disable_exceptions)};

    // select_t result_type_count result_type ...
    // [                 safe               ] unsafe (could be the section_end)
    //                                        ^^ code_curr

    // Field commits are transactional: a count-LEB decode failure leaves the cursor after the opcode; a decoded-count
    // arity rejection or result-type decode failure leaves it after the count; a type-policy rejection follows the complete value encoding.
    auto const result_type{static_cast<curr_operand_stack_value_type>(result_type_byte)};
    // Core 3 exnref was checked by the bounded value decoder, including the
    // independent exceptions and reference-types gates. The wasm1p1 type
    // enum predates 0x69; applying its legacy gate here would reject a valid
    // typed select after the complete immediate was already accepted.
    if(result_type_byte != 0x69u)
    { ensure_wasm1p1_value_type_enabled(op_begin, result_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }

    auto const event{validate_typed_select(op_begin, result_type, result_core_type)};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(is_runtime_wasm_value_type_llvm_storage_supported(static_cast<runtime_operand_stack_value_type>(result_type)))
        {
            if(!try_emit_runtime_local_func_llvm_jit_typed_select(llvm_jit_emit_state, event)) [[unlikely]] { disable_inline_llvm_jit_emission(); }
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
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end); dispatch proved the opcode.
    auto const op_begin{code_curr};
    ++code_curr;
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end)
    //                ^^ code_curr after the proven one-byte opcode.
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_byte(wasm1p1_code::table_get),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    // [consumed opcode][tableidx bytes ...] | code_end
    // [safe opcode] unsafe (could be code_end); existing bounded LEB
    // decoder commits code_curr only after the WHOLE u32 and within code_end.
    auto const table_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.get")};
    check_table_index(op_begin, table_index, opcode_byte(wasm1p1_code::table_get));
    auto const event{validate_table_access.template operator()<0x25u>(op_begin, table_index, u8"table.get")};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_table_access(llvm_jit_emit_state, event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::table_set):
{
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end); dispatch proved the opcode.
    auto const op_begin{code_curr};
    ++code_curr;
    // [table opcode] tableidx ... code_end
    // [safe        ] unsafe (could be code_end)
    //                ^^ code_curr after the proven one-byte opcode.
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::table_instructions)) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    opcode_byte(wasm1p1_code::table_set),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    // [consumed opcode][tableidx bytes ...] | code_end
    // [safe opcode] unsafe (could be code_end); existing bounded LEB
    // decoder commits code_curr only after the WHOLE u32 and within code_end.
    auto const table_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"table.set")};
    check_table_index(op_begin, table_index, opcode_byte(wasm1p1_code::table_set));
    auto const event{validate_table_access.template operator()<0x26u>(op_begin, table_index, u8"table.set")};

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_table_access(llvm_jit_emit_state, event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_null):
{
    // [ref.null] heap ... code_end
    // [safe    ] unsafe (could be code_end)
    // ^^ code_curr / op_begin: outer dispatch proved this opcode byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.null] heap ... code_end
    // [safe    ] unsafe (could be code_end)
    //            ^^ code_curr: now checked by the shared bounded heap decoder.
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_null);
    }
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    t3::heap_type decoded_heap{};
    unsigned reference_type_byte{};
    bool exact_function_heap{};
    // [ref.null][heap immediate ... code_end)
    // [safe    ] unsafe (possibly code_end)
    //            ^^ code_curr: inspect one byte only when nonempty. The two single-byte
    // exn/noexn heaps use the Core 3 decoder even if GC and function-references
    // are disabled; every other heap keeps its existing precise feature gate.
    auto const exception_heap_immediate{code_curr != code_end &&
        (::std::to_integer<unsigned>(*code_curr) == 0x69u || ::std::to_integer<unsigned>(*code_curr) == 0x74u)};
    if(!wasm1p1_para.disable_gc || !typesec.core3_context.records.empty() ||
       exception_heap_immediate)
    {
        auto const decoded{::uwvm2::validation::standard::wasm3::scan_core3_ref_null_heap(
            code_curr, code_end, typesec.core3_context, !wasm1p1_para.disable_gc,
            !wasm1p1_para.disable_function_references, !wasm1p1_para.disable_exceptions)};
        // [ref.null][checked signed-33 heap] next ... code_end
        // [safe                            ] unsafe (could be code_end)
        //                                  ^^ code_curr: decoder commits only a valid feature-enabled heap.
        using error = ::uwvm2::validation::standard::wasm3::core3_ref_null_error;
        if(decoded.error == error::gc_disabled) [[unlikely]]
        { fail_wasm1p1_feature_required(op_begin, opcode_byte(wasm1p1_code::ref_null),
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
        if(decoded.error == error::function_references_disabled) [[unlikely]]
        { fail_wasm1p1_feature_required(op_begin, opcode_byte(wasm1p1_code::ref_null),
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
        if(decoded.error == error::exceptions_disabled) [[unlikely]]
        { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(false, 0xd0u, op_begin, err); }
        if(decoded.error != error::ok) [[unlikely]] { fail_invalid_immediate(op_begin, u8"ref.null"); }
        decoded_heap = decoded.heap;
        reference_type_byte = decoded.carrier;
        if(decoded.heap.is_defined())
        {
            // Decoder proved this index is inside the validated type-kind context.
            exact_function_heap = typesec.core3_context.records.index_unchecked(
                static_cast<::std::size_t>(decoded.heap.code)).kind == t3::composite_kind::function;
        }
    }
    else
    {
        // [ref.null] heap ... code_end
        // [safe    ] unsafe (could be code_end)
        //            ^^ code_curr: the bounded scanner commits only a complete valid heap.
        auto const decoded{::uwvm2::validation::standard::wasm3::scan_function_ref_null_heap(
            code_curr, code_end, !wasm1p1_para.disable_function_references, typesec.types.size(), true, !wasm1p1_para.disable_gc)};
        // [ref.null][complete signed heap immediate] next ... code_end on success.
        // [safe                                   ] unsafe (could be code_end)
        //                                           ^^ code_curr: unchanged on every scanner failure.
        if(decoded.error != ::uwvm2::validation::standard::wasm3::function_heap_immediate_error::ok) [[unlikely]]
        {
            // A failed bounded scan did not commit code_curr. Only this cold error path
            // repeats the immutable immediate to preserve the shared diagnostic policy.
            // [ref.null] [failed heap ... code_end)
            // [safe    ] unsafe (could be code_end)
            //            ^^ code_curr: the wrapper retains it on failure, then throws.
            static_cast<void>(::uwvm2::validation::standard::wasm3::read_function_ref_null_carrier(
                code_curr, code_end, !wasm1p1_para.disable_function_references, typesec.types.size(),
                op_begin, err, true, !wasm1p1_para.disable_gc));
            ::fast_io::fast_terminate(); // The same immutable immediate cannot succeed after its failed scan.
        }
        reference_type_byte = decoded.carrier;
        decoded_heap = decoded.heap;
        exact_function_heap = decoded.heap.is_defined();
    }
    if(reference_type_byte == 0x69u)
    {
        ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(
            !wasm1p1_para.disable_exceptions, 0xd0u, op_begin, err);
    }
    using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
    auto const reference_type_value{static_cast<reference_type>(reference_type_byte)};
    if(reference_type_byte != 0x69u && reference_type_value != reference_type::funcref &&
       reference_type_value != reference_type::externref) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = reference_type_byte;
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const value_type{reference_type_byte == 0x69u ? static_cast<curr_operand_stack_value_type>(0x69u) :
        static_cast<curr_operand_stack_value_type>(
            ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(reference_type_value))};
    if(reference_type_byte != 0x69u)
    { ensure_wasm1p1_value_type_enabled(op_begin, value_type, ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type); }
    // The first bounded decode supplies both the exact heap and ABI carrier.
    // No source bytes, context classification or typed stack check is replayed.
    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_null_event(
        decoded_heap, reference_type_byte, exact_function_heap)};
    operand_stack_push(value_type, false, event.exact_function_type_index, event.result_type, true);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_decoded_ref_null(llvm_jit_emit_state, event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(0xd3u): // Core 3 ref.eq
{
    // [ref.eq] next ... code_end
    // [safe  ] unsafe (could be code_end)
    // ^^ op_begin is the dispatch-checked opcode; there is no immediate.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.eq] next ... code_end
    // [safe  ] unsafe (possibly one-past)
    //          ^^ code_curr advances exactly one checked opcode byte.
    if(wasm1p1_para.disable_gc) [[unlikely]]
    { fail_wasm1p1_feature_required(op_begin, 0xd3u,
        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
        ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction); }
    if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.eq", 2uz); }
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    auto const eq_type{t3::core_value_type{t3::value_kind::reference,
        {static_cast<::std::int_least64_t>(t3::abstract_heap_type::eq)}, true}};
    for(unsigned index{}; index != 2u; ++index)
    {
        auto const operand{try_pop_concrete_operand()};
        if(operand.from_stack && !core3_value_matches(operand,
            curr_operand_stack_value_type::funcref, eq_type, true)) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Borrowed checked opcode; no pointer movement.
            err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<unsigned>(operand.type);
            err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    operand_stack_push(curr_operand_stack_value_type::i32);
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_is_null):
{
    // [ref.is_null] next opcode ... code_end
    // [safe       ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved the opcode byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.is_null] next opcode ... code_end
    // [safe       ] unsafe (could be code_end)
    //               ^^ code_curr; this opcode has no immediate.
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_is_null),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::reference_type);
    }
    namespace reference_semantics = ::uwvm2::validation::standard::wasm3;
    decltype(try_pop_concrete_operand()) reference{};
    auto const consume_reference{[&]() constexpr noexcept
    {
        // Count > 0 in the common transition proves the actual top entry.
        // The native pop copies its rich type before retiring that stack slot.
        reference = try_pop_concrete_operand();
        return reference_semantics::core3_reference_operand{
            reference_semantics::core3_operand_effective_type(reference),
            !reference.from_stack || reference.is_unknown, static_cast<unsigned>(reference.type)};
    }};
    auto const transition{reference_semantics::apply_core3_ref_is_null_typed_transition(
        is_polymorphic, concrete_operand_count, consume_reference)};
    if(transition.error == reference_semantics::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.is_null", 1uz); }
    if(transition.error != reference_semantics::typed_stack_error::ok) [[unlikely]]
    {
        // [caller-saved checked opcode] next ... | code_end
        // [safe                       ]         | one-past is never read.
        // ^^ op_begin -> err.err_curr: copy only; dispatch owns the range proof.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value =
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const& event{transition.event};
    operand_stack_push(curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_decoded_ref_is_null(llvm_jit_emit_state, event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}


case static_cast<wasm1_code>(0xd5u):
case static_cast<wasm1_code>(0xd6u):
{
    // [br_on_null / br_on_non_null] labelidx ... next opcode ... code_end
    // [safe                      ] unsafe (could be code_end)
    // ^^ code_curr: outer dispatch checked this opcode byte.
    auto const op_begin{code_curr};
    auto const branch_non_null{static_cast<unsigned>(*code_curr) == 0xd6u};
    auto const op_name{branch_non_null ? ::uwvm2::utils::container::u8string_view{u8"br_on_non_null"} : ::uwvm2::utils::container::u8string_view{u8"br_on_null"}};
    ++code_curr;
    // [branch opcode] labelidx ... next opcode ... code_end
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr: bounded decoder must check the complete u32 immediate.
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, branch_non_null ? 0xd6u : 0xd5u, op_begin, err);
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 label_index;
    auto const [label_next, label_error]{::fast_io::parse_by_scan(
        reinterpret_cast<char8_t const*>(code_curr), reinterpret_cast<char8_t const*>(code_end), ::fast_io::mnp::leb128_get(label_index))};
    if(label_error != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Borrow the checked opcode for diagnostics; no dereference.
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_error);
    }
    // [branch opcode][complete labelidx] next opcode ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                 ^^ code_curr; label_next is proven within [code_curr, code_end].
    code_curr = reinterpret_cast<::std::byte const*>(label_next);
    // [branch opcode][complete labelidx] next opcode ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                                   ^^ code_curr
    auto const label_count{control_flow_stack.size()};
    if(static_cast<::std::uint_least64_t>(label_index) >= label_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Diagnostic borrow within the checked instruction.
        err.err_selectable.illegal_label_index = {.label_index = label_index,
            .all_label_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(label_count)};
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto& target_frame{control_flow_stack.index_unchecked(label_count - 1uz - static_cast<::std::size_t>(label_index))};
    auto const target_types{target_frame.type == block_type::loop ? target_frame.params : target_frame.result};
    auto const target_arity{target_types.begin == target_types.end ? 0uz : static_cast<::std::size_t>(target_types.end - target_types.begin)};
    if(branch_non_null && (target_arity == 0uz ||
       !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(target_types.begin[target_arity - 1uz]))) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Diagnostic borrow; the short-circuit above protects the last-type access.
        err.err_selectable.wasm1p1_invalid_reference_type.value = target_arity == 0uz ? 0x40u : static_cast<unsigned>(target_types.begin[target_arity - 1uz]);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const prefix_arity{target_arity - static_cast<::std::size_t>(branch_non_null)};
    if(!is_polymorphic && concrete_operand_count() <= prefix_arity) [[unlikely]]
    { report_operand_stack_underflow(op_begin, op_name, prefix_arity + 1uz); }
    auto const reference{try_pop_concrete_operand()};
    if(reference.from_stack && !reference.is_unknown &&
       !::uwvm2::validation::standard::wasm3::is_legacy_reference_carrier(reference.type)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin; // Borrow only, no input-pointer read.
        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<unsigned>(reference.type);
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const bottom{!reference.from_stack || reference.is_unknown || reference.is_reference_bottom};
    auto const carrier{bottom ? curr_operand_stack_value_type::funcref : reference.type};
    auto const require_match{[&](auto const& actual, ::std::size_t index) constexpr UWVM_THROWS
    {
        if(block_value_matches(actual, target_types, target_frame.signature_type_index,
            target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
            target_frame.singleton_result_core_type, index)) { return; }
        // [branch opcode][complete labelidx] next opcode ... code_end
        // [safe                           ] unsafe (could be code_end)
        // ^^ op_begin / err_curr borrow the checked opcode; no cursor advances.
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch = {.op_code_name = op_name,
            .expected_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(target_types.begin[index]),
            .actual_type = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::value_type>(actual.type)};
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }};
    if(branch_non_null && reference.from_stack)
    {
        auto narrowed{reference};
        narrowed.core_type = operand_core_type(reference);
        narrowed.core_type.nullable = false;
        narrowed.has_core_type = !bottom;
        require_match(narrowed, target_arity - 1uz);
    }
    auto const concrete_count{concrete_operand_count() < prefix_arity ? concrete_operand_count() : prefix_arity};
    for(::std::size_t i{}; i != concrete_count; ++i)
    { require_match(operand_stack[operand_stack.size() - 1uz - i], prefix_arity - 1uz - i); }
    // Reify the declared label tuple on every fallthrough, preserving its exact
    // Core 3 heap witnesses. For br_on_non_null the final label operand is
    // branch-only, so remove it after constructing the full rich tuple.
    operand_stack_pop_n(concrete_count);
    operand_stack_push_types(target_types, target_frame.signature_type_index,
        target_frame.type != block_type::loop, target_frame.singleton_result_witness,
        target_frame.singleton_result_core_type, target_frame.has_singleton_result_core_type);
    if(branch_non_null)
    {
        // The preceding target_arity > 0 check proves this pushed operand exists.
        static_cast<void>(operand_stack_pop_unchecked());
    }
    if(!branch_non_null)
    {
        // The fallthrough value is narrowed to non-null; bottom retains its reference-only marker.
        auto narrowed_core{operand_core_type(reference)};
        narrowed_core.nullable = false;
        operand_stack_push(carrier, false, reference.exact_function_type_index, narrowed_core, !bottom);
        operand_stack.back().is_reference_bottom = bottom;
    }
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_br_on_reference(llvm_jit_emit_state, label_index, branch_non_null)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

case static_cast<wasm1_code>(0xd4u):
{
    // [ref.as_non_null] next opcode ... (code_end)
    // [safe           ] unsafe (could be code_end)
    // ^^ code_curr / op_begin; the outer dispatch proved this opcode exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.as_non_null] next opcode ... (code_end)
    // [safe           ] unsafe (could be code_end)
    //                   ^^ code_curr; no immediate belongs to this opcode.
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, 0xd4u, op_begin, err);
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    decltype(try_pop_concrete_operand()) reference{};
    auto const consume{[&]() constexpr noexcept
    {
        // Shared count > 0 proves a concrete top; copy before its actual stack retirement.
        reference = try_pop_concrete_operand();
        return v3::core3_operand{operand_core_type(reference), !reference.from_stack || reference.is_unknown};
    }};
    v3::core3_operand normalized{};
    auto const pop_error{v3::pop_core3_typed_operand(is_polymorphic, concrete_operand_count, consume, normalized)};
    if(pop_error == v3::typed_stack_error::stack_underflow) [[unlikely]]
    { report_operand_stack_underflow(op_begin, u8"ref.as_non_null", 1uz); }
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    t3::core_value_type narrowed_core{};
    if(v3::narrow_core3_non_null_reference(normalized, narrowed_core) != v3::typed_stack_error::ok) [[unlikely]]
    {
        // [checked ref.as_non_null opcode] ... | code_end
        // [safe opcode byte             ] unsafe (at code_end); no guest byte is read here.
        // ^^ err.err_curr borrows the dispatch-checked opcode.
        err.err_curr = op_begin;
        err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference.type);
        err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const reference_bottom{narrowed_core.heap.code == t3::heap_type::bottom_code};
    auto const carrier{reference_bottom ? curr_operand_stack_value_type::funcref : reference.type};
    // Reify the shared exact reference-only result. Unknown Bot cannot become numeric Bot.
    operand_stack_push(carrier, false, reference.exact_function_type_index, narrowed_core, true);
    operand_stack.back().is_reference_bottom = reference_bottom;
    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_instruction(llvm_jit_emit_state, op_begin, code_curr)) [[unlikely]]
        { disable_inline_llvm_jit_emission(); }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::ref_func):
{
    // [ref.func] function-index LEB ... code_end
    // [safe    ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved the opcode byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [ref.func] function-index LEB ... code_end
    // [safe    ] unsafe (could be code_end)
    //            ^^ code_curr; bounded LEB decoding checks the immediate.
    if(!wasm2_feature_enabled(::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::reference_types)) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin,
                                      opcode_byte(wasm1p1_code::ref_func),
                                      ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                                      ::uwvm2::parser::wasm::base::wasm1p1_error_subject::init_ref_func);
    }
    auto const function_index{
        read_leb128.template operator()<validation_module_traits_t::wasm_u32>(code_curr, code_end, op_begin, u8"ref.func")};
    // [ref.func][checked u32 immediate] next ... code_end
    // [safe                           ] unsafe (could be code_end)
    //                                   ^^ code_curr: sole bounded decoder committed the complete index.
    check_ref_func_index(op_begin, function_index);
    // ref.func identifies a type-section declaration, not the erased funcref supertype.
    // Imported and local function indices were bounded by check_ref_func_index above.
    auto const function_ordinal{static_cast<::std::size_t>(function_index)};
    auto const declared_type{function_ordinal < import_func_count ?
        importsec.importdesc.index_unchecked(0u).index_unchecked(function_ordinal)->imports.storage.function :
        ::std::addressof(typesec.types.index_unchecked(funcsec.funcs.index_unchecked(function_ordinal - import_func_count)))};
    ::std::size_t exact_type{(::std::numeric_limits<::std::size_t>::max)()};
    for(::std::size_t i{}; i != typesec.types.size(); ++i)
    { if(::std::addressof(typesec.types.index_unchecked(i)) == declared_type) { exact_type = i; break; } }
    if(exact_type == (::std::numeric_limits<::std::size_t>::max)()) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const event{::uwvm2::validation::standard::wasm3::make_core3_ref_func_event(function_index, exact_type)};
    operand_stack_push(curr_operand_stack_value_type::funcref, false, event.exact_function_type_index, event.result_type, true);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_decoded_ref_func(llvm_jit_emit_state, event)) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }
    break;
}

case static_cast<wasm1_code>(wasm1p1_code::simd_prefix):
{
    auto const op_begin{code_curr};
    // 0xfd ...
    // [safe] unsafe (could be code_end)
    // ^^ code_curr: outer dispatch proved one byte readable.
    ++code_curr;
    // [safe] unsafe (could be code_end)
    //        ^^ code_curr: only the consumed prefix is behind the cursor.
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

    // 0xfd subopcode ...
    // [safe         ] unsafe (could be code_end)
    //                 ^^ code_curr: bounded read_leb128 consumed the complete u32.
    if(::uwvm2::validation::standard::wasm3::relaxed_simd_operand_count(subopcode) != 0u && wasm1p1_para.disable_relaxed_simd)
    {
        fail_wasm1p1_feature_required(op_begin, subopcode,
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::relaxed_simd,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }

    auto const fail_simd_operand_type{[&](curr_operand_stack_value_type expected,
                                          curr_operand_stack_value_type actual) constexpr UWVM_THROWS
                                      {
                                          // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                          // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                          // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

    ::uwvm2::validation::standard::wasm3::validated_simd_event simd_event{.opcode = subopcode};
    bool normalized_simd_event_complete{};
    auto const valid_simd_opcode{shared_simd::visit_wasm1p1_simd_instruction(
        static_cast<wasm1p1_simd_code>(subopcode),
        [&]<shared_simd::wasm1p1_simd_details::simd_code Op,
            shared_simd::wasm1p1_simd_instruction_kind Kind,
            shared_simd::wasm1p1_simd_scalar_kind ScalarKind,
            ::std::size_t LaneCount,
            ::std::uint_least32_t MaxAlign>() constexpr UWVM_THROWS -> bool
        {
            static_cast<void>(Op);
            auto address_type{curr_operand_stack_value_type::i32};
            if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load ||
                         Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
            {
                // The bounded scanner checks flags, optional memory index and offset before committing code_curr.
                auto const memarg{::uwvm2::validation::standard::wasm3::read_memory_argument64(
                    code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, ::uwvm2::validation::standard::wasm3::uses_core3_validation_policy(wasm1p1_para), all_memory_count, memory_address_type_at, MaxAlign, u8"simd.memory", err)};
                // op_name [validated memarg] ...
                // [safe                   ] unsafe (could be code_end)
                //                           ^^ code_curr
                simd_event.memory = memarg; // Owned checked DATA, no second memarg decode.
                address_type = memarg.address_type == ::uwvm2::validation::standard::wasm3::storage_address_type::i64 ?
                    curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32;
                if constexpr(LaneCount != 0uz)
                {
                    auto const lane{read_u8_immediate(code_curr, code_end, op_begin, u8"simd.memory.lane")};
                    if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { fail_invalid_immediate(op_begin, u8"simd.memory.lane"); }
                    simd_event.lane = lane;
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
                        auto const byte{code_curr[i]};
                        if(::std::to_integer<::std::uint_least8_t>(byte) >= 32u) [[unlikely]]
                        {
                            fail_invalid_immediate(op_begin, u8"i8x16.shuffle");
                        }
                        simd_event.vector_bytes[i] = byte;
                    }
                }
                else
                {
                    // [complete 16-byte vector immediate] | code_end
                    // [safe 16 bytes] bounds above; DATA copy precedes the sole cursor advance.
                    for(::std::size_t i{}; i != 16uz; ++i) { simd_event.vector_bytes[i] = code_curr[i]; }
                }
                // [i8x16.shuffle immediate (16 bytes)] next opcode ... code_end
                // [safe                              ] unsafe (could be code_end)
                // ^^ code_curr; the complete 16-byte immediate and its lane bytes were checked above.
                code_curr += 16uz;
                // [i8x16.shuffle immediate (16 bytes)] next opcode ... code_end
                // [safe                              ] unsafe (could be code_end)
                //                                    ^^ code_curr; it may equal code_end.
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::extract_lane ||
                              Kind == shared_simd::wasm1p1_simd_instruction_kind::replace_lane)
            {
                auto const lane{read_u8_immediate(code_curr, code_end, op_begin, u8"simd.lane")};
                if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { fail_invalid_immediate(op_begin, u8"simd.lane"); }
                simd_event.lane = lane;
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
                    pop_simd_operand(address_type);
                }
                else
                {
                    require_simd_operands(2uz);
                    pop_simd_operand(curr_operand_stack_value_type::v128);
                    pop_simd_operand(address_type);
                }
                operand_stack_push(curr_operand_stack_value_type::v128);
            }
            else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
            {
                require_simd_operands(2uz);
                pop_simd_operand(curr_operand_stack_value_type::v128);
                pop_simd_operand(address_type);
            }
            // The original typed transition above has completed. An internal DATA
            // description failure declines IR lowering, not specification validation.
            normalized_simd_event_complete = ::uwvm2::validation::standard::wasm3::complete_simd_event(
                simd_event, llvm_jit_simd_event_descriptor<Kind, ScalarKind, LaneCount, MaxAlign>(),
                static_cast<::std::size_t>(op_begin - code_begin),
                static_cast<::std::size_t>(code_curr - op_begin), control_flow_stack.size(), !is_polymorphic);
            return true;
        })};

    if(!valid_simd_opcode) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
        err.err_code = code_validation_error_code::illegal_opbase;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!normalized_simd_event_complete ||
           !try_emit_runtime_local_func_llvm_jit_simd(llvm_jit_emit_state, simd_event)) [[unlikely]]
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
    // [numeric prefix] subopcode LEB ... code_end
    // [safe         ] unsafe (could be code_end)
    // ^^ code_curr; outer dispatch proved the prefix byte exists.
    auto const op_begin{code_curr};
    ++code_curr;
    // [numeric prefix] subopcode LEB ... code_end
    // [safe         ] unsafe (could be code_end)
    //                  ^^ code_curr; bounded LEB decoding checks the subopcode.

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

    auto const check_memory_index{
        [&](validation_module_traits_t::wasm_u32 memory_index, ::uwvm2::utils::container::u8string_view op_name) constexpr UWVM_THROWS
        {
            if(all_memory_count == 0u) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.no_memory.op_code_name = op_name;
                err.err_selectable.no_memory.align = 0u;
                err.err_selectable.no_memory.offset = 0u;
                err.err_code = code_validation_error_code::no_memory;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            if(memory_index >= all_memory_count) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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
                                                 // [caller-saved opcode/prefix] immediate bytes ... | code_end
                                                 // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                                                 // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

    auto const validate_checked_bulk_memory{[&](::uwvm2::utils::container::u8string_view name,
        ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const& decoded) constexpr UWVM_THROWS
    {
        namespace bulk = ::uwvm2::validation::standard::wasm3;
        decltype(try_pop_concrete_operand()) actual_bulk_operand{};
        auto const consume_bulk_operand{[&]() constexpr noexcept
        {
            // The common sequence preflight and single-pop proof bound this owned
            // current-frame operand removal. No source or guest pointer advances.
            actual_bulk_operand = try_pop_concrete_operand();
            return bulk::core3_operand{bulk::core3_operand_effective_type(actual_bulk_operand),
                !actual_bulk_operand.from_stack || actual_bulk_operand.is_unknown};
        }};
        auto const failure{bulk::validate_bulk_memory_operand_sequence(
            decoded, is_polymorphic, concrete_operand_count, consume_bulk_operand)};
        if(failure.error == bulk::typed_stack_error::stack_underflow) [[unlikely]]
        { report_operand_stack_underflow(op_begin, name, bulk::bulk_memory_operand_count(decoded)); }
        if(failure.error != bulk::typed_stack_error::ok) [[unlikely]]
        {
            // [dispatch-checked FC][checked data/memory immediates] | code_end
            // [safe opcode        ][safe                         ] | one-past not read
            // ^^ op_begin -> err.err_curr: copy the original diagnostic span only.
            err.err_curr = op_begin;
            auto const core{bulk::bulk_memory_expected_operand_type(decoded, failure.failed_pop_index)};
            auto const expected{core.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64 ?
                curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32};
            err.err_selectable.numeric_operand_type_mismatch = {.op_code_name = name,
                .expected_type = to_wasm1_diagnostic_value_type(expected), .actual_type = to_wasm1_diagnostic_value_type(actual_bulk_operand.type)};
            err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
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

    auto const emit_checked_bulk_memory{[&](::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const& decoded) constexpr noexcept
    {
        if(emit_llvm_jit_active)
        {
            llvm_jit_instruction_emitted_inline = true;
            ::uwvm2::validation::standard::wasm3::validated_bulk_memory_event event{};
            // [original checked FC ... code_curr] | code_end
            // [safe same expression allocation ] | offsets only; no raw reread.
            if(!::uwvm2::validation::standard::wasm3::complete_bulk_memory_event(event, decoded,
                static_cast<::std::size_t>(op_begin - code_begin), static_cast<::std::size_t>(code_curr - op_begin),
                control_flow_stack.size(), !is_polymorphic) ||
               !try_emit_runtime_local_func_llvm_jit_bulk_memory(llvm_jit_emit_state, event)) [[unlikely]]
            { disable_inline_llvm_jit_emission(); }
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
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(memory_index, u8"memory.init");
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::init,
                .data_index = data_index, .destination_memory_index = memory_index,
                .destination_address = memory_address_type_at(memory_index)};
            validate_checked_bulk_memory(u8"memory.init", decoded_bulk);
            emit_checked_bulk_memory(decoded_bulk);
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
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::data_drop, .data_index = data_index};
            validate_checked_bulk_memory(u8"data.drop", decoded_bulk);
            emit_checked_bulk_memory(decoded_bulk);
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
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const dst_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(dst_memory_index, u8"memory.copy");
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const src_memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(src_memory_index, u8"memory.copy");
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::copy,
                .destination_memory_index = dst_memory_index, .source_memory_index = src_memory_index,
                .destination_address = memory_address_type_at(dst_memory_index), .source_address = memory_address_type_at(src_memory_index)};
            validate_checked_bulk_memory(u8"memory.copy", decoded_bulk);
            emit_checked_bulk_memory(decoded_bulk);
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
            // [opcode] memidx ... (code_end): bounded decoder commits code_curr only within this range.
            auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
            // [opcode memidx] ... unsafe (could be code_end); code_curr points after the decoded index.
            check_memory_index(memory_index, u8"memory.fill");
            ::uwvm2::validation::standard::wasm3::decoded_bulk_memory_instruction const decoded_bulk{
                .kind = ::uwvm2::validation::standard::wasm3::bulk_memory_instruction_kind::fill,
                .destination_memory_index = memory_index, .destination_address = memory_address_type_at(memory_index)};
            validate_checked_bulk_memory(u8"memory.fill", decoded_bulk);
            emit_checked_bulk_memory(decoded_bulk);
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
            // [elemsec.elems begin ... element_index ... end) is retained for this LLVM translation.
            // [safe                                          ] check_element_index proved the borrow.
            //                 ^^ element is metadata only; code_curr and emitted LLVM pointers do not advance.
            auto const& element{elemsec.elems.index_unchecked(element_index).storage.segment};
            auto const element_value_type{static_cast<curr_operand_stack_value_type>(
                ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(element.reftype))};
            auto const table_value_type{get_table_value_type(table_index)};
            auto const table_core{get_table_core_type(table_index)};
            auto const element_core{element.has_core_type ? element.core_type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(element_value_type)};
            auto const destination_core{table_core.has_type ? table_core.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(table_value_type)};
            if(!runtime_core3_value_type_matches(
                element_core, destination_core, typesec.owned_signatures)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.init";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_value_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(element_value_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            validate_i32_operands(u8"table.init", 2uz);
            validate_table_operand(op_begin, u8"table.init", table_operand_type(table_index));
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
            auto const dst_core{get_table_core_type(dst_table_index)};
            auto const src_core{get_table_core_type(src_table_index)};
            // Each checked table declaration has an independent rich-type witness.
            // Legacy funcref is an erased supertype; its missing witness must not
            // erase a typed destination's heap or nullability. Metadata borrows
            // above stay within the index-checked table records; no cursor moves.
            auto const source_core{src_core.has_type ? src_core.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(src_type)};
            auto const destination_core{dst_core.has_type ? dst_core.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(dst_type)};
            auto const compatible{runtime_core3_value_type_matches(
                source_core, destination_core, typesec.owned_signatures)};
            if(!compatible) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.copy";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(dst_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(src_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const destination_type{table_operand_type(dst_table_index)};
            auto const source_type{table_operand_type(src_table_index)};
            validate_table_operand(op_begin, u8"table.copy", destination_type == curr_operand_stack_value_type::i64 &&
                source_type == curr_operand_stack_value_type::i64 ? curr_operand_stack_value_type::i64 : curr_operand_stack_value_type::i32);
            validate_table_operand(op_begin, u8"table.copy", source_type);
            validate_table_operand(op_begin, u8"table.copy", destination_type);
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
            auto const table_core{get_table_core_type(table_index)};
            if(!is_polymorphic && concrete_operand_count() < 2uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.grow", 2uz); }
            auto const delta{try_pop_concrete_operand()};
            if(delta.from_stack && !delta.is_unknown && delta.type != table_operand_type(table_index)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(delta.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const value{try_pop_concrete_operand()};
            if(value.from_stack && !value.is_unknown &&
               !core3_value_matches(value, table_type, table_core.type, table_core.has_type)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.grow";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            operand_stack_push(table_operand_type(table_index));
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
            operand_stack_push(table_operand_type(table_index));
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
            auto const table_core{get_table_core_type(table_index)};
            if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"table.fill", 3uz); }
            auto const len{try_pop_concrete_operand()};
            if(len.from_stack && !len.is_unknown && len.type != table_operand_type(table_index)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(len.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const value{try_pop_concrete_operand()};
            if(value.from_stack && !value.is_unknown &&
               !core3_value_matches(value, table_type, table_core.type, table_core.has_type)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(value.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const index{try_pop_concrete_operand()};
            if(index.from_stack && !index.is_unknown && index.type != table_operand_type(table_index)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch.op_code_name = u8"table.fill";
                err.err_selectable.numeric_operand_type_mismatch.expected_type = to_wasm1_diagnostic_value_type(table_operand_type(table_index));
                err.err_selectable.numeric_operand_type_mismatch.actual_type = to_wasm1_diagnostic_value_type(index.type);
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            emit_validated_prefixed_instruction();
            break;
        }
        [[unlikely]] default:
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.u8 = static_cast<::std::uint_least8_t>(subopcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    break;
}
