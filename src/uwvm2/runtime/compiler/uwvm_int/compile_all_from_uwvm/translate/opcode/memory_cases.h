// Memory opcode translation separates Wasm validation from runtime address execution. Each case
// validates memarg alignment/offset and memory-0 availability first, then emits a typed opfunc that
// can trust those invariants and focus on bounds checking plus the actual load/store.
/// @warning Extension point: memory64, multi-memory, SIMD memory ops, or bulk-memory opcodes require validator, immediate parsing, and opfunc updates here.
case wasm1_code::i32_load:
{
    // Validate the static memarg before touching combine state; malformed immediates must be
    // reported at the opcode site even if a preceding `local.get` could otherwise be fused away.
    auto const full_offset{validate_mem_load.template operator()<0x28u>(u8"i32.load", 2u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 4uz, false, false>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                            // [checked fused successor] next bytes ... | end
                            // [safe consumed bytes]       | one-past is never dereferenced here
                            // ^^ fused_next_ip: after_const is a bounded parser result and was checked != code_end before the +1.
                            fused_next_ip = after_const + 1;
                        }
                        else if(op_after_const == wasm1_code::i32_and)
                        {
                            fuse_load_and_imm = true;
                            fused_imm = imm;
                            // [checked fused successor] next bytes ... | end
                            // [safe consumed bytes]       | one-past is never dereferenced here
                            // ^^ fused_next_ip: after_const is a bounded parser result and was checked != code_end before the +1.
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
                    translate::get_uwvmint_i32_load_add_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            }
            else if(fuse_load_and_imm)
            {
                emit_opfunc_to(
                    bytecode,
                    translate::get_uwvmint_i32_load_and_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            }
            else
            {
                emit_opfunc_to(
                    bytecode,
                    translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
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
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_imm_to(bytecode, offset);
        if(fuse_load_add_imm || fuse_load_and_imm)
        {
            emit_imm_to(bytecode, fused_imm);
            // fused memory op ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            // ^^ fused_next_ip: preceding bounded scan/lookahead proved a position in this code slice.
            // fused memory operation ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
            code_curr = fused_next_ip;
            // fused memory op ... code_end
            // [safe consumed bytes] unsafe (could be code_end)
            //                       ^^ code_curr may be one-past; no read occurs here.
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
                translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        break;
    }
#endif
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    // Emit the resolved memory pointer as an immediate so the hot helper avoids a module lookup on
    // every load dispatch.
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i64_load:
{
    // 64-bit loads still consume an i32 address in Wasm MVP, but produce an i64 stack value.
    // The stack-top bookkeeping below reflects that cross-type pop/push transition explicitly.
    auto const full_offset{validate_mem_load.template operator()<0x29u>(u8"i64.load", 3u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 8uz, false, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                                                                                                                         *resolved_memory.memory_p,
                                                                                                                         interpreter_tuple));
                        emit_imm_to(bytecode, conbine_pending.off1);
                        emit_imm_to(bytecode, local_offset_from_index(local_index));
                        emit_imm_to(bytecode, resolved_memory.memory_p);
                        emit_imm_to(bytecode, offset);

                        conbine_pending.kind = conbine_pending_kind::none;
                        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                        operand_stack_pop_unchecked();
                        // fused memory op ... code_end
                        // [safe consumed bytes] unsafe (could be code_end)
                        // ^^ next_ip: preceding bounded scan/lookahead proved a position in this code slice.
                        // fused memory operation ... code_end
                        // [safe consumed bytes] unsafe (could be code_end)
                        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
                        code_curr = reinterpret_cast<::std::byte const*>(next_ip);
                        // fused memory op ... code_end
                        // [safe consumed bytes] unsafe (could be code_end)
                        //                       ^^ code_curr may be one-past; no read occurs here.
                        break;
                    }
                }
            }

            if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }

            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    // Emit memory0 and the static offset as immediates so the generic i64 load helper only consumes
    // the dynamic address from the operand stack.
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x2au>(u8"f32.load", 2u, wasm_value_type_u::f32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_f32, 4uz, false, false>(
           full_offset, curr_operand_stack_value_type::f32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_load_localget_off_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, resolved_memory.memory_p);
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
                translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory.memory_p);
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
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    // Emit memory0 and the static offset as immediates; the f32 load helper consumes only the
    // dynamic i32 address from the stack.
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x2bu>(u8"f64.load", 3u, wasm_value_type_u::f64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_f64, 8uz, false, false>(
           full_offset, curr_operand_stack_value_type::f64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_load_localget_off_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, resolved_memory.memory_p);
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
                translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_load_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, resolved_memory.memory_p);
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
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_load_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_load_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    // Emit memory0 and the static offset as immediates; the f64 load helper consumes only the
    // dynamic i32 address from the stack.
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x2cu>(u8"i32.load8_s", 0u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 1uz, true, false>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load8_u:
{
    auto const full_offset{validate_mem_load.template operator()<0x2du>(u8"i32.load8_u", 0u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 1uz, false, false>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load16_s:
{
    auto const full_offset{validate_mem_load.template operator()<0x2eu>(u8"i32.load16_s", 1u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 2uz, true, false>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i32_load16_u:
{
    auto const full_offset{validate_mem_load.template operator()<0x2fu>(u8"i32.load16_u", 1u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 2uz, false, false>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
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
                translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
            // Preserve the load object while subsequent opcodes can select another memory.
            pending_u16_memory = resolved_memory;
            break;
        }
# endif
#endif
    }

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    break;
}
case wasm1_code::i64_load8_s:
{
    auto const full_offset{validate_mem_load.template operator()<0x30u>(u8"i64.load8_s", 0u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 1uz, true, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load8_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x31u>(u8"i64.load8_u", 0u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 1uz, false, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load8_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x32u>(u8"i64.load16_s", 1u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 2uz, true, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load16_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x33u>(u8"i64.load16_u", 1u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 2uz, false, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load16_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x34u>(u8"i64.load32_s", 2u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 4uz, true, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load32_s_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    auto const full_offset{validate_mem_load.template operator()<0x35u>(u8"i64.load32_u", 2u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 4uz, false, false>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_load32_u_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_load32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
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
    // Stores consume address and value. Fusion is valuable here because common Wasm emits both as
    // locals, and skipping two stack materializations reduces dispatch and memory traffic.
    auto const full_offset{validate_mem_store.template operator()<0x36u>(u8"i32.store", 2u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 4uz, false, true>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get addr; i32.const; i32.add; local.get v; i32.store` fused into `i32_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory.memory_p);
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
                translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    // Stores also receive memory0 as an immediate; the helper then only needs address/value operands
    // plus the static offset to perform bounds checking and the write.
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    // All stores consume exactly address and value and do not push a result.
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store:
{
    auto const full_offset{validate_mem_store.template operator()<0x37u>(u8"i64.store", 3u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 8uz, false, true>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get_i32_localget && conbine_pending.vt == curr_operand_stack_value_type::i64)
        {
            // Conbine: `local.get addr; local.get v; i64.store` fused into `i64_store_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::f32_store:
{
    auto const full_offset{validate_mem_store.template operator()<0x38u>(u8"f32.store", 2u, wasm_value_type_u::f32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_f32, 4uz, false, true>(
           full_offset, curr_operand_stack_value_type::f32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; local.get v; f32.store` fused into `f32_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::f64_store:
{
    auto const full_offset{validate_mem_store.template operator()<0x39u>(u8"f64.store", 3u, wasm_value_type_u::f64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_f64, 8uz, false, true>(
           full_offset, curr_operand_stack_value_type::f64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_add_localget && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        // Conbine (heavy): `local.get addr; i32.const; i32.add; local.get v; f64.store` fused into `f64_store_local_plus_imm`.
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_store_local_plus_imm_fptr<CompileOption, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_stacktop));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, resolved_memory.memory_p);
        emit_imm_to(bytecode, offset);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
#endif

    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_store_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_store_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i32_store8:
{
    auto const full_offset{validate_mem_store.template operator()<0x3au>(u8"i32.store8", 0u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 1uz, false, true>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
        {
            // Conbine: `local.get addr; local.get v; i32.store8` fused into `i32_store8_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                                                                                                             *resolved_memory.memory_p,
                                                                                                             interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, offset);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
            break;
        }
#endif
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i32_store8_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store8_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i32_store16:
{
    auto const full_offset{validate_mem_store.template operator()<0x3bu>(u8"i32.store16", 1u, wasm_value_type_u::i32)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i32, 2uz, false, true>(
           full_offset, curr_operand_stack_value_type::i32)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::u16_copy_scaled_index_after_load &&
           pending_u16_memory.memory_p == resolved_memory.memory_p)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory.memory_p);
            emit_imm_to(bytecode, conbine_pending.imm_u32);
            wasm_u32 const offset_u32{static_cast<wasm_u32>(offset)};
            emit_imm_to(bytecode, offset_u32);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::u16_copy_scaled_index_after_load)
        {
            // Different objects cannot share the fused helper's one memory pointer. Emit the saved
            // source load BEFORE the store, materializing its address/result in the current ring positions.
            flush_conbine_pending();
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
                translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                                                                                                              *resolved_memory.memory_p,
                                                                                                              interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                       translate::get_uwvmint_i32_store16_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_store16_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store8:
{
    auto const full_offset{validate_mem_store.template operator()<0x3cu>(u8"i64.store8", 0u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 1uz, false, true>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_store8_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store8_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store16:
{
    auto const full_offset{validate_mem_store.template operator()<0x3du>(u8"i64.store16", 1u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 2uz, false, true>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(CompileOption.is_tail_call)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_i64_store16_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store16_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::i64_store32:
{
    auto const full_offset{validate_mem_store.template operator()<0x3eu>(u8"i64.store32", 2u, wasm_value_type_u::i64)};
    ensure_memory_resolved();
    if(emit_memory64_scalar_if_selected.template operator()<wasm_i64, 4uz, false, true>(
           full_offset, curr_operand_stack_value_type::i64)) { break; }
    // The selected memory32 type was checked before narrowing its static offset.
    wasm_u32 const offset{static_cast<wasm_u32>(full_offset)};
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if constexpr(CompileOption.is_tail_call)
    {
        if(conbine_pending.kind == conbine_pending_kind::local_get_i32_localget && conbine_pending.vt == curr_operand_stack_value_type::i64)
        {
            // Conbine: `local.get addr; local.get v; i64.store32` fused into `i64_store32_localget_off`.
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.off2);
            emit_imm_to(bytecode, resolved_memory.memory_p);
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
                       translate::get_uwvmint_i64_store32_fptr_from_tuple<CompileOption>(curr_stacktop, *resolved_memory.memory_p, interpreter_tuple));
    }
    else
    {
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
        flush_conbine_pending();
#endif
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_store32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, offset);
    stacktop_after_pop_n_if_reachable(bytecode, 2uz);
    break;
}
case wasm1_code::memory_size:
{
    // The selected declaration determines the page-count type (i32 or i64).
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

    // [memory.size] memidx ... (code_end); the scanner bounds-checks the entire immediate.
    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
    // [memory.size memidx] ... unsafe (could be code_end)
    //                      ^^ code_curr
    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);
    current_memory_index = memory_index;
    current_memory_address64 = memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64;
    auto const address_type{current_memory_address64 ? wasm_value_type_u::i64 : wasm_value_type_u::i32};

    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), false);

    ensure_memory_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    flush_conbine_pending();
#endif
    stacktop_prepare_push1_if_reachable(bytecode, address_type);
    if(current_memory_address64)
    { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_pages_fptr_from_tuple<false, CompileOption>(curr_stacktop, interpreter_tuple)); }
    else
    { emit_opfunc_to(bytecode, translate::get_uwvmint_memory_size_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    stacktop_commit_push1_typed_if_reachable(address_type);

    operand_stack_push(address_type);
    break;
}
case wasm1_code::memory_grow:
{
    // `memory.grow` consumes the requested page delta and returns the previous page count or -1.
    // The maximum limit is emitted as an immediate so the runtime helper can enforce module limits
    // without re-reading compile-time metadata.
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

    // [memory.grow] memidx ... (code_end); the scanner bounds-checks the entire immediate.
    auto const memory_index{::uwvm2::validation::standard::wasm3::read_memory_index(
        code_curr, code_end, op_begin, !wasm1p1_para.disable_multi_memory, err)};
    // [memory.grow memidx] ... unsafe (could be code_end)
    //                      ^^ code_curr
    ::uwvm2::validation::standard::wasm3::validate_memory_index(memory_index, all_memory_count, op_begin, err);
    current_memory_index = memory_index;
    current_memory_address64 = memory_address_type_at(memory_index) == ::uwvm2::validation::standard::wasm3::storage_address_type::i64;
    auto const address_type{current_memory_address64 ? wasm_value_type_u::i64 : wasm_value_type_u::i32};

    validate_checked_memory_page(op_begin, memory_address_type_at(memory_index), true);

    ensure_memory_resolved();
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    flush_conbine_pending();
#endif
    if(current_memory_address64)
    { emit_opfunc_to(bytecode, translate::get_uwvmint_memory64_pages_fptr_from_tuple<true, CompileOption>(curr_stacktop, interpreter_tuple)); }
    else
    { emit_opfunc_to(bytecode, translate::get_uwvmint_memory_grow_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)); }
    emit_imm_to(bytecode, resolved_memory.memory_p);
    emit_imm_to(bytecode, resolved_memory.max_limit_memory_length);

    operand_stack_push(address_type);
    break;
}
