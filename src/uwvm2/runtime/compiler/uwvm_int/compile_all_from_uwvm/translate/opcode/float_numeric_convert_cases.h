case wasm1_code::f32_abs:
{
    validate_numeric_unary(u8"f32.abs", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_abs_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_abs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f32_neg:
{
    validate_numeric_unary(u8"f32.neg", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_neg_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_neg_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f32_ceil:
{
    validate_numeric_unary(u8"f32.ceil", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_ceil_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ceil_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}
case wasm1_code::f32_floor:
{
    validate_numeric_unary(u8"f32.floor", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_floor_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_floor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}
case wasm1_code::f32_trunc:
{
    validate_numeric_unary(u8"f32.trunc", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_trunc_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_trunc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    break;
}
case wasm1_code::f32_nearest:
{
    validate_numeric_unary(u8"f32.nearest", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_nearest_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_nearest_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f32_sqrt:
{
    validate_numeric_unary(u8"f32.sqrt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sqrt_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sqrt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f32_add:
{
    validate_numeric_binary(u8"f32.add", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_floor_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_floor_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_ceil_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_ceil_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_trunc_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_trunc_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_nearest_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_nearest_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_abs_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_abs_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_negabs_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_negabs_localget_set_acc;
        break;
    }

    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::mac_after_mul && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_add;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_mul_2localget_local3 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_add_3localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_after_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_add_2mul_4localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f32_2mul_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off4);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        // Conbine (heavy): `local.get a; local.get b; f32.add; local.set/local.tee dst`
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
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f32)
                {
                    conbine_pending.off3 = local_offset_from_index(next_local_index);
                    conbine_pending.kind = (next_op == wasm1_code::local_set) ? conbine_pending_kind::f32_add_2localget_local_set
                                                                              : conbine_pending_kind::f32_add_2localget_local_tee;
                    break;
                }
            }
        }

# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
# endif
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
    {
        // Conbine: `local.get + const + add + local.set/local.tee` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f32)
                {
                    conbine_pending.kind = conbine_pending_kind::f32_add_imm_local_settee_same;
                    break;
                }
            }
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            auto const before_curr_stacktop{curr_stacktop};
            emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);

            bool fused_add_fill1{};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if(stacktop_memory_count != 0uz)
            {
                auto const vt0{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                if(vt0 == curr_operand_stack_value_type::f32)
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
                                decltype(translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
                            ::std::size_t const patch_site{bytecode.size() - sizeof(opfunc_ptr_t)};
                            auto const fused_fptr{
                                translate::get_uwvmint_f32_add_then_fill1_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple)};
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

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_sub:
{
    validate_numeric_binary(u8"f32.sub", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::const_f32_localget)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
    {
        // Conbine: `local.get + const + sub + local.set` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f32)
                {
                    conbine_pending.kind = conbine_pending_kind::f32_sub_imm_local_settee_same;
                    break;
                }
            }
        }

        flush_conbine_pending();
    }
    if(conbine_pending.kind == conbine_pending_kind::float_mul_2localget_local3 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_sub_3localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_after_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_sub_2mul_4localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off4);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_mul:
{
    validate_numeric_binary(u8"f32.mul", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::mac_localget3 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_mul;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_wait_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        conbine_pending.kind = conbine_pending_kind::float_2mul_after_second_mul;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        conbine_pending.kind = conbine_pending_kind::float_mul_2localget;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
    {
        // Conbine: `local.get + const + mul + local.set` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f32)
                {
                    conbine_pending.kind = conbine_pending_kind::f32_mul_imm_local_settee_same;
                    break;
                }
            }
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_div:
{
    validate_numeric_binary(u8"f32.div", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::div>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::div>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::const_f32_localget)
    {
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }

        if(next_opbase == wasm1_code::local_tee)
        {
            conbine_pending.kind = conbine_pending_kind::f32_div_from_imm_localtee_wait;

            break;
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_div_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_div_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_div_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_min:
{
    validate_numeric_binary(u8"f32.min", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::min>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::min>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_min_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_min_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_min_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_max:
{
    validate_numeric_binary(u8"f32.max", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::max>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::max>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_max_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_max_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_max_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f32_copysign:
{
    validate_numeric_binary(u8"f32.copysign", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_negabs_localget_wait_copysign)
    {
        conbine_pending.kind = conbine_pending_kind::f32_acc_add_negabs_localget_wait_add;
        break;
    }
#endif
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                           CompileOption,
                           ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::copysign>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f32)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f32_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::copysign>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_abs:
{
    validate_numeric_unary(u8"f64.abs", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_abs_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_abs_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_abs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_neg:
{
    validate_numeric_unary(u8"f64.neg", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_neg_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_neg_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_ceil:
{
    validate_numeric_unary(u8"f64.ceil", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_ceil_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ceil_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ceil_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_floor:
{
    validate_numeric_unary(u8"f64.floor", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_floor_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_floor_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_floor_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_trunc:
{
    validate_numeric_unary(u8"f64.trunc", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_trunc_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_trunc_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_trunc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_nearest:
{
    validate_numeric_unary(u8"f64.nearest", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_nearest_localget_wait_add) { break; }
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_nearest_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_nearest_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_sqrt:
{
    validate_numeric_unary(u8"f64.sqrt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sqrt_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sqrt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    break;
}
case wasm1_code::f64_add:
{
    validate_numeric_binary(u8"f64.add", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_floor_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_floor_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_ceil_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_ceil_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_trunc_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_trunc_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_nearest_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_nearest_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_abs_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_abs_localget_set_acc;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_negabs_localget_wait_add)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_negabs_localget_set_acc;
        break;
    }

    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::add>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::mac_after_mul && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_add;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_mul_2localget_local3 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_add_3localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_after_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        if constexpr(CompileOption.is_tail_call)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_add_2mul_4localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f64_2mul_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off4);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        // Conbine (heavy): `local.get a; local.get b; f64.add; local.set/local.tee dst`
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
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f64)
                {
                    conbine_pending.off3 = local_offset_from_index(next_local_index);
                    conbine_pending.kind = (next_op == wasm1_code::local_set) ? conbine_pending_kind::f64_add_2localget_local_set
                                                                              : conbine_pending_kind::f64_add_2localget_local_tee;
                    break;
                }
            }
        }

# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        bool fused_spill_and_add{};
        [[maybe_unused]] ::std::size_t fuse_site{};

        if constexpr(stacktop_enabled)
        {
            ::std::size_t const bc_before{bytecode.size()};
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);

            if(bytecode.size() != bc_before)
            {
                constexpr bool f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                             CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

                auto const spilled_vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};

                using opfunc_ptr_t = decltype(translate::get_uwvmint_f64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                fuse_site = bytecode.size() - sizeof(opfunc_ptr_t);

                auto patch_with{[&](auto fused_fptr) constexpr UWVM_THROWS
                                {
                                    ::std::byte tmp[sizeof(fused_fptr)];
                                    ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
                                    ::std::memcpy(bytecode.data() + fuse_site, tmp, sizeof(fused_fptr));
                                    fused_spill_and_add = true;
                                }};

                if(spilled_vt == curr_operand_stack_value_type::f64)
                {
                    patch_with(translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f64>(curr_stacktop,
                                                                                                                                            interpreter_tuple));
                }
                else if(spilled_vt == curr_operand_stack_value_type::f32)
                {
                    if constexpr(f32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_f32>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i32)
                {
                    if constexpr(i32_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i32>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
                else if(spilled_vt == curr_operand_stack_value_type::i64)
                {
                    if constexpr(i64_f64_merge)
                    {
                        patch_with(
                            translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<CompileOption, wasm_i64>(curr_stacktop,
                                                                                                                                         interpreter_tuple));
                    }
                }
            }
        }

        if(!fused_spill_and_add)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        conbine_pending.kind = conbine_pending_kind::none;

        break;
# endif
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f64)
    {
        // Conbine: `local.get + const + add + local.set/local.tee` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f64)
                {
                    conbine_pending.kind = conbine_pending_kind::f64_add_imm_local_settee_same;
                    break;
                }
            }
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);

        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            auto const before_curr_stacktop{curr_stacktop};
            emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);

            bool fused_add_fill1{};

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            if(stacktop_memory_count != 0uz)
            {
                auto const vt0{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                if(vt0 == curr_operand_stack_value_type::f64)
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
                                decltype(translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple));
                            ::std::size_t const patch_site{bytecode.size() - sizeof(opfunc_ptr_t)};
                            auto const fused_fptr{
                                translate::get_uwvmint_f64_add_then_fill1_fptr_from_tuple<CompileOption>(before_curr_stacktop, interpreter_tuple)};
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

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_sub:
{
    validate_numeric_binary(u8"f64.sub", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::sub>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::const_f64_localget)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f64)
    {
        // Conbine: `local.get + const + sub + local.set` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f64)
                {
                    conbine_pending.kind = conbine_pending_kind::f64_sub_imm_local_settee_same;
                    break;
                }
            }
        }

        flush_conbine_pending();
    }
    if(conbine_pending.kind == conbine_pending_kind::float_mul_2localget_local3 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_sub_3localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_after_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_sub_2mul_4localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off4);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_mul:
{
    validate_numeric_binary(u8"f64.mul", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        wasm1_code next_op{};  // init
        if(!is_polymorphic && code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(!is_polymorphic && next_op == wasm1_code::local_set)
        {
            emit_opfunc_to(bytecode,
                           translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                               CompileOption,
                               ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(curr_stacktop, interpreter_tuple));
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
                               translate::get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple<
                                   CompileOption,
                                   ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_pending.off1);
                conbine_pending.kind = conbine_pending_kind::none;
                break;
            }
        }

        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::mul>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::mac_localget3 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        conbine_pending.kind = conbine_pending_kind::mac_after_mul;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::float_2mul_wait_second_mul && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        conbine_pending.kind = conbine_pending_kind::float_2mul_after_second_mul;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        conbine_pending.kind = conbine_pending_kind::float_mul_2localget;

        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f64)
    {
        // Conbine: `local.get + const + mul + local.set` (same local) -> update-local mega-op.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::local_set || next_op == wasm1_code::local_tee)
            {
                wasm_u32 next_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                  ::fast_io::mnp::leb128_get(next_local_index))};
                (void)next_local_index_next;
                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                   local_type_from_index(next_local_index) == curr_operand_stack_value_type::f64)
                {
                    conbine_pending.kind = conbine_pending_kind::f64_mul_imm_local_settee_same;
                    break;
                }
            }
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    break;
}
case wasm1_code::f64_div:
{
    validate_numeric_binary(u8"f64.div", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::div>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::div>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
    if(conbine_pending.kind == conbine_pending_kind::const_f64_localget)
    {
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }

        if(next_opbase == wasm1_code::local_tee)
        {
            conbine_pending.kind = conbine_pending_kind::f64_div_from_imm_localtee_wait;
            break;
        }

        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_div_from_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_div_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_div_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_min:
{
    validate_numeric_binary(u8"f64.min", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::min>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::min>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_min_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_min_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_min_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_max:
{
    validate_numeric_binary(u8"f64.max", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<CompileOption,
                                                                          ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::max>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::max>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_max_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_max_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_max_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::f64_copysign:
{
    validate_numeric_binary(u8"f64.copysign", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_negabs_localget_wait_copysign)
    {
        conbine_pending.kind = conbine_pending_kind::f64_acc_add_negabs_localget_wait_add;
        break;
    }
#endif
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        emit_opfunc_to(bytecode,
                       translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                           CompileOption,
                           ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::copysign>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
# endif
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::const_f64)
    {
        emit_opfunc_to(
            bytecode,
            translate::get_uwvmint_f64_binop_imm_stack_fptr_from_tuple<CompileOption,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::float_binop::copysign>(
                curr_stacktop,
                interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_copysign_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
case wasm1_code::i32_wrap_i64:
{
    validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_wrap_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i32_trunc_f32_s:
{
    validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_trunc_f32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i32_trunc_f32_u:
{
    validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;

        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_trunc_f32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i32_trunc_f64_s:
{
    validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_trunc_f64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i32_trunc_f64_u:
{
    validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_trunc_f64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i64_extend_i32_s:
{
    validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64))
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        }

        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend_i32_s_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }

        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend_i32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::i64_extend_i32_u:
{
    validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64))
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
        }

        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend_i32_u_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }

        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_extend_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::i64_trunc_f32_s:
{
    validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_trunc_f32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::i64_trunc_f32_u:
{
    validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_trunc_f32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::i64_trunc_f64_s:
{
    validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_trunc_f64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::i64_trunc_f64_u:
{
    validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_trunc_f64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::f32_convert_i32_s:
{
    validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_from_i32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_convert_i32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f32_convert_i32_u:
{
    validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_from_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_convert_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f32_convert_i64_s:
{
    validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_convert_i64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f32_convert_i64_u:
{
    validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_convert_i64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f32_demote_f64:
{
    validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_demote_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f64_convert_i32_s:
{
    validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_from_i32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_convert_i32_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
case wasm1_code::f64_convert_i32_u:
{
    validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert;
        break;
    }

    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_from_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);
        conbine_pending.kind = conbine_pending_kind::none;
        break;
    }
#endif

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_convert_i32_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
case wasm1_code::f64_convert_i64_s:
{
    validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_convert_i64_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
case wasm1_code::f64_convert_i64_u:
{
    validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_convert_i64_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
case wasm1_code::f64_promote_f32:
{
    validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_promote_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
case wasm1_code::i32_reinterpret_f32:
{
    validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_reinterpret_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }

    break;
}
case wasm1_code::i64_reinterpret_f64:
{
    validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_reinterpret_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i64);
    }

    break;
}
case wasm1_code::f32_reinterpret_i32:
{
    validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_reinterpret_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f32); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f32);
    }

    break;
}
case wasm1_code::f64_reinterpret_i64:
{
    validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::f64) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
    }

    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_reinterpret_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64))
    {
        if constexpr(stacktop_enabled)
        {
            if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::f64); }
        }
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::f64);
    }

    break;
}
