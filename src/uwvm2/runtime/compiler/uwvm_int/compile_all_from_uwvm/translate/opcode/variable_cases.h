case wasm1_code::local_get:
{
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
    auto const local_off{local_offset_from_index(local_index)};

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // Heavy combine: `local.get` xN + `add` x(N-1) -> one fused "add-reduce" opfunc (push 1).
    // This eliminates deep stack spill/fill traffic for large local-get expression trees (e.g. `stack_spill_*` benches).
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::none &&
       (curr_local_type == curr_operand_stack_value_type::i32 || curr_local_type == curr_operand_stack_value_type::i64 ||
        curr_local_type == curr_operand_stack_value_type::f64))
    {
        wasm1_code const add_op{curr_local_type == curr_operand_stack_value_type::i32   ? wasm1_code::i32_add
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
                auto const try_form_common_add_2local_window{
                    [&]() constexpr UWVM_THROWS -> bool
                    {
                        if(code_curr == code_end) { return false; }

                        wasm1_code add_op{};  // init
                        ::std::memcpy(::std::addressof(add_op), code_curr, sizeof(add_op));

                        wasm1_code expected_add{};
                        if(curr_local_type == curr_operand_stack_value_type::i32) { expected_add = wasm1_code::i32_add; }
                        else if(curr_local_type == curr_operand_stack_value_type::i64) { expected_add = wasm1_code::i64_add; }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                        else if(curr_local_type == curr_operand_stack_value_type::f32) { expected_add = wasm1_code::f32_add; }
                        else if(curr_local_type == curr_operand_stack_value_type::f64) { expected_add = wasm1_code::f64_add; }
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
                        auto const [next_local_index_next,
                                    next_local_index_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 2uz),
                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                   ::fast_io::mnp::leb128_get(next_local_index))};

                        if(next_local_index_err != ::fast_io::parse_code::ok || next_local_index >= all_local_count ||
                           local_type_from_index(next_local_index) != curr_local_type)
                        {
                            return false;
                        }

                        if(curr_local_type == curr_operand_stack_value_type::i32 && update_op == wasm1_code::local_tee)
                        {
                            auto const* const after_local_tee{reinterpret_cast<::std::byte const*>(next_local_index_next)};
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
    if(operand_stack.empty()) [[unlikely]]
    {
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.set";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    else
    {
        have_set_operand = true;
        set_operand_type = operand_stack.back_unchecked().type;
        if(!is_polymorphic && set_operand_type != curr_local_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
            err.err_selectable.local_variable_type_mismatch.actual_type = set_operand_type;
            err.err_code = code_validation_error_code::local_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

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

    if(operand_stack.empty()) [[unlikely]]
    {
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"local.tee";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        else
        {
            operand_stack_push(curr_local_type);
        }
    }
    else
    {
        auto const value{operand_stack.back_unchecked()};
        if(value.type != curr_local_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.local_variable_type_mismatch.local_index = local_index;
            err.err_selectable.local_variable_type_mismatch.expected_type = curr_local_type;
            err.err_selectable.local_variable_type_mismatch.actual_type = value.type;
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
            auto const* const const_imm_begin{code_curr + sizeof(wasm1_code)};
            auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(const_imm_begin),
                                                                    reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                    ::fast_io::mnp::leb128_get(imm))};
            if(imm_err == ::fast_io::parse_code::ok)
            {
                auto const* const after_const{reinterpret_cast<::std::byte const*>(imm_next)};
                if(after_const != code_end)
                {
                    wasm1_code op_after_const{};  // init
                    ::std::memcpy(::std::addressof(op_after_const), after_const, sizeof(wasm1_code));
                    if(op_after_const == wasm1_code::i32_add)
                    {
                        auto const* const after_add{after_const + sizeof(wasm1_code)};
                        if(after_add != code_end)
                        {
                            wasm1_code op_after_add{};  // init
                            ::std::memcpy(::std::addressof(op_after_add), after_add, sizeof(wasm1_code));
                            if(op_after_add == wasm1_code::global_set)
                            {
                                wasm_u32 set_global_index{};
                                auto const* const set_index_begin{after_add + sizeof(wasm1_code)};
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

    if(operand_stack.empty()) [[unlikely]]
    {
        if(!is_polymorphic)
        {
            err.err_curr = op_begin;
            err.err_selectable.operand_stack_underflow.op_code_name = u8"global.set";
            err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
            err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
            err.err_code = code_validation_error_code::operand_stack_underflow;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
    else
    {
        auto const value{operand_stack.back_unchecked()};
        operand_stack_pop_unchecked();
        if(value.type != curr_global_type) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.global_variable_type_mismatch.global_index = global_index;
            err.err_selectable.global_variable_type_mismatch.expected_type = curr_global_type;
            err.err_selectable.global_variable_type_mismatch.actual_type = value.type;
            err.err_code = code_validation_error_code::global_set_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Translate: typed `global.set`.
    {
        namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
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
