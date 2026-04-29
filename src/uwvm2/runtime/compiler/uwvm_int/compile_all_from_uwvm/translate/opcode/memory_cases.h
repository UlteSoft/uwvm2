case wasm1_code::i32_load:
{
    wasm_u32 const offset{validate_mem_load(u8"i32.load", 2u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: allow fusing the *top* `local.get` into this load even when there is a deeper `local.get`
        // kept on the operand stack (e.g. `local.get p; local.get p; i32.load` as part of `i32.store` address/value ordering).
        emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::local_get;
        conbine_pending.off1 = conbine_pending.off2;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get addr; i32.load` fused into `i32_load_localget_off`.
        // Stack effect: push 1 (result), because the address is taken from the local (not from the operand stack).
        bool fuse_load_add_imm{};
        bool fuse_load_and_imm{};
        wasm_i32 fused_imm{};  // no init
        ::std::byte const* fused_next_ip{code_curr};

        // Further fuse: `local.get addr; i32.load; i32.const imm; (i32.add|i32.and)`.
        // This remains a push-1 (result) fusion, because the address comes from the local.
        if(code_curr != code_end)
        {
            wasm1_code next_op;  // no init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::i32_const)
            {
                wasm_i32 imm{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                        ::fast_io::mnp::leb128_get(imm))};
                if(imm_err == ::fast_io::parse_code::ok)
                {
                    auto const after_const{reinterpret_cast<::std::byte const*>(imm_next)};
                    if(after_const != code_end)
                    {
                        wasm1_code op_after_const;  // no init
                        ::std::memcpy(::std::addressof(op_after_const), after_const, sizeof(op_after_const));
                        if(op_after_const == wasm1_code::i32_add)
                        {
                            fuse_load_add_imm = true;
                            fused_imm = imm;
                            fused_next_ip = after_const + 1;
                        }
                        else if(op_after_const == wasm1_code::i32_and)
                        {
                            fuse_load_and_imm = true;
                            fused_imm = imm;
                            fused_next_ip = after_const + 1;
                        }
                    }
                }
            }
        }

        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
        if constexpr(CompileOption.is_tail_call)
        {
            if(fuse_load_add_imm)
            {
                emit_opfunc_to(
                    bytecode,
                    translate::get_uwvmint_i32_load_add_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            }
            else if(fuse_load_and_imm)
            {
                emit_opfunc_to(
                    bytecode,
                    translate::get_uwvmint_i32_load_and_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(
                    bytecode,
                    translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            }
        }
        else
        {
            if(fuse_load_add_imm)
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_load_add_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
            }
            else if(fuse_load_and_imm)
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_load_and_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
            }
            else
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_load_localget_off_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
            }
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        if(fuse_load_add_imm || fuse_load_and_imm)
        {
            emit_imm_to(bytecode, fused_imm);
            code_curr = fused_next_ip;
        }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add)
    {
        // Conbine: `local.get addr; i32.const; i32.add; i32.load` fused into `i32_load_local_plus_imm`.
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        break;
    }
#endif
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i64_load:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load", 3u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(!CompileOption.is_tail_call)
    {
        // Byref/loop mode: `i64.load` does not implement conbine consume sites.
        // Ensure any pending local.get/const chains are materialized before emitting the load.
        if(conbine_pending.kind != conbine_pending_kind::none) [[unlikely]] { flush_conbine_pending(); }
    }
#endif
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; i64.load` fused into `i64_load_localget_off` (push 1),
            // or `local.get addr; i64.load; local.set dst` fused into `i64_load_localget_set_local` (net 0).
            if(!is_polymorphic && code_curr != code_end)
            {
                wasm1_code next_op{};  // init
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
                if(next_op == wasm1_code::local_set)
                {
                    wasm_u32 local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_ip, parse_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                             ::fast_io::mnp::leb128_get(local_index))};
                    if(parse_err == ::fast_io::parse_code::ok && local_index < all_local_count &&
                       local_type_from_index(local_index) == curr_operand_stack_value_type::i64)
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_i64_load_localget_set_local_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                                         *resolved_memory0.memory_p,
                                                                                                                         interpreter_tuple));
                        emit_imm_to(bytecode, conbine_pending.off1);
                        emit_imm_to(bytecode, local_offset_from_index(local_index));
                        emit_imm_to(bytecode, resolved_memory0.memory_p);
                        emit_imm_to(bytecode, offset);

                        conbine_pending.kind = conbine_pending_kind::none;
                        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                        operand_stack_pop_unchecked();
                        code_curr = reinterpret_cast<::std::byte const*>(next_ip);
                        break;
                    }
                }
            }

            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }

            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);

            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
            break;
        }
    }
#endif
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        // i64.load is a pop(i32)+push(i64) op; ensure the destination ring has a free slot before the opfunc writes it.
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::f32_load:
{
    wasm_u32 const offset{validate_mem_load(u8"f32.load", 2u, wasm_value_type_u::f32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::local_get;
        conbine_pending.off1 = conbine_pending.off2;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get addr; f32.load` fused into `f32_load_localget_off`.
        // Stack effect: push 1 (result), because the address is taken directly from the local.
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_load_localget_off_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        break;
    }
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; f32.load` fused into `f32_load_local_plus_imm`.
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        break;
    }
#endif
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        // f32.load is a pop(i32)+push(f32) op; the opfunc writes into the f32 ring.
        // Ensure the destination ring has a free slot before emitting the opfunc, or the runtime write may overwrite
        // the deepest cached value and corrupt subsequent computations (notably inside libc printf/vsnprintf paths).
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
        else
        {
            // i32 addr -> f32 result: cross-ring, depth unchanged => model as pop+push.
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
        }
    }
    break;
}
case wasm1_code::f64_load:
{
    wasm_u32 const offset{validate_mem_load(u8"f64.load", 3u, wasm_value_type_u::f64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::local_get;
        conbine_pending.off1 = conbine_pending.off2;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get addr; f64.load` fused into `f64_load_localget_off`.
        // Stack effect: push 1 (result), because the address is taken directly from the local.
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_load_localget_off_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        break;
    }
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; f64.load` fused into `f64_load_local_plus_imm`.
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        break;
    }
#endif
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
    {
        // f64.load is a pop(i32)+push(f64) op; ensure the destination ring has a free slot before emitting the opfunc.
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
        else
        {
            // i32 addr -> f64 result: cross-ring, depth unchanged => model as pop+push.
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
        }
    }
    break;
}
case wasm1_code::i32_load8_s:
{
    wasm_u32 const offset{validate_mem_load(u8"i32.load8_s", 0u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.off1 = conbine_pending.off2;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; i32.load8_s` fused into `i32_load8_s_localget_off`.
            // Stack effect: push 1 (result), because the address is taken from the local.
            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load8_u:
{
    wasm_u32 const offset{validate_mem_load(u8"i32.load8_u", 0u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.off1 = conbine_pending.off2;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; i32.load8_u` fused into `i32_load8_u_localget_off`.
            // Stack effect: push 1 (result), because the address is taken from the local.
            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load16_s:
{
    wasm_u32 const offset{validate_mem_load(u8"i32.load16_s", 1u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.off1 = conbine_pending.off2;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; i32.load16_s` fused into `i32_load16_s_localget_off`.
            // Stack effect: push 1 (result), because the address is taken from the local.
            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load16_u:
{
    wasm_u32 const offset{validate_mem_load(u8"i32.load16_u", 1u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.off1 = conbine_pending.off2;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; i32.load16_u` fused into `i32_load16_u_localget_off`.
            // Stack effect: push 1 (result), because the address is taken from the local.
            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::u16_copy_scaled_index_after_shl)
        {
            conbine_pending.kind = conbine_pending_kind::u16_copy_scaled_index_after_load;
            conbine_pending.imm_u32 = offset;
            break;
        }
# endif
#endif
    }

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i64_load8_s:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load8_s", 0u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i64_load8_u:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load8_u", 0u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i64_load16_s:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load16_s", 1u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i64_load16_u:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load16_u", 1u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i64_load32_s:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load32_s", 2u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load32_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i64_load32_u:
{
    wasm_u32 const offset{validate_mem_load(u8"i64.load32_u", 2u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load32_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    if constexpr(stacktop_enabled)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
        }
    }
    break;
}
case wasm1_code::i32_store:
{
    wasm_u32 const offset{validate_mem_store(u8"i32.store", 2u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get addr; i32.const; i32.add; local.get v; i32.store` fused into `i32_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get_i32_localget && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; local.get v; i32.store` fused into `i32_store_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; local.get v; i32.store` fused into `i32_store_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
        {
            // Conbine: `local.get addr; i32.const imm; i32.store` fused into `i32_store_imm_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store:
{
    wasm_u32 const offset{validate_mem_store(u8"i64.store", 3u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get_i32_localget && conbine_pending.vt == curr_operand_stack_value_type::i64)
        {
            // Conbine: `local.get addr; local.get v; i64.store` fused into `i64_store_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::f32_store:
{
    wasm_u32 const offset{validate_mem_store(u8"f32.store", 2u, wasm_value_type_u::f32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; local.get v; f32.store` fused into `f32_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending(); 
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::f64_store:
{
    wasm_u32 const offset{validate_mem_store(u8"f64.store", 3u, wasm_value_type_u::f64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; local.get v; f64.store` fused into `f64_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory0.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    { 
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i32_store8:
{
    wasm_u32 const offset{validate_mem_store(u8"i32.store8", 0u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; local.get v; i32.store8` fused into `i32_store8_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
        {
            // Conbine: `local.get addr; i32.const imm; i32.store8` fused into `i32_store8_imm_localget_off`.
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                             *resolved_memory0.memory_p,
                                                                                                             interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_store8_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store8_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i32_store16:
{
    wasm_u32 const offset{validate_mem_store(u8"i32.store16", 1u, wasm_value_type_u::i32)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::u16_copy_scaled_index_after_load)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, conbine_pending.imm_u32);
            wasm_u32 const offset_u32{static_cast<wasm_u32>(offset)};
            emit_imm_to(bytecode, offset_u32);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }
    }
#endif
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; local.get v; i32.store16` fused into `i32_store16_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
        {
            // Conbine: `local.get addr; i32.const imm; i32.store16` fused into `i32_store16_imm_localget_off`.
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop,
                                                                                                              *resolved_memory0.memory_p,
                                                                                                              interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
#endif
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_store16_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store16_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store8:
{
    wasm_u32 const offset{validate_mem_store(u8"i64.store8", 0u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_store8_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store8_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store16:
{
    wasm_u32 const offset{validate_mem_store(u8"i64.store16", 1u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_store16_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store16_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store32:
{
    wasm_u32 const offset{validate_mem_store(u8"i64.store32", 2u, wasm_value_type_u::i64)};
    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get_i32_localget && conbine_pending.vt == curr_operand_stack_value_type::i64)
        {
            // Conbine: `local.get addr; local.get v; i64.store32` fused into `i64_store32_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory0.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
    }
#endif
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_store32_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory0.memory_p, interpreter_tuple));
    }
    else
    {
        flush_conbine_pending();
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::memory_size:
{
    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.size memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // Wasm MVP encodes the reserved memidx operand of `memory.size` as unsigned LEB128.
    // Validation here must follow the same rule as the core validator:
    // parse a well-formed LEB128 `u32`, then require that the decoded value is zero.
    //
    // Do not tighten this to a literal `0x00` byte check. The W3C binary integer grammar permits
    // non-canonical zero encodings with trailing-zero continuation bytes as long as the LEB128 is
    // otherwise well-formed within the width bounds.
    wasm_u32 memidx;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.size memidx ...
    // [ safe           ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // The MVP restriction is semantic: only memory index 0 is legal here.
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.size";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    emit_opfunc_to(bytecode, translate::get_uwvmint_memory_size_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);

    operand_stack_push(wasm_value_type_u::i32);
    break;
}
case wasm1_code::memory_grow:
{
    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // memory.grow memidx ...
    // [ safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    // `memory.grow` has the same reserved memidx encoding rule as `memory.size`.
    // We must validate the decoded LEB128 value, not assume the encoding is a single raw byte.
    wasm_u32 memidx;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [mem_next, mem_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(memidx))};
    if(mem_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(mem_err);
    }

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //             ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(mem_next);

    // memory.grow memidx ...
    // [        safe    ] unsafe (could be the section_end)
    //                    ^^ code_curr

    // Again, check the decoded memidx rather than the exact byte pattern.
    if(memidx != 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_memory_index.memory_index = memidx;
        err.err_selectable.illegal_memory_index.all_memory_count = all_memory_count;
        err.err_code = code_validation_error_code::illegal_memory_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(all_memory_count == 0u) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.no_memory.op_code_name = u8"memory.grow";
        err.err_selectable.no_memory.align = 0u;
        err.err_selectable.no_memory.offset = 0u;
        err.err_code = code_validation_error_code::no_memory;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"memory.grow", 1uz);
    }

    if(auto const delta{try_pop_concrete_operand()}; delta.from_stack && delta.type != wasm_value_type_u::i32) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.memory_grow_delta_type_not_i32.delta_type = delta.type;
        err.err_code = code_validation_error_code::memory_grow_delta_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    ensure_memory0_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_memory_grow_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, resolved_memory0.memory_p);
    emit_imm_to(bytecode, resolved_memory0.max_limit_memory_length);

    operand_stack_push(wasm_value_type_u::i32);
    break;
}
