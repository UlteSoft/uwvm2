case wasm1_code::i32_clz:
{
    validate_numeric_unary(u8"i32.clz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_clz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_clz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i32_ctz:
{
    validate_numeric_unary(u8"i32.ctz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ctz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ctz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i32_popcnt:
{
    validate_numeric_unary(u8"i32.popcnt", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_popcnt_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_popcnt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i32_add:
{
    validate_numeric_binary(u8"i32.add", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Delay-local: `...; local.get rhs; i32.add` -> one dispatch (net stack effect 0 relative to pre-`local.get`).
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }
        if(!is_polymorphic && next_op == wasm1_code::local_tee)
        {
            // If `local.tee` is immediately followed by `br_if`, the translator may fuse them into
            // `br_if_local_tee_nz`, which means the original `local.tee` opfunc will not be
            // emitted as a standalone bytecode op. The delay-local `*_local_tee` mega-op relies
            // on skipping that standalone `local.tee` opfunc, so avoid this variant and let the
            // `local.tee + br_if` fusion handle the store+branch.
            bool const local_tee_followed_by_br_if{
                [&]() constexpr UWVM_THROWS
                {
                    wasm_u32 tmp_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_code, parse_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                               reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                               ::fast_io::mnp::leb128_get(tmp_local_index))};
                    if(parse_err != ::fast_io::parse_code::ok) { return false; }
                    auto const* const next_code_bytes{reinterpret_cast<::std::byte const*>(next_code)};
                    if(next_code_bytes == code_end) { return false; }
                    wasm1_code after_tee{};  // init
                    ::std::memcpy(::std::addressof(after_tee), next_code_bytes, sizeof(after_tee));
                    return after_tee == wasm1_code::br_if;
                }()};

            if(!local_tee_followed_by_br_if)
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::mac_after_mul && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_add;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_xor_constc)
    {
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rot_xor_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.imm_i32_2);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2_const_i32_mul)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_mul_imm_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get2_const_i32_shl)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_shl_imm_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        // Conbine: `local.get + const + add + local.set/local.tee` (same local).
        // Lookahead is safe: we do not advance `code_curr`, and `code_curr` already points to the next opcode.
        // [ ... i32.add ] | next_op ...
        // [     safe    ] | unsafe
        //                 ^^ code_curr

        if(code_curr != code_end)
        {
            wasm1_code next_op;  // no init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count)
                {
                    auto const next_local_off{local_offset_from_index(next_local_index)};
                    if(next_local_off == conbine_pending.off1)
                    {
                        conbine_pending.kind = conbine_pending_kind::i32_add_imm_local_settee_same;
                        break;
                    }
                }
            }
        }
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        // Conbine: `local.get + const + add` followed by load/store using the computed address.
        if(code_curr != code_end)
        {
            wasm1_code next_op;  // no init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

            if(next_op == wasm1_code::local_get || next_op == wasm1_code::i32_load
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
               || next_op == wasm1_code::f32_load || next_op == wasm1_code::f64_load
# endif
            )
            {
                conbine_pending.kind = conbine_pending_kind::local_get_const_i32_add;
                break;
            }
        }
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get a; local.get b; i32.add; local.set/local.tee dst`
        // If the result is immediately stored, fold the whole sequence into one combined opcode to
        // avoid materializing the add-result on the operand stack.
        if(code_curr != code_end)
        {
            wasm1_code next_op;  // no init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const* const next_local_imm_begin{reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1u)};
                auto const* const next_local_imm_end{reinterpret_cast<char8_t_const_may_alias_ptr>(code_end)};
                auto const [next_local_index_next, next_local_index_err]{
                    ::fast_io::parse_by_scan(next_local_imm_begin, next_local_imm_end, ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;

                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::i32)
                {
                    conbine_pending.off3 = local_offset_from_index(next_local_index);
                    conbine_pending.kind = (next_op == wasm1_code::local_set) ? conbine_pending_kind::i32_add_2localget_local_set
                                                                              : conbine_pending_kind::i32_add_2localget_local_tee;
                    break;
                }
            }
        }

        bool fused_spill_and_add{};
        [[maybe_unused]] ::std::size_t fuse_site{};

        if constexpr(stacktop_enabled)
        {
            // i32_add_2localget pushes 1 result; spill if ring is full.
            ::std::size_t const bc_before{bytecode.size()};
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);

            // If a spill opfunc was emitted, rewrite that spill into a fused "spill1 + i32_add_2localget" opfunc and
            // reuse the would-be `i32_add_2localget` opfunc slot for immediates.
            if(bytecode.size() != bc_before)
            {
                constexpr bool i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_i32_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                fuse_site = bytecode.size() - sizeof(opfunc_ptr_t);

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(bytecode.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_add = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop,
                                                                                                                                            interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    if constexpr(i32_i64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(i32_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    if constexpr(i32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
            }
        }

        if(!fused_spill_and_add)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            auto const before_curr_stacktop{curr_stacktop};
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);

            bool fused_add_fill1{};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(stacktop_memory_count != 0uz)
            {
                auto const vt0{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                if(vt0 == curr_operand_stack_value_type::i32)
                {
                    ::std::size_t const begin0{stacktop_range_begin_pos(vt0)};
                    ::std::size_t const end0{stacktop_range_end_pos(vt0)};
                    ::std::size_t const ring0{end0 - begin0};
                    if(ring0 != 0uz && stacktop_cache_count_for_range(begin0, end0) == ring0 - 1uz)
                    {
                        bool stop_after_one{};
                        if(stacktop_memory_count == 1uz) { stop_after_one = true; }
                        else
                        {
                            auto const vt1{codegen_operand_stack.index_unchecked(stacktop_memory_count - 2uz).type};
                            if(stacktop_ranges_merged_for(vt0, vt1)) { stop_after_one = true; }
                            else
                            {
                                ::std::size_t const begin1{stacktop_range_begin_pos(vt1)};
                                ::std::size_t const end1{stacktop_range_end_pos(vt1)};
                                ::std::size_t const ring1{end1 - begin1};
                                if(ring1 != 0uz && stacktop_cache_count_for_range(begin1, end1) == ring1) { stop_after_one = true; }
                            }
                        }

                        if(stop_after_one)
                        {
                            using opfunc_ptr_t =
                                decltype(translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
                            ::std::size_t const patch_site{bytecode.size() - sizeof(opfunc_ptr_t)};
                            auto const fused_fptr{
                                translate::get_uwvmint_i32_add_then_fill1_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple)};
                            ::std::byte tmp[sizeof(fused_fptr)];
                            ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                            ::std::memcpy(bytecode.data() + patch_site, tmp, sizeof(fused_fptr));

                            --stacktop_memory_count;
                            ++stacktop_cache_count;
                            ++stacktop_cache_count_ref_for_vt(vt0);
                            fused_add_fill1 = true;
                        }
                    }
                }
            }
#endif

            if(!fused_add_fill1) { stacktop_fill_to_canonical(bytecode); }
            break;
        }
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_sub:
{
    validate_numeric_binary(u8"i32.sub", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_sub_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_sub_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_mul:
{
    validate_numeric_binary(u8"i32.mul", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::mac_localget3 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_mul;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get2_const_i32)
    {
        conbine_pending.kind = conbine_pending_kind::local_get2_const_i32_mul;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_div_s:
{
    validate_numeric_binary(u8"i32.div_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::div_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_div_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_div_u:
{
    validate_numeric_binary(u8"i32.div_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::div_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_div_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_rem_s:
{
    validate_numeric_binary(u8"i32.rem_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rem_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: `local.get a; local.get b; i32.rem_s` fused into `i32_rem_s_2localget`.
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_s_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_rem_u:
{
    validate_numeric_binary(u8"i32.rem_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rem_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
#  ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
        // Heavy: `local.get a; local.get b; i32.rem_u; i32.eqz; br_if <L>` -> local-based `br_if_i32_rem_u_eqz_2localget`.
        if(!is_polymorphic)
        {
            wasm1_code next_op{};  // init
            if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
            if(next_op == wasm1_code::i32_eqz && static_cast<::std::size_t>(code_end - code_curr) >= sizeof(wasm1_code) * 2uz)
            {
                wasm1_code next2_op{};  // init
                ::std::memcpy(::std::addressof(next2_op), code_curr + sizeof(wasm1_code), sizeof(next2_op));
                if(next2_op == wasm1_code::br_if)
                {
                    conbine_pending.kind = conbine_pending_kind::i32_rem_u_2localget_wait_eqz;
                    break;
                }
            }
        }
#  endif
        // Conbine: `local.get a; local.get b; i32.rem_u` fused into `i32_rem_u_2localget`.
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_u_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rem_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_and:
{
    validate_numeric_binary(u8"i32.and", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        // Prefer keeping the existing `i32.and ; br_if` fusion when the and-result is immediately consumed by a branch.
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase != wasm1_code::br_if)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                           ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(
                    curr_stacktop,
                    interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }

        flush_conbine_pending();
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Prefer the existing `i32.and ; br_if` fusion when immediately branching on the result.
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase != wasm1_code::br_if)
        {
            emit_opfunc_to(
                bytecode,
                translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                              ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(
                    curr_stacktop,
                    interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }

        flush_conbine_pending();
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_and_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_and_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }

    br_if_fuse.kind = br_if_fuse_kind::i32_and_nz;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;

    // else: Combine disabled: no `i32.and ; br_if` fusion tracking.
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_and_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }

    break;
}
case wasm1_code::i32_or:
{
    validate_numeric_binary(u8"i32.or", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_or_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_or_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_or_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_xor:
{
    validate_numeric_binary(u8"i32.xor", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Delay-local: `...; local.get rhs; i32.xor` -> one dispatch (net stack effect 0 relative to pre-`local.get`).
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }
        if(!is_polymorphic && next_op == wasm1_code::local_tee)
        {
            bool const local_tee_followed_by_br_if{
                [&]() constexpr UWVM_THROWS
                {
                    wasm_u32 tmp_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_code, parse_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                               reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                               ::fast_io::mnp::leb128_get(tmp_local_index))};
                    if(parse_err != ::fast_io::parse_code::ok) { return false; }
                    auto const* const next_code_bytes{reinterpret_cast<::std::byte const*>(next_code)};
                    if(next_code_bytes == code_end) { return false; }
                    wasm1_code after_tee{};  // init
                    ::std::memcpy(::std::addressof(after_tee), next_code_bytes, sizeof(after_tee));
                    return after_tee == wasm1_code::br_if;
                }()};

            if(!local_tee_followed_by_br_if)
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::xorshift_after_shr)
    {
        conbine_pending.kind = conbine_pending_kind::xorshift_after_xor1;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::xorshift_after_shl)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xorshift_mix_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, conbine_pending.imm_i32_2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_gety)
    {
        conbine_pending.kind = conbine_pending_kind::rot_xor_add_after_xor;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_after_rotl)
    {
        conbine_pending.kind = conbine_pending_kind::rotl_xor_local_set_after_xor;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_shl:
{
    validate_numeric_binary(u8"i32.shl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::xorshift_after_xor1_getx_constb)
    {
        conbine_pending.kind = conbine_pending_kind::xorshift_after_shl;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get2_const_i32)
    {
        // Lookahead is safe: `validate_numeric_binary` already advanced `code_curr` to the next opcode.
        // [ ... i32.shl ] | next_op ...
        // [     safe    ] | unsafe
        //                 ^^ code_curr
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
        if constexpr(CompileOption.is_tail_call)
        {
            wasm1_code next_op{};  // init
            if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
            if(next_op == wasm1_code::i32_load16_u)
            {
                conbine_pending.kind = conbine_pending_kind::u16_copy_scaled_index_after_shl;
                break;
            }
        }
# endif
        conbine_pending.kind = conbine_pending_kind::local_get2_const_i32_shl;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shl_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_shr_s:
{
    validate_numeric_binary(u8"i32.shr_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shr_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shr_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_shr_u:
{
    validate_numeric_binary(u8"i32.shr_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::xorshift_pre_shr)
    {
        conbine_pending.kind = conbine_pending_kind::xorshift_after_shr;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shr_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_shr_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_rotl:
{
    validate_numeric_binary(u8"i32.rotl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_tee)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_i32_binop_imm_stack_local_tee_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            conbine_pending.kind = conbine_pending_kind::none;
            break;
        }
# endif
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get2_const_i32)
    {
        // Prefer the full `y ^= rotl(x, r)` update-local fusion when followed by `i32.xor; local.set/local.tee`.
        wasm1_code next_op{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::i32_xor)
        {
            conbine_pending.kind = conbine_pending_kind::rotl_xor_local_set_after_rotl;
            break;
        }

        emit_local_get_typed_to(bytecode, curr_operand_stack_value_type::i32, conbine_pending.off1);
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        conbine_pending.kind = conbine_pending_kind::rot_xor_add_after_rotl;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_rotr:
{
    validate_numeric_binary(u8"i32.rotr", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotr_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotr_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_clz:
{
    validate_numeric_unary(u8"i64.clz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_clz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_clz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i64_ctz:
{
    validate_numeric_unary(u8"i64.ctz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ctz_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ctz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i64_popcnt:
{
    validate_numeric_unary(u8"i64.popcnt", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_popcnt_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_popcnt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::i64_add:
{
    validate_numeric_binary(u8"i64.add", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::mac_after_mul && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_add;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        bool fused_spill_and_add{};
        [[maybe_unused]] ::std::size_t fuse_site{};

        if constexpr(stacktop_enabled)
        {
            ::std::size_t const bc_before{bytecode.size()};
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);

            if(bytecode.size() != bc_before)
            {
                constexpr bool i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_i64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                fuse_site = bytecode.size() - sizeof(opfunc_ptr_t);

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(bytecode.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_add = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop,
                                                                                                                                            interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    if constexpr(i32_i64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(i64_f32_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    if constexpr(i64_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
            }
        }

        if(!fused_spill_and_add)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            auto const before_curr_stacktop{curr_stacktop};
            emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);

            bool fused_add_fill1{};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(stacktop_memory_count != 0uz)
            {
                auto const vt0{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                if(vt0 == curr_operand_stack_value_type::i64)
                {
                    ::std::size_t const begin0{stacktop_range_begin_pos(vt0)};
                    ::std::size_t const end0{stacktop_range_end_pos(vt0)};
                    ::std::size_t const ring0{end0 - begin0};
                    if(ring0 != 0uz && stacktop_cache_count_for_range(begin0, end0) == ring0 - 1uz)
                    {
                        bool stop_after_one{};
                        if(stacktop_memory_count == 1uz) { stop_after_one = true; }
                        else
                        {
                            auto const vt1{codegen_operand_stack.index_unchecked(stacktop_memory_count - 2uz).type};
                            if(stacktop_ranges_merged_for(vt0, vt1)) { stop_after_one = true; }
                            else
                            {
                                ::std::size_t const begin1{stacktop_range_begin_pos(vt1)};
                                ::std::size_t const end1{stacktop_range_end_pos(vt1)};
                                ::std::size_t const ring1{end1 - begin1};
                                if(ring1 != 0uz && stacktop_cache_count_for_range(begin1, end1) == ring1) { stop_after_one = true; }
                            }
                        }

                        if(stop_after_one)
                        {
                            using opfunc_ptr_t =
                                decltype(translate::get_uwvmint_i64_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
                            ::std::size_t const patch_site{bytecode.size() - sizeof(opfunc_ptr_t)};
                            auto const fused_fptr{
                                translate::get_uwvmint_i64_add_then_fill1_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple)};
                            ::std::byte tmp[sizeof(fused_fptr)];
                            ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                            ::std::memcpy(bytecode.data() + patch_site, tmp, sizeof(fused_fptr));

                            --stacktop_memory_count;
                            ++stacktop_cache_count;
                            ++stacktop_cache_count_ref_for_vt(vt0);
                            fused_add_fill1 = true;
                        }
                    }
                }
            }
#endif

            if(!fused_add_fill1) { stacktop_fill_to_canonical(bytecode); }
            break;
        }
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_sub:
{
    validate_numeric_binary(u8"i64.sub", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_mul:
{
    validate_numeric_binary(u8"i64.mul", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::mac_localget3 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_mul;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_div_s:
{
    validate_numeric_binary(u8"i64.div_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::div_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_div_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_div_u:
{
    validate_numeric_binary(u8"i64.div_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::div_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_div_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_rem_s:
{
    validate_numeric_binary(u8"i64.rem_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rem_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rem_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_rem_u:
{
    validate_numeric_binary(u8"i64.rem_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rem_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rem_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_and:
{
    validate_numeric_binary(u8"i64.and", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_and_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_and_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_or:
{
    validate_numeric_binary(u8"i64.or", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_or_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_xor:
{
    validate_numeric_binary(u8"i64.xor", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_xor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_shl:
{
    validate_numeric_binary(u8"i64.shl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_shl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_shr_s:
{
    validate_numeric_binary(u8"i64.shr_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_shr_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_shr_u:
{
    validate_numeric_binary(u8"i64.shr_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_shr_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_rotl:
{
    validate_numeric_binary(u8"i64.rotl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rotl_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i64_rotr:
{
    validate_numeric_binary(u8"i64.rotr", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::const_i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_i64_binop_2localget_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rotr_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
