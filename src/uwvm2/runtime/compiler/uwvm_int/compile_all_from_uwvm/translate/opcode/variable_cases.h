// Variable opcode translation keeps the validated Wasm local/global model synchronized with the
// interpreter bytecode model. Most of the code below exists to explain when a stack value can be
// delayed, fused, or read directly from storage without violating Wasm's typed stack semantics.
/// @warning Extension point: new local/global value categories require local frame layout, stack-top cache, global storage, and opfunc support here.
case wasm1_code::local_get:
{
    // `local.get` is both a semantic stack push and the most common seed for later fusions.
    // We first validate the encoded local index exactly, then decide whether to emit now or keep a
    // pending local reference that a following load/store/arithmetic opcode can consume directly.
    // local.get ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.get ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.get local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_u32 local_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};
    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.get local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.get local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    if(local_index >= all_local_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};
    // Runtime local helpers address locals by byte offset inside the compiled frame, not by Wasm
    // index. Resolve the offset once so every fusion emits the same frame address.
    auto const local_off{local_offset_from_index(local_index)};

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // Heavy combine: `local.get` xN + `add` x(N-1) -> one fused "add-reduce" opfunc (push 1).
    // This eliminates deep stack spill/fill traffic for large local-get expression trees (e.g. `stack_spill_*` benches).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::none &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::i64 ||
        curr_local_type == curr_operand_stack_value_type::f64))
    {
        wasm1_code const add_op{curr_local_type == curr_operand_stack_value_type::i32 ? wasm1_code::i32_add
                                : curr_local_type == curr_operand_stack_value_type::i64 ? wasm1_code::i64_add
                                                                                        : wasm1_code::f64_add};

        auto const try_add_reduce{
            [&](::std::size_t want) constexpr UWVM_THROWS -> bool
            {
                ::uwvm2::utils::container::array<local_offset_t, 12uz> offs{};  // max (f64)
                offs[0] = local_offset_from_index(local_index);

                ::std::byte const* scan{code_curr};
                bool ok{true};

                // Parse the remaining `local.get`s.
                for(::std::size_t i{1uz}; i < want; ++i)
                {
                    if(scan == code_end)
                    {
                        ok = false;
                        break;
                    }

                    wasm1_code op{};  // init
                    ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                    if(op != wasm1_code::local_get)
                    {
                        ok = false;
                        break;
                    }

                    wasm_u32 next_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                      ::fast_io::mnp::leb128_get(next_local_index))};
                    if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                       local_type_from_index(next_local_index) != curr_local_type)
                    {
                        ok = false;
                        break;
                    }

                    offs[i] = local_offset_from_index(next_local_index);
                    scan = reinterpret_cast<::std::byte const*>(next_local_index_next);
                }

                // Parse the trailing adds.
                for(::std::size_t i{}; ok && i + 1uz < want; ++i)
                {
                    if(scan == code_end)
                    {
                        ok = false;
                        break;
                    }
                    wasm1_code op{};  // init
                    ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                    if(op != add_op)
                    {
                        ok = false;
                        break;
                    }
                    ++scan;
                }

                if(!ok) { return false; }

                // Net stack effect: push 1 value of `curr_local_type`.
                operand_stack_push(curr_local_type);

                // Ensure locals read by this fusion are covered by the zero-init prefix.
                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    for(::std::size_t i{}; i < want; ++i)
                    {
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                if(curr_local_type == curr_operand_stack_value_type::i32)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_reduce_8localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else if(curr_local_type == curr_operand_stack_value_type::i64)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_reduce_8localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    if(want == 12uz)
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_f64_add_reduce_12localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    else
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_f64_add_reduce_7localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                }

                for(::std::size_t i{}; i < want; ++i) { emit_imm_to(bytecode, offs[i]); }
                stacktop_commit_push1_typed_if_reachable(curr_local_type);

                code_curr = scan;
                return true;
            }};

        if(curr_local_type == curr_operand_stack_value_type::f64)
        {
            if(try_add_reduce(12uz) || try_add_reduce(7uz)) { break; }
        }
        else
        {
            if(try_add_reduce(8uz)) { break; }
        }
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_INSTRUCTION_REORDER)
    // Instruction reorder: recompile a shallow LLVM-style integer left fold
    // `local.get a; local.get b; i*.binop; local.get c; i*.binop; ...`
    // into one register-ring-friendly local reduction dispatch.
    //
    // Legal ops are the no-trap associative integer operations add/mul/and/or/xor. Floating-point
    // arithmetic and trapping integer ops are intentionally excluded from this first reorder layer.
    [[maybe_unused]] bool const instruction_reorder_pending_clean{
# if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        conbine_pending.kind == conbine_pending_kind::none
# else
        true
# endif
    };
    [[maybe_unused]] bool const instruction_reorder_runtime_candidate{runtime_uwvm_int_instruction_reorder_enabled && !is_polymorphic &&
                                                                      instruction_reorder_pending_clean};
    [[maybe_unused]] wasm1_code instruction_reorder_follow_op{};  // init
    [[maybe_unused]] bool instruction_reorder_has_follow_op{};
    if(instruction_reorder_runtime_candidate && code_curr != code_end)
    {
        ::std::memcpy(::std::addressof(instruction_reorder_follow_op), code_curr, sizeof(instruction_reorder_follow_op));
        instruction_reorder_has_follow_op = true;
    }
    [[maybe_unused]] bool const instruction_reorder_follow_is_local_get{instruction_reorder_has_follow_op &&
                                                                        instruction_reorder_follow_op == wasm1_code::local_get};
    [[maybe_unused]] bool const instruction_reorder_follow_is_typed_int_operand{
        instruction_reorder_follow_is_local_get ||
        (curr_local_type == curr_operand_stack_value_type::i32 && instruction_reorder_has_follow_op &&
         instruction_reorder_follow_op == wasm1_code::i32_const) ||
        (curr_local_type == curr_operand_stack_value_type::i64 && instruction_reorder_has_follow_op && instruction_reorder_follow_op == wasm1_code::i64_const)};

    if(instruction_reorder_runtime_candidate && instruction_reorder_follow_is_typed_int_operand &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::i64))
    {
        namespace reorder_optable = ::uwvm2::runtime::compiler::uwvm_int::optable;
        using reorder_int_binop = reorder_optable::numeric_details::int_binop;

        auto const decode_reorder_reduce_op{[](curr_operand_stack_value_type vt, wasm1_code op, reorder_int_binop& out) constexpr noexcept -> bool
                                            {
                                                if(vt == curr_operand_stack_value_type::i32)
                                                {
                                                    switch(op)
                                                    {
                                                        case wasm1_code::i32_add: out = reorder_int_binop::add; return true;
                                                        case wasm1_code::i32_mul: out = reorder_int_binop::mul; return true;
                                                        case wasm1_code::i32_and: out = reorder_int_binop::and_; return true;
                                                        case wasm1_code::i32_or: out = reorder_int_binop::or_; return true;
                                                        case wasm1_code::i32_xor:
                                                            out = reorder_int_binop::xor_;
                                                            return true;
                                                        [[unlikely]] default:
                                                            return false;
                                                    }
                                                }

                                                if(vt == curr_operand_stack_value_type::i64)
                                                {
                                                    switch(op)
                                                    {
                                                        case wasm1_code::i64_add: out = reorder_int_binop::add; return true;
                                                        case wasm1_code::i64_mul: out = reorder_int_binop::mul; return true;
                                                        case wasm1_code::i64_and: out = reorder_int_binop::and_; return true;
                                                        case wasm1_code::i64_or: out = reorder_int_binop::or_; return true;
                                                        case wasm1_code::i64_xor:
                                                            out = reorder_int_binop::xor_;
                                                            return true;
                                                        [[unlikely]] default:
                                                            return false;
                                                    }
                                                }

                                                return false;
                                            }};

        using reorder_expr_binop = reorder_optable::instruction_reorder_details::int_expr_binop;
        using reorder_expr_operand_kind = reorder_optable::instruction_reorder_details::int_expr_operand_kind;

        auto const decode_reorder_expr_op{[](curr_operand_stack_value_type vt, wasm1_code op, reorder_expr_binop& out) constexpr noexcept -> bool
                                          {
                                              if(vt == curr_operand_stack_value_type::i32)
                                              {
                                                  switch(op)
                                                  {
                                                      case wasm1_code::i32_add: out = reorder_expr_binop::add; return true;
                                                      case wasm1_code::i32_sub: out = reorder_expr_binop::sub; return true;
                                                      case wasm1_code::i32_mul: out = reorder_expr_binop::mul; return true;
                                                      case wasm1_code::i32_and: out = reorder_expr_binop::and_; return true;
                                                      case wasm1_code::i32_or: out = reorder_expr_binop::or_; return true;
                                                      case wasm1_code::i32_xor: out = reorder_expr_binop::xor_; return true;
                                                      case wasm1_code::i32_shl: out = reorder_expr_binop::shl; return true;
                                                      case wasm1_code::i32_shr_s: out = reorder_expr_binop::shr_s; return true;
                                                      case wasm1_code::i32_shr_u: out = reorder_expr_binop::shr_u; return true;
                                                      case wasm1_code::i32_rotl: out = reorder_expr_binop::rotl; return true;
                                                      case wasm1_code::i32_rotr:
                                                          out = reorder_expr_binop::rotr;
                                                          return true;
                                                      [[unlikely]] default:
                                                          return false;
                                                  }
                                              }

                                              if(vt == curr_operand_stack_value_type::i64)
                                              {
                                                  switch(op)
                                                  {
                                                      case wasm1_code::i64_add: out = reorder_expr_binop::add; return true;
                                                      case wasm1_code::i64_sub: out = reorder_expr_binop::sub; return true;
                                                      case wasm1_code::i64_mul: out = reorder_expr_binop::mul; return true;
                                                      case wasm1_code::i64_and: out = reorder_expr_binop::and_; return true;
                                                      case wasm1_code::i64_or: out = reorder_expr_binop::or_; return true;
                                                      case wasm1_code::i64_xor: out = reorder_expr_binop::xor_; return true;
                                                      case wasm1_code::i64_shl: out = reorder_expr_binop::shl; return true;
                                                      case wasm1_code::i64_shr_s: out = reorder_expr_binop::shr_s; return true;
                                                      case wasm1_code::i64_shr_u: out = reorder_expr_binop::shr_u; return true;
                                                      case wasm1_code::i64_rotl: out = reorder_expr_binop::rotl; return true;
                                                      case wasm1_code::i64_rotr:
                                                          out = reorder_expr_binop::rotr;
                                                          return true;
                                                      [[unlikely]] default:
                                                          return false;
                                                  }
                                              }

                                              return false;
                                          }};

        auto const try_reschedule_left_reduce_local_update{
            [&]() constexpr UWVM_THROWS -> bool
            {
                constexpr ::std::size_t max_reschedule_local_count{8uz};
                ::uwvm2::utils::container::array<local_offset_t, max_reschedule_local_count> offs{};
                offs[0] = local_off;

                ::std::byte const* scan{code_curr};
                ::std::size_t local_count{1uz};
                wasm1_code reduce_wasm_op{};    // init
                reorder_int_binop reduce_op{};  // init
                bool has_reduce_op{};

                while(local_count < max_reschedule_local_count)
                {
                    if(scan == code_end) { break; }

                    wasm1_code op;  // no init
                    ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                    if(op != wasm1_code::local_get) { break; }

                    wasm_u32 next_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                      ::fast_io::mnp::leb128_get(next_local_index))};
                    if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                       local_type_from_index(next_local_index) != curr_local_type)
                    {
                        break;
                    }

                    auto const after_local_get{reinterpret_cast<::std::byte const*>(next_local_index_next)};
                    if(after_local_get == code_end) { break; }

                    wasm1_code next_op{};  // init
                    ::std::memcpy(::std::addressof(next_op), after_local_get, sizeof(next_op));

                    reorder_int_binop next_reduce_op{};  // init
                    if(!decode_reorder_reduce_op(curr_local_type, next_op, next_reduce_op)) { break; }
                    if(has_reduce_op)
                    {
                        if(next_op != reduce_wasm_op) { break; }
                    }
                    else
                    {
                        reduce_wasm_op = next_op;
                        reduce_op = next_reduce_op;
                        has_reduce_op = true;
                    }

                    offs[local_count] = local_offset_from_index(next_local_index);
                    scan = after_local_get + 1u;
                    ++local_count;
                }

                if(local_count < 3uz || scan == code_end) { return false; }

                wasm1_code update_op{};  // init
                ::std::memcpy(::std::addressof(update_op), scan, sizeof(update_op));
                if(update_op != wasm1_code::local_set && update_op != wasm1_code::local_tee) { return false; }
                ++scan;

                wasm_u32 dst_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [dst_local_index_next, dst_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(dst_local_index))};
                if(dst_local_index_err != ::fast_io::parse_code::ok || dst_local_index >= all_local_count ||
                   local_type_from_index(dst_local_index) != curr_local_type)
                {
                    return false;
                }

                scan = reinterpret_cast<::std::byte const*>(dst_local_index_next);

                if(update_op == wasm1_code::local_tee && scan != code_end)
                {
                    wasm1_code after_tee{};  // init
                    ::std::memcpy(::std::addressof(after_tee), scan, sizeof(after_tee));
                    if(after_tee == wasm1_code::br_if) { return false; }
                }

                if(update_op == wasm1_code::local_tee && !stacktop_has_push_slots_without_spill(curr_local_type, 1uz))
                {
                    if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.instr_reorder_ring_slot_reject_count; }
                    return false;
                }

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    for(::std::size_t i{}; i != local_count; ++i)
                    {
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    if(update_op == wasm1_code::local_set) { ++runtime_log_stats.instr_reorder_local_reduce_set_count; }
                    else
                    {
                        ++runtime_log_stats.instr_reorder_local_reduce_tee_count;
                    }
                    if(update_op == wasm1_code::local_tee) { ++runtime_log_stats.instr_reorder_ring_slot_used_count; }
                    runtime_log_stats.instr_reorder_local_read_count += local_count;
                }

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_reduce_update{
                    [&]<bool KeepResult, reorder_int_binop Op, ::std::size_t LocalCount>() constexpr UWVM_THROWS -> void
                    {
                        if(curr_local_type == curr_operand_stack_value_type::i32)
                        {
                            if constexpr(KeepResult)
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr_from_tuple<CompileOption, Op, LocalCount>(
                                                   curr_stacktop,
                                                   interpreter_tuple));
                            }
                            else
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr_from_tuple<CompileOption, Op, LocalCount>(
                                                   curr_stacktop,
                                                   interpreter_tuple));
                            }
                        }
                        else
                        {
                            if constexpr(KeepResult)
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr_from_tuple<CompileOption, Op, LocalCount>(
                                                   curr_stacktop,
                                                   interpreter_tuple));
                            }
                            else
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr_from_tuple<CompileOption, Op, LocalCount>(
                                                   curr_stacktop,
                                                   interpreter_tuple));
                            }
                        }
                    }};

                auto const emit_reduce_update_for_count{[&]<bool KeepResult, reorder_int_binop Op>() constexpr UWVM_THROWS -> void
                                                        {
                                                            switch(local_count)
                                                            {
                                                                case 3uz: emit_reduce_update.template operator()<KeepResult, Op, 3uz>(); break;
                                                                case 4uz: emit_reduce_update.template operator()<KeepResult, Op, 4uz>(); break;
                                                                case 5uz: emit_reduce_update.template operator()<KeepResult, Op, 5uz>(); break;
                                                                case 6uz: emit_reduce_update.template operator()<KeepResult, Op, 6uz>(); break;
                                                                case 7uz: emit_reduce_update.template operator()<KeepResult, Op, 7uz>(); break;
                                                                case 8uz:
                                                                    emit_reduce_update.template operator()<KeepResult, Op, 8uz>();
                                                                    break;
                                                                [[unlikely]] default:
                                                                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                                    ::fast_io::fast_terminate();
                                                                }
                                                            }
                                                        }};

                auto const emit_reduce_update_for_op{[&]<bool KeepResult>() constexpr UWVM_THROWS -> void
                                                     {
                                                         switch(reduce_op)
                                                         {
                                                             case reorder_int_binop::add:
                                                                 emit_reduce_update_for_count.template operator()<KeepResult, reorder_int_binop::add>();
                                                                 break;
                                                             case reorder_int_binop::mul:
                                                                 emit_reduce_update_for_count.template operator()<KeepResult, reorder_int_binop::mul>();
                                                                 break;
                                                             case reorder_int_binop::and_:
                                                                 emit_reduce_update_for_count.template operator()<KeepResult, reorder_int_binop::and_>();
                                                                 break;
                                                             case reorder_int_binop::or_:
                                                                 emit_reduce_update_for_count.template operator()<KeepResult, reorder_int_binop::or_>();
                                                                 break;
                                                             case reorder_int_binop::xor_:
                                                                 emit_reduce_update_for_count.template operator()<KeepResult, reorder_int_binop::xor_>();
                                                                 break;
                                                             [[unlikely]] default:
                                                             {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                                 ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                                 ::fast_io::fast_terminate();
                                                             }
                                                         }
                                                     }};

                if(update_op == wasm1_code::local_tee)
                {
                    operand_stack_push(curr_local_type);
                    stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                    emit_reduce_update_for_op.template operator()<true>();
                }
                else
                {
                    emit_reduce_update_for_op.template operator()<false>();
                }

                emit_imm_to(bytecode, static_cast<::std::uint8_t>(local_count));
                emit_imm_to(bytecode, local_offset_from_index(dst_local_index));
                for(::std::size_t i{}; i != local_count; ++i) { emit_imm_to(bytecode, offs[i]); }

                if(update_op == wasm1_code::local_tee) { stacktop_commit_push1_typed_if_reachable(curr_local_type); }
                code_curr = scan;
                return true;
            }};

        if(instruction_reorder_follow_is_local_get && try_reschedule_left_reduce_local_update()) { break; }

        auto const try_reschedule_const_binop_local_update{
            [&]() constexpr UWVM_THROWS -> bool
            {
                if(!instruction_reorder_has_follow_op) { return false; }
                if(curr_local_type == curr_operand_stack_value_type::i32)
                {
                    if(instruction_reorder_follow_op != wasm1_code::i32_const) { return false; }
                }
                else if(curr_local_type == curr_operand_stack_value_type::i64)
                {
                    if(instruction_reorder_follow_op != wasm1_code::i64_const) { return false; }
                }
                else
                {
                    return false;
                }

                ::std::byte const* scan{code_curr};
                wasm_i64 imm{};

                if(curr_local_type == curr_operand_stack_value_type::i32)
                {
                    wasm_i32 imm_i32{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(imm_i32))};
                    if(imm_err != ::fast_io::parse_code::ok) { return false; }
                    imm = static_cast<wasm_i64>(imm_i32);
                    scan = reinterpret_cast<::std::byte const*>(imm_next);
                }
                else
                {
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(imm))};
                    if(imm_err != ::fast_io::parse_code::ok) { return false; }
                    scan = reinterpret_cast<::std::byte const*>(imm_next);
                }

                if(scan == code_end) { return false; }

                wasm1_code binop{};  // init
                ::std::memcpy(::std::addressof(binop), scan, sizeof(binop));

                reorder_expr_binop expr_op{};  // init
                if(!decode_reorder_expr_op(curr_local_type, binop, expr_op)) { return false; }
                ++scan;

                if(scan == code_end) { return false; }

                wasm1_code update_op{};  // init
                ::std::memcpy(::std::addressof(update_op), scan, sizeof(update_op));
                if(update_op != wasm1_code::local_set && update_op != wasm1_code::local_tee) { return false; }
                ++scan;

                wasm_u32 dst_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [dst_local_index_next, dst_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(dst_local_index))};
                if(dst_local_index_err != ::fast_io::parse_code::ok || dst_local_index >= all_local_count ||
                   local_type_from_index(dst_local_index) != curr_local_type)
                {
                    return false;
                }

                auto const dst_off{local_offset_from_index(dst_local_index)};
                if(dst_off == local_off)
                {
                    // Same-local updates are already covered by ordinary conbine update-local opfuncs.
                    return false;
                }

                scan = reinterpret_cast<::std::byte const*>(dst_local_index_next);

                if(update_op == wasm1_code::local_tee && scan != code_end)
                {
                    wasm1_code after_tee{};  // init
                    ::std::memcpy(::std::addressof(after_tee), scan, sizeof(after_tee));
                    if(after_tee == wasm1_code::br_if) { return false; }
                }

                if(update_op == wasm1_code::local_tee && !stacktop_has_push_slots_without_spill(curr_local_type, 1uz))
                {
                    if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.instr_reorder_ring_slot_reject_count; }
                    return false;
                }

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    auto const src_end_off{static_cast<local_offset_t>(local_off + local_size)};
                    if(src_end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = src_end_off; }
                }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    if(update_op == wasm1_code::local_set) { ++runtime_log_stats.instr_reorder_const_binop_local_set_count; }
                    else
                    {
                        ++runtime_log_stats.instr_reorder_const_binop_local_tee_count;
                    }
                    if(update_op == wasm1_code::local_tee) { ++runtime_log_stats.instr_reorder_ring_slot_used_count; }
                    ++runtime_log_stats.instr_reorder_expr_step_count;
                    ++runtime_log_stats.instr_reorder_local_read_count;
                }

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_const_update{
                    [&]<bool KeepResult, reorder_expr_binop Op>() constexpr UWVM_THROWS -> void
                    {
                        if(curr_local_type == curr_operand_stack_value_type::i32)
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i32_const_binop_local_update_fptr_from_tuple<CompileOption, Op, KeepResult>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                        }
                        else
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i64_const_binop_local_update_fptr_from_tuple<CompileOption, Op, KeepResult>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                        }
                    }};

                auto const emit_const_update_for_op{[&]<reorder_expr_binop Op>() constexpr UWVM_THROWS -> void
                                                    {
                                                        if(update_op == wasm1_code::local_tee)
                                                        {
                                                            operand_stack_push(curr_local_type);
                                                            stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                                                            emit_const_update.template operator()<true, Op>();
                                                        }
                                                        else
                                                        {
                                                            emit_const_update.template operator()<false, Op>();
                                                        }
                                                    }};

                switch(expr_op)
                {
                    case reorder_expr_binop::add: emit_const_update_for_op.template operator()<reorder_expr_binop::add>(); break;
                    case reorder_expr_binop::sub: emit_const_update_for_op.template operator()<reorder_expr_binop::sub>(); break;
                    case reorder_expr_binop::mul: emit_const_update_for_op.template operator()<reorder_expr_binop::mul>(); break;
                    case reorder_expr_binop::and_: emit_const_update_for_op.template operator()<reorder_expr_binop::and_>(); break;
                    case reorder_expr_binop::or_: emit_const_update_for_op.template operator()<reorder_expr_binop::or_>(); break;
                    case reorder_expr_binop::xor_: emit_const_update_for_op.template operator()<reorder_expr_binop::xor_>(); break;
                    case reorder_expr_binop::shl: emit_const_update_for_op.template operator()<reorder_expr_binop::shl>(); break;
                    case reorder_expr_binop::shr_s: emit_const_update_for_op.template operator()<reorder_expr_binop::shr_s>(); break;
                    case reorder_expr_binop::shr_u: emit_const_update_for_op.template operator()<reorder_expr_binop::shr_u>(); break;
                    case reorder_expr_binop::rotl: emit_const_update_for_op.template operator()<reorder_expr_binop::rotl>(); break;
                    case reorder_expr_binop::rotr:
                        emit_const_update_for_op.template operator()<reorder_expr_binop::rotr>();
                        break;
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, local_off);
                if(curr_local_type == curr_operand_stack_value_type::i32) { emit_imm_to(bytecode, static_cast<wasm_i32>(imm)); }
                else
                {
                    emit_imm_to(bytecode, imm);
                }
                emit_imm_to(bytecode, dst_off);

                if(update_op == wasm1_code::local_tee) { stacktop_commit_push1_typed_if_reachable(curr_local_type); }
                code_curr = scan;
                return true;
            }};

        if(!instruction_reorder_follow_is_local_get && try_reschedule_const_binop_local_update()) { break; }

        auto const try_reschedule_mixed_int_expr_local_update{
            [&]() constexpr UWVM_THROWS -> bool
            {
                constexpr ::std::size_t min_update_step_count{4uz};
                constexpr ::std::size_t max_update_step_count{8uz};

                ::uwvm2::utils::container::array<reorder_expr_binop, max_update_step_count> ops{};
                ::uwvm2::utils::container::array<reorder_expr_operand_kind, max_update_step_count> kinds{};
                ::uwvm2::utils::container::array<local_offset_t, max_update_step_count> offs{};
                ::uwvm2::utils::container::array<wasm_i64, max_update_step_count> imms{};

                ::std::byte const* scan{code_curr};
                ::std::size_t step_count{};
                ::std::size_t local_read_count{1uz};

                while(step_count < max_update_step_count)
                {
                    if(scan == code_end) { break; }
                    ::std::byte const* const step_begin{scan};

                    reorder_expr_operand_kind operand_kind{};  // init
                    local_offset_t operand_off{};              // init
                    wasm_i64 operand_imm{};                    // init

                    wasm1_code operand_op{};  // init
                    ::std::memcpy(::std::addressof(operand_op), scan, sizeof(operand_op));

                    if(operand_op == wasm1_code::local_get)
                    {
                        wasm_u32 next_local_index{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [next_local_index_next,
                                    next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                   ::fast_io::mnp::leb128_get(next_local_index))};
                        if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                           local_type_from_index(next_local_index) != curr_local_type)
                        {
                            break;
                        }

                        operand_kind = reorder_expr_operand_kind::local;
                        operand_off = local_offset_from_index(next_local_index);
                        scan = reinterpret_cast<::std::byte const*>(next_local_index_next);
                        ++local_read_count;
                    }
                    else if(curr_local_type == curr_operand_stack_value_type::i32 && operand_op == wasm1_code::i32_const)
                    {
                        wasm_i32 imm{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) { break; }
                        operand_kind = reorder_expr_operand_kind::imm;
                        operand_imm = static_cast<wasm_i64>(imm);
                        scan = reinterpret_cast<::std::byte const*>(imm_next);
                    }
                    else if(curr_local_type == curr_operand_stack_value_type::i64 && operand_op == wasm1_code::i64_const)
                    {
                        wasm_i64 imm{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) { break; }
                        operand_kind = reorder_expr_operand_kind::imm;
                        operand_imm = imm;
                        scan = reinterpret_cast<::std::byte const*>(imm_next);
                    }
                    else
                    {
                        break;
                    }

                    if(scan == code_end)
                    {
                        scan = step_begin;
                        break;
                    }

                    wasm1_code binop{};  // init
                    ::std::memcpy(::std::addressof(binop), scan, sizeof(binop));

                    reorder_expr_binop expr_op{};  // init
                    if(!decode_reorder_expr_op(curr_local_type, binop, expr_op))
                    {
                        scan = step_begin;
                        break;
                    }

                    ops[step_count] = expr_op;
                    kinds[step_count] = operand_kind;
                    offs[step_count] = operand_off;
                    imms[step_count] = operand_imm;
                    ++scan;
                    ++step_count;
                }

                if(step_count < min_update_step_count || scan == code_end) { return false; }

                wasm1_code update_op{};  // init
                ::std::memcpy(::std::addressof(update_op), scan, sizeof(update_op));
                if(update_op != wasm1_code::local_set && update_op != wasm1_code::local_tee) { return false; }
                ++scan;

                wasm_u32 dst_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [dst_local_index_next, dst_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                ::fast_io::mnp::leb128_get(dst_local_index))};
                if(dst_local_index_err != ::fast_io::parse_code::ok || dst_local_index >= all_local_count ||
                   local_type_from_index(dst_local_index) != curr_local_type)
                {
                    return false;
                }

                scan = reinterpret_cast<::std::byte const*>(dst_local_index_next);

                if(update_op == wasm1_code::local_tee && scan != code_end)
                {
                    wasm1_code after_tee{};  // init
                    ::std::memcpy(::std::addressof(after_tee), scan, sizeof(after_tee));
                    if(after_tee == wasm1_code::br_if) { return false; }
                }

                if(update_op == wasm1_code::local_tee && !stacktop_has_push_slots_without_spill(curr_local_type, 1uz))
                {
                    if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.instr_reorder_ring_slot_reject_count; }
                    return false;
                }

                auto const dst_off{local_offset_from_index(dst_local_index)};

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    auto const first_end_off{static_cast<local_offset_t>(local_off + local_size)};
                    if(first_end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = first_end_off; }
                    for(::std::size_t i{}; i != step_count; ++i)
                    {
                        if(kinds[i] != reorder_expr_operand_kind::local) { continue; }
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    if(update_op == wasm1_code::local_set) { ++runtime_log_stats.instr_reorder_expr_local_set_count; }
                    else
                    {
                        ++runtime_log_stats.instr_reorder_expr_local_tee_count;
                    }
                    if(update_op == wasm1_code::local_tee) { ++runtime_log_stats.instr_reorder_ring_slot_used_count; }
                    runtime_log_stats.instr_reorder_expr_step_count += step_count;
                    runtime_log_stats.instr_reorder_local_read_count += local_read_count;
                }

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_expr_local_update{
                    [&]<bool KeepResult, ::std::size_t StepCount>() constexpr UWVM_THROWS -> void
                    {
                        if(curr_local_type == curr_operand_stack_value_type::i32)
                        {
                            if constexpr(KeepResult)
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i32_expr_local_tee_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            }
                            else
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i32_expr_local_set_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            }
                        }
                        else
                        {
                            if constexpr(KeepResult)
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i64_expr_local_tee_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            }
                            else
                            {
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_reorder_i64_expr_local_set_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop,
                                                                                                                                           interpreter_tuple));
                            }
                        }
                    }};

                auto const emit_expr_local_update_for_step_count{[&]<bool KeepResult>() constexpr UWVM_THROWS -> void
                                                                 {
                                                                     switch(step_count)
                                                                     {
                                                                         case 3uz: emit_expr_local_update.template operator()<KeepResult, 3uz>(); break;
                                                                         case 4uz: emit_expr_local_update.template operator()<KeepResult, 4uz>(); break;
                                                                         case 5uz: emit_expr_local_update.template operator()<KeepResult, 5uz>(); break;
                                                                         case 6uz: emit_expr_local_update.template operator()<KeepResult, 6uz>(); break;
                                                                         case 7uz: emit_expr_local_update.template operator()<KeepResult, 7uz>(); break;
                                                                         case 8uz:
                                                                             emit_expr_local_update.template operator()<KeepResult, 8uz>();
                                                                             break;
                                                                         [[unlikely]] default:
                                                                         {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                                             ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                                             ::fast_io::fast_terminate();
                                                                         }
                                                                     }
                                                                 }};

                if(update_op == wasm1_code::local_tee)
                {
                    operand_stack_push(curr_local_type);
                    stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                    emit_expr_local_update_for_step_count.template operator()<true>();
                }
                else
                {
                    emit_expr_local_update_for_step_count.template operator()<false>();
                }

                emit_imm_to(bytecode, static_cast<::std::uint8_t>(step_count));
                emit_imm_to(bytecode, local_off);
                emit_imm_to(bytecode, dst_off);
                for(::std::size_t i{}; i != step_count; ++i)
                {
                    emit_imm_to(bytecode, static_cast<::std::uint8_t>(ops[i]));
                    emit_imm_to(bytecode, static_cast<::std::uint8_t>(kinds[i]));
                    if(kinds[i] == reorder_expr_operand_kind::local) { emit_imm_to(bytecode, offs[i]); }
                    else if(curr_local_type == curr_operand_stack_value_type::i32) { emit_imm_to(bytecode, static_cast<wasm_i32>(imms[i])); }
                    else
                    {
                        emit_imm_to(bytecode, imms[i]);
                    }
                }

                if(update_op == wasm1_code::local_tee) { stacktop_commit_push1_typed_if_reachable(curr_local_type); }
                code_curr = scan;
                return true;
            }};

        if(try_reschedule_mixed_int_expr_local_update()) { break; }

        auto const try_reschedule_left_reduce_chain{
            [&]() constexpr UWVM_THROWS -> bool
            {
                constexpr ::std::size_t max_reschedule_local_count{8uz};
                ::uwvm2::utils::container::array<local_offset_t, max_reschedule_local_count> offs{};
                offs[0] = local_off;

                ::std::byte const* scan{code_curr};
                ::std::size_t local_count{1uz};
                wasm1_code reduce_wasm_op{};    // init
                reorder_int_binop reduce_op{};  // init
                bool has_reduce_op{};

                while(local_count < max_reschedule_local_count)
                {
                    if(scan == code_end) { break; }

                    wasm1_code op;  // no init
                    ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                    if(op != wasm1_code::local_get) { break; }

                    wasm_u32 next_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                      ::fast_io::mnp::leb128_get(next_local_index))};
                    if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                       local_type_from_index(next_local_index) != curr_local_type)
                    {
                        break;
                    }

                    auto const after_local_get{reinterpret_cast<::std::byte const*>(next_local_index_next)};
                    if(after_local_get == code_end) { break; }

                    wasm1_code next_op{};  // init
                    ::std::memcpy(::std::addressof(next_op), after_local_get, sizeof(next_op));

                    reorder_int_binop next_reduce_op{};  // init
                    if(!decode_reorder_reduce_op(curr_local_type, next_op, next_reduce_op)) { break; }
                    if(has_reduce_op)
                    {
                        if(next_op != reduce_wasm_op) { break; }
                    }
                    else
                    {
                        reduce_wasm_op = next_op;
                        reduce_op = next_reduce_op;
                        has_reduce_op = true;
                    }

                    offs[local_count] = local_offset_from_index(next_local_index);
                    scan = after_local_get + 1u;
                    ++local_count;
                }

                if(local_count < 3uz) { return false; }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    ++runtime_log_stats.instr_reorder_local_reduce_count;
                    runtime_log_stats.instr_reorder_local_read_count += local_count;
                }

                // Net Wasm stack effect of the consumed chain is one pushed integer value.
                operand_stack_push(curr_local_type);

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    for(::std::size_t i{}; i != local_count; ++i)
                    {
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_reduce{
                    [&]<reorder_int_binop Op, ::std::size_t LocalCount>() constexpr UWVM_THROWS -> void
                    {
                        if(curr_local_type == curr_operand_stack_value_type::i32)
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i32_reduce_nlocalget_fptr_from_tuple<CompileOption, Op, LocalCount>(curr_stacktop,
                                                                                                                                   interpreter_tuple));
                        }
                        else
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i64_reduce_nlocalget_fptr_from_tuple<CompileOption, Op, LocalCount>(curr_stacktop,
                                                                                                                                   interpreter_tuple));
                        }
                    }};

                auto const emit_reduce_for_op{[&]<reorder_int_binop Op>() constexpr UWVM_THROWS -> void
                                              {
                                                  switch(local_count)
                                                  {
                                                      case 3uz: emit_reduce.template operator()<Op, 3uz>(); break;
                                                      case 4uz: emit_reduce.template operator()<Op, 4uz>(); break;
                                                      case 5uz: emit_reduce.template operator()<Op, 5uz>(); break;
                                                      case 6uz: emit_reduce.template operator()<Op, 6uz>(); break;
                                                      case 7uz: emit_reduce.template operator()<Op, 7uz>(); break;
                                                      case 8uz:
                                                          emit_reduce.template operator()<Op, 8uz>();
                                                          break;
                                                      [[unlikely]] default:
                                                      {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                          ::fast_io::fast_terminate();
                                                      }
                                                  }
                                              }};

                switch(reduce_op)
                {
                    case reorder_int_binop::add: emit_reduce_for_op.template operator()<reorder_int_binop::add>(); break;
                    case reorder_int_binop::mul: emit_reduce_for_op.template operator()<reorder_int_binop::mul>(); break;
                    case reorder_int_binop::and_: emit_reduce_for_op.template operator()<reorder_int_binop::and_>(); break;
                    case reorder_int_binop::or_: emit_reduce_for_op.template operator()<reorder_int_binop::or_>(); break;
                    case reorder_int_binop::xor_:
                        emit_reduce_for_op.template operator()<reorder_int_binop::xor_>();
                        break;
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, static_cast<::std::uint8_t>(local_count));
                for(::std::size_t i{}; i != local_count; ++i) { emit_imm_to(bytecode, offs[i]); }
                stacktop_commit_push1_typed_if_reachable(curr_local_type);

                code_curr = scan;
                return true;
            }};

        if(instruction_reorder_follow_is_local_get && try_reschedule_left_reduce_chain()) { break; }

        auto const try_reschedule_mixed_int_expr_chain{
            [&]() constexpr UWVM_THROWS -> bool
            {
                constexpr ::std::size_t min_expr_step_count{4uz};
                constexpr ::std::size_t max_expr_step_count{8uz};

                ::uwvm2::utils::container::array<reorder_expr_binop, max_expr_step_count> ops{};
                ::uwvm2::utils::container::array<reorder_expr_operand_kind, max_expr_step_count> kinds{};
                ::uwvm2::utils::container::array<local_offset_t, max_expr_step_count> offs{};
                ::uwvm2::utils::container::array<wasm_i64, max_expr_step_count> imms{};

                ::std::byte const* scan{code_curr};
                ::std::size_t step_count{};
                ::std::size_t local_read_count{1uz};

                while(step_count < max_expr_step_count)
                {
                    if(scan == code_end) { break; }
                    ::std::byte const* const step_begin{scan};

                    reorder_expr_operand_kind operand_kind{};  // init
                    local_offset_t operand_off{};              // init
                    wasm_i64 operand_imm{};                    // init

                    wasm1_code operand_op{};  // init
                    ::std::memcpy(::std::addressof(operand_op), scan, sizeof(operand_op));

                    if(operand_op == wasm1_code::local_get)
                    {
                        wasm_u32 next_local_index{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [next_local_index_next,
                                    next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                   ::fast_io::mnp::leb128_get(next_local_index))};
                        if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                           local_type_from_index(next_local_index) != curr_local_type)
                        {
                            break;
                        }

                        operand_kind = reorder_expr_operand_kind::local;
                        operand_off = local_offset_from_index(next_local_index);
                        scan = reinterpret_cast<::std::byte const*>(next_local_index_next);
                        ++local_read_count;
                    }
                    else if(curr_local_type == curr_operand_stack_value_type::i32 && operand_op == wasm1_code::i32_const)
                    {
                        wasm_i32 imm{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) { break; }
                        operand_kind = reorder_expr_operand_kind::imm;
                        operand_imm = static_cast<wasm_i64>(imm);
                        scan = reinterpret_cast<::std::byte const*>(imm_next);
                    }
                    else if(curr_local_type == curr_operand_stack_value_type::i64 && operand_op == wasm1_code::i64_const)
                    {
                        wasm_i64 imm{};
                        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                        auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                ::fast_io::mnp::leb128_get(imm))};
                        if(imm_err != ::fast_io::parse_code::ok) { break; }
                        operand_kind = reorder_expr_operand_kind::imm;
                        operand_imm = imm;
                        scan = reinterpret_cast<::std::byte const*>(imm_next);
                    }
                    else
                    {
                        break;
                    }

                    if(scan == code_end)
                    {
                        scan = step_begin;
                        break;
                    }

                    wasm1_code binop{};  // init
                    ::std::memcpy(::std::addressof(binop), scan, sizeof(binop));

                    reorder_expr_binop expr_op{};  // init
                    if(!decode_reorder_expr_op(curr_local_type, binop, expr_op))
                    {
                        scan = step_begin;
                        break;
                    }

                    ops[step_count] = expr_op;
                    kinds[step_count] = operand_kind;
                    offs[step_count] = operand_off;
                    imms[step_count] = operand_imm;
                    ++scan;
                    ++step_count;
                }

                if(step_count < min_expr_step_count) { return false; }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    ++runtime_log_stats.instr_reorder_expr_fold_count;
                    runtime_log_stats.instr_reorder_expr_step_count += step_count;
                    runtime_log_stats.instr_reorder_local_read_count += local_read_count;
                }

                // Net stack effect of a left-fold expression is one pushed integer value.
                operand_stack_push(curr_local_type);

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    auto const first_end_off{static_cast<local_offset_t>(local_off + local_size)};
                    if(first_end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = first_end_off; }
                    for(::std::size_t i{}; i != step_count; ++i)
                    {
                        if(kinds[i] != reorder_expr_operand_kind::local) { continue; }
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                stacktop_prepare_push1_if_reachable(bytecode, curr_local_type);
                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_expr_fold{
                    [&]<::std::size_t StepCount>() constexpr UWVM_THROWS -> void
                    {
                        if(curr_local_type == curr_operand_stack_value_type::i32)
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i32_expr_fold_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop, interpreter_tuple));
                        }
                        else
                        {
                            emit_opfunc_to(
                                bytecode,
                                translate::get_uwvmint_reorder_i64_expr_fold_fptr_from_tuple<CompileOption, StepCount>(curr_stacktop, interpreter_tuple));
                        }
                    }};

                switch(step_count)
                {
                    case 3uz: emit_expr_fold.template operator()<3uz>(); break;
                    case 4uz: emit_expr_fold.template operator()<4uz>(); break;
                    case 5uz: emit_expr_fold.template operator()<5uz>(); break;
                    case 6uz: emit_expr_fold.template operator()<6uz>(); break;
                    case 7uz: emit_expr_fold.template operator()<7uz>(); break;
                    case 8uz:
                        emit_expr_fold.template operator()<8uz>();
                        break;
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, static_cast<::std::uint8_t>(step_count));
                emit_imm_to(bytecode, local_off);
                for(::std::size_t i{}; i != step_count; ++i)
                {
                    emit_imm_to(bytecode, static_cast<::std::uint8_t>(ops[i]));
                    emit_imm_to(bytecode, static_cast<::std::uint8_t>(kinds[i]));
                    if(kinds[i] == reorder_expr_operand_kind::local) { emit_imm_to(bytecode, offs[i]); }
                    else if(curr_local_type == curr_operand_stack_value_type::i32) { emit_imm_to(bytecode, static_cast<wasm_i32>(imms[i])); }
                    else
                    {
                        emit_imm_to(bytecode, imms[i]);
                    }
                }

                stacktop_commit_push1_typed_if_reachable(curr_local_type);
                code_curr = scan;
                return true;
            }};

        if(try_reschedule_mixed_int_expr_chain()) { break; }
    }

    // Instruction reorder base layer: recompile a consecutive same-typed `local.get` burst into
    // one preload dispatch. This is the register-ring stack-caching form of the pass: it preserves
    // the original producer order and lets any following opcode consume the now-cached operands.
    if(instruction_reorder_runtime_candidate && instruction_reorder_follow_is_local_get &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::i64 ||
        curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64))
    {
        auto const try_preload_local_get_burst{
            [&]() constexpr UWVM_THROWS -> bool
            {
                constexpr ::std::size_t max_supported_preload_ring_size{8uz};
                ::uwvm2::utils::container::array<local_offset_t, max_supported_preload_ring_size> offs{};
                offs[0] = local_off;

                ::std::size_t local_limit{};
                ::std::size_t local_min{};
                if constexpr(stacktop_enabled)
                {
                    if(!stacktop_enabled_for_vt(curr_local_type)) { return false; }

                    auto const ring_size{stacktop_ring_size_for_vt(curr_local_type)};
                    // The preload limit is the active physical register-ring size for the current
                    // architecture/CompileOption. If a future ABI exposes a larger ring than this
                    // generated opfunc family supports, do not silently treat the fixed template
                    // limit as "full ring"; leave the window to ordinary local handling instead.
                    if(ring_size < 2uz || ring_size > max_supported_preload_ring_size) { return false; }
                    auto const free_slots{stacktop_free_slot_count_for_vt(curr_local_type)};
                    if(free_slots < 2uz)
                    {
                        if(runtime_log_on) [[unlikely]] { ++runtime_log_stats.instr_reorder_ring_slot_reject_count; }
                        return false;
                    }
                    local_limit = free_slots < ring_size ? free_slots : ring_size;
                    local_min = local_limit;
                    if(local_limit == ring_size && ring_size > 3uz) { local_min = ring_size - 1uz; }
                }
                else
                {
                    return false;
                }
                if(local_limit < local_min) { return false; }

                ::std::byte const* scan{code_curr};
                ::std::size_t local_count{1uz};

                while(local_count < local_limit)
                {
                    if(scan == code_end) { break; }

                    wasm1_code op{};  // init
                    ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                    if(op != wasm1_code::local_get) { break; }

                    wasm_u32 next_local_index{};
                    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                    auto const [next_local_index_next, next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan + 1u),
                                                                                                      reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                                      ::fast_io::mnp::leb128_get(next_local_index))};
                    if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                       local_type_from_index(next_local_index) != curr_local_type)
                    {
                        break;
                    }

                    offs[local_count] = local_offset_from_index(next_local_index);
                    scan = reinterpret_cast<::std::byte const*>(next_local_index_next);
                    ++local_count;
                }

                if(local_count < local_min) { return false; }

                if(runtime_log_on) [[unlikely]]
                {
                    ++runtime_log_stats.instr_reorder_candidate_count;
                    ++runtime_log_stats.instr_reorder_applied_count;
                    ++runtime_log_stats.instr_reorder_local_preload_count;
                    runtime_log_stats.instr_reorder_local_read_count += local_count;
                    runtime_log_stats.instr_reorder_ring_slot_used_count += local_count;
                }

                for(::std::size_t i{}; i != local_count; ++i) { operand_stack_push(curr_local_type); }

                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    for(::std::size_t i{}; i != local_count; ++i)
                    {
                        auto const end_off{static_cast<local_offset_t>(offs[i] + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                stacktop_prepare_push_n_same_vt_if_reachable(bytecode, curr_local_type, local_count);
                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const emit_preload{
                    [&]<::std::size_t LocalCount>() constexpr UWVM_THROWS -> void
                    {
                        switch(curr_local_type)
                        {
                            case curr_operand_stack_value_type::i32:
                            {
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_reorder_i32_preload_nlocalget_fptr_from_tuple<CompileOption, LocalCount>(curr_stacktop,
                                                                                                                                    interpreter_tuple));
                                break;
                            }
                            case curr_operand_stack_value_type::i64:
                            {
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_reorder_i64_preload_nlocalget_fptr_from_tuple<CompileOption, LocalCount>(curr_stacktop,
                                                                                                                                    interpreter_tuple));
                                break;
                            }
                            case curr_operand_stack_value_type::f32:
                            {
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_reorder_f32_preload_nlocalget_fptr_from_tuple<CompileOption, LocalCount>(curr_stacktop,
                                                                                                                                    interpreter_tuple));
                                break;
                            }
                            case curr_operand_stack_value_type::f64:
                            {
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_reorder_f64_preload_nlocalget_fptr_from_tuple<CompileOption, LocalCount>(curr_stacktop,
                                                                                                                                    interpreter_tuple));
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
                    }};

                switch(local_count)
                {
                    case 2uz: emit_preload.template operator()<2uz>(); break;
                    case 3uz: emit_preload.template operator()<3uz>(); break;
                    case 4uz: emit_preload.template operator()<4uz>(); break;
                    case 5uz: emit_preload.template operator()<5uz>(); break;
                    case 6uz: emit_preload.template operator()<6uz>(); break;
                    case 7uz: emit_preload.template operator()<7uz>(); break;
                    case 8uz:
                        emit_preload.template operator()<8uz>();
                        break;
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, static_cast<::std::uint8_t>(local_count));
                for(::std::size_t i{}; i != local_count; ++i) { emit_imm_to(bytecode, offs[i]); }
                for(::std::size_t i{}; i != local_count; ++i) { stacktop_commit_push1_typed_if_reachable(curr_local_type); }

                code_curr = scan;
                return true;
            }};

        if(try_preload_local_get_burst()) { break; }
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // Heavy combine: collapse the hot chain
    // `local.get src; f{32,64}.const mul; f{32,64}.mul; f{32,64}.const add; f{32,64}.add; local.set src`
    // into one opfunc dispatch (net stack effect 0).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::none &&
       (curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64) && code_curr != code_end)
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        auto const local_off{local_offset_from_index(local_index)};

        auto const try_emit_affine_update{
            [&]<typename FpT>(wasm1_code const const_op, wasm1_code const mul_op, wasm1_code const add_op) constexpr UWVM_THROWS -> bool
            {
                ::std::byte const* scan{code_curr};
                ::std::byte const* const endp{code_end};

                auto const need_n{static_cast<::std::size_t>((endp - scan))};
                constexpr ::std::size_t min_n{1uz + sizeof(FpT) + 1uz + 1uz + sizeof(FpT) + 1uz + 1uz};
                if(need_n < min_n) { return false; }

                wasm1_code op{};  // init
                ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                if(op != const_op) { return false; }
                ++scan;

                FpT mul{};  // init
                ::std::memcpy(::std::addressof(mul), scan, sizeof(mul));
                scan += sizeof(mul);

                ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                if(op != mul_op) { return false; }
                ++scan;

                ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                if(op != const_op) { return false; }
                ++scan;

                FpT add{};  // init
                ::std::memcpy(::std::addressof(add), scan, sizeof(add));
                scan += sizeof(add);

                ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                if(op != add_op) { return false; }
                ++scan;

                ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                if(op != wasm1_code::local_set) { return false; }
                ++scan;

                wasm_u32 dst_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [dst_next, dst_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                        ::fast_io::mnp::leb128_get(dst_local_index))};
                if(dst_err != ::fast_io::parse_code::ok || dst_local_index != local_index) { return false; }
                scan = reinterpret_cast<::std::byte const*>(dst_next);

                // Ensure `src` local read is covered by the zero-init prefix.
                auto const local_size{operand_stack_valtype_size(curr_local_type)};
                if(local_size != 0uz)
                {
                    auto const end_off{static_cast<local_offset_t>(local_off + local_size)};
                    if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                }

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                if constexpr(::std::same_as<FpT, wasm_f32>)
                {
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_f32_mul_add_2imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_f64_mul_add_2imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                emit_imm_to(bytecode, local_off);
                emit_imm_to(bytecode, mul);
                emit_imm_to(bytecode, add);

                code_curr = scan;
                return true;
            }};

        if(curr_local_type == curr_operand_stack_value_type::f32)
        {
            if(try_emit_affine_update.template operator()<wasm_f32>(wasm1_code::f32_const, wasm1_code::f32_mul, wasm1_code::f32_add)) { break; }
        }
        else
        {
            if(try_emit_affine_update.template operator()<wasm_f64>(wasm1_code::f64_const, wasm1_code::f64_mul, wasm1_code::f64_add)) { break; }
        }
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
    // Extra-heavy combine: collapse the hot chain
    // `local.get src; f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst`
    // into one opfunc dispatch (pushes 1 f64).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::none && curr_local_type == curr_operand_stack_value_type::f64)
    {
        ::std::byte const* const endp{code_end};

        auto const try_emit_chain{
            [&](::std::size_t stages) constexpr UWVM_THROWS -> bool
            {
                ::std::byte const* scan{code_curr};

                auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                      {
                                          if(scan == endp) { return false; }
                                          wasm1_code op{};  // init
                                          ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                                          if(op != expected) { return false; }
                                          ++scan;
                                          return true;
                                      }};

                auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                           {
                                               using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                               auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                               reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                               ::fast_io::mnp::leb128_get(v))};
                                               if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                               scan = reinterpret_cast<::std::byte const*>(next);
                                               return true;
                                           }};

                auto const consume_f64_const{[&](wasm_f64& out) constexpr noexcept -> bool
                                             {
                                                 if(!consume_op(wasm1_code::f64_const)) { return false; }
                                                 if(static_cast<::std::size_t>(endp - scan) < 8uz) [[unlikely]] { return false; }
                                                 ::std::memcpy(::std::addressof(out), scan, sizeof(out));
                                                 scan += sizeof(out);
                                                 return true;
                                             }};

                auto const same_bits{[](wasm_f64 const& a, wasm_f64 const& b) constexpr noexcept -> bool
                                     { return ::std::memcmp(::std::addressof(a), ::std::addressof(b), sizeof(a)) == 0; }};

                wasm_f64 mul{};  // init
                wasm_f64 add{};  // init
                wasm_u32 d1{};   // init
                wasm_u32 d2{};   // init
                wasm_u32 d3{};   // init
                wasm_u32 d4{};   // init

                bool ok = consume_f64_const(mul) && consume_op(wasm1_code::f64_mul) && consume_f64_const(add) && consume_op(wasm1_code::f64_add) &&
                          consume_op(wasm1_code::local_tee) && consume_u32_leb(d1);
                if(!ok) { return false; }
                if(d1 >= all_local_count || local_type_from_index(d1) != curr_operand_stack_value_type::f64) { return false; }

                if(stages == 4uz)
                {
                    auto const consume_mul_add_tee_same{[&](wasm_u32& dst) constexpr noexcept -> bool
                                                        {
                                                            wasm_f64 m{};  // init
                                                            wasm_f64 a{};  // init
                                                            if(!consume_f64_const(m) || !consume_op(wasm1_code::f64_mul) || !consume_f64_const(a) ||
                                                               !consume_op(wasm1_code::f64_add) || !consume_op(wasm1_code::local_tee) || !consume_u32_leb(dst))
                                                            {
                                                                return false;
                                                            }
                                                            return same_bits(m, mul) && same_bits(a, add);
                                                        }};

                    ok = consume_mul_add_tee_same(d2) && consume_mul_add_tee_same(d3) && consume_mul_add_tee_same(d4);
                    if(!ok) { return false; }
                    if(d2 >= all_local_count || d3 >= all_local_count || d4 >= all_local_count) { return false; }
                    if(local_type_from_index(d2) != curr_operand_stack_value_type::f64 || local_type_from_index(d3) != curr_operand_stack_value_type::f64 ||
                       local_type_from_index(d4) != curr_operand_stack_value_type::f64)
                    {
                        return false;
                    }

                    // Net stack effect: push 1 f64.
                    operand_stack_push(curr_operand_stack_value_type::f64);

                    // Ensure `src` local read is covered by the zero-init prefix.
                    auto const src_off{local_offset_from_index(local_index)};
                    {
                        auto const local_size{operand_stack_valtype_size(curr_operand_stack_value_type::f64)};
                        if(local_size != 0uz)
                        {
                            auto const end_off{static_cast<local_offset_t>(src_off + local_size)};
                            if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                        }
                    }

                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
                    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                    emit_opfunc_to(
                        bytecode,
                        translate::get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, src_off);
                    emit_imm_to(bytecode, local_offset_from_index(d1));
                    emit_imm_to(bytecode, local_offset_from_index(d2));
                    emit_imm_to(bytecode, local_offset_from_index(d3));
                    emit_imm_to(bytecode, local_offset_from_index(d4));
                    emit_imm_to(bytecode, mul);
                    emit_imm_to(bytecode, add);
                    stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);

                    code_curr = scan;
                    return true;
                }

                // stages == 1
                operand_stack_push(curr_operand_stack_value_type::f64);

                auto const src_off{local_offset_from_index(local_index)};
                {
                    auto const local_size{operand_stack_valtype_size(curr_operand_stack_value_type::f64)};
                    if(local_size != 0uz)
                    {
                        auto const end_off{static_cast<local_offset_t>(src_off + local_size)};
                        if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                    }
                }

                stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64);
                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, src_off);
                emit_imm_to(bytecode, local_offset_from_index(d1));
                emit_imm_to(bytecode, mul);
                emit_imm_to(bytecode, add);
                stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64);

                code_curr = scan;
                return true;
            }};

        if(try_emit_chain(4uz) || try_emit_chain(1uz)) { break; }
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // Heavy combine (loop_i64 hot chain):
    // `local.get x:i64; local.get y:i32; i64.extend_i32_{s,u}; i64.xor; local.set/local.tee dst`
    // -> one cross-type update-local opfunc.
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64 &&
       curr_local_type == curr_operand_stack_value_type::i32)
    {
        auto const try_emit_extend_xor_local_update{
            [&]<bool SignedExtend, bool Tee>() constexpr UWVM_THROWS -> bool
            {
                ::std::byte const* scan{code_curr};

                auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                      {
                                          if(scan == code_end) { return false; }
                                          wasm1_code op{};  // init
                                          ::std::memcpy(::std::addressof(op), scan, sizeof(op));
                                          if(op != expected) { return false; }
                                          ++scan;
                                          return true;
                                      }};

                if(!consume_op(SignedExtend ? wasm1_code::i64_extend_i32_s : wasm1_code::i64_extend_i32_u)) { return false; }
                if(!consume_op(wasm1_code::i64_xor)) { return false; }
                if(!consume_op(Tee ? wasm1_code::local_tee : wasm1_code::local_set)) { return false; }

                wasm_u32 dst_local_index{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [dst_next, dst_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                        ::fast_io::mnp::leb128_get(dst_local_index))};
                if(dst_err != ::fast_io::parse_code::ok || dst_local_index >= all_local_count ||
                   local_type_from_index(dst_local_index) != curr_operand_stack_value_type::i64)
                {
                    return false;
                }
                scan = reinterpret_cast<::std::byte const*>(dst_next);

                if constexpr(Tee)
                {
                    if(scan != code_end)
                    {
                        wasm1_code after_tee{};  // init
                        ::std::memcpy(::std::addressof(after_tee), scan, sizeof(after_tee));
                        if(after_tee == wasm1_code::br_if) { return false; }
                    }
                }

                auto const extend_zeroinit_end{[&](local_offset_t off, curr_operand_stack_value_type vt) constexpr noexcept
                                               {
                                                   auto const local_size{operand_stack_valtype_size(vt)};
                                                   if(local_size == 0uz) { return; }
                                                   auto const end_off{static_cast<local_offset_t>(off + local_size)};
                                                   if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
                                               }};

                extend_zeroinit_end(conbine_pending.off1, curr_operand_stack_value_type::i64);
                extend_zeroinit_end(local_off, curr_operand_stack_value_type::i32);

                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                auto const dst_off{local_offset_from_index(dst_local_index)};

                if constexpr(Tee)
                {
                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
                    if constexpr(SignedExtend)
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_i64_extend_i32_s_xor_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    else
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_i64_extend_i32_u_xor_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, local_off);
                    emit_imm_to(bytecode, dst_off);
                    stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64);
                }
                else
                {
                    if constexpr(SignedExtend)
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_i64_extend_i32_s_xor_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    else
                    {
                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_i64_extend_i32_u_xor_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    emit_imm_to(bytecode, conbine_pending.off1);
                    emit_imm_to(bytecode, local_off);
                    emit_imm_to(bytecode, dst_off);

                    // The first delayed `local.get x` is already modeled on the validation stack.
                    operand_stack_pop_unchecked();
                }

                conbine_pending.kind = conbine_pending_kind::none;
                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                code_curr = scan;
                return true;
            }};

        if(try_emit_extend_xor_local_update.template operator()<true, false>() || try_emit_extend_xor_local_update.template operator()<true, true>() ||
           try_emit_extend_xor_local_update.template operator()<false, false>() || try_emit_extend_xor_local_update.template operator()<false, true>())
        {
            break;
        }
    }
#endif

    operand_stack_push(curr_local_type);
    {
        auto const local_size{operand_stack_valtype_size(curr_local_type)};
        if(local_size != 0uz)
        {
            auto const end_off{static_cast<local_offset_t>(local_off + local_size)};
            if(end_off > local_bytes_zeroinit_end) { local_bytes_zeroinit_end = end_off; }
        }
    }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine: delay `local.get` emission to enable `local.get + ...` fusions.
    switch(conbine_pending.kind)
    {
        case conbine_pending_kind::none:
        {
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::local_get:
        {
            if(conbine_pending.vt == curr_local_type)
            {
                // Tail-call combine: (local.get addr:i32) (local.get v:*) (store) -> one fused store opfunc.
                // This keeps the delay-local provider elimination while allowing store fusion in non-extra modes.
                if(!is_polymorphic && conbine_pending.vt == curr_operand_stack_value_type::i32)
                {
                    wasm1_code next_op{};  // init
                    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
                    if(next_op == wasm1_code::i32_store)
                    {
                        conbine_pending.kind = conbine_pending_kind::local_get_i32_localget;
                        conbine_pending.off2 = local_off;
                        break;
                    }
                }
                // Conbine: for two consecutive same-type `local.get`, try heavy fusions first, then form either a 2-local pending window
                // (extra-heavy) or keep only one delayed local.get (heavy/soft).
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                // Heavy: `local.get acc; local.get x; f.unop; f.add; local.set acc` -> one mega-op (`acc += unop(x)`).
                // This is a big win for local-heavy kernels (e.g. round_f32/round_f64 dense loops) without enabling a generic 2-local
                // pending window.
                if(!is_polymorphic && (curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64) &&
                   code_curr != code_end)
                {
                    // Heavy: `acc += copysign(x, -1.0)` (`local.get acc; local.get x; f.const -1; f.copysign; f.add; local.set acc`)
                    // This is emitted by some compilers for `acc += -abs(x)` patterns.
                    {
                        wasm1_code op0{};  // init
                        ::std::memcpy(::std::addressof(op0), code_curr, sizeof(op0));

                        if(curr_local_type == curr_operand_stack_value_type::f32 && op0 == wasm1_code::f32_const &&
                           static_cast<::std::size_t>(code_end - code_curr) >= (1uz + sizeof(wasm_f32) + 3uz))
                        {
                            wasm_f32 imm{};  // init
                            ::std::memcpy(::std::addressof(imm), code_curr + 1uz, sizeof(imm));

                            wasm1_code op1{};  // init
                            ::std::memcpy(::std::addressof(op1), code_curr + 1uz + sizeof(wasm_f32), sizeof(op1));
                            wasm1_code op2{};  // init
                            ::std::memcpy(::std::addressof(op2), code_curr + 2uz + sizeof(wasm_f32), sizeof(op2));
                            wasm1_code op3{};  // init
                            ::std::memcpy(::std::addressof(op3), code_curr + 3uz + sizeof(wasm_f32), sizeof(op3));

                            if(imm == static_cast<wasm_f32>(-1.0f) && op1 == wasm1_code::f32_copysign && op2 == wasm1_code::f32_add &&
                               op3 == wasm1_code::local_set)
                            {
                                wasm_u32 next_local_index{};
                                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                auto const [next_local_index_next, next_local_index_err]{
                                    ::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 4uz + sizeof(wasm_f32)),
                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                             ::fast_io::mnp::leb128_get(next_local_index))};
                                (void)next_local_index_next;
                                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                                   local_type_from_index(next_local_index) == curr_local_type)
                                {
                                    conbine_pending.kind = conbine_pending_kind::f32_acc_add_negabs_localget_wait_const;
                                    conbine_pending.off2 = local_off;
                                    break;
                                }
                            }
                        }
                        else if(curr_local_type == curr_operand_stack_value_type::f64 && op0 == wasm1_code::f64_const &&
                                static_cast<::std::size_t>(code_end - code_curr) >= (1uz + sizeof(wasm_f64) + 3uz))
                        {
                            wasm_f64 imm{};  // init
                            ::std::memcpy(::std::addressof(imm), code_curr + 1uz, sizeof(imm));

                            wasm1_code op1{};  // init
                            ::std::memcpy(::std::addressof(op1), code_curr + 1uz + sizeof(wasm_f64), sizeof(op1));
                            wasm1_code op2{};  // init
                            ::std::memcpy(::std::addressof(op2), code_curr + 2uz + sizeof(wasm_f64), sizeof(op2));
                            wasm1_code op3{};  // init
                            ::std::memcpy(::std::addressof(op3), code_curr + 3uz + sizeof(wasm_f64), sizeof(op3));

                            if(imm == static_cast<wasm_f64>(-1.0) && op1 == wasm1_code::f64_copysign && op2 == wasm1_code::f64_add &&
                               op3 == wasm1_code::local_set)
                            {
                                wasm_u32 next_local_index{};
                                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                auto const [next_local_index_next, next_local_index_err]{
                                    ::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 4uz + sizeof(wasm_f64)),
                                                             reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                             ::fast_io::mnp::leb128_get(next_local_index))};
                                (void)next_local_index_next;
                                if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                                   local_offset_from_index(next_local_index) == conbine_pending.off1 &&
                                   local_type_from_index(next_local_index) == curr_local_type)
                                {
                                    conbine_pending.kind = conbine_pending_kind::f64_acc_add_negabs_localget_wait_const;
                                    conbine_pending.off2 = local_off;
                                    break;
                                }
                            }
                        }
                    }

                    wasm1_code unop{};  // init
                    ::std::memcpy(::std::addressof(unop), code_curr, sizeof(unop));

                    conbine_pending_kind acc_kind{conbine_pending_kind::none};
                    wasm1_code add_op_expect{};

                    if(curr_local_type == curr_operand_stack_value_type::f32)
                    {
                        add_op_expect = wasm1_code::f32_add;
                        switch(unop)
                        {
                            case wasm1_code::f32_floor: acc_kind = conbine_pending_kind::f32_acc_add_floor_localget_wait_add; break;
                            case wasm1_code::f32_ceil: acc_kind = conbine_pending_kind::f32_acc_add_ceil_localget_wait_add; break;
                            case wasm1_code::f32_trunc: acc_kind = conbine_pending_kind::f32_acc_add_trunc_localget_wait_add; break;
                            case wasm1_code::f32_nearest: acc_kind = conbine_pending_kind::f32_acc_add_nearest_localget_wait_add; break;
                            case wasm1_code::f32_abs: acc_kind = conbine_pending_kind::f32_acc_add_abs_localget_wait_add; break;
                            default: break;
                        }
                    }
                    else
                    {
                        add_op_expect = wasm1_code::f64_add;
                        switch(unop)
                        {
                            case wasm1_code::f64_floor: acc_kind = conbine_pending_kind::f64_acc_add_floor_localget_wait_add; break;
                            case wasm1_code::f64_ceil: acc_kind = conbine_pending_kind::f64_acc_add_ceil_localget_wait_add; break;
                            case wasm1_code::f64_trunc: acc_kind = conbine_pending_kind::f64_acc_add_trunc_localget_wait_add; break;
                            case wasm1_code::f64_nearest: acc_kind = conbine_pending_kind::f64_acc_add_nearest_localget_wait_add; break;
                            case wasm1_code::f64_abs: acc_kind = conbine_pending_kind::f64_acc_add_abs_localget_wait_add; break;
                            default: break;
                        }
                    }

                    if(acc_kind != conbine_pending_kind::none && static_cast<::std::size_t>(code_end - code_curr) >= 3uz)
                    {
                        wasm1_code op_add{};  // init
                        ::std::memcpy(::std::addressof(op_add), code_curr + 1uz, sizeof(op_add));
                        wasm1_code op_set{};  // init
                        ::std::memcpy(::std::addressof(op_set), code_curr + 2uz, sizeof(op_set));

                        if(op_add == add_op_expect && op_set == wasm1_code::local_set)
                        {
                            wasm_u32 next_local_index{};
                            using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                            auto const [next_local_index_next,
                                        next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 3uz),
                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                       ::fast_io::mnp::leb128_get(next_local_index))};
                            (void)next_local_index_next;
                            if(next_local_index_err == ::fast_io::parse_code::ok && next_local_index < all_local_count &&
                               local_offset_from_index(next_local_index) == conbine_pending.off1 && local_type_from_index(next_local_index) == curr_local_type)
                            {
                                conbine_pending.kind = acc_kind;
                                conbine_pending.off2 = local_off;
                                break;
                            }
                        }
                    }
                }
# endif
                auto const try_form_common_add_2local_window{[&]() constexpr UWVM_THROWS -> bool
                                                             {
                                                                 if(code_curr == code_end) { return false; }

                                                                 wasm1_code add_op{};  // init
                                                                 ::std::memcpy(::std::addressof(add_op), code_curr, sizeof(add_op));

                                                                 wasm1_code expected_add{};
                                                                 if(curr_local_type == curr_operand_stack_value_type::i32)
                                                                 {
                                                                     expected_add = wasm1_code::i32_add;
                                                                 }
                                                                 else if(curr_local_type == curr_operand_stack_value_type::i64)
                                                                 {
                                                                     expected_add = wasm1_code::i64_add;
                                                                 }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                                                 else if(curr_local_type == curr_operand_stack_value_type::f32)
                                                                 {
                                                                     expected_add = wasm1_code::f32_add;
                                                                 }
                                                                 else if(curr_local_type == curr_operand_stack_value_type::f64)
                                                                 {
                                                                     expected_add = wasm1_code::f64_add;
                                                                 }
# endif
                                                                 else
                                                                 {
                                                                     return false;
                                                                 }

                                                                 if(add_op != expected_add) { return false; }
                                                                 if(static_cast<::std::size_t>(code_end - code_curr) < 2uz) { return false; }

                                                                 wasm1_code update_op{};  // init
                                                                 ::std::memcpy(::std::addressof(update_op), code_curr + 1uz, sizeof(update_op));
                                                                 if(update_op != wasm1_code::local_set && update_op != wasm1_code::local_tee) { return false; }

                                                                 wasm_u32 next_local_index{};
                                                                 using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                                 auto const [next_local_index_next, next_local_index_err]{
                                                                     ::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 2uz),
                                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                              ::fast_io::mnp::leb128_get(next_local_index))};

                                                                 if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                                                                    local_type_from_index(next_local_index) != curr_local_type)
                                                                 {
                                                                     return false;
                                                                 }

                                                                 if(curr_local_type == curr_operand_stack_value_type::i32 && update_op == wasm1_code::local_tee)
                                                                 {
                                                                     auto const after_local_tee{reinterpret_cast<::std::byte const*>(next_local_index_next)};
                                                                     if(after_local_tee != code_end)
                                                                     {
                                                                         wasm1_code after_tee{};  // init
                                                                         ::std::memcpy(::std::addressof(after_tee), after_local_tee, sizeof(after_tee));
                                                                         if(after_tee == wasm1_code::br_if) { return false; }
                                                                     }
                                                                 }

                                                                 conbine_pending.kind = conbine_pending_kind::local_get2;
                                                                 conbine_pending.off2 = local_off;
                                                                 return true;
                                                             }};

                if(try_form_common_add_2local_window()) { break; }
# ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
                // Extra-heavy: allow forming a 2-local pending window for 2-local fusions.
                //
                // NOTE: Byref/loop interpreter mode does not implement all 2-local extra-heavy consume sites.
                // Keep only a single delayed local.get there, to avoid skipping the emission of the first local.get.
                if constexpr(CompileOption.is_tail_call)
                {
                    conbine_pending.kind = conbine_pending_kind::local_get2;
                    conbine_pending.off2 = local_off;
                }
                else
                {
                    flush_conbine_pending();
                    conbine_pending.kind = conbine_pending_kind::local_get;
                    conbine_pending.vt = curr_local_type;
                    conbine_pending.off1 = local_off;
                }
# else
                // Heavy/soft: keep only a single delayed local.get so delay-local can fuse it into the first use site.
                // This compiles `local.get a; local.get b; binop` into:
                // - `local.get a`
                // - `delay-local(b)+binop`
                // instead of materializing both locals or relying on 2-local mega fusions (code-size heavy).
                flush_conbine_pending();
                conbine_pending.kind = conbine_pending_kind::local_get;
                conbine_pending.vt = curr_local_type;
                conbine_pending.off1 = local_off;
# endif
            }
            else
            {
                if constexpr(CompileOption.is_tail_call)
                {
                    if(!is_polymorphic && conbine_pending.vt == curr_operand_stack_value_type::i32 && curr_local_type == curr_operand_stack_value_type::i64)
                    {
                        wasm1_code next_op{};  // init
                        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
                        if(next_op == wasm1_code::i64_store || next_op == wasm1_code::i64_store32)
                        {
                            conbine_pending.kind = conbine_pending_kind::local_get_i32_localget;
                            conbine_pending.vt = curr_operand_stack_value_type::i64;
                            conbine_pending.off2 = local_off;
                            break;
                        }
                    }
                }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                // Heavy (test8 hot loop): start `local.get(f64); local.get(i32); i32.const ...` fusion chain.
                if(!is_polymorphic && conbine_pending.vt == curr_operand_stack_value_type::f64 && curr_local_type == curr_operand_stack_value_type::i32)
                {
                    wasm1_code next_op{};  // init
                    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
                    if(next_op == wasm1_code::i32_const)
                    {
                        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets;
                        conbine_pending.off2 = local_off;
                        break;
                    }
                }
# endif
                flush_conbine_pending();
                conbine_pending.kind = conbine_pending_kind::local_get;
                conbine_pending.vt = curr_local_type;
                conbine_pending.off1 = local_off;
            }
            break;
        }
        case conbine_pending_kind::local_get2:
        {
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
            // Heavy: `local.get a; local.get b; local.get cond; select` and `local.get acc; local.get x; local.get y; mul; add; local.set
            // acc`. Lookahead is safe: `code_curr` already points to the next opcode.
            if(code_curr != code_end)
            {
                wasm1_code next_op;  // no init
                ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));

                if(next_op == wasm1_code::select)
                {
                    if(curr_local_type == curr_operand_stack_value_type::i32 &&
                       (conbine_pending.vt == curr_operand_stack_value_type::i32 || conbine_pending.vt == curr_operand_stack_value_type::i64 ||
                        conbine_pending.vt == curr_operand_stack_value_type::f32 || conbine_pending.vt == curr_operand_stack_value_type::f64))
                    {
                        conbine_pending.kind = conbine_pending_kind::select_localget3;
                        conbine_pending.off3 = local_off;
                        break;
                    }
                }

                bool mac_candidate{};
                if(conbine_pending.vt == curr_operand_stack_value_type::i32 && curr_local_type == curr_operand_stack_value_type::i32 &&
                   next_op == wasm1_code::i32_mul)
                {
                    mac_candidate = true;
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::i64 && curr_local_type == curr_operand_stack_value_type::i64 &&
                        next_op == wasm1_code::i64_mul)
                {
                    mac_candidate = true;
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::f32 && curr_local_type == curr_operand_stack_value_type::f32 &&
                        next_op == wasm1_code::f32_mul)
                {
                    mac_candidate = true;
                }
                else if(conbine_pending.vt == curr_operand_stack_value_type::f64 && curr_local_type == curr_operand_stack_value_type::f64 &&
                        next_op == wasm1_code::f64_mul)
                {
                    mac_candidate = true;
                }

                if(mac_candidate)
                {
                    conbine_pending.kind = conbine_pending_kind::mac_localget3;
                    conbine_pending.off3 = local_off;
                    break;
                }
            }
# endif

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::local_get_const_i32_add:
        {
            if(curr_local_type == curr_operand_stack_value_type::i32
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
               || curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64
# endif
            )
            {
                conbine_pending.kind = conbine_pending_kind::local_get_const_i32_add_localget;
                conbine_pending.vt = curr_local_type;
                conbine_pending.off2 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::local_get_const_i32_add_localget:
        {
            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
        case conbine_pending_kind::const_f32:
        {
            if(curr_local_type == curr_operand_stack_value_type::f32)
            {
                conbine_pending.kind = conbine_pending_kind::const_f32_localget;
                conbine_pending.off1 = local_off;
                conbine_pending.vt = curr_operand_stack_value_type::f32;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::const_f64:
        {
            if(curr_local_type == curr_operand_stack_value_type::f64)
            {
                conbine_pending.kind = conbine_pending_kind::const_f64_localget;
                conbine_pending.off1 = local_off;
                conbine_pending.vt = curr_operand_stack_value_type::f64;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::float_mul_2localget:
        {
            if(conbine_pending.vt == curr_local_type &&
               (curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64))
            {
                conbine_pending.kind = conbine_pending_kind::float_mul_2localget_local3;
                conbine_pending.off3 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::float_mul_2localget_local3:
        {
            if(conbine_pending.vt == curr_local_type &&
               (curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64))
            {
                conbine_pending.kind = conbine_pending_kind::float_2mul_wait_second_mul;
                conbine_pending.off4 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }

#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        case conbine_pending_kind::for_ptr_inc_after_tee:
        {
            if(curr_local_type == curr_operand_stack_value_type::i32)
            {
                conbine_pending.kind = conbine_pending_kind::for_ptr_inc_after_pend_get;
                conbine_pending.off2 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
#  endif

        case conbine_pending_kind::xorshift_after_xor1:
        {
            if(curr_local_type == curr_operand_stack_value_type::i32 && local_off == conbine_pending.off1)
            {
                conbine_pending.kind = conbine_pending_kind::xorshift_after_xor1_getx;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::rot_xor_add_after_rotl:
        {
            if(curr_local_type == curr_operand_stack_value_type::i32)
            {
                conbine_pending.kind = conbine_pending_kind::rot_xor_add_after_gety;
                conbine_pending.off2 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
        case conbine_pending_kind::rot_xor_add_i64_after_rotl:
        {
            if(curr_local_type == curr_operand_stack_value_type::i64)
            {
                conbine_pending.kind = conbine_pending_kind::rot_xor_add_i64_after_gety;
                conbine_pending.off2 = local_off;
                break;
            }

            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
# endif
        [[unlikely]] default:
        {
            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::local_get;
            conbine_pending.vt = curr_local_type;
            conbine_pending.off1 = local_off;
            break;
        }
    }
#else
    emit_local_get_typed_to(bytecode, curr_local_type, local_off);
#endif
    break;
}
case wasm1_code::local_set:
{
    // `local.set` consumes a value and writes it to local storage. It is also the point where many
    // pending expression patterns must be committed, because after the write the value no longer
    // exists as a normal operand-stack result.
    // local.set ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.set ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.set local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_u32 local_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};
    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.set local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.set local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    if(local_index >= all_local_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    bool have_set_operand{};
    curr_operand_stack_value_type set_operand_type{};
    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"local.set", 1uz); }
    // Peek instead of pop during validation: the generic path and several fusions still need to
    // consume the same value after choosing the final emission strategy.
    else if(auto const value{try_peek_concrete_operand()}; value.from_stack)
    {
        have_set_operand = true;
        set_operand_type = value.type;
        if(!operand_type_matches(value, curr_local_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(set_operand_type);
            err.err_code = code_validation_error_code::local_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // `local.tee` emits both a store and a logical push of the same value, so all fused helpers need
    // the destination offset while preserving the operand-stack value model.
    auto const local_off{local_offset_from_index(local_index)};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine: `i32.const imm; local.set dst` (common in crypto init sequences).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::const_i32 && curr_local_type == curr_operand_stack_value_type::i32)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, local_off);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine: `i64.const imm; local.set dst` (rare, but must preserve correctness when `const_i64` is pending).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::const_i64 && curr_local_type == curr_operand_stack_value_type::i64)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_const_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, local_off);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    // Conbine (heavy): `select(local.get a,b,cond) ; local.set dst`
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::select_after_select && conbine_pending.vt == curr_local_type &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::f32))
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if(curr_local_type == curr_operand_stack_value_type::i32)
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_select_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }
        else
        {
            emit_opfunc_to(bytecode, translate::get_uwvmint_f32_select_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        }

        // immediates: a_off, b_off, cond_off, dst_off
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, local_off);

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine (heavy): `mac(acc + x*y -> acc) ; local.set acc`
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::mac_after_add && conbine_pending.vt == curr_local_type &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        switch(curr_local_type)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mac_local_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_mac_local_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mac_local_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mac_local_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            [[unlikely]] default:
            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                break;
            }
        }

        // immediates: x_off, y_off, acc_off
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off1);

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif
    // Conbine: `local.get a; local.get b; i32.add; local.set dst`
# ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::i32_add_2localget_local_set &&
       local_off == conbine_pending.off3)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_local_set_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && conbine_pending.kind == conbine_pending_kind::i64_add_2localget_local_set &&
       local_off == conbine_pending.off3)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_local_set_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif
    // Conbine: `local.get + const + add + local.set` (same local).
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::i32_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i32 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        bool handled_i32_update_local{};
        auto emit_i32_update_local_set_same{[&](auto fptr) constexpr UWVM_THROWS
                                            {
                                                if(have_set_operand) { operand_stack_pop_unchecked(); }
                                                emit_opfunc_to(bytecode, fptr);
                                                emit_imm_to(bytecode, conbine_pending.off1);
                                                emit_imm_to(bytecode, conbine_pending.imm_i32);
                                                conbine_pending.kind = conbine_pending_kind::none;
                                                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                                                handled_i32_update_local = true;
                                            }};

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::i32_sub_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_mul_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_and_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_or_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_xor_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shl_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shr_s_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shr_u_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_rotl_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_rotr_imm_local_settee_same:
            {
                emit_i32_update_local_set_same(
                    translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(curr_stacktop, interpreter_tuple));
                break;
            }
            default: break;
        }

        if(handled_i32_update_local) { break; }
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        bool handled_i64_update_local{};
        auto emit_i64_update_local_set_same{[&](auto fptr) constexpr UWVM_THROWS
                                            {
                                                if(have_set_operand) { operand_stack_pop_unchecked(); }
                                                emit_opfunc_to(bytecode, fptr);
                                                emit_imm_to(bytecode, conbine_pending.off1);
                                                emit_imm_to(bytecode, conbine_pending.imm_i64);
                                                conbine_pending.kind = conbine_pending_kind::none;
                                                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                                                handled_i64_update_local = true;
                                            }};

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::i64_add_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_sub_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_mul_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_mul_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_and_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_or_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_xor_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shl_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shr_s_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shr_u_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_rotl_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_rotr_imm_local_settee_same:
            {
                emit_i64_update_local_set_same(
                    translate::get_uwvmint_i64_rotr_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            default: break;
        }

        if(handled_i64_update_local) { break; }
    }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    // Conbine (heavy): `local.get + const + f.add/mul/sub + local.set` (same local) -> update-local mega-op.
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_mul_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_sub_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_mul_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_sub_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine (heavy): `local.get acc; local.get x; f.unop; f.add; local.set acc` -> one mega-op (`acc += unop(x)`).
    if(curr_local_type == curr_operand_stack_value_type::f32 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        auto emit_acc_add_unop{[&](auto fptr) constexpr UWVM_THROWS
                               {
                                   if(have_set_operand) { operand_stack_pop_unchecked(); }
                                   emit_opfunc_to(bytecode, fptr);
                                   emit_imm_to(bytecode, conbine_pending.off2);  // x_off
                                   emit_imm_to(bytecode, conbine_pending.off1);  // acc_off
                                   conbine_pending.kind = conbine_pending_kind::none;
                                   conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                               }};

        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_floor_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_floor_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_ceil_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_ceil_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_trunc_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_trunc_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_nearest_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_nearest_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_abs_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_abs_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f32_acc_add_negabs_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f32_acc_add_negabs_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        auto emit_acc_add_unop{[&](auto fptr) constexpr UWVM_THROWS
                               {
                                   if(have_set_operand) { operand_stack_pop_unchecked(); }
                                   emit_opfunc_to(bytecode, fptr);
                                   emit_imm_to(bytecode, conbine_pending.off2);  // x_off
                                   emit_imm_to(bytecode, conbine_pending.off1);  // acc_off
                                   conbine_pending.kind = conbine_pending_kind::none;
                                   conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                               }};

        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_floor_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_floor_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_ceil_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_ceil_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_trunc_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_trunc_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_nearest_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_nearest_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_abs_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_abs_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
        if(conbine_pending.kind == conbine_pending_kind::f64_acc_add_negabs_localget_set_acc)
        {
            emit_acc_add_unop(translate::get_uwvmint_f64_acc_add_negabs_localget_set_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            break;
        }
    }
# endif
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    // Conbine (heavy): `local.get a; local.get b; f32.add; local.set dst`
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_add_2localget_local_set &&
       local_off == conbine_pending.off3)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_local_set_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    // Conbine (heavy): `local.get a; local.get b; f64.add; local.set dst`
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_add_2localget_local_set &&
       local_off == conbine_pending.off3)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_local_set_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    // Conbine (heavy): `local.get y; local.get x; i32.const r; i32.rotl; i32.xor; local.set dst`
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_after_xor)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_xor_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, local_off);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_i64_after_xor)
    {
        if(have_set_operand) { operand_stack_pop_unchecked(); }
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rotl_xor_local_set_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        emit_imm_to(bytecode, local_off);
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::i32_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_and_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_or_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_xor_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shr_s_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shr_u_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_rotl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_rotr_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_and_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_or_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_xor_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shr_s_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shr_u_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_rotl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_rotr_imm_local_settee_same)
    {
        flush_conbine_pending();
    }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::f32_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f32_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f32_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f32_acc_add_floor_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f32_acc_add_ceil_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f32_acc_add_trunc_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f32_acc_add_nearest_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f32_acc_add_abs_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f64_acc_add_floor_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f64_acc_add_ceil_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f64_acc_add_trunc_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f64_acc_add_nearest_localget_set_acc ||
       conbine_pending.kind == conbine_pending_kind::f64_acc_add_abs_localget_set_acc)
    {
        flush_conbine_pending();
    }
# endif

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::select_after_select || conbine_pending.kind == conbine_pending_kind::mac_after_add)
    {
        flush_conbine_pending();
    }
# endif
#endif

    if(have_set_operand) { operand_stack_pop_unchecked(); }

    emit_local_set_typed_to(bytecode, curr_local_type, local_off);
    break;
}
case wasm1_code::local_tee:
{
    // `local.tee` is intentionally handled separately from `local.set`: it writes the local while
    // preserving the value on the operand stack. That dual effect is why this case carries both
    // store-fusion logic and stack-top push/branch-fusion bookkeeping.
    // local.tee ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // local.tee ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // local.tee local_index ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_u32 local_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [local_index_next, local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                            ::fast_io::mnp::leb128_get(local_index))};
    if(local_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(local_index_err);
    }

    // local.tee local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(local_index_next);

    // local.tee local_index ...
    // [     safe          ] unsafe (could be the section_end)
    //                       ^^ code_curr

    if(local_index >= all_local_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_local_index.local_index = local_index;
        err.err_selectable.illegal_local_index.all_local_count = all_local_count;
        err.err_code = code_validation_error_code::illegal_local_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const curr_local_type{local_type_from_index(local_index)};

    if(concrete_operand_count() == 0uz) [[unlikely]]
    {
        if(!is_polymorphic) { report_operand_stack_underflow(op_begin, u8"local.tee", 1uz); }
        else
        {
            operand_stack_push(curr_local_type);
        }
    }
    else if(auto const value{try_peek_concrete_operand()}; value.from_stack)
    {
        if(!operand_type_matches(value, curr_local_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_local_type);
            err.err_selectable.local_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
            err.err_code = code_validation_error_code::local_tee_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    auto const local_off{local_offset_from_index(local_index)};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    // Heavy (test8 hot loop): `local.get(f64); local.get(i32); i32.const; i32.add; local.tee` chain.
    if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_add)
    {
        if(!is_polymorphic && curr_local_type == curr_operand_stack_value_type::i32 && local_off == conbine_pending.off2)
        {
            conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_tee;
            break;
        }

        // Pattern mismatch: flush the deferred ops so the `local.tee` below sees the correct stack shape.
        flush_conbine_pending();
    }

    // Conbine (heavy): `f32.const imm; local.get src; f32.div; local.tee dst`
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::f32_div_from_imm_localtee_wait && curr_local_type == curr_operand_stack_value_type::f32)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_div_from_imm_localtee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

        // immediates: src_off, imm_f32, dst_off
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        emit_imm_to(bytecode, local_off);

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine (heavy): `f64.const imm; local.get src; f64.div; local.tee dst`
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::f64_div_from_imm_localtee_wait && curr_local_type == curr_operand_stack_value_type::f64)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_div_from_imm_localtee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

        // immediates: src_off, imm_f64, dst_off
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        emit_imm_to(bytecode, local_off);

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine (heavy): `select(local.get a,b,cond) ; local.tee dst` (f32 only)
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::select_after_select && conbine_pending.vt == curr_operand_stack_value_type::f32 &&
       curr_local_type == curr_operand_stack_value_type::f32)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_select_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

        // immediates: a_off, b_off, cond_off, dst_off
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, local_off);

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }

    // Conbine (heavy): `mac(acc + x*y -> acc) ; local.tee acc`
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::mac_after_add && conbine_pending.vt == curr_local_type &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::i64 ||
        curr_local_type == curr_operand_stack_value_type::f32 || curr_local_type == curr_operand_stack_value_type::f64) &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_local_type); }
        switch(curr_local_type)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_i32_mac_local_tee_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_i64_mac_local_tee_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mac_local_tee_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mac_local_tee_acc_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            [[unlikely]] default:
            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                break;
            }
        }

        // immediates: x_off, y_off, acc_off
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        emit_imm_to(bytecode, conbine_pending.off1);

        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_local_type); }

        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif

    // Conbine: `local.get a; local.get b; i32.add; local.tee dst`
# ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::i32_add_2localget_local_tee &&
       local_off == conbine_pending.off3)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_2localget_local_tee_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && conbine_pending.kind == conbine_pending_kind::i64_add_2localget_local_tee &&
       local_off == conbine_pending.off3)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_add_2localget_local_tee_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    // Conbine (heavy): `local.get a; local.get b; f32.add; local.tee dst`
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_add_2localget_local_tee &&
       local_off == conbine_pending.off3)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_2localget_local_tee_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    // Conbine (heavy): `local.get a; local.get b; f64.add; local.tee dst`
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_add_2localget_local_tee &&
       local_off == conbine_pending.off3)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  else
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_2localget_local_tee_common_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#  endif
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.off3);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    // Conbine (heavy): `local.get y; local.get x; i32.const r; i32.rotl; i32.xor; local.tee dst`
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_after_xor)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_rotl_xor_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        emit_imm_to(bytecode, local_off);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && conbine_pending.kind == conbine_pending_kind::rotl_xor_local_set_i64_after_xor)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_i64_rotl_xor_local_tee_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.off2);
        emit_imm_to(bytecode, conbine_pending.imm_i64);
        emit_imm_to(bytecode, local_off);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif

    // Conbine: `local.get + const + add + local.tee` (same local).
    if(curr_local_type == curr_operand_stack_value_type::i32 && conbine_pending.kind == conbine_pending_kind::i32_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        // Heavy loop fusions: delay emission so we can potentially fold the following tight-loop skeleton
        // into a single `for_*_br_if` combined opcode:
        //
        //  i32 loop:
        //    local.get i; i32.const step; i32.add; local.tee i; i32.const end; i32.lt_u; br_if <loop>
        //
        //  ptr loop:
        //    local.get p; i32.const step; i32.add; local.tee p; local.get pend; i32.ne; br_if <loop>
        //
        // If the pattern does not match, the pending state flushes back to the normal `i32_add_imm_local_tee_same` emission.
        if(!is_polymorphic && code_curr != code_end)
        {
            wasm1_code next_op{};  // init
            ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op));
            if(next_op == wasm1_code::i32_const)
            {
                conbine_pending.kind = conbine_pending_kind::for_i32_inc_after_tee;
                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                break;
            }
            if(next_op == wasm1_code::local_get)
            {
                conbine_pending.kind = conbine_pending_kind::for_ptr_inc_after_tee;
                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                break;
            }
        }
# endif
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::i32 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        bool handled_i32_update_local{};
        auto emit_i32_update_local_tee_same{[&](auto fptr) constexpr UWVM_THROWS
                                            {
                                                if constexpr(stacktop_enabled)
                                                {
                                                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
                                                }
                                                emit_opfunc_to(bytecode, fptr);
                                                emit_imm_to(bytecode, conbine_pending.off1);
                                                emit_imm_to(bytecode, conbine_pending.imm_i32);
                                                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
                                                conbine_pending.kind = conbine_pending_kind::none;
                                                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                                                handled_i32_update_local = true;
                                            }};

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::i32_sub_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_mul_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::mul>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_and_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_or_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_xor_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shl_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shr_s_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_shr_u_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_rotl_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i32_rotr_imm_local_settee_same:
            {
                emit_i32_update_local_tee_same(
                    translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotr>(curr_stacktop, interpreter_tuple));
                break;
            }
            default: break;
        }

        if(handled_i32_update_local) { break; }
    }
    if(curr_local_type == curr_operand_stack_value_type::i64 && local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        bool handled_i64_update_local{};
        auto emit_i64_update_local_tee_same{[&](auto fptr) constexpr UWVM_THROWS
                                            {
                                                if constexpr(stacktop_enabled)
                                                {
                                                    stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64);
                                                }
                                                emit_opfunc_to(bytecode, fptr);
                                                emit_imm_to(bytecode, conbine_pending.off1);
                                                emit_imm_to(bytecode, conbine_pending.imm_i64);
                                                if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
                                                conbine_pending.kind = conbine_pending_kind::none;
                                                conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
                                                handled_i64_update_local = true;
                                            }};

        switch(conbine_pending.kind)
        {
            case conbine_pending_kind::i64_add_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_sub_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::sub>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_mul_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_mul_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_and_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::and_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_or_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::or_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_xor_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::xor_>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shl_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shr_s_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_s>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_shr_u_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::shr_u>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_rotl_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_binop_imm_local_tee_same_fptr_from_tuple<
                        CompileOption,
                        ::uwvm2::runtime::compiler::uwvm_int::optable::numeric_details::int_binop::rotl>(curr_stacktop, interpreter_tuple));
                break;
            }
            case conbine_pending_kind::i64_rotr_imm_local_settee_same:
            {
                emit_i64_update_local_tee_same(
                    translate::get_uwvmint_i64_rotr_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                break;
            }
            default: break;
        }

        if(handled_i64_update_local) { break; }
    }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_mul_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_mul_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f32 && conbine_pending.kind == conbine_pending_kind::f32_sub_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f32); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_sub_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f32);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f32); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_add_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_add_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_mul_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_mul_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
    if(curr_local_type == curr_operand_stack_value_type::f64 && conbine_pending.kind == conbine_pending_kind::f64_sub_imm_local_settee_same &&
       local_off == conbine_pending.off1)
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::f64); }
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_sub_imm_local_tee_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_f64);
        if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::f64); }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::i32_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_and_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_or_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_xor_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shr_s_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_shr_u_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_rotl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i32_rotr_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_and_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_or_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_xor_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shr_s_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_shr_u_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_rotl_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::i64_rotr_imm_local_settee_same)
    {
        flush_conbine_pending();
    }

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::f32_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f32_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f32_sub_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_add_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_mul_imm_local_settee_same ||
       conbine_pending.kind == conbine_pending_kind::f64_sub_imm_local_settee_same)
    {
        flush_conbine_pending();
    }
    if(conbine_pending.kind == conbine_pending_kind::select_after_select || conbine_pending.kind == conbine_pending_kind::mac_after_add ||
       conbine_pending.kind == conbine_pending_kind::f32_div_from_imm_localtee_wait)
    {
        flush_conbine_pending();
    }
# endif

    auto const fuse_site{emit_local_tee_typed_to(bytecode, curr_local_type, local_off)};
    if(curr_local_type == curr_operand_stack_value_type::i32)
    {
        br_if_fuse.kind = br_if_fuse_kind::local_tee_nz;
        br_if_fuse.site = fuse_site;
        br_if_fuse.end = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
    }
#else
    static_cast<void>(emit_local_tee_typed_to(bytecode, curr_local_type, local_off));
#endif
    break;
}
case wasm1_code::global_get:
{
    // Global access uses the Wasm index space directly, so imported and local-defined globals must
    // be resolved through different storage vectors before selecting the typed runtime opfunc.
    // global.get ...
    // [  safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // global.get ...
    // [ safe   ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // global.get global_index ...
    // [ safe   ] unsafe (could be the section_end)
    //            ^^ code_curr

    wasm_u32 global_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(global_index))};
    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
    }

    // global.get global_index ...
    // [     safe            ] unsafe (could be the section_end)
    //            ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

    // global.get global_index ...
    // [      safe           ] unsafe (could be the section_end)
    //                         ^^ code_curr

    if(global_index >= all_global_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    curr_operand_stack_value_type curr_global_type{};
    [[maybe_unused]] bool curr_global_mutable{};
    if(global_index < imported_global_count)
    {
        auto const& imported_global_rec{curr_module.imported_global_vec_storage.index_unchecked(static_cast<::std::size_t>(global_index))};
        auto const imported_global_ptr{imported_global_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        auto const& imported_global{imported_global_ptr->imports.storage.global};
        curr_global_type = imported_global.type;
        curr_global_mutable = imported_global.is_mutable;
    }
    else
    {
        auto const local_global_index{static_cast<::std::size_t>(global_index - imported_global_count)};
        auto const& local_global_rec{curr_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
        auto const local_global_ptr{local_global_rec.global_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        curr_global_type = local_global_ptr->type;
        curr_global_mutable = local_global_ptr->is_mutable;
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine: fuse `global.get i32; i32.const imm; i32.add; global.set (same)` into `i32_add_imm_global_set_same`.
    if(curr_global_type == curr_operand_stack_value_type::i32 && curr_global_mutable && code_curr != code_end)
    {
        wasm1_code next_op{};  // init
        ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(wasm1_code));
        if(next_op == wasm1_code::i32_const)
        {
            wasm_i32 imm{};
            auto const const_imm_begin{code_curr + sizeof(wasm1_code)};
            auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(const_imm_begin),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                    ::fast_io::mnp::leb128_get(imm))};
            if(imm_err == ::fast_io::parse_code::ok)
            {
                auto const after_const{reinterpret_cast<::std::byte const*>(imm_next)};
                if(after_const != code_end)
                {
                    wasm1_code op_after_const{};  // init
                    ::std::memcpy(::std::addressof(op_after_const), after_const, sizeof(wasm1_code));
                    if(op_after_const == wasm1_code::i32_add)
                    {
                        auto const after_add{after_const + sizeof(wasm1_code)};
                        if(after_add != code_end)
                        {
                            wasm1_code op_after_add{};  // init
                            ::std::memcpy(::std::addressof(op_after_add), after_add, sizeof(wasm1_code));
                            if(op_after_add == wasm1_code::global_set)
                            {
                                wasm_u32 set_global_index{};
                                auto const set_index_begin{after_add + sizeof(wasm1_code)};
                                auto const [set_index_next,
                                            set_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(set_index_begin),
                                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                    ::fast_io::mnp::leb128_get(set_global_index))};
                                if(set_index_err == ::fast_io::parse_code::ok && set_global_index == global_index)
                                {
                                    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                                    wasm_global_storage_t* const global_p{resolve_global_storage_ptr(global_index)};
                                    emit_opfunc_to(
                                        bytecode,
                                        translate::get_uwvmint_i32_add_imm_global_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                    emit_imm_to(bytecode, global_p);
                                    emit_imm_to(bytecode, imm);

                                    // Skip `i32.const imm; i32.add; global.set <same>`: net stack effect is 0.
                                    code_curr = reinterpret_cast<::std::byte const*>(set_index_next);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    operand_stack_push(curr_global_type);
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        // Resolve the storage pointer only after index/type validation; emitted opfuncs are lean and
        // assume this immediate already points at the correct global slot.
        wasm_global_storage_t* const global_p{resolve_global_storage_ptr(global_index)};

        if constexpr(stacktop_enabled)
        {
            // global.get pushes 1 value to stack-top cache; spill if ring is full.
            stacktop_prepare_push1_if_reachable(bytecode, curr_global_type);
        }

        switch(curr_global_type)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_get_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_get_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_get_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_get_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::v128:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_get_typed_fptr_from_tuple<CompileOption, wasm_v128_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::funcref:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_get_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::externref:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_get_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
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

        if constexpr(stacktop_enabled)
        {
            // Model effects: 1 value pushed into stack-top cache.
            stacktop_commit_push1_typed_if_reachable(curr_global_type);
        }
    }
    break;
}
case wasm1_code::global_set:
{
    // `global.set` validates mutability and value type before emission because the runtime opfunc
    // only receives a resolved storage pointer; publishing an unchecked pointer would make later
    // type errors impossible to report as Wasm validation failures.
    // global.set ...
    // [  safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // global.set ...
    // [ safe   ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // global.set global_index ...
    // [ safe   ] unsafe (could be the section_end)
    //            ^^ code_curr

    wasm_u32 global_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [global_index_next, global_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                              ::fast_io::mnp::leb128_get(global_index))};
    if(global_index_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(global_index_err);
    }

    // global.set global_index ...
    // [     safe            ] unsafe (could be the section_end)
    //            ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(global_index_next);

    // global.set global_index ...
    // [      safe           ] unsafe (could be the section_end)
    //                         ^^ code_curr

    if(global_index >= all_global_count) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_global_index.global_index = global_index;
        err.err_selectable.illegal_global_index.all_global_count = all_global_count;
        err.err_code = code_validation_error_code::illegal_global_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    curr_operand_stack_value_type curr_global_type{};
    bool curr_global_mutable{};
    if(global_index < imported_global_count)
    {
        auto const& imported_global_rec{curr_module.imported_global_vec_storage.index_unchecked(static_cast<::std::size_t>(global_index))};
        auto const imported_global_ptr{imported_global_rec.import_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(imported_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        auto const& imported_global{imported_global_ptr->imports.storage.global};
        curr_global_type = imported_global.type;
        curr_global_mutable = imported_global.is_mutable;
    }
    else
    {
        auto const local_global_index{static_cast<::std::size_t>(global_index - imported_global_count)};
        auto const& local_global_rec{curr_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
        auto const local_global_ptr{local_global_rec.global_type_ptr};
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_global_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        curr_global_type = local_global_ptr->type;
        curr_global_mutable = local_global_ptr->is_mutable;
    }

    if(!curr_global_mutable) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.immutable_global_set.global_index = global_index;
        err.err_code = code_validation_error_code::immutable_global_set;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(!is_polymorphic && concrete_operand_count() == 0uz) [[unlikely]] { report_operand_stack_underflow(op_begin, u8"global.set", 1uz); }
    else if(auto const value{try_pop_concrete_operand()}; value.from_stack)
    {
        if(!operand_type_matches(value, curr_global_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.global_variable_type_mismatch.global_index = global_index;
            err.err_selectable.global_variable_type_mismatch.expected_type = to_wasm1_value_type(curr_global_type);
            err.err_selectable.global_variable_type_mismatch.actual_type = to_wasm1_value_type(value.type);
            err.err_code = code_validation_error_code::global_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Translate: typed `global.set`.
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
        // The runtime setter receives a direct storage pointer, so mutability/type checks must be
        // complete before this immediate is embedded in bytecode.
        wasm_global_storage_t* const global_p{resolve_global_storage_ptr(global_index)};
        switch(curr_global_type)
        {
            case curr_operand_stack_value_type::i32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_set_i32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::i64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_set_i64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::f32:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_set_f32_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::f64:
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_global_set_f64_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::v128:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_set_typed_fptr_from_tuple<CompileOption, wasm_v128_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::funcref:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_set_typed_fptr_from_tuple<CompileOption, wasm_funcref_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
                break;
            }
            case curr_operand_stack_value_type::externref:
            {
                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_global_set_typed_fptr_from_tuple<CompileOption, wasm_externref_t>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, global_p);
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

    // Stack-top cache model: `global.set` consumes 1 stack value.
    stacktop_after_pop_n_if_reachable(bytecode, 1uz);

    break;
}
