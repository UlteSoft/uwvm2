/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
case static_cast<wasm_byte>(0xfbu):
{
    // [FB] subopcode ... code_end
    // [safe] unsafe (could be code_end)
    // ^^ op_begin borrows the outer-dispatch-checked prefix byte.
    auto const op_begin{code_curr};
    ++code_curr;
    // [FB] subopcode ... code_end
    // [safe] unsafe (could be code_end)
    //        ^^ code_curr: only the proved prefix was consumed; scanner checks the complete immediate.
    if(wasm1p1_para.disable_gc) [[unlikely]]
    {
        fail_wasm1p1_feature_required(op_begin, 0xfbu,
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc,
            ::uwvm2::parser::wasm::base::wasm1p1_error_subject::instruction);
    }
    auto const decoded{::uwvm2::validation::standard::wasm3::scan_gc_instruction(code_curr, code_end)};
    if(decoded.error != ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok) [[unlikely]]
    { fail_invalid_immediate(op_begin, u8"gc"); }
    // [FB][checked complete subopcode/immediate] next ... code_end
    // [safe                                   ] unsafe (could be code_end)
    //                                           ^^ code_curr: scanner changed it only after bounded decoding.
    ::uwvm2::validation::standard::wasm3::require_gc_cast_exception_policy(
        !wasm1p1_para.disable_exceptions, decoded.opcode, decoded.from, decoded.to, op_begin, err);
    namespace t3 = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    namespace store_ns = ::uwvm2::uwvm::runtime::storage;
    switch(decoded.opcode)
    {
        case 0u: case 1u: case 2u: case 3u: case 4u: case 5u:
        case 6u: case 7u: case 8u: case 9u: case 10u:
        case 11u: case 12u: case 13u: case 14u: case 15u: case 16u: case 17u:
        case 18u: case 19u:
        {
            using operation = ::uwvm2::runtime::compiler::uwvm_int::optable::gc_aggregate_operation;
            auto* store{curr_module.gc_store.get()};
            if(store == nullptr || !store->valid()) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc type storage"); }
            namespace v3 = ::uwvm2::validation::standard::wasm3;
            auto const type_index{decoded.first};
            auto const field_index{decoded.second};
            auto const carrier = [](t3::core_value_type value) constexpr noexcept -> curr_operand_stack_value_type
            {
                switch(value.kind)
                {
                    case t3::value_kind::i32: return curr_operand_stack_value_type::i32;
                    case t3::value_kind::i64: return curr_operand_stack_value_type::i64;
                    case t3::value_kind::f32: return curr_operand_stack_value_type::f32;
                    case t3::value_kind::f64: return curr_operand_stack_value_type::f64;
                    case t3::value_kind::v128: return curr_operand_stack_value_type::v128;
                    case t3::value_kind::reference:
                    {
                        auto const heap{value.heap.code};
                        return heap == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                               heap == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern) ?
                               curr_operand_stack_value_type::externref :
                               heap == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                               heap == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn) ?
                               static_cast<curr_operand_stack_value_type>(0x69u) : curr_operand_stack_value_type::funcref;
                    }
                }
                return curr_operand_stack_value_type::i32;
            };
            ::std::size_t pop_count{};
            ::std::size_t input_bytes{};
            bool has_result{};
            t3::core_value_type result_type{};
            auto const push_type = [&](t3::core_value_type exact) constexpr noexcept
            {
                operand_stack_push(carrier(exact));
                auto& top{operand_stack.back_unchecked()}; // The append above proves a live entry.
                top.core_type = exact;
                top.has_core_type = true;
                has_result = true;
                result_type = exact;
            };
            // Adapt the compiler's existing stack, while collecting only emission
            // metadata. No second operand stack or instruction-validation pass is built.
            struct gc_stack_adapter
            {
                decltype(concrete_operand_count) const& count;
                decltype(try_pop_concrete_operand) const& consume;
                decltype(push_type) const& append;
                bool const& polymorphic;
                ::std::size_t& consumed;
                ::std::size_t& bytes;
                bool measure_inputs;
                [[nodiscard]] inline bool record_inputs(t3::core_value_type value, ::std::size_t requested) noexcept
                {
                    if(requested > (::std::numeric_limits<::std::size_t>::max)() - consumed) { return false; }
                    consumed += requested;
                    if(!measure_inputs) { return true; }
                    auto const width{store_ns::gc_object_value::wasm_value_size(value.kind)};
                    if(width == 0uz) { return false; }
                    if(requested > ((::std::numeric_limits<::std::size_t>::max)() - bytes) / width)
                    {
                        // An unreachable array.new_fixed may request more bytes than
                        // this host can address. It emits no reachable handler payload.
                        bytes = 0uz;
                        return polymorphic;
                    }
                    bytes += requested * width;
                    return true;
                }
                [[nodiscard]] inline v3::core3_reference_error pop_expected(
                    t3::core_value_type expected, v3::recursive_type_context const& context) noexcept
                {
                    auto const owned_consume{[&]() noexcept
                    {
                        // Shared kernel count > 0 proves the actual adapter top is live;
                        // copy its normalized rich type before retiring the stack entry.
                        auto const operand{consume()};
                        return v3::core3_operand{v3::core3_operand_effective_type(operand), !operand.from_stack || operand.is_unknown};
                    }};
                    auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                    auto const error{v3::core3_reference_error_from_typed_stack(
                        v3::pop_core3_expected_operand(polymorphic, count, owned_consume, expected, matches))};
                    if(error == v3::core3_reference_error::ok && !record_inputs(expected, 1uz))
                    { return v3::core3_reference_error::type_mismatch; }
                    return error;
                }
                [[nodiscard]] inline v3::core3_reference_error pop_repeated(
                    t3::core_value_type expected, ::std::uint_least32_t requested, v3::recursive_type_context const& context) noexcept
                {
                    auto const owned_consume{[&]() noexcept
                    {
                        // Bounded shared repetition proves each top exists above the actual frame.
                        auto const operand{consume()};
                        return v3::core3_operand{v3::core3_operand_effective_type(operand), !operand.from_stack || operand.is_unknown};
                    }};
                    auto const matches{[&](auto actual, auto wanted) noexcept { return context.matches(actual, wanted); }};
                    auto const error{v3::core3_reference_error_from_typed_stack(
                        v3::pop_core3_repeated_operands(polymorphic, count, owned_consume, expected, requested, matches))};
                    if(error == v3::core3_reference_error::ok && !record_inputs(expected, requested))
                    { return v3::core3_reference_error::type_mismatch; }
                    return error;
                }
                inline void push(t3::core_value_type value) noexcept { append(value); }
            };
            gc_stack_adapter stack{concrete_operand_count, try_pop_concrete_operand, push_type,
                is_polymorphic, pop_count, input_bytes, decoded.opcode == 0u || decoded.opcode == 8u};
            auto const* const recursive_types{curr_module.type_section_storage.core3_recursive_types_ptr};
            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
            v3::recursive_type_context const empty_context{};
            auto const& context{retained_context == nullptr ? empty_context : *retained_context};
            if(decoded.opcode != 15u && (recursive_types == nullptr || retained_context == nullptr)) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc type index"); }
            if((decoded.opcode == 10u || decoded.opcode == 19u) && !gc_element_types_ready)
            {
                gc_element_types.reserve(curr_module.local_defined_element_vec_storage.size());
                for(auto const& element : curr_module.local_defined_element_vec_storage)
                {
                    // [runtime element records] own stable parser-declaration borrows.
                    // [safe                   ] check the pointer before reading its payload.
                    // ^^ element_type_ptr is copied, never advanced or retained by generated code.
                    auto const* const declaration{element.element_type_ptr};
                    if(declaration == nullptr) [[unlikely]] { runtime_storage_bug(); }
                    auto const& payload{declaration->storage.segment};
                    gc_element_types.push_back_unchecked(payload.has_core_type ? payload.core_type :
                        v3::core3_legacy_carrier_type(payload.reftype));
                }
                gc_element_types_ready = true;
            }
            v3::core3_gc_environment const environment{{},
                {gc_element_types.cbegin(), gc_element_types.size()},
                curr_module.data_count_section_present ? curr_module.data_count_section_count : 0u, recursive_types};
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
            // GC aggregate operations are register-ring barriers: the variable-size source
            // sequence is materialized in order before the handler reads a bounded stack range.
            if(!is_polymorphic) { stacktop_flush_all_to_operand_stack(bytecode); }
            auto const emit = [&]<operation Op>() constexpr UWVM_THROWS
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_gc_aggregate_fptr_from_tuple<Op, CompileOption>(interpreter_tuple));
                emit_imm_to(bytecode, store);
                if constexpr(Op != operation::array_len) { emit_imm_to(bytecode, type_index); }
                if constexpr(Op == operation::struct_get || Op == operation::struct_get_s ||
                             Op == operation::struct_get_u || Op == operation::struct_set)
                { emit_imm_to(bytecode, field_index); }
                if constexpr(Op == operation::struct_new || Op == operation::array_new_fixed)
                { emit_imm_to(bytecode, input_bytes); }
                if constexpr(Op == operation::array_new_fixed) { emit_imm_to(bytecode, decoded.second); }
                if constexpr(Op == operation::array_new_data || Op == operation::array_new_elem ||
                             Op == operation::array_init_data || Op == operation::array_init_elem)
                {
                    // The module outlives its compiled functions; the borrowed
                    // address is stable and typed const throughout execution.
                    emit_imm_to(bytecode, ::std::addressof(curr_module));
                    emit_imm_to(bytecode, decoded.second);
                }
                if constexpr(Op == operation::array_copy)
                { emit_imm_to(bytecode, decoded.second); }
            };
            switch(decoded.opcode)
            {
                case 0u: emit.template operator()<operation::struct_new>(); break;
                case 1u: emit.template operator()<operation::struct_new_default>(); break;
                case 2u: emit.template operator()<operation::struct_get>(); break;
                case 3u: emit.template operator()<operation::struct_get_s>(); break;
                case 4u: emit.template operator()<operation::struct_get_u>(); break;
                case 5u: emit.template operator()<operation::struct_set>(); break;
                case 6u: emit.template operator()<operation::array_new>(); break;
                case 7u: emit.template operator()<operation::array_new_default>(); break;
                case 8u: emit.template operator()<operation::array_new_fixed>(); break;
                case 9u: emit.template operator()<operation::array_new_data>(); break;
                case 10u: emit.template operator()<operation::array_new_elem>(); break;
                case 11u: emit.template operator()<operation::array_get>(); break;
                case 12u: emit.template operator()<operation::array_get_s>(); break;
                case 13u: emit.template operator()<operation::array_get_u>(); break;
                case 14u: emit.template operator()<operation::array_set>(); break;
                case 15u: emit.template operator()<operation::array_len>(); break;
                case 16u: emit.template operator()<operation::array_fill>(); break;
                case 17u: emit.template operator()<operation::array_copy>(); break;
                case 18u: emit.template operator()<operation::array_init_data>(); break;
                case 19u: emit.template operator()<operation::array_init_elem>(); break;
                default: ::fast_io::fast_terminate();
            }
            if(has_result)
            { stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, pop_count, carrier(result_type)); }
            else { stacktop_after_pop_n_if_reachable(bytecode, pop_count); }
            break;
        }
        case 20u: case 21u: // ref.test and ref.test null
        case 22u: case 23u: // ref.cast and ref.cast null
        {
            auto* store{curr_module.gc_store.get()};
            if(store == nullptr || !store->valid()) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc type storage"); }
            ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
            auto const* retained_context{curr_module.type_section_storage.core3_context_ptr};
            auto const& context{retained_context == nullptr ? empty_context : *retained_context};
            t3::heap_type top{};
            if(!::uwvm2::validation::standard::wasm3::reference_cast_details::heap_top(
                   decoded.to.heap, context, top)) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc cast heap type"); }
            if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"ref.test/ref.cast", 1uz); }
            auto const operand{try_pop_concrete_operand()};
            t3::core_value_type const expected{t3::value_kind::reference, top, true};
            auto const signatures{::uwvm2::validation::standard::wasm3::core3_signature_view<
                ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{
                    rich_owned_begin, rich_owned_available ? runtime_type_count : 0uz}};
            if(operand.from_stack && !operand.is_unknown &&
               !runtime_core3_value_type_matches(
                   ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(operand),
                   expected, signatures)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin; // Checked 0xfb prefix borrow; no pointer movement.
                err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(operand.type);
                err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const test_only{decoded.opcode <= 21u};
            curr_operand_stack_value_type result_carrier{curr_operand_stack_value_type::i32};
            if(!test_only)
            {
                result_carrier = top.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ?
                    curr_operand_stack_value_type::externref :
                    top.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ?
                    static_cast<curr_operand_stack_value_type>(0x69u) : curr_operand_stack_value_type::funcref;
            }
            operand_stack_push(result_carrier);
            if(!test_only)
            {
                operand_stack.back_unchecked().core_type = decoded.to;
                operand_stack.back_unchecked().has_core_type = true;
            }
            // Cast/test reads the complete reference carrier from the materialized operand stack.
            if(!is_polymorphic) { stacktop_flush_all_to_operand_stack(bytecode); }
            if(test_only)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_ref_cast_fptr_from_tuple<true, CompileOption>(interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_ref_cast_fptr_from_tuple<false, CompileOption>(interpreter_tuple)); }
            emit_imm_to(bytecode, store);
            emit_imm_to(bytecode, decoded.to.heap.code);
            emit_imm_to(bytecode, decoded.to.nullable);
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, result_carrier);
            break;
        }
        case 24u: case 25u: // br_on_cast / br_on_cast_fail
        case 26u: case 27u: // any.convert_extern / extern.convert_any
        {
            namespace v3 = ::uwvm2::validation::standard::wasm3;
            using core_type = t3::core_value_type;
            auto* const store{curr_module.gc_store.get()};
            if(store == nullptr || !store->valid()) [[unlikely]]
            { fail_invalid_immediate(op_begin, u8"gc type storage"); }
            v3::recursive_type_context const empty_context{};
            auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
            auto const& context{retained_context == nullptr ? empty_context : *retained_context};
            struct reference_stack_adapter
            {
                decltype(concrete_operand_count) const& count;
                decltype(try_pop_concrete_operand) const& consume;
                decltype(operand_stack_push) const& append;
                decltype(operand_stack)& values;
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
                    output = {v3::core3_operand_effective_type(operand),
                        !operand.from_stack || operand.is_unknown};
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
                    curr_operand_stack_value_type carrier{curr_operand_stack_value_type::funcref};
                    if(value.kind == t3::value_kind::reference)
                    {
                        if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::extern_) ||
                           value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noextern))
                        { carrier = curr_operand_stack_value_type::externref; }
                        else if(value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::exn) ||
                                value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::noexn))
                        { carrier = static_cast<curr_operand_stack_value_type>(0x69u); }
                    }
                    else
                    {
                        carrier = value.kind == t3::value_kind::i32 ? curr_operand_stack_value_type::i32 :
                            value.kind == t3::value_kind::i64 ? curr_operand_stack_value_type::i64 :
                            value.kind == t3::value_kind::f32 ? curr_operand_stack_value_type::f32 :
                            value.kind == t3::value_kind::f64 ? curr_operand_stack_value_type::f64 :
                                curr_operand_stack_value_type::v128;
                    }
                    append(carrier);
                    auto& top{values.back_unchecked()}; // append() established a live final entry.
                    top.core_type = value;
                    top.has_core_type = true;
                    constexpr auto no_witness{(::std::numeric_limits<::std::size_t>::max)()};
                    if(value.kind == t3::value_kind::reference && value.heap.is_defined())
                    {
                        auto const index{static_cast<::std::uint_least64_t>(value.heap.code)};
                        // [metadata.records[0], records[size)) is the immutable validated type table.
                        // [safe                               ] contains(index) proves this kind read.
                        if(metadata.contains(index) &&
                           metadata.records.index_unchecked(static_cast<::std::size_t>(index)).kind ==
                               t3::composite_kind::function)
                        { top.exact_function_type_index = static_cast<::std::size_t>(index); }
                    }
                    else if(value.kind == t3::value_kind::reference &&
                            value.heap.code == static_cast<::std::int_least64_t>(t3::abstract_heap_type::nofunc))
                    { top.exact_function_type_index = no_witness - 1uz; }
                }
            };
            reference_stack_adapter stack{concrete_operand_count, try_pop_concrete_operand,
                operand_stack_push, operand_stack, is_polymorphic, context};
            v3::core3_reference_error failure{};
            if(decoded.opcode <= 25u)
            {
                auto const label_count{control_flow_stack.size()};
                if(static_cast<::std::uint_least64_t>(decoded.first) >= label_count) [[unlikely]]
                {
                    // [caller-saved opcode/prefix] immediate bytes ... | code_end
                    // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                    // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                    err.err_curr = op_begin; // Borrowed checked prefix; no pointer movement.
                    err.err_selectable.illegal_label_index = {.label_index = decoded.first,
                        .all_label_count = static_cast<wasm_u32>(label_count)};
                    err.err_code = code_validation_error_code::illegal_label_index;
                    ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                }
                // decoded.first < label_count proves the reverse control-frame index.
                auto& target_frame{control_flow_stack.index_unchecked(
                    label_count - 1uz - static_cast<::std::size_t>(decoded.first))};
                auto const label_carriers{target_frame.label};
                // The two endpoints borrow one retained tuple; never subtract two null endpoints.
                auto const arity{label_carriers.begin == label_carriers.end ? 0uz :
                    static_cast<::std::size_t>(label_carriers.end - label_carriers.begin)};
                ::uwvm2::utils::container::vector<core_type> label_types{};
                label_types.reserve(arity);
                for(::std::size_t i{}; i != arity; ++i)
                {
                    auto const rich{block_core_type_at(label_carriers, target_frame.signature_type_index,
                        target_frame.type != block_type::loop, target_frame.has_singleton_result_core_type,
                        target_frame.singleton_result_core_type, i)};
                    // [label_carriers.begin, label_carriers.end) contains arity live entries.
                    // [safe                                    ] i < arity proves this read.
                    label_types.push_back_unchecked(rich.has_type ? rich.type :
                        v3::core3_legacy_carrier_type(label_carriers.begin[i]));
                }
                failure = v3::validate_core3_branch_on_cast(stack, decoded.from, decoded.to,
                    {label_types.cbegin(), label_types.size()}, context, decoded.opcode == 25u, true);
                if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, u8"br_on_cast", arity); }
                if(failure != v3::core3_reference_error::ok) [[unlikely]]
                { fail_invalid_immediate(op_begin, u8"br_on_cast type/label"); }
                if(!is_polymorphic && codegen_reachable)
                {
                    // The handler reads the complete top reference in memory. Numeric ring entries
                    // in the label prefix are flushed once before the conditional branch.
                    stacktop_flush_all_to_operand_stack(bytecode);
                    auto const target_label_id{get_branch_target_label_id(target_frame)};
                    auto branch_label_id{target_label_id};
                    auto const taken_size{operand_stack.size()}; // Cast branches preserve prefix + reference.
                    auto const target_base{target_frame.operand_stack_base};
                    auto const need_repair{taken_size != target_base + arity};
                    if(need_repair || stacktop_enabled)
                    {
                        auto const thunk_start{thunks.size()};
                        branch_label_id = new_label(true);
                        set_label_offset(branch_label_id, thunk_start);
                        auto const saved_curr_stacktop{curr_stacktop};
                        auto const saved_memory_count{stacktop_memory_count};
                        auto const saved_cache_count{stacktop_cache_count};
                        auto const saved_cache_i32_count{stacktop_cache_i32_count};
                        auto const saved_cache_i64_count{stacktop_cache_i64_count};
                        auto const saved_cache_f32_count{stacktop_cache_f32_count};
                        auto const saved_cache_f64_count{stacktop_cache_f64_count};
                        auto const saved_codegen_operand_stack{codegen_operand_stack};
                        if(need_repair)
                        { emit_preserve_top_values_drop_to_base_restore(thunks, label_carriers, target_base, taken_size, true); }
                        if constexpr(stacktop_enabled && strict_cf_entry_like_call)
                        { stacktop_canonicalize_edge_to_memory(thunks); }
                        bool const loop_transform{stacktop_enabled && target_frame.type == block_type::loop &&
                            stacktop_regtransform_cf_entry && stacktop_cache_count != 0uz};
                        if(thunks.size() == thunk_start && !loop_transform) { branch_label_id = target_label_id; }
                        else if(loop_transform) { emit_br_to_with_stacktop_transform(thunks, target_label_id, true); }
                        else { emit_br_to(thunks, target_label_id, true); }
                        if constexpr(stacktop_enabled)
                        {
                            if constexpr(!strict_cf_entry_like_call)
                            {
                                if(target_label_id == target_frame.end_label_id)
                                {
                                    target_frame.stacktop_has_end_state = true;
                                    target_frame.stacktop_currpos_at_end = curr_stacktop;
                                    target_frame.stacktop_memory_count_at_end = stacktop_memory_count;
                                    target_frame.stacktop_cache_count_at_end = stacktop_cache_count;
                                    target_frame.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                                    target_frame.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                                    target_frame.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                                    target_frame.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                                    target_frame.codegen_operand_stack_at_end = codegen_operand_stack;
                                }
                            }
                        }
                        curr_stacktop = saved_curr_stacktop;
                        stacktop_memory_count = saved_memory_count;
                        stacktop_cache_count = saved_cache_count;
                        stacktop_cache_i32_count = saved_cache_i32_count;
                        stacktop_cache_i64_count = saved_cache_i64_count;
                        stacktop_cache_f32_count = saved_cache_f32_count;
                        stacktop_cache_f64_count = saved_cache_f64_count;
                        codegen_operand_stack = saved_codegen_operand_stack;
                    }
                    if(decoded.opcode == 24u)
                    { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_br_on_cast_fptr_from_tuple<true, CompileOption>(interpreter_tuple)); }
                    else
                    { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_br_on_cast_fptr_from_tuple<false, CompileOption>(interpreter_tuple)); }
                    emit_imm_to(bytecode, store);
                    emit_imm_to(bytecode, static_cast<::std::int64_t>(decoded.to.heap.code));
                    emit_imm_to(bytecode, decoded.to.nullable);
                    emit_ptr_label_placeholder(branch_label_id, false);
                }
            }
            else
            {
                failure = v3::validate_core3_convert_reference(stack, context, decoded.opcode == 26u, true);
                if(failure == v3::core3_reference_error::stack_underflow) [[unlikely]]
                { report_operand_stack_underflow(op_begin, u8"reference conversion", 1uz); }
                if(failure != v3::core3_reference_error::ok) [[unlikely]]
                { fail_invalid_immediate(op_begin, u8"reference conversion type"); }
                if(!is_polymorphic && codegen_reachable)
                {
                    stacktop_flush_all_to_operand_stack(bytecode);
                    if(decoded.opcode == 26u)
                    { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_extern_convert_fptr_from_tuple<true, CompileOption>(interpreter_tuple)); }
                    else
                    { emit_opfunc_to(bytecode, translate::get_uwvmint_gc_extern_convert_fptr_from_tuple<false, CompileOption>(interpreter_tuple)); }
                    emit_imm_to(bytecode, store);
                    stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz,
                        decoded.opcode == 26u ? curr_operand_stack_value_type::funcref :
                            curr_operand_stack_value_type::externref);
                }
            }
            break;
        }
        case 28u: // ref.i31
        {
            namespace v3 = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) input{};
            auto const consume{[&]() constexpr noexcept
            {
                // Shared count > 0 proves the current frame's concrete top;
                // copy its normalized rich type before owned stack retirement.
                input = try_pop_concrete_operand();
                return v3::core3_operand{v3::core3_operand_effective_type(input), !input.from_stack || input.is_unknown};
            }};
            auto const matches{[&](auto actual, auto expected) constexpr noexcept
            { return runtime_core3_value_type_matches(actual, expected,
                v3::core3_signature_view<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{
                    rich_owned_begin, rich_owned_available ? runtime_type_count : 0uz}); }};
            auto const transition{v3::apply_core3_i31_typed_transition(
                decoded.opcode, is_polymorphic, concrete_operand_count, consume, matches)};
            if(!transition.supported) [[unlikely]] { fail_invalid_immediate(op_begin, u8"gc i31"); }
            if(transition.error == v3::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, u8"ref.i31", 1uz); }
            if(transition.error != v3::typed_stack_error::ok) [[unlikely]]
            {
                // op_begin borrows the checked FB prefix; this diagnostic copies
                // that pointer only and never reads or advances a guest cursor.
                err.err_curr = op_begin;
                err.err_selectable.numeric_operand_type_mismatch = {
                    .op_code_name = u8"ref.i31",
                    .expected_type = to_wasm1_value_type(curr_operand_stack_value_type::i32),
                    .actual_type = to_wasm1_value_type(input.type)};
                err.err_code = code_validation_error_code::numeric_operand_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            operand_stack_push(curr_operand_stack_value_type::funcref);
            operand_stack.back_unchecked().core_type = transition.output;
            operand_stack.back_unchecked().has_core_type = true;
            if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32))
            {
                if(!is_polymorphic && codegen_reachable)
                {
                    // ref.i31 reads the top i32 from its ring slot and writes
                    // the 16-byte reference to memory. The memory layout must
                    // contain every deeper operand first; otherwise a cached
                    // lower i32 would remain below a memory-resident reference,
                    // reversing the register-ring prefix/suffix model.
                    if(stacktop_cache_count == 0uz)
                    { stacktop_fill_one_from_memory_to(bytecode); }
                    while(stacktop_cache_count > 1uz)
                    { stacktop_spill_one_deepest_to(bytecode, 1uz); }
                }
            }
            emit_opfunc_to(bytecode, translate::get_uwvmint_ref_i31_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            stacktop_after_pop_n_push1_memory_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::funcref);
            break;
        }
        case 29u: // i31.get_s
        case 30u: // i31.get_u
        {
            auto const name{decoded.opcode == 29u ? ::uwvm2::utils::container::u8string_view{u8"i31.get_s"} :
                ::uwvm2::utils::container::u8string_view{u8"i31.get_u"}};
            namespace v3 = ::uwvm2::validation::standard::wasm3;
            decltype(try_pop_concrete_operand()) input{};
            auto const consume{[&]() constexpr noexcept
            {
                // Shared count > 0 proves the current frame's concrete top;
                // copy its normalized rich type before owned stack retirement.
                input = try_pop_concrete_operand();
                return v3::core3_operand{v3::core3_operand_effective_type(input), !input.from_stack || input.is_unknown};
            }};
            auto const matches{[&](auto actual, auto expected) constexpr noexcept
            { return runtime_core3_value_type_matches(actual, expected,
                v3::core3_signature_view<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{
                    rich_owned_begin, rich_owned_available ? runtime_type_count : 0uz}); }};
            auto const transition{v3::apply_core3_i31_typed_transition(
                decoded.opcode, is_polymorphic, concrete_operand_count, consume, matches)};
            if(!transition.supported) [[unlikely]] { fail_invalid_immediate(op_begin, u8"gc i31"); }
            if(transition.error == v3::typed_stack_error::stack_underflow) [[unlikely]]
            { report_operand_stack_underflow(op_begin, name, 1uz); }
            if(transition.error != v3::typed_stack_error::ok) [[unlikely]]
            {
                // op_begin borrows the checked FB prefix; this diagnostic copies
                // that pointer only and never reads or advances a guest cursor.
                err.err_curr = op_begin;
                err.err_selectable.wasm1p1_invalid_reference_type.value = static_cast<wasm_byte>(input.type);
                err.err_code = code_validation_error_code::wasm1p1_invalid_reference_type;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            operand_stack_push(curr_operand_stack_value_type::i32);
            if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32))
            { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
            if(decoded.opcode == 29u)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_i31_get_fptr_from_tuple<CompileOption, true>(curr_stacktop, interpreter_tuple)); }
            else
            { emit_opfunc_to(bytecode, translate::get_uwvmint_i31_get_fptr_from_tuple<CompileOption, false>(curr_stacktop, interpreter_tuple)); }
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
            break;
        }
        [[unlikely]] default:
        {
            // The GC immediate scanner consumed the entire standard immediate. Aggregate and cast
            // execution are added separately; never treat a recognized opcode as a no-op.
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Checked prefix borrow; no pointer arithmetic or dereference.
            err.err_selectable.u8 = static_cast<wasm_byte>(decoded.opcode);
            err.err_code = code_validation_error_code::illegal_opbase;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    break;
}
