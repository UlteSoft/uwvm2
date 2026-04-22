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

    auto const& func_frame{control_flow_stack.index_unchecked(0u)};
    ::std::size_t const return_arity{static_cast<::std::size_t>(func_frame.result.end - func_frame.result.begin)};

    if(!is_polymorphic && concrete_operand_count() < return_arity) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"return", return_arity);
    }

    if(return_arity != 0uz)
    {
        auto const available_result_count{concrete_operand_count()};
        auto const concrete_to_check{available_result_count < return_arity ? available_result_count : return_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{func_frame.result.begin[return_arity - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"return";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
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
            auto const result_type{func_frame.result.begin[0]};

            if(curr_size - target_base > 1uz)
            {
                emit_local_set_typed_to_no_fill(bytecode, result_type, internal_temp_local_off);
                if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(bytecode, target_base); }
                else
                {
                    for(::std::size_t i{curr_size - 1uz}; i-- > target_base;) { emit_drop_typed_to_no_fill(bytecode, operand_stack.index_unchecked(i).type); }
                }
                emit_local_get_typed_to(bytecode, result_type, internal_temp_local_off);
            }

            emit_return_to(bytecode);
        }
    }

    if(return_arity != 0uz) { operand_stack_pop_n(return_arity); }

    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    break;
}
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

    wasm_u32 func_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [func_next, func_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(func_index))};
    if(func_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
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
        callee_type_ptr = imported_func_ptr->imports.storage.function;
    }
    else
    {
        auto const local_idx{static_cast<::std::size_t>(func_index) - import_func_count};
        callee_type_ptr = curr_module.local_defined_function_vec_storage.index_unchecked(local_idx).function_type_ptr;
    }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(callee_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

    auto const& callee_type{*callee_type_ptr};
    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};
    bool const allow_call_fusion{param_count <= 3uz};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    ::std::size_t call_module_id{options.curr_wasm_id};
    ::std::size_t call_function_imm{func_index_uz};
    if(func_index_uz >= import_func_count)
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

    auto const stack_size{operand_stack.size()};

    if(!is_polymorphic && concrete_operand_count() < param_count) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"call", param_count);
    }

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Optional: stack-top fast-path `call` for hot same-type signatures.
    bool use_stacktop_call_fast{};
    bool use_stacktop_call0_void_fast{};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    curr_operand_stack_value_type stacktop_call_fast_vt{curr_operand_stack_value_type::i32};

    if constexpr(stacktop_enabled && CompileOption.is_tail_call)
    {
        if(!is_polymorphic)
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
        if(!is_polymorphic)
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

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(allow_call_fusion && result_count == 1uz && code_curr != code_end)
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
                    ++code_curr;
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
                            code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
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
                ++code_curr;
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
                        code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
                    }
                }
            }
        }
    }
#endif

    auto const result_type_ok{result_count == 1uz && (callee_type.result.begin[0] == curr_operand_stack_value_type::i32 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::i64 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::f32 ||
                                                      callee_type.result.begin[0] == curr_operand_stack_value_type::f64)};
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
        emit_opfunc_to(bytecode, translate::get_uwvmint_call_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, call_module_id);
        emit_imm_to(bytecode, call_function_imm);
    }

    // Update the validation operand stack after the `call` is encoded.
    if(param_count != 0uz) { operand_stack_pop_n(param_count); }
    ::std::size_t const effective_result_count{(fuse_call_drop || fuse_call_local_set) ? 0uz : result_count};
    if(effective_result_count != 0uz)
    {
        for(::std::size_t i{}; i != effective_result_count; ++i) { operand_stack_push(callee_type.result.begin[i]); }
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

    wasm_u32 type_index;
    auto const [type_next, type_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(type_index))};
    if(type_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
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

    wasm_u32 table_index;
    auto const [table_next, table_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(table_index))};
    if(table_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_table_index;
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
        err.err_code = code_validation_error_code::illegal_table_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto types_begin{curr_module.type_section_storage.type_section_begin};
    auto types_end{curr_module.type_section_storage.type_section_end};

    auto const all_type_count_uz{static_cast<::std::size_t>(types_end - types_begin)};
    if(static_cast<::std::size_t>(type_index) >= all_type_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_type_index.type_index = type_index;
        err.err_selectable.illegal_type_index.all_type_count = static_cast<wasm_u32>(all_type_count_uz);
        err.err_code = code_validation_error_code::illegal_type_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& callee_type{types_begin[static_cast<::std::size_t>(type_index)]};
    auto const param_count{static_cast<::std::size_t>(callee_type.parameter.end - callee_type.parameter.begin)};
    auto const result_count{static_cast<::std::size_t>(callee_type.result.end - callee_type.result.begin)};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine must be flushed before `call_indirect` because the runtime call bridge requires a fully materialized operand stack.
    flush_conbine_pending();
#endif

    auto constexpr max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const param_count_plus_table_index_overflows{param_count == max_operand_stack_requirement};
    auto const required_stack_size{param_count_plus_table_index_overflows ? max_operand_stack_requirement : (param_count + 1uz)};
    auto const stack_size{operand_stack.size()};

    if(!is_polymorphic && (param_count_plus_table_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"call_indirect", required_stack_size);
    }

    if(auto const idx{try_pop_concrete_operand()}; idx.from_stack)
    {
        if(idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"call_indirect";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(idx.type);
            err.err_code = code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(param_count != 0uz)
    {
        auto const available_param_count{concrete_operand_count()};
        auto const concrete_to_check{available_param_count < param_count ? available_param_count : param_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{callee_type.parameter.begin[param_count - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"call_indirect";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
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
        if(!is_polymorphic)
        {
            bool const n_ok{param_count <= 4uz};
            bool const state_ok{
                stacktop_memory_count == 0uz && stacktop_cache_count == stack_size && !param_count_plus_table_index_overflows &&
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
                    ++code_curr;
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
                            code_curr = reinterpret_cast<::std::byte const*>(local_index_next);
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
        [&]() noexcept
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_call_indirect_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, options.curr_wasm_id);
            emit_imm_to(bytecode, static_cast<::std::size_t>(type_index));
            emit_imm_to(bytecode, static_cast<::std::size_t>(table_index));
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
        for(::std::size_t i{}; i != effective_result_count; ++i) { operand_stack_push(callee_type.result.begin[i]); }
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
case wasm1_code::drop:
{
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
    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack)
    {
        cond_from_stack = true;
        cond_type = cond.type;
    }

    if(cond_from_stack && cond_type != curr_operand_stack_value_type::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_cond_type_not_i32.cond_type = cond_type;
        err.err_code = code_validation_error_code::select_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    bool v2_from_stack{};
    curr_operand_stack_value_type v2_type{};
    if(auto const v2{try_pop_concrete_operand()}; v2.from_stack)
    {
        v2_from_stack = true;
        v2_type = v2.type;
    }

    bool v1_from_stack{};
    curr_operand_stack_value_type v1_type{};
    if(auto const v1{try_peek_concrete_operand()}; v1.from_stack)
    {
        v1_from_stack = true;
        v1_type = v1.type;
    }

    if(v1_from_stack && v2_from_stack && v1_type != v2_type) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.select_type_mismatch.type_v1 = v1_type;
        err.err_selectable.select_type_mismatch.type_v2 = v2_type;
        err.err_code = code_validation_error_code::select_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Translate: typed `select`.
    if(v1_from_stack)
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
    if(v1_from_stack) { stacktop_after_pop_n_if_reachable(bytecode, 2uz); }

    if(!v1_from_stack && v2_from_stack) { operand_stack_push(v2_type); }

    break;
}
