// Call-related opcodes are the boundary between the translated interpreter body and runtime call
// machinery. The comments emphasize why operands must be materialized, how direct-call metadata is
// selected, and why stack-top fast paths are guarded by narrow state checks.
/// @warning Extension point: new function result/parameter value categories require call materialization, ABI lowering, and stack repair updates here.
case wasm1_code::return_:
{
    // `return` is equivalent to branching to the function frame. We validate result arity/types and
    // then repair the operand stack because the runtime return helper expects only function results.
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

    auto const& func_frame{control_flow_stack.index_unchecked(0u)};
    ::std::size_t const return_arity{static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

    if(!is_polymorphic && concrete_operand_count() < return_arity) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"return", return_arity); }

    if(return_arity != 0uz)
    {
        auto const available_result_count{concrete_operand_count()};
        auto const concrete_to_check{available_result_count < return_arity ? available_result_count : return_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{func_frame.result.begin[return_arity - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            auto const matches{rich_owned_available && !actual_operand.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                    rich_owned_begin[curr_owned_type_index].results.index_unchecked(return_arity - 1uz - i),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                stack_entry_type_matches(actual_operand, expected_type)};
            if(!matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Translate: `return` terminates the function. Repair the operand stack so it contains only the function results.
    if(is_polymorphic) { emit_return_to(bytecode); }
    else
    {
        auto const target_base{func_frame.operand_stack_base};  // function base (0)
        auto const curr_size{operand_stack.size()};

        if(return_arity == 0uz)
        {
            // Safety: `target_base` must be <= `curr_size` in the non-polymorphic path.
            emit_drop_to_stack_size_no_fill(bytecode, target_base);
            emit_return_to(bytecode);
        }
        else
        {
            if(curr_size > target_base + return_arity)
            {
                emit_preserve_top_values_drop_to_base_restore(bytecode, func_frame.result, target_base, curr_size, false);
            }

            emit_return_to(bytecode);
        }
    }

    if(return_arity != 0uz) { operand_stack_pop_n(return_arity); }

    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;
    codegen_reachable = false;

    break;
}
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Tail-call opcode extends the shared wasm1 enum.
#endif
case static_cast<wasm1_code>(0x12u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
{
    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x12u, code_curr, err);
    // Direct calls can target imports or local-defined functions. The translator resolves local
    // callees to compiled return_call-info records when possible so the runtime bridge can skip index lookup.
    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // return_call     func_index ...
    // [ safe ] unsafe (could be the section_end)
    //          ^^ code_curr

    wasm_u32 func_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(func_index))};
    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_function_index_encoding;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
    }

    // return_call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //      ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(func_next);

    // return_call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //                ^^ code_curr

    auto const all_function_size{import_func_count + local_func_count};
    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
        err.err_code = code_validation_error_code::invalid_function_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* callee_type_ptr{};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        auto const& imported_rec{curr_module.imported_function_vec_storage.index_unchecked(static_cast<::std::size_t>(func_index))};
        auto const imported_func_ptr{imported_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        // [validated import/type storage] end; function/type index checked above.
        // [safe                         ]; borrowed until module retirement.
        // ^^ callee_type_ptr receives the complete function-type record.
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        // [validated import/type storage] end; function/type index checked above.
        // [safe                         ]; borrowed until module retirement.
        // ^^ callee_type_ptr receives the complete function-type record.
        callee_type_ptr = curr_module.local_defined_function_vec_storage.index_unchecked(local_idx).function_type_ptr;
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};
    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
    ::uwvm2::validation::standard::wasm3::validate_tail_call_results(control_flow_stack.index_unchecked(0u).result, callee_type.result, op_begin, u8"return_call", err);
    auto const rich_callee_index{rich_type_index_from_pointer(callee_type_ptr)};
    // [rich_owned_begin, rich_owned_end) is initializer-retained for this compilation.
    // [safe                            ] only an index below runtime_type_count is read.
    //                   ^^ rich_callee is a borrowed validation-only signature.
    auto const* rich_callee{rich_callee_index < runtime_type_count ? ::std::addressof(rich_owned_begin[rich_callee_index]) : nullptr};
    if(rich_callee != nullptr)
    {
        auto const& caller_results{rich_owned_begin[curr_owned_type_index].results};
        auto const& callee_results{rich_callee->results};
        auto const fail_rich_tail_result{[&]() UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Dispatch-checked opcode; no input cursor advances.
            err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call";
            err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
            err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
            err.err_code = code_validation_error_code::br_value_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }};
        if(callee_results.size() != caller_results.size()) [[unlikely]] { fail_rich_tail_result(); }
        for(::std::size_t i{}; i != caller_results.size(); ++i)
        {
            if(!runtime_core3_value_type_matches(
                callee_results.index_unchecked(i), caller_results.index_unchecked(i),
                ::uwvm2::validation::standard::wasm3::core3_signature_view<
                    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count})) [[unlikely]]
            { fail_rich_tail_result(); }
        }
    }
    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"return_call", param_count); }

    // Parameters are checked from the top of the operand stack downward because Wasm pushes operands
    // in declaration order but the newest value is physically at the top.
    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                    rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                stack_entry_type_matches(actual_operand, expected_type)};
            if(!matches) [[unlikely]]
            {
                // [tail opcode] immediates ... code_end
                // [safe       ] unsafe (could be code_end); no dereference.
                // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    flush_conbine_pending();
#endif
    // Match ordinary call emission: codegen_reachable tracks register-ring
    // merges and is not an execution predicate for the uncached/byref backend.
    if(!is_polymorphic)
    {
        stacktop_flush_all_to_operand_stack(bytecode);
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if(static_cast<::std::size_t>(func_index) == function_index)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_return_call_self_fptr_from_tuple<CompileOption>(interpreter_tuple));
            emit_imm(param_bytes_off);
            // Every declared local is reset, including locals first read after this
            // instruction in bytecode order. Internal temporary slots stay private.
            emit_imm(local_offsets.index_unchecked(static_cast<::std::size_t>(all_local_count)) - param_bytes_off);
            emit_imm(operand_stack_bytes);
            auto const entry_label{new_label(false)};
            set_label_offset(entry_label, 0uz);
            // The reset handler consumes the relocated entry directly. Keep the
            // existing bounded label fixup instead of embedding a movable vector pointer.
            emit_ptr_label_placeholder(entry_label, false);
        }
        else
        {
            local_func_symbol.has_tail_transfer = true;
            emit_opfunc_to(bytecode, translate::get_uwvmint_return_call_transfer_fptr_from_tuple<CompileOption>(interpreter_tuple));
            emit_imm(options.curr_wasm_id);
            emit_imm(static_cast<::std::size_t>(func_index));
            ::std::size_t argument_bytes{};
            for(::std::size_t i{}; i != param_count; ++i)
            {
                auto const size{operand_stack_valtype_size(callee_type.parameter.begin[i])};
                if(size > SIZE_MAX - argument_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                argument_bytes += size;
            }
            emit_imm(argument_bytes);
        }
    }
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;
    codegen_reachable = false;
    break;
}
case wasm1_code::call:
{
    // Direct calls can target imports or local-defined functions. The translator resolves local
    // callees to compiled call-info records when possible so the runtime bridge can skip index lookup.
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

    wasm_u32 func_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(func_index))};
    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_function_index_encoding;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(func_err);
    }

    // call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //      ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(func_next);

    // call func_index ...
    // [      safe   ] unsafe (could be the section_end)
    //                ^^ code_curr

    auto const all_function_size{import_func_count + local_func_count};
    if(static_cast<::std::size_t>(func_index) >= all_function_size) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.invalid_function_index.function_index = static_cast<::std::size_t>(func_index);
        err.err_selectable.invalid_function_index.all_function_size = all_function_size;
        err.err_code = code_validation_error_code::invalid_function_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* callee_type_ptr{};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        auto const& imported_rec{curr_module.imported_function_vec_storage.index_unchecked(static_cast<::std::size_t>(func_index))};
        auto const imported_func_ptr{imported_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_func_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        // callee type: [checked import's function type record] record end
        // [safe complete borrowed record                  ] unsafe (past record end)
        // ^^ callee_type_ptr takes the checked import's function type pointer for this call.
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        // callee type: [checked local function's type record] record end
        // [safe complete borrowed record               ] unsafe (past record end)
        // ^^ callee_type_ptr takes the checked local function's type pointer for this call.
        callee_type_ptr = curr_module.local_defined_function_vec_storage.index_unchecked(local_idx).function_type_ptr;
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};
    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
    auto const rich_callee_index{rich_type_index_from_pointer(callee_type_ptr)};
    // [rich_owned_begin, rich_owned_begin + runtime_type_count) is one retained vector.
    // [safe                                               ] only an index strictly below the count is added.
    //                     ^^ rich_callee; the false branch never performs null-pointer arithmetic.
    auto const* rich_callee{rich_callee_index < runtime_type_count ? rich_owned_begin + rich_callee_index : nullptr};
    // A protected call must expose the complete caller prefix and argument tuple to its cold
    // continuation. Keep ordinary call fusion/cache paths unchanged outside active try_table handlers.
    [[maybe_unused]] bool const protected_call{exception_has_active_handlers()};
    [[maybe_unused]] auto const exception_source_stack_bytes{operand_stack_bytes};
    bool const allow_call_fusion{!protected_call && param_count <= 3uz};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    // Normal calls encode module/function identity. Direct local-call fast paths replace that pair
    // with a pointer to compiled call metadata and mark it with `SIZE_MAX` as the module sentinel.
    ::std::size_t call_module_id{options.curr_wasm_id};
    ::std::size_t call_function_imm{func_index_uz};
    if(func_index_uz < import_func_count)
    {
        auto const direct_callee{details::resolve_runtime_import_direct_defined_call(curr_module, func_index_uz)};
        if(direct_callee.direct_callable && direct_callee.function_type_ptr != nullptr &&
           details::runtime_wasm_function_types_equal(*direct_callee.function_type_ptr, callee_type))
        {
            auto const info_ptr{::std::addressof(storage.local_defined_call_info.index_unchecked(direct_callee.local_defined_index))};
            call_module_id = SIZE_MAX;
            call_function_imm = reinterpret_cast<::std::size_t>(info_ptr);
        }
    }
    else
    {
        auto const local_idx{func_index_uz - import_func_count};
        auto const info_ptr{::std::addressof(storage.local_defined_call_info.index_unchecked(local_idx))};
        call_module_id = SIZE_MAX;
        call_function_imm = reinterpret_cast<::std::size_t>(info_ptr);
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine must be flushed before `call` because the runtime call bridge requires a fully materialized operand stack.
    flush_conbine_pending();
#endif

    [[maybe_unused]] auto const stack_size{operand_stack.size()};

    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"call", param_count); }

    // Parameters are checked from the top of the operand stack downward because Wasm pushes operands
    // in declaration order but the newest value is physically at the top.
    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                    rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                stack_entry_type_matches(actual_operand, expected_type)};
            if(!matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Optional: stack-top fast-path `call` for hot same-type signatures.
    // It is intentionally conservative: a call can re-enter arbitrary runtime code, so all cached
    // operands must already be in a layout the bridge understands.
    bool use_stacktop_call_fast{};
    bool use_stacktop_call0_void_fast{};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    curr_operand_stack_value_type stacktop_call_fast_vt{curr_operand_stack_value_type::i32};

    if constexpr(stacktop_enabled && CompileOption.is_tail_call)
    {
        if(!protected_call && !is_polymorphic)
        {
            // Fast path constraints:
            // - all operand stack values are cached (no memory segment),
            // - signature: (T x N) -> (T | void) where T is i32/f32/f64 (and some f32<->f64 when merged),
            // - we only support small N.
            bool const n_ok{param_count != 0uz && param_count <= 4uz};
            bool const state_ok{stacktop_memory_count == 0uz && stacktop_cache_count == stack_size && stack_size >= param_count};

            if(n_ok && state_ok)
            {
                auto const param_vt{callee_type.parameter.begin[0]};
                bool all_same_type_params{true};
                for(::std::size_t i{}; i != param_count; ++i)
                {
                    if(callee_type.parameter.begin[i] != param_vt)
                    {
                        all_same_type_params = false;
                        break;
                    }
                }

                bool const vt_ok{param_vt == value_type_enum::i32 || param_vt == value_type_enum::f32 || param_vt == value_type_enum::f64};

                // Fast-path return types:
                // - i32  -> i32|void
                // - f32  -> f32|void (and optionally f64 when f32/f64 are merged)
                // - f64  -> f64|void (and optionally f32 when f32/f64 are merged)
                constexpr bool fp_ranges_merged{stacktop_f32_enabled && stacktop_f64_enabled &&
                                                CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                bool res_ok{};
                if(result_count == 0uz) { res_ok = true; }
                else if(result_count == 1uz)
                {
                    auto const ret_vt{callee_type.result.begin[0]};
                    if(param_vt == value_type_enum::i32) { res_ok = (ret_vt == value_type_enum::i32); }
                    else if(param_vt == value_type_enum::f32)
                    {
                        res_ok = (ret_vt == value_type_enum::f32) || (fp_ranges_merged && ret_vt == value_type_enum::f64);
                    }
                    else if(param_vt == value_type_enum::f64)
                    {
                        res_ok = (ret_vt == value_type_enum::f64) || (fp_ranges_merged && ret_vt == value_type_enum::f32);
                    }
                }

                if(all_same_type_params && res_ok && vt_ok)
                {
                    use_stacktop_call_fast = true;
                    stacktop_call_fast_vt = param_vt;
                }
            }
        }
    }

    // Special-case: `call` with 0 params and 0 results does not need operand-stack materialization.
    // If we have no operand-stack memory segment, we can skip the pre-call spill and post-call fill.
    if constexpr(stacktop_enabled && CompileOption.is_tail_call)
    {
        if(!protected_call && !is_polymorphic)
        {
            bool const state_ok{stacktop_memory_count == 0uz && stacktop_cache_count == stack_size};
            if(param_count == 0uz && result_count == 0uz && state_ok) { use_stacktop_call0_void_fast = true; }
        }
    }
#endif

    // Stack-top optimization: default `call` requires all args in operand-stack memory (optable/call.h contract).
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            if(!use_stacktop_call_fast && !use_stacktop_call0_void_fast)
            {
                // Spill all cached values so `type...[1u]` points at the full operand stack.
                stacktop_flush_all_to_operand_stack(bytecode);
            }
        }
    }

    // Translate: `call` bridge (module_id + function_index).
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    bool fuse_call_drop{};
    bool fuse_call_local_set{};
    [[maybe_unused]] bool fuse_call_local_tee{};
    [[maybe_unused]] local_offset_t fused_local_off{};

    // Fusion has typed runtime stubs only for scalar results. Decide this before peeking at and
    // consuming the following opcode: a v128/reference `call; drop` must leave `drop` for its own
    // validation/emission path.
    auto const result_type_ok{result_count == 1uz && (callee_type.result.begin[0] == curr_operand_stack_value_type::i32 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::i64 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::f32 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::f64)};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(allow_call_fusion && result_type_ok && code_curr != code_end)
    {
        if(use_stacktop_call_fast)
        {
            // Stack-top fast-path currently only supports fused `drop` / `local.set` for i32->i32 hot signatures.
            if(stacktop_call_fast_vt == curr_operand_stack_value_type::i32 && callee_type.result.begin[0] == curr_operand_stack_value_type::i32)
            {
                wasm1_code next_op;  // no init
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

                if(next_op == wasm1_code::drop)
                {
                    fuse_call_drop = true;
                    // fused call; drop opcode ... code_end
                    // [safe byte              ] unsafe (could be code_end)
                    // ^^ code_curr: fusion guard proved a live next opcode.
                    ++code_curr;
                    // [safe byte              ] unsafe (could be code_end)
                    //                            ^^ code_curr may be one-past.
                }
                else if(next_op == wasm1_code::local_set)
                {
                    wasm_u32 local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};
                    if(local_index_err == ::fast_io::parse_code::ok && local_index < all_local_count)
                    {
                        auto const local_type{local_type_from_index(local_index)};
                        if(local_type == callee_type.result.begin[0])
                        {
                            fused_local_off = local_offset_from_index(local_index);
                            fuse_call_local_set = true;
                            // fused local-index LEB ... code_end
                            // [safe consumed bytes] unsafe (could be code_end)
                            // ^^ local_index_next: successful bounded lookahead produced a position in the current code slice.
                            // [bounded decoded/immediate cursor] next bytes ... | end
                            // [safe consumed bytes]       | one-past is never dereferenced here
                            // ^^ code_curr: the successful scanner or checked lookahead supplies a position within the current code slice.
                            code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
                            // fused local-index LEB ... code_end
                            // [safe consumed bytes] unsafe (could be code_end)
                            //                       ^^ code_curr may be one-past; no read occurs here.
                        }
                    }
                }
            }
        }
        else
        {
            wasm1_code next_op;  // no init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

            if(next_op == wasm1_code::drop)
            {
                fuse_call_drop = true;
                // fused call; drop opcode ... code_end
                // [safe byte              ] unsafe (could be code_end)
                // ^^ code_curr: fusion guard proved a live next opcode.
                ++code_curr;
                // [safe byte              ] unsafe (could be code_end)
                //                            ^^ code_curr may be one-past.
            }
            else if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                        ::fast_io::mnp::leb128_get(local_index))};
                if(local_index_err == ::fast_io::parse_code::ok && local_index < all_local_count)
                {
                    auto const local_type{local_type_from_index(local_index)};

                    if(local_type == callee_type.result.begin[0])
                    {
                        fused_local_off = local_offset_from_index(local_index);
                        if(next_op == wasm1_code::local_set) { fuse_call_local_set = true; }
                        else
                        {
                            fuse_call_local_tee = true;
                        }
                        // fused local-index LEB ... code_end
                        // [safe consumed bytes] unsafe (could be code_end)
                        // ^^ local_index_next: successful bounded lookahead produced a position in the current code slice.
                        // [bounded decoded/immediate cursor] next bytes ... | end
                        // [safe consumed bytes]       | one-past is never dereferenced here
                        // ^^ code_curr: the successful scanner or checked lookahead supplies a position within the current code slice.
                        code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
                        // fused local-index LEB ... code_end
                        // [safe consumed bytes] unsafe (could be code_end)
                        //                       ^^ code_curr may be one-past; no read occurs here.
                    }
                }
            }
        }
    }
#endif

    if(!allow_call_fusion || !result_type_ok)
    {
        fuse_call_drop = false;
        fuse_call_local_set = false;
        fuse_call_local_tee = false;
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(use_stacktop_call0_void_fast)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_call_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, call_module_id);
        emit_imm_to(bytecode, call_function_imm);
    }
    else if(use_stacktop_call_fast)
    {
        switch(stacktop_call_fast_vt)
        {
            case curr_operand_stack_value_type::i32:
            {
                if(result_count == 0uz)
                {
                    switch(param_count)
                    {
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 1uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 2uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 3uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 4uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    if(fuse_call_drop)
                    {
                        switch(param_count)
                        {
                            case 1uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<CompileOption, 1uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 2uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<CompileOption, 2uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 3uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<CompileOption, 3uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 4uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_drop_fptr_from_tuple<CompileOption, 4uz>(curr_stacktop, interpreter_tuple));
                                break;
                            [[unlikely]] default:
                                ::fast_io::fast_terminate();
                        }
                    }
                    else if(fuse_call_local_set)
                    {
                        switch(param_count)
                        {
                            case 1uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 1uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 2uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 2uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 3uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 3uz>(curr_stacktop, interpreter_tuple));
                                break;
                            case 4uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 4uz>(curr_stacktop, interpreter_tuple));
                                break;
                            [[unlikely]] default:
                                ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        switch(param_count)
                        {
                            case 1uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 1uz, wasm_i32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 2uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 2uz, wasm_i32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 3uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 3uz, wasm_i32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 4uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<CompileOption, 4uz, wasm_i32>(curr_stacktop, interpreter_tuple));
                                break;
                            [[unlikely]] default:
                                ::fast_io::fast_terminate();
                        }
                    }
                }
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                if(result_count == 0uz)
                {
                    switch(param_count)
                    {
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 1uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 2uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 3uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 4uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else if(callee_type.result.begin[0] == curr_operand_stack_value_type::f32)
                {
                    switch(param_count)
                    {
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 1uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 2uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 3uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 4uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    // f32 -> f64 stacktop fast-path requires merged fp ranges.
                    // Note: `use_stacktop_call_fast` already checks this constraint (via `res_ok`), but we
                    // must also guard template instantiation here so split f32/f64 layouts remain buildable.
                    constexpr bool fp_ranges_merged_for_call_stacktop{stacktop_f32_enabled && stacktop_f64_enabled &&
                                                                      CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                                      CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                    if constexpr(fp_ranges_merged_for_call_stacktop)
                    {
                        switch(param_count)
                        {
                            case 1uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 1uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                                break;
                            case 2uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 2uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                                break;
                            case 3uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 3uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                                break;
                            case 4uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<CompileOption, 4uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                                break;
                            [[unlikely]] default:
                                ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                if(result_count == 0uz)
                {
                    switch(param_count)
                    {
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 1uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 2uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 3uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 4uz, void>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else if(callee_type.result.begin[0] == curr_operand_stack_value_type::f64)
                {
                    switch(param_count)
                    {
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 1uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 2uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 3uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 4uz, wasm_f64>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    // f64 -> f32 stacktop fast-path requires merged fp ranges.
                    // Note: `use_stacktop_call_fast` already checks this constraint (via `res_ok`), but we
                    // must also guard template instantiation here so split f32/f64 layouts remain buildable.
                    constexpr bool fp_ranges_merged_for_call_stacktop{stacktop_f32_enabled && stacktop_f64_enabled &&
                                                                      CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                                      CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                    if constexpr(fp_ranges_merged_for_call_stacktop)
                    {
                        switch(param_count)
                        {
                            case 1uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 1uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 2uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 2uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 3uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 3uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                                break;
                            case 4uz:
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<CompileOption, 4uz, wasm_f32>(curr_stacktop, interpreter_tuple));
                                break;
                            [[unlikely]] default:
                                ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
                break;
            }
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::fast_io::fast_terminate();
            }
        }

        emit_imm_to(bytecode, call_module_id);
        emit_imm_to(bytecode, call_function_imm);
        if(fuse_call_local_set || fuse_call_local_tee) { emit_imm_to(bytecode, fused_local_off); }
    }
    else if(fuse_call_drop || fuse_call_local_set || fuse_call_local_tee)
    {
        switch(callee_type.result.begin[0])
        {
            case curr_operand_stack_value_type::i32:
            {
                if(fuse_call_drop)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_drop_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop, interpreter_tuple));
                }
                else if(fuse_call_local_set)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_set_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_tee_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop, interpreter_tuple));
                }
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                if(fuse_call_drop)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_drop_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop, interpreter_tuple));
                }
                else if(fuse_call_local_set)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_set_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_tee_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop, interpreter_tuple));
                }
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                if(fuse_call_drop)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_drop_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop, interpreter_tuple));
                }
                else if(fuse_call_local_set)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_set_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_tee_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop, interpreter_tuple));
                }
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                if(fuse_call_drop)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_drop_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop, interpreter_tuple));
                }
                else if(fuse_call_local_set)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_set_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_call_local_tee_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop, interpreter_tuple));
                }
                break;
            }
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                emit_opfunc_to(bytecode, translate::get_uwvmint_call_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                fuse_call_drop = false;
                fuse_call_local_set = false;
                fuse_call_local_tee = false;
                break;
            }
        }

        emit_imm_to(bytecode, call_module_id);
        emit_imm_to(bytecode, call_function_imm);
        if(fuse_call_local_set || fuse_call_local_tee) { emit_imm_to(bytecode, fused_local_off); }
    }
    else
#endif
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(protected_call)
        { emit_opfunc_to(bytecode, translate::get_uwvmint_call_catching_fptr_from_tuple<false, CompileOption>(interpreter_tuple)); }
        else
#endif
        { emit_opfunc_to(bytecode, translate::get_uwvmint_call_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
        emit_imm_to(bytecode, call_module_id);
        emit_imm_to(bytecode, call_function_imm);
#ifdef UWVM_CPP_EXCEPTIONS
        if(protected_call) { emit_exception_call_site(op_begin, exception_source_stack_bytes); }
#endif
    }

    // Update the validation operand stack after the `call` is encoded.
    if(param_count != 0uz) { operand_stack_pop_n(param_count); }
    // Fused call + drop/local.set suppresses the logical result push, but the ordinary byref/by-value call ABI
    // first copies the result into this caller's operand stack before the fused handler removes it.  Reserve that
    // transient result in the caller frame.  The stack-top fast handler instead uses its own local scratch buffer.
    // [caller operands after argument pop][temporary result bytes] | allocated operand frame end
    // [safe checked transient byte maximum                    ] unsafe (past the frame)
    // ^^ the result write advances the runtime stack pointer only after this compile-time maximum is recorded.
    if((fuse_call_drop || fuse_call_local_set) && !use_stacktop_call_fast && !is_polymorphic)
    {
        auto const transient_result_bytes{operand_stack_valtype_size(callee_type.result.begin[0])};
        if(transient_result_bytes == 0uz ||
           transient_result_bytes > (::std::numeric_limits<::std::size_t>::max() - operand_stack_bytes)) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        auto const transient_byte_depth{operand_stack_bytes + transient_result_bytes};
        if(transient_byte_depth > runtime_operand_stack_byte_max) { runtime_operand_stack_byte_max = transient_byte_depth; }

        if(operand_stack.size() == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const transient_value_depth{operand_stack.size() + 1uz};
        if(transient_value_depth > runtime_operand_stack_max) { runtime_operand_stack_max = transient_value_depth; }
    }
    ::std::size_t const effective_result_count{(fuse_call_drop || fuse_call_local_set) ? 0uz : result_count};
    if(effective_result_count != 0uz)
    {
        for(::std::size_t i{}; i != effective_result_count; ++i)
        {
            operand_stack_push(callee_type.result.begin[i]);
            if(rich_callee != nullptr)
            {
                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                operand_stack.back_unchecked().has_core_type = true;
            }
        }
    }

    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            if(use_stacktop_call_fast || use_stacktop_call0_void_fast)
            {
                // Fast path: call consumes cached params and produces cached result (if any).
                stacktop_commit_pop_n(param_count);
                codegen_stack_pop_n(param_count);

                for(::std::size_t i{}; i != effective_result_count; ++i)
                {
                    stacktop_commit_push1_typed(callee_type.result.begin[i]);
                    codegen_stack_push(callee_type.result.begin[i]);
                }
            }
            else
            {
                // Slow path: model call stack effect on the memory-only operand stack (cache is empty after the pre-call spill).
                // Pop params (from memory stack): advance per-type cursors and adjust memory_count.
                stacktop_commit_pop_n(param_count);
                codegen_stack_pop_n(param_count);

                // Push results back to the memory stack (call bridge contract).
                auto const stacktop_commit_push1_to_memory{[&](curr_operand_stack_value_type vt) constexpr noexcept
                                                           {
                                                               ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                                               ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                                               ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                                               ::std::size_t const new_pos{stacktop_ring_prev(currpos, begin_pos, end_pos)};
                                                               stacktop_set_currpos_for_range(begin_pos, end_pos, new_pos);
                                                               ++stacktop_memory_count;
                                                           }};

                for(::std::size_t i{}; i != effective_result_count; ++i)
                {
                    codegen_stack_push(callee_type.result.begin[i]);
                    stacktop_commit_push1_to_memory(callee_type.result.begin[i]);
                }

                // Call leaves cache empty; restore canonical cache after the call returns.
                stacktop_cache_count = 0uz;
                stacktop_cache_i32_count = 0uz;
                stacktop_cache_i64_count = 0uz;
                stacktop_cache_f32_count = 0uz;
                stacktop_cache_f64_count = 0uz;

                // Restore canonical cache after the call returns.
                stacktop_fill_to_canonical(bytecode);
            }
        }
    }

    break;
}
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Tail-call opcode extends the shared wasm1 enum.
#endif
case static_cast<wasm1_code>(0x13u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
{
    ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x13u, code_curr, err);
    // return_call_indirect consumes one dynamic i32 table-element index above the function arguments.
    // This stack operand is distinct from the encoded table_index immediate, which selects the table.
    // return_call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // return_call_indirect  type_index table_index ...
    // [ safe      ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // return_call_indirect type_index table_index ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    wasm_u32 type_index;
    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(type_index))};
    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
    }

    // return_call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //               ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(type_next);

    // return_call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //                          ^^ code_curr

    // Decode through a local scanner and commit code_curr only after the complete trailing field.
    // A malformed trailing field leaves code_curr at table_index; return_call_indirect
    // always uses a u32 tableidx, independently of the multiple-tables gate.
    wasm_u32 table_index{};
    // Reference Types/Core 2.0 encode a real `tableidx ::= u32`.  A
    // single-table feature policy still decodes this ULEB128 before requiring zero.
    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(table_index))};
    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
    }
    // return_call_indirect type_index table_index ... code_end
    // [          decoded and safe          ] unsafe (could be code_end)
    //                          ^^ code_curr; table_next was checked before this commit.
    code_curr = reinterpret_cast<::std::byte const*>(table_next);

    // return_call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                                      ^^ code_curr

    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
    // type_index so uwvm-int, LLVM, and the standard validators agree on compound-invalid operands.
    auto types_begin{curr_module.type_section_storage.type_section_begin};
    auto types_end{curr_module.type_section_storage.type_section_end};

    auto const all_type_count_uz{types_begin == types_end ? 0uz : static_cast<::std::size_t>(types_end - types_begin)};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
        err.err_code = code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(type_index), op_begin, u8"return_call_indirect", err);

    if(!wasm2_feature_enabled(wasm2_feature_kind::multiple_tables) && table_index != 0u) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    static_cast<wasm_u32>(static_cast<wasm_byte>(static_cast<wasm1_code>(0x13u))),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    if(table_index >= all_table_count) [[unlikely]]
    {
        // [tail opcode] immediates ... code_end
        // [safe       ] unsafe (could be code_end); no dereference.
        // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
        err.err_curr = op_begin;
        err.err_selectable.illegal_table_index.table_index = table_index;
        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
        err.err_code = code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Core 3 return_call_indirect requires a function-reference table even
    // when the instruction is unreachable; externref has no callable signature.
    if(!indirect_call_table_type_matches(table_index)) [[unlikely]]
    {
        // [return_call_indirect opcode] immediates ... code_end
        // [safe                       ] unsafe; diagnostic only.
        // ^^ op_begin / err_curr
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(get_table_value_type(table_index));
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& callee_type{types_begin[static_cast<::std::size_t>(type_index)]};
    auto const param_count{(callee_type.parameter.begin == callee_type.parameter.end ? 0uz : static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin))};
    ::uwvm2::validation::standard::wasm3::validate_tail_call_results(control_flow_stack.index_unchecked(0u).result, callee_type.result, op_begin, u8"return_call_indirect", err);
    // The immediate was range-checked against the retained type section above.
    // [rich_owned_begin, rich_owned_end) has the same count as the carrier types.
    // [safe                            ] index < runtime_type_count when available.
    //                   ^^ rich_callee is borrowed only for validation.
    auto const* rich_callee{rich_owned_available ?
        ::std::addressof(rich_owned_begin[static_cast<::std::size_t>(type_index)]) : nullptr};
    if(rich_callee != nullptr)
    {
        auto const& caller_results{rich_owned_begin[curr_owned_type_index].results};
        auto const& callee_results{rich_callee->results};
        auto const fail_rich_tail_result{[&]() UWVM_THROWS
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin; // Dispatch-checked opcode; no input cursor advances.
            err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
            err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
            err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
            err.err_code = code_validation_error_code::br_value_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }};
        if(callee_results.size() != caller_results.size()) [[unlikely]] { fail_rich_tail_result(); }
        for(::std::size_t i{}; i != caller_results.size(); ++i)
        {
            if(!runtime_core3_value_type_matches(
                callee_results.index_unchecked(i), caller_results.index_unchecked(i),
                ::uwvm2::validation::standard::wasm3::core3_signature_view<
                    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count})) [[unlikely]]
            { fail_rich_tail_result(); }
        }
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine must be flushed before `return_call_indirect` because the runtime call bridge requires a fully materialized operand stack.
    flush_conbine_pending();
#endif

    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};
    [[maybe_unused]] auto const stack_size{operand_stack.size()};

    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"return_call_indirect", required_stack_size);
    }

    validate_table_operand(op_begin, u8"return_call_indirect", table_operand_type(table_index));

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                    rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                stack_entry_type_matches(actual_operand, expected_type)};
            if(!matches) [[unlikely]]
            {
                // [tail opcode] immediates ... code_end
                // [safe       ] unsafe (could be code_end); no dereference.
                // ^^ op_begin / err_curr: dispatch proved the opcode byte exists.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_indirect";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    if(!is_polymorphic)
    {
        stacktop_flush_all_to_operand_stack(bytecode);
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        local_func_symbol.has_tail_transfer = true;
        emit_opfunc_to(bytecode, translate::get_uwvmint_return_call_indirect_transfer_fptr_from_tuple<CompileOption>(interpreter_tuple));
        emit_imm(options.curr_wasm_id);
        emit_imm(static_cast<::std::size_t>(type_index));
        emit_imm(static_cast<::std::size_t>(table_index));
        ::std::size_t argument_bytes{};
        for(::std::size_t i{}; i != param_count; ++i)
        {
            auto const size{operand_stack_valtype_size(callee_type.parameter.begin[i])};
            if(size > SIZE_MAX - argument_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            argument_bytes += size;
        }
        emit_imm(argument_bytes);
    }
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;
    codegen_reachable = false;
    break;
}
case wasm1_code::call_indirect:
{
    // call_indirect consumes one dynamic i32 table-element index above the function arguments.
    // This stack operand is distinct from the encoded table_index immediate, which selects the table.
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

    wasm_u32 type_index;
    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(type_index))};
    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(type_err);
    }

    // call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //               ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(type_next);

    // call_indirect type_index table_index ...
    // [          safe        ] unsafe (could be the section_end)
    //                          ^^ code_curr

    // Decode through a local scanner and commit code_curr only after the complete trailing field.
    // MVP likewise advances only after matching its literal 0x00, so every trailing-immediate decode
    // failure leaves code_curr at the table_index position shown above.
    wasm_u32 table_index{};
    if(!::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_call_indirect_reserved_byte(wasm1p1_para))
    {
        // Reference Types/Core 2.0 encode a real `tableidx ::= u32`.  A
        // single-table feature policy still decodes this ULEB128 before requiring zero.
        auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                    ::fast_io::mnp::leb128_get(table_index))};
        if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::invalid_table_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(table_err);
        }
        // call_indirect table-index LEB ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ table_next: successful bounded scanner produced a position in this code slice.
        // [bounded decoded/immediate cursor] next bytes ... | end
        // [safe consumed bytes]       | one-past is never dereferenced here
        // ^^ code_curr: the successful scanner or checked lookahead supplies a position within the current code slice.
        code_curr = reinterpret_cast<::std::byte const*>(table_next);
        // call_indirect table-index LEB ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        //                       ^^ code_curr may be one-past; no read occurs here.
    }
    else
    {
        // Core 1.0 section 5.4.1 uses the literal reserved byte in
        // `0x11 typeidx 0x00`; it is not a ULEB128 table index.
        if(code_curr == code_end || *code_curr != ::std::byte{}) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::invalid_table_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // call_indirect reserved table byte ... code_end
        // [safe byte                          ] unsafe (could be code_end)
        // ^^ code_curr: the non-end and zero-byte checks proved it exists.
        ++code_curr;
        // [safe byte                          ] unsafe (could be code_end)
        //                                       ^^ code_curr may be one-past.
    }

    // call_indirect type_index table_index ...
    // [                safe              ] unsafe (could be the section_end)
    //                                      ^^ code_curr

    // Both immediate fields now have valid encodings.  Semantic checks intentionally start with
    // type_index so uwvm-int, LLVM, and the standard validators agree on compound-invalid operands.
    auto types_begin{curr_module.type_section_storage.type_section_begin};
    auto types_end{curr_module.type_section_storage.type_section_end};

    auto const all_type_count_uz{types_begin == types_end ? 0uz : static_cast<::std::size_t>(types_end - types_begin)};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
        err.err_code = code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(type_index), op_begin, u8"call_indirect", err);

    if(!wasm2_feature_enabled(wasm2_feature_kind::multiple_tables) && table_index != 0u) [[unlikely]]
    {
        fail_wasm2_feature_required(op_begin,
                                    static_cast<wasm_u32>(static_cast<wasm_byte>(wasm1_code::call_indirect)),
                                    ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                    ::uwvm2::parser::wasm::base::wasm2_error_subject::instruction);
    }

    if(table_index >= all_table_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_table_index.table_index = table_index;
        err.err_selectable.illegal_table_index.all_table_count = all_table_count;
        err.err_code = code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(!indirect_call_table_type_matches(table_index)) [[unlikely]]
    {
        // [call_indirect] complete typeidx/tableidx ... | code_end
        // [safe        ] opcode was proved by dispatch; this diagnostic borrow does not advance it.
        // ^^ op_begin -> err.err_curr; one-past code_end is never dereferenced.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(get_table_value_type(table_index));
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& callee_type{types_begin[static_cast<::std::size_t>(type_index)]};
    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
    // The decoded type index was checked against the retained carrier type table above.
    // [rich_owned_begin, rich_owned_end) contains an equally indexed rich signature.
    // [safe                            ] rich_owned_available proves equal counts.
    //                   ^^ rich_callee is borrowed only when the retained table exists.
    auto const* rich_callee{rich_owned_available ?
        ::std::addressof(rich_owned_begin[static_cast<::std::size_t>(type_index)]) : nullptr};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine must be flushed before `call_indirect` because the runtime call bridge requires a fully materialized operand stack.
    flush_conbine_pending();
#endif

    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_element_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_element_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};
    [[maybe_unused]] auto const stack_size{operand_stack.size()};

    if(!is_polymorphic && (param_count_plus_element_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"call_indirect", required_stack_size);
    }

    [[maybe_unused]] bool const protected_call{exception_has_active_handlers()};
    // validate_table_operand pops the selector from the validation stack. The runtime protected
    // opcode receives the original top, so retain its byte extent INCLUDING that selector now.
    [[maybe_unused]] auto const exception_source_stack_bytes{operand_stack_bytes};
    validate_table_operand(op_begin, u8"call_indirect", table_operand_type(table_index));

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
            auto const matches{rich_callee != nullptr && !actual_operand.is_unknown ?
                runtime_core3_value_type_matches(
                    ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual_operand),
                    rich_callee->parameters.index_unchecked(param_count - 1uz - i),
                    ::uwvm2::validation::standard::wasm3::core3_signature_view<
                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin, runtime_type_count}) :
                stack_entry_type_matches(actual_operand, expected_type)};
            if(!matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Optional: stack-top fast-path `call_indirect` for hot i32 signatures.
    bool use_stacktop_call_indirect_fast{};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    bool fuse_call_indirect_drop{};
    bool fuse_call_indirect_local_set{};
    [[maybe_unused]] local_offset_t fused_local_off{};
#endif
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(stacktop_enabled && CompileOption.is_tail_call)
    {
        if(!protected_call && table_operand_type(table_index) == curr_operand_stack_value_type::i32 && !is_polymorphic)
        {
            bool const n_ok{param_count <= 4uz};
            bool const state_ok{stacktop_memory_count == 0uz && stacktop_cache_count == stack_size && !param_count_plus_element_index_overflows &&
                                stack_size >= required_stack_size};

            if(n_ok && state_ok)
            {
                bool all_i32_params{true};
                for(::std::size_t i{}; i != param_count; ++i)
                {
                    if(callee_type.parameter.begin[i] != value_type_enum::i32)
                    {
                        all_i32_params = false;
                        break;
                    }
                }

                bool res_ok{result_count == 0uz};
                if(result_count == 1uz) { res_ok = (callee_type.result.begin[0] == value_type_enum::i32); }

                if(all_i32_params && res_ok) { use_stacktop_call_indirect_fast = true; }
            }
        }
    }

    // Optional fusion: `call_indirect (i32...) -> i32` + `drop`/`local.set`.
    // Only valid when stack-top fast-path is used (selector+params in cache; no spill) and result is i32.
    if constexpr(stacktop_enabled && CompileOption.is_tail_call)
    {
        if(use_stacktop_call_indirect_fast && result_count == 1uz && callee_type.result.begin[0] == value_type_enum::i32)
        {
            if(code_curr != code_end)
            {
                wasm1_code next_op;  // no init
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

                if(next_op == wasm1_code::drop)
                {
                    fuse_call_indirect_drop = true;
                    // fused call; drop opcode ... code_end
                    // [safe byte              ] unsafe (could be code_end)
                    // ^^ code_curr: fusion guard proved a live next opcode.
                    ++code_curr;
                    // [safe byte              ] unsafe (could be code_end)
                    //                            ^^ code_curr may be one-past.
                }
                else if(next_op == wasm1_code::local_set)
                {
                    wasm_u32 local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                            ::fast_io::mnp::leb128_get(local_index))};
                    if(local_index_err == ::fast_io::parse_code::ok && local_index < all_local_count)
                    {
                        auto const local_type{local_type_from_index(local_index)};
                        if(local_type == value_type_enum::i32)
                        {
                            fused_local_off = local_offset_from_index(local_index);
                            fuse_call_indirect_local_set = true;
                            // fused local-index LEB ... code_end
                            // [safe consumed bytes] unsafe (could be code_end)
                            // ^^ local_index_next: successful bounded lookahead produced a position in the current code slice.
                            // [bounded decoded/immediate cursor] next bytes ... | end
                            // [safe consumed bytes]       | one-past is never dereferenced here
                            // ^^ code_curr: the successful scanner or checked lookahead supplies a position within the current code slice.
                            code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
                            // fused local-index LEB ... code_end
                            // [safe consumed bytes] unsafe (could be code_end)
                            //                       ^^ code_curr may be one-past; no read occurs here.
                        }
                    }
                }
            }
        }
    }
#endif

    // Stack-top optimization: default `call_indirect` requires all args and the selector index in operand-stack memory (optable/call.h
    // contract).
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            if(!use_stacktop_call_indirect_fast)
            {
                // Spill all cached values so `type...[1u]` points at the full operand stack.
                stacktop_flush_all_to_operand_stack(bytecode);
            }
        }
    }

    // Translate: `call_indirect` bridge (module_id + type_index + table_index).
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    auto const emit_call_indirect_normal{
        [&]() constexpr UWVM_THROWS
        {
#ifdef UWVM_CPP_EXCEPTIONS
            if(protected_call)
            { emit_opfunc_to(bytecode, translate::get_uwvmint_call_catching_fptr_from_tuple<true, CompileOption>(interpreter_tuple)); }
            else
#endif
            { emit_opfunc_to(bytecode, translate::get_uwvmint_call_indirect_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
            emit_imm_to(bytecode, options.curr_wasm_id);
            emit_imm_to(bytecode, static_cast<::std::size_t>(type_index));
            emit_imm_to(bytecode, static_cast<::std::size_t>(table_index));
#ifdef UWVM_CPP_EXCEPTIONS
            if(protected_call) { emit_exception_call_site(op_begin, exception_source_stack_bytes); }
#endif
        }};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(use_stacktop_call_indirect_fast)
        {
            if(result_count == 0uz)
            {
                switch(param_count)
                {
                    case 0uz:
                        emit_opfunc_to(
                            bytecode,
                            translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 0uz, void>(curr_stacktop, interpreter_tuple));
                        break;
                    case 1uz:
                        emit_opfunc_to(
                            bytecode,
                            translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 1uz, void>(curr_stacktop, interpreter_tuple));
                        break;
                    case 2uz:
                        emit_opfunc_to(
                            bytecode,
                            translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 2uz, void>(curr_stacktop, interpreter_tuple));
                        break;
                    case 3uz:
                        emit_opfunc_to(
                            bytecode,
                            translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 3uz, void>(curr_stacktop, interpreter_tuple));
                        break;
                    case 4uz:
                        emit_opfunc_to(
                            bytecode,
                            translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 4uz, void>(curr_stacktop, interpreter_tuple));
                        break;
                    [[unlikely]] default:
                        ::fast_io::fast_terminate();
                }
            }
            else
            {
                // i32 -> i32
                if(fuse_call_indirect_drop)
                {
                    switch(param_count)
                    {
                        case 0uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<CompileOption, 0uz>(curr_stacktop, interpreter_tuple));
                            break;
                        case 1uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<CompileOption, 1uz>(curr_stacktop, interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<CompileOption, 2uz>(curr_stacktop, interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<CompileOption, 3uz>(curr_stacktop, interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_call_indirect_stacktop_i32_drop_fptr_from_tuple<CompileOption, 4uz>(curr_stacktop, interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else if(fuse_call_indirect_local_set)
                {
                    switch(param_count)
                    {
                        case 0uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 0uz>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 1uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 1uz>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 2uz>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 3uz>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_local_set_fptr_from_tuple<CompileOption, 4uz>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
                else
                {
                    switch(param_count)
                    {
                        case 0uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 0uz, wasm_i32>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 1uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 1uz, wasm_i32>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 2uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 2uz, wasm_i32>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 3uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 3uz, wasm_i32>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        case 4uz:
                            emit_opfunc_to(bytecode,
                                           translate::get_uwvmint_call_indirect_stacktop_i32_fptr_from_tuple<CompileOption, 4uz, wasm_i32>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            break;
                        [[unlikely]] default:
                            ::fast_io::fast_terminate();
                    }
                }
            }

            emit_imm_to(bytecode, options.curr_wasm_id);
            emit_imm_to(bytecode, static_cast<::std::size_t>(type_index));
            emit_imm_to(bytecode, static_cast<::std::size_t>(table_index));
            if(fuse_call_indirect_local_set) { emit_imm_to(bytecode, fused_local_off); }
        }
        else
        {
            emit_call_indirect_normal();
        }
    }
    else
#endif
    {
        emit_call_indirect_normal();
    }

    // Update the validation operand stack after the `call_indirect` is encoded.
    // The selector index was already consumed by `try_pop_concrete_operand()` above;
    // only the call arguments remain to be removed here.
    if(param_count != 0uz) { operand_stack_pop_n(param_count); }
    ::std::size_t effective_result_count{result_count};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(use_stacktop_call_indirect_fast && (fuse_call_indirect_drop || fuse_call_indirect_local_set)) { effective_result_count = 0uz; }
    }
#endif
    if(effective_result_count != 0uz)
    {
        for(::std::size_t i{}; i != effective_result_count; ++i)
        {
            operand_stack_push(callee_type.result.begin[i]);
            if(rich_callee != nullptr)
            {
                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                operand_stack.back_unchecked().has_core_type = true;
            }
        }
    }

    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            ::std::size_t const pop_count{required_stack_size};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(use_stacktop_call_indirect_fast)
            {
                // Fast path: indirect call consumes cached selector+params and produces cached result (if any).
                stacktop_commit_pop_n(pop_count);
                codegen_stack_pop_n(pop_count);

                ::std::size_t effective_stacktop_result_count{result_count};
                if(fuse_call_indirect_drop || fuse_call_indirect_local_set) { effective_stacktop_result_count = 0uz; }
                for(::std::size_t i{}; i != effective_stacktop_result_count; ++i)
                {
                    stacktop_commit_push1_typed(callee_type.result.begin[i]);
                    codegen_stack_push(callee_type.result.begin[i]);
                }
            }
            else
#endif
            {
                // Slow path: model call_indirect stack effect on the memory-only operand stack (cache is empty after the pre-call spill).
                stacktop_commit_pop_n(pop_count);
                codegen_stack_pop_n(pop_count);

                // Push results back to the memory stack (call bridge contract).
                auto const stacktop_commit_push1_to_memory{[&](curr_operand_stack_value_type vt) constexpr noexcept
                                                           {
                                                               ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                                               ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                                               ::std::size_t const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                                                               ::std::size_t const new_pos{stacktop_ring_prev(currpos, begin_pos, end_pos)};
                                                               stacktop_set_currpos_for_range(begin_pos, end_pos, new_pos);
                                                               ++stacktop_memory_count;
                                                           }};

                for(::std::size_t i{}; i != result_count; ++i)
                {
                    codegen_stack_push(callee_type.result.begin[i]);
                    stacktop_commit_push1_to_memory(callee_type.result.begin[i]);
                }

                stacktop_cache_count = 0uz;
                stacktop_cache_i32_count = 0uz;
                stacktop_cache_i64_count = 0uz;
                stacktop_cache_f32_count = 0uz;
                stacktop_cache_f64_count = 0uz;

                stacktop_fill_to_canonical(bytecode);
            }
        }
    }

    break;
}
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Typed-reference opcodes extend the shared wasm1 enum.
#endif
case static_cast<wasm1_code>(0x14u):
case static_cast<wasm1_code>(0x15u):
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
{
    // Core 3 typed function references use a single u32 typeidx immediate.
    // The reference operand is the last stack value, above every argument.
    // [call_ref or return_call_ref] typeidx ... code_end
    // [safe                       ] unsafe (could be code_end)
    // ^^ op_begin; dispatch proved the opcode byte exists.
    auto const op_begin{code_curr};
    auto const tail{*code_curr == ::std::byte{0x15u}};
    ::uwvm2::validation::standard::wasm3::require_function_references_enabled(
        !wasm1p1_para.disable_function_references, tail ? 0x15u : 0x14u, op_begin, err);
    if(tail) { ::uwvm2::validation::standard::wasm3::require_tail_call_enabled(!wasm1p1_para.disable_tail_call, 0x15u, op_begin, err); }
    // [call_ref or return_call_ref] typeidx ... code_end
    // [safe                       ] unsafe (could be code_end)
    //                               ^^ code_curr after the checked opcode; the LEB decoder owns the next advance.
    ++code_curr;
    auto const op_name{tail ? ::uwvm2::utils::container::u8string_view{u8"return_call_ref"} :
                              ::uwvm2::utils::container::u8string_view{u8"call_ref"}};
    auto const type_index{read_leb128.template operator()<wasm_u32>(code_curr, code_end, op_begin, op_name)};
    // [opcode][complete typeidx] ... code_end
    // [safe                    ] unsafe (could be code_end)
    //                            ^^ code_curr: decoder committed only after complete bounded u32.
    auto const types_begin{curr_module.type_section_storage.type_section_begin};
    auto const type_count{get_runtime_type_section_count(curr_module)};
    if(static_cast<::std::size_t>(type_index) >= type_count) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(type_count);
        err.err_code = code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    ::uwvm2::validation::standard::wasm3::require_core3_function_type_index_policy(
        curr_module.type_section_storage.core3_context_ptr, static_cast<::std::size_t>(type_index), op_begin, op_name, err);
    auto const& callee_type{types_begin[static_cast<::std::size_t>(type_index)]};
    auto const param_count{callee_type.parameter.begin == callee_type.parameter.end ? 0uz :
        static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{callee_type.result.begin == callee_type.result.end ? 0uz :
        static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
    auto const rich_begin{curr_module.type_section_storage.owned_signature_begin};
    auto const rich_end{curr_module.type_section_storage.owned_signature_end};
    // The initializer binds both endpoints from the same retained vector, or both as null.
    // [rich_begin, rich_end) is never subtracted unless both are non-null.
    // [safe                ] its checked count must equal the executable carrier type count.
    auto const rich_available{rich_begin != nullptr && rich_end != nullptr &&
        static_cast<::std::size_t>(rich_end - rich_begin) == type_count};
    ::uwvm2::validation::standard::wasm3::core3_signature_view<
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t> const rich_signatures{
        rich_available ? rich_begin : nullptr, rich_available ? type_count : 0uz};
    auto const* rich_callee{rich_available ? ::std::addressof(rich_signatures.index_unchecked(
        static_cast<::std::size_t>(type_index))) : nullptr};
    if(tail)
    {
        ::uwvm2::validation::standard::wasm3::validate_tail_call_results(
            control_flow_stack.index_unchecked(0u).result, callee_type.result, op_begin, u8"return_call_ref", err);
        if(rich_callee != nullptr)
        {
            ::std::size_t caller_type_index{type_count};
            for(::std::size_t i{}; i != type_count; ++i)
            {
                // [types_begin, types_begin + type_count) is one retained module allocation.
                // [safe                               ] i < type_count proves this read live.
                //         ^^ types_begin[i]; no unrelated pointer subtraction is used.
                if(::std::addressof(types_begin[i]) == curr_local_func.function_type_ptr)
                { caller_type_index = i; break; }
            }
            if(caller_type_index == type_count) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& caller_results{rich_signatures.index_unchecked(caller_type_index).results};
            auto const fail_rich_tail_result{[&]() UWVM_THROWS
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin; // Borrow the dispatch-checked opcode; no input read.
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return_call_ref";
                err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
                err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }};
            if(rich_callee->results.size() != caller_results.size()) [[unlikely]] { fail_rich_tail_result(); }
            for(::std::size_t i{}; i != caller_results.size(); ++i)
            {
                if(!runtime_core3_value_type_matches(
                    rich_callee->results.index_unchecked(i), caller_results.index_unchecked(i), rich_signatures)) [[unlikely]]
                { fail_rich_tail_result(); }
            }
        }
    }
    constexpr auto max_size{(::std::numeric_limits<::std::size_t>::max)()};
    auto const required{param_count == max_size ? max_size : param_count + 1uz};
    if(!is_polymorphic && (param_count == max_size || concrete_operand_count() < required)) [[unlikely]]
    { report_operand_stack_underflow(op_begin, op_name, required); }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    flush_conbine_pending();
#endif
    [[maybe_unused]] bool const protected_call{!tail && exception_has_active_handlers()};
    [[maybe_unused]] auto const exception_source_stack_bytes{operand_stack_bytes};
    auto const reference{try_pop_concrete_operand()};
    auto const ref_matches{::uwvm2::validation::standard::wasm3::core3_call_ref_reference_matches(
        reference, static_cast<::std::size_t>(type_index), type_count, rich_callee != nullptr,
        [&](auto actual, auto expected) constexpr noexcept
        { return runtime_core3_value_type_matches(actual, expected, rich_signatures); },
        [&](::std::size_t index) constexpr noexcept -> auto const&
        {
            // [types_begin ... index ... type_count) retained parser allocation
            // [safe] shared matcher proved index<type_count BEFORE indexed borrow.
            return types_begin[index];
        })};
    if(!ref_matches) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
        err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(curr_operand_stack_value_type::funcref);
        err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(reference.type);
        err.err_code = code_validation_error_code::br_value_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }
    auto const concrete_to_check{concrete_operand_count() < param_count ? concrete_operand_count() : param_count};
    for(::std::size_t i{}; i != concrete_to_check; ++i)
    {
        auto const expected{callee_type.parameter.begin[param_count - 1uz - i]};
        auto const actual{operand_stack.index_unchecked(operand_stack.size() - 1uz - i)};
        auto const matches{rich_callee != nullptr && !actual.is_unknown ?
            runtime_core3_value_type_matches(
                ::uwvm2::validation::standard::wasm3::core3_operand_effective_type(actual),
                rich_callee->parameters.index_unchecked(param_count - 1uz - i), rich_signatures) :
            stack_entry_type_matches(actual, expected)};
        if(!matches) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_selectable.br_value_type_mismatch.op_code_name = op_name;
            err.err_selectable.br_value_type_mismatch.expected_type = to_wasm1_value_type(expected);
            err.err_selectable.br_value_type_mismatch.actual_type = to_wasm1_value_type(actual.type);
            err.err_code = code_validation_error_code::br_value_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    if constexpr(stacktop_enabled)
    { if(!is_polymorphic) { stacktop_flush_all_to_operand_stack(bytecode); } }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if(tail)
    {
        if(!is_polymorphic)
        {
            local_func_symbol.has_tail_transfer = true;
            emit_opfunc_to(bytecode, translate::get_uwvmint_return_call_ref_transfer_fptr_from_tuple<CompileOption>(interpreter_tuple));
            emit_imm(options.curr_wasm_id);
            emit_imm(static_cast<::std::size_t>(type_index));
            ::std::size_t argument_bytes{};
            for(::std::size_t i{}; i != param_count; ++i)
            {
                auto const size{operand_stack_valtype_size(callee_type.parameter.begin[i])};
                if(size > max_size - argument_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
                argument_bytes += size;
            }
            emit_imm(argument_bytes);
        }
        auto const base{control_flow_stack.back_unchecked().operand_stack_base};
        operand_stack_truncate_to(base);
        is_polymorphic = true;
        codegen_reachable = false;
    }
    else
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(protected_call)
        { emit_opfunc_to(bytecode, translate::get_uwvmint_call_ref_catching_fptr_from_tuple<CompileOption>(interpreter_tuple)); }
        else
#endif
        { emit_opfunc_to(bytecode, translate::get_uwvmint_call_ref_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
        emit_imm_to(bytecode, options.curr_wasm_id);
        emit_imm_to(bytecode, static_cast<::std::size_t>(type_index));
#ifdef UWVM_CPP_EXCEPTIONS
        if(protected_call) { emit_exception_call_site(op_begin, exception_source_stack_bytes); }
#endif
        operand_stack_pop_n(param_count);
        for(::std::size_t i{}; i != result_count; ++i)
        {
            operand_stack_push(callee_type.result.begin[i]);
            if(rich_callee != nullptr)
            {
                operand_stack.back_unchecked().core_type = rich_callee->results.index_unchecked(i);
                operand_stack.back_unchecked().has_core_type = true;
            }
        }
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic)
            {
                stacktop_commit_pop_n(required);
                codegen_stack_pop_n(required);
                for(::std::size_t i{}; i != result_count; ++i)
                {
                    auto const value_type{callee_type.result.begin[i]};
                    codegen_stack_push(value_type);
                    auto const begin_pos{stacktop_range_begin_pos(value_type)};
                    auto const end_pos{stacktop_range_end_pos(value_type)};
                    auto const currpos{stacktop_currpos_for_range(begin_pos, end_pos)};
                    stacktop_set_currpos_for_range(begin_pos, end_pos, stacktop_ring_prev(currpos, begin_pos, end_pos));
                    ++stacktop_memory_count;
                }
                stacktop_cache_count = 0uz;
                stacktop_cache_i32_count = 0uz;
                stacktop_cache_i64_count = 0uz;
                stacktop_cache_f32_count = 0uz;
                stacktop_cache_f64_count = 0uz;
                stacktop_fill_to_canonical(bytecode);
            }
        }
    }
    break;
}
case wasm1_code::drop:
{
    // `drop` has no runtime side effect beyond removing a stack value. It still needs explicit
    // emission when the value may live in the runtime operand stack or stack-top cache.
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

    bool have_drop_operand{};
    curr_operand_stack_value_type drop_operand_type{};

    if(concrete_operand_count() == 0uz) [[unlikely]]
    {
        // Polymorphic stack: underflow is allowed, so `drop` becomes a no-op on the concrete stack.
        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"drop", 1uz); }
    }
    else
    {
        auto const operand{operand_stack.back_unchecked()};
        operand_stack_pop_unchecked();
        have_drop_operand = true;
        drop_operand_type = operand.type;
    }

    // Translate: typed `drop`.
    if(have_drop_operand) { emit_drop_typed_to(bytecode, drop_operand_type); }

    break;
}
case wasm1_code::select:
{
    // `select` consumes condition, false-value, and true-value, then pushes one value whose type must
    // match both alternatives. Heavy combine records the pattern for later local.set/local.tee fusion.
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

    if(!is_polymorphic && concrete_operand_count() < 3uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"select", 3uz); }

    bool cond_from_stack{};
    curr_operand_stack_value_type cond_type{};
    bool cond_is_unknown{};
    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack)
    {
        cond_from_stack = true;
        cond_type = cond.type;
        cond_is_unknown = cond.is_unknown;
    }

    if(cond_from_stack && !cond_is_unknown && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_cond_type_not_i32.cond_type = to_wasm1_value_type(cond_type);
        err.err_code = code_validation_error_code::select_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    bool v2_from_stack{};
    curr_operand_stack_value_type v2_type{};
    bool v2_is_unknown{};
    if(auto const v2{try_pop_concrete_operand()}; v2.from_stack)
    {
        v2_from_stack = true;
        v2_type = v2.type;
        v2_is_unknown = v2.is_unknown;
    }

    bool v1_from_stack{};
    curr_operand_stack_value_type v1_type{};
    bool v1_is_unknown{};
    if(auto const v1{try_peek_concrete_operand()}; v1.from_stack)
    {
        v1_from_stack = true;
        v1_type = v1.type;
        v1_is_unknown = v1.is_unknown;
    }

    if(v1_from_stack && v2_from_stack && !v1_is_unknown && !v2_is_unknown && v1_type != v2_type) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(v1_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(v2_type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    bool const have_known_select_value_type{(v1_from_stack && !v1_is_unknown) || (v2_from_stack && !v2_is_unknown)};
    auto const select_value_type{(v1_from_stack && !v1_is_unknown) ? v1_type : v2_type};
    if(have_known_select_value_type && !is_untyped_select_value_type(select_value_type)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = to_wasm1_value_type(select_value_type);
        err.err_selectable.select_type_mismatch.type_v2 = to_wasm1_value_type(select_value_type);
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Translate: typed `select`.
    if(v1_from_stack && !v1_is_unknown)
    {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if(conbine_pending.kind == conbine_pending_kind::select_localget3 && v1_type == conbine_pending.vt &&
           (v1_type == curr_operand_stack_value_type::i32 || v1_type == curr_operand_stack_value_type::i64 || v1_type == curr_operand_stack_value_type::f32 ||
            v1_type == curr_operand_stack_value_type::f64))
        {
            conbine_pending.kind = conbine_pending_kind::select_after_select;
            break;
        }
#endif
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        switch(v1_type)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::v128:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_select_v128_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                break;
            }
        }
    }

    // Stack-top cache model: `select` consumes (v2, cond) and keeps v1 (net -2).
    if(v1_from_stack && !v1_is_unknown) { stacktop_after_pop_n_if_reachable(bytecode, 2uz); }

    if(!v1_from_stack)
    {
        if(v2_from_stack)
        {
            if(v2_is_unknown) { push_unknown_operand(); }
            else { operand_stack_push(v2_type); }
        }
        else if(is_polymorphic)
        {
            push_unknown_operand();
        }
    }
    else if(v1_is_unknown && v2_from_stack && !v2_is_unknown)
    {
        // The known right operand refines the bottom left operand. Use pop/push
        // to keep byte accounting correct when the placeholder's width changes.
        operand_stack_pop_unchecked();
        operand_stack_push(v2_type);
    }

    break;
}
