case wasm1_code::i32_const:
{
    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_i32 imm;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(imm))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"i32.const";
        err.err_code = code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
    }

    // i32.const i32 ...
    // [    safe   ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

    // i32.const i32 ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Fast-elide: `i32.const* + drop*` (dead stack traffic).
    // Many real-world Wasm producers (and our microbenchmarks) can emit long runs of pure stack
    // providers followed by the same number of `drop`s. Since `i32.const` is side-effect free,
    // `N×i32.const` immediately followed by `N×drop` is a semantic no-op and can be removed at
    // translation time to avoid dispatch-bound slowdowns (e.g. deepstack tests).
    if(!is_polymorphic)
    {
        auto const try_elide_const_run_with_drops{[&]() noexcept -> bool
                                                  {
                                                      ::std::byte const* scan{code_curr};
                                                      ::std::size_t const_count{1uz};
                                                      using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                                      while(scan < code_end && scan[0] == static_cast<::std::byte>(wasm1_code::i32_const))
                                                      {
                                                          ++scan;
                                                          wasm_i32 tmp;
                                                          auto const [next, parse_err]{
                                                              ::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                       ::fast_io::mnp::leb128_get(tmp))};
                                                          if(parse_err != ::fast_io::parse_code::ok) { return false; }
                                                          scan = reinterpret_cast<::std::byte const*>(next);
                                                          ++const_count;
                                                      }

                                                      ::std::byte const* const drop_begin{scan};
                                                      ::std::byte const* drop_scan{drop_begin};
                                                      while(drop_scan < code_end && drop_scan[0] == static_cast<::std::byte>(wasm1_code::drop)) { ++drop_scan; }
                                                      ::std::size_t const drop_count{static_cast<::std::size_t>(drop_scan - drop_begin)};
                                                      if(drop_count < const_count) { return false; }

                                                      code_curr = drop_begin + const_count;
                                                      return true;
                                                  }};

        if(try_elide_const_run_with_drops()) { break; }
    }

    // Conbine: `local.get(i32) + i32.const` (delay emission for imm/localget fusions).
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_gets)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_step_const;
        conbine_pending.imm_i32 = imm;
    }
    else
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
        if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_after_tee)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_after_end_const;
        conbine_pending.imm_i32_2 = imm;
    }
    else
#  endif
        if(conbine_pending.kind == conbine_pending_kind::rot_xor_add_after_xor)
    {
        conbine_pending.kind = conbine_pending_kind::rot_xor_add_after_xor_constc;
        conbine_pending.imm_i32_2 = imm;
    }
    else if(conbine_pending.kind == conbine_pending_kind::xorshift_after_xor1_getx)
    {
        conbine_pending.kind = conbine_pending_kind::xorshift_after_xor1_getx_constb;
        conbine_pending.imm_i32_2 = imm;
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32 &&
            conbine_pending.off1 == conbine_pending.off2)
    {
        // Lookahead is safe: we do not advance `code_curr`.
        // [ ... i32.const a ] | next_op ...
        // [       safe      ] | unsafe
        //                     ^^ code_curr
        wasm1_code next_op{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_op), code_curr, sizeof(next_op)); }
        if(next_op == wasm1_code::i32_shr_u)
        {
            conbine_pending.kind = conbine_pending_kind::xorshift_pre_shr;
            conbine_pending.imm_i32 = imm;
        }
        else
        {
            conbine_pending.kind = conbine_pending_kind::local_get2_const_i32;
            conbine_pending.imm_i32 = imm;
        }
    }
    else
# endif
        if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        conbine_pending.kind = conbine_pending_kind::local_get2_const_i32;
        conbine_pending.imm_i32 = imm;
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        conbine_pending.kind = conbine_pending_kind::local_get_const_i32;
        conbine_pending.imm_i32 = imm;
    }
    else
    {
        flush_conbine_pending();
        conbine_pending.kind = conbine_pending_kind::const_i32;
        conbine_pending.imm_i32 = imm;
    }
#else
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32); }
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, imm);
    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32); }
#endif

    operand_stack_push(wasm_value_type_u::i32);
    break;
}
case wasm1_code::i64_const:
{
    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_i64 imm;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(imm))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"i64.const";
        err.err_code = code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
    }

    // i64.const i64 ...
    // [     safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

    // i64.const i64 ...
    // [     safe  ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Fast-elide: `i64.const* + drop*` (dead stack traffic).
    if(!is_polymorphic)
    {
        auto const try_elide_const_run_with_drops{[&]() noexcept -> bool
                                                  {
                                                      ::std::byte const* scan{code_curr};
                                                      ::std::size_t const_count{1uz};
                                                      using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                                                      while(scan < code_end && scan[0] == static_cast<::std::byte>(wasm1_code::i64_const))
                                                      {
                                                          ++scan;
                                                          wasm_i64 tmp;
                                                          auto const [next, parse_err]{
                                                              ::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(scan),
                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                                       ::fast_io::mnp::leb128_get(tmp))};
                                                          if(parse_err != ::fast_io::parse_code::ok) { return false; }
                                                          scan = reinterpret_cast<::std::byte const*>(next);
                                                          ++const_count;
                                                      }

                                                      ::std::byte const* const drop_begin{scan};
                                                      ::std::byte const* drop_scan{drop_begin};
                                                      while(drop_scan < code_end && drop_scan[0] == static_cast<::std::byte>(wasm1_code::drop)) { ++drop_scan; }
                                                      ::std::size_t const drop_count{static_cast<::std::size_t>(drop_scan - drop_begin)};
                                                      if(drop_count < const_count) { return false; }

                                                      code_curr = drop_begin + const_count;
                                                      return true;
                                                  }};

        if(try_elide_const_run_with_drops()) { break; }
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine: `local.get(i64) + i64.const` (delay emission for imm/localget fusions).
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i64)
    {
        conbine_pending.kind = conbine_pending_kind::local_get_const_i64;
        conbine_pending.imm_i64 = imm;
    }
    else
    {
        flush_conbine_pending();
        conbine_pending.kind = conbine_pending_kind::const_i64;
        conbine_pending.imm_i64 = imm;
    }
#else
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled) { stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i64); }
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_const_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_imm_to(bytecode, imm);
    if constexpr(stacktop_enabled) { stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i64); }
#endif

    operand_stack_push(wasm_value_type_u::i64);
    break;
}
case wasm1_code::f32_const:
{
    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(wasm_f32)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"f32.const";
        err.err_code = code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // f32.const f32 ...
    // [ safe      ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_f32 imm;
    ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));

    code_curr += sizeof(imm);

    // f32.const f32 ...
    // [ safe      ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Fast-elide: `f32.const* + drop*` (dead stack traffic).
    if(!is_polymorphic)
    {
        auto const try_elide_const_run_with_drops{[&]() noexcept -> bool
                                                  {
                                                      ::std::byte const* scan{code_curr};
                                                      ::std::size_t const_count{1uz};

                                                      constexpr ::std::size_t kInstBytes{1uz + sizeof(wasm_f32)};
                                                      while(scan < code_end && scan[0] == static_cast<::std::byte>(wasm1_code::f32_const))
                                                      {
                                                          if(static_cast<::std::size_t>(code_end - scan) < kInstBytes) { return false; }
                                                          scan += kInstBytes;
                                                          ++const_count;
                                                      }

                                                      ::std::byte const* const drop_begin{scan};
                                                      ::std::byte const* drop_scan{drop_begin};
                                                      while(drop_scan < code_end && drop_scan[0] == static_cast<::std::byte>(wasm1_code::drop)) { ++drop_scan; }
                                                      ::std::size_t const drop_count{static_cast<::std::size_t>(drop_scan - drop_begin)};
                                                      if(drop_count < const_count) { return false; }

                                                      code_curr = drop_begin + const_count;
                                                      return true;
                                                  }};

        if(try_elide_const_run_with_drops()) { break; }
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine (heavy): `local.get(f32) + f32.const` (delay emission for imm/localget fusions).
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::f32_acc_add_negabs_localget_wait_const)
    {
        if(imm == static_cast<wasm_f32>(-1.0f)) { conbine_pending.kind = conbine_pending_kind::f32_acc_add_negabs_localget_wait_copysign; }
        else
        {
            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::const_f32;
            conbine_pending.imm_f32 = imm;
        }
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        conbine_pending.kind = conbine_pending_kind::local_get_const_f32;
        conbine_pending.imm_f32 = imm;
    }
    else if(conbine_pending.kind == conbine_pending_kind::none)
    {
        conbine_pending.kind = conbine_pending_kind::const_f32;
        conbine_pending.imm_f32 = imm;
    }
    else
# endif
    {
        flush_conbine_pending();
        emit_const_f32_to(bytecode, imm);
    }
#else
    emit_const_f32_to(bytecode, imm);
#endif

    operand_stack_push(wasm_value_type_u::f32);
    break;
}
case wasm1_code::f64_const:
{
    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(wasm_f64)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"f64.const";
        err.err_code = code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // f64.const f64 ...
    // [     safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    wasm_f64 imm;
    ::std::memcpy(::std::addressof(imm), code_curr, sizeof(imm));

    code_curr += sizeof(imm);

    // f64.const f64 ...
    // [     safe  ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Fast-elide: `f64.const* + drop*` (dead stack traffic).
    if(!is_polymorphic)
    {
        auto const try_elide_const_run_with_drops{[&]() noexcept -> bool
                                                  {
                                                      ::std::byte const* scan{code_curr};
                                                      ::std::size_t const_count{1uz};

                                                      constexpr ::std::size_t kInstBytes{1uz + sizeof(wasm_f64)};
                                                      while(scan < code_end && scan[0] == static_cast<::std::byte>(wasm1_code::f64_const))
                                                      {
                                                          if(static_cast<::std::size_t>(code_end - scan) < kInstBytes) { return false; }
                                                          scan += kInstBytes;
                                                          ++const_count;
                                                      }

                                                      ::std::byte const* const drop_begin{scan};
                                                      ::std::byte const* drop_scan{drop_begin};
                                                      while(drop_scan < code_end && drop_scan[0] == static_cast<::std::byte>(wasm1_code::drop)) { ++drop_scan; }
                                                      ::std::size_t const drop_count{static_cast<::std::size_t>(drop_scan - drop_begin)};
                                                      if(drop_count < const_count) { return false; }

                                                      code_curr = drop_begin + const_count;
                                                      return true;
                                                  }};

        if(try_elide_const_run_with_drops()) { break; }
    }

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    // Conbine (heavy): `local.get(f64) + f64.const` (delay emission for imm/localget fusions).
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::f64_acc_add_negabs_localget_wait_const)
    {
        if(imm == static_cast<wasm_f64>(-1.0)) { conbine_pending.kind = conbine_pending_kind::f64_acc_add_negabs_localget_wait_copysign; }
        else
        {
            flush_conbine_pending();
            conbine_pending.kind = conbine_pending_kind::const_f64;
            conbine_pending.imm_f64 = imm;
        }
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        conbine_pending.kind = conbine_pending_kind::local_get_const_f64;
        conbine_pending.imm_f64 = imm;
    }
    else if(conbine_pending.kind == conbine_pending_kind::none)
    {
        conbine_pending.kind = conbine_pending_kind::const_f64;
        conbine_pending.imm_f64 = imm;
    }
    else
# endif
    {
        flush_conbine_pending();
        emit_const_f64_to(bytecode, imm);
    }
#else
    emit_const_f64_to(bytecode, imm);
#endif

    operand_stack_push(wasm_value_type_u::f64);
    break;
}
case wasm1_code::i32_eqz:
{
    validate_numeric_unary(u8"i32.eqz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::i32_rem_u_2localget_wait_eqz)
    {
        conbine_pending.kind = conbine_pending_kind::i32_rem_u_eqz_2localget_wait_brif;
        break;
    }
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_eqz;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::i32)
    {
        // Conbine: if the next opcode is `br_if`, defer emission so `br_if_local_eqz` can be produced.
        conbine_pending.kind = conbine_pending_kind::local_get_eqz_i32;
    }
    else
    {
        br_if_fuse.kind = br_if_fuse_kind::i32_eqz;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    break;
}
case wasm1_code::i32_eq:
{
    validate_numeric_binary(u8"i32.eq", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        // i32.eq ...
        // [ safe ] unsafe (could be the section_end)
        // ^^ code_curr

        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if)
        {
            conbine_pending.kind = conbine_pending_kind::local_get_const_i32_cmp_brif;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::i32_eq;
        }
        else
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eq_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        }
    }
    else
    {
        br_if_fuse.kind = br_if_fuse_kind::i32_eq;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i32_ne:
{
    validate_numeric_binary(u8"i32.ne", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::for_ptr_inc_after_pend_get)
    {
        conbine_pending.kind = conbine_pending_kind::for_ptr_inc_after_cmp;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ne_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        emit_imm_to(bytecode, conbine_pending.imm_i32);
        stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
        conbine_pending.kind = conbine_pending_kind::none;
    }
    else
    {
        br_if_fuse.kind = br_if_fuse_kind::i32_ne;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i32_lt_s:
{
    validate_numeric_binary(u8"i32.lt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if)
        {
            conbine_pending.kind = conbine_pending_kind::local_get_const_i32_cmp_brif;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::i32_lt_s;
        }
        else
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        }
    }
    else
    {
        // Peephole: `i32.lt_s; i32.const 1; i32.and; i32.eqz; br_if` is a common LLVM/Clang boolean-normalize pattern.
        // Because `i32.lt_s` already yields 0/1, `& 1` is redundant and `eqz` simply inverts the compare:
        //   (lhs < rhs) & 1 == (lhs < rhs)
        //   eqz(...)        == (lhs >= rhs)
        //
        // Rewrite into `i32.ge_s; br_if` so the existing compare+br_if fusion can fire (reduces dispatch in tight loops).

        br_if_fuse_kind fuse_kind{br_if_fuse_kind::i32_lt_s};
        auto fused_fptr{translate::get_uwvmint_i32_lt_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)};

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if(!is_polymorphic)
        {
            bool fuse_to_ge_s{};
            ::std::byte const* brif_ip{};

            do
            {
                if(code_curr == code_end) { break; }
                wasm1_code op1{};  // init
                ::std::memcpy(::std::addressof(op1), code_curr, sizeof(op1));
                if(op1 != wasm1_code::i32_const) { break; }

                wasm_i32 imm{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                        ::fast_io::mnp::leb128_get(imm))};
                if(imm_err != ::fast_io::parse_code::ok || imm != 1) { break; }

                auto const after_const{reinterpret_cast<::std::byte const*>(imm_next)};
                if(after_const == code_end) { break; }
                wasm1_code op2{};  // init
                ::std::memcpy(::std::addressof(op2), after_const, sizeof(op2));
                if(op2 != wasm1_code::i32_and) { break; }

                auto const after_and{after_const + 1};
                if(after_and == code_end) { break; }
                wasm1_code op3{};  // init
                ::std::memcpy(::std::addressof(op3), after_and, sizeof(op3));
                if(op3 != wasm1_code::i32_eqz) { break; }

                auto const after_eqz{after_and + 1};
                if(after_eqz == code_end) { break; }
                wasm1_code op4{};  // init
                ::std::memcpy(::std::addressof(op4), after_eqz, sizeof(op4));
                if(op4 != wasm1_code::br_if) { break; }

                fuse_to_ge_s = true;
                brif_ip = after_eqz;  // skip const/and/eqz, re-enter at br_if
            }
            while(false);

            if(fuse_to_ge_s)
            {
                fuse_kind = br_if_fuse_kind::i32_ge_s;
                fused_fptr = translate::get_uwvmint_i32_ge_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple);
                code_curr = brif_ip;
            }
        }
# endif

        br_if_fuse.kind = fuse_kind;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, fused_fptr);
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i32_lt_u:
{
    validate_numeric_binary(u8"i32.lt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::for_i32_inc_after_end_const)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_after_cmp;
        break;
    }
# endif
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        // i32.lt_u ...
        // [ safe ] unsafe (could be the section_end)
        // ^^ code_curr

        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if)
        {
            conbine_pending.kind = conbine_pending_kind::local_get_const_i32_cmp_brif;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::i32_lt_u;
        }
        else
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        }
    }
    else
    {
        // Same boolean-normalize peephole as `i32.lt_s` case, but for unsigned compares:
        // `i32.lt_u; i32.const 1; i32.and; i32.eqz; br_if` => `i32.ge_u; br_if`

        br_if_fuse_kind fuse_kind{br_if_fuse_kind::i32_lt_u};
        auto fused_fptr{translate::get_uwvmint_i32_lt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)};

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if(!is_polymorphic)
        {
            bool fuse_to_ge_u{};
            ::std::byte const* brif_ip{};

            do
            {
                if(code_curr == code_end) { break; }
                wasm1_code op1{};  // init
                ::std::memcpy(::std::addressof(op1), code_curr, sizeof(op1));
                if(op1 != wasm1_code::i32_const) { break; }

                wasm_i32 imm{};
                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr + 1),
                                                                        reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                        ::fast_io::mnp::leb128_get(imm))};
                if(imm_err != ::fast_io::parse_code::ok || imm != 1) { break; }

                auto const after_const{reinterpret_cast<::std::byte const*>(imm_next)};
                if(after_const == code_end) { break; }
                wasm1_code op2{};  // init
                ::std::memcpy(::std::addressof(op2), after_const, sizeof(op2));
                if(op2 != wasm1_code::i32_and) { break; }

                auto const after_and{after_const + 1};
                if(after_and == code_end) { break; }
                wasm1_code op3{};  // init
                ::std::memcpy(::std::addressof(op3), after_and, sizeof(op3));
                if(op3 != wasm1_code::i32_eqz) { break; }

                auto const after_eqz{after_and + 1};
                if(after_eqz == code_end) { break; }
                wasm1_code op4{};  // init
                ::std::memcpy(::std::addressof(op4), after_eqz, sizeof(op4));
                if(op4 != wasm1_code::br_if) { break; }

                fuse_to_ge_u = true;
                brif_ip = after_eqz;
            }
            while(false);

            if(fuse_to_ge_u)
            {
                fuse_kind = br_if_fuse_kind::i32_ge_u;
                fused_fptr = translate::get_uwvmint_i32_ge_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple);
                code_curr = brif_ip;
            }
        }
# endif

        br_if_fuse.kind = fuse_kind;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, fused_fptr);
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_lt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i32_gt_s:
{
    validate_numeric_binary(u8"i32.gt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i32_gt_s;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_gt_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
    break;
}
case wasm1_code::i32_gt_u:
{
    validate_numeric_binary(u8"i32.gt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i32_gt_u;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_gt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
    break;
}
case wasm1_code::i32_le_s:
{
    validate_numeric_binary(u8"i32.le_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i32_le_s;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_le_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
    break;
}
case wasm1_code::i32_le_u:
{
    validate_numeric_binary(u8"i32.le_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i32_le_u;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_le_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
    break;
}
case wasm1_code::i32_ge_s:
{
    validate_numeric_binary(u8"i32.ge_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if)
        {
            conbine_pending.kind = conbine_pending_kind::local_get_const_i32_cmp_brif;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::i32_ge_s;
        }
        else
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_s_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        }
    }
    else
    {
        br_if_fuse.kind = br_if_fuse_kind::i32_ge_s;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i32_ge_u:
{
    validate_numeric_binary(u8"i32.ge_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32)
    {
        // i32.ge_u ...
        // [ safe ] unsafe (could be the section_end)
        // ^^ code_curr

        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if)
        {
            conbine_pending.kind = conbine_pending_kind::local_get_const_i32_cmp_brif;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::i32_ge_u;
        }
        else
        {
            stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_u_imm_localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            emit_imm_to(bytecode, conbine_pending.off1);
            emit_imm_to(bytecode, conbine_pending.imm_i32);
            stacktop_commit_push1_typed_if_reachable(curr_operand_stack_value_type::i32);
            conbine_pending.kind = conbine_pending_kind::none;
            conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
        }
    }
    else
    {
        br_if_fuse.kind = br_if_fuse_kind::i32_ge_u;
        br_if_fuse.site = bytecode.size();
        br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
        emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        wasm1_code next_opbase{};  // init
        if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
        if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
        else
        {
            stacktop_after_pop_n_if_reachable(bytecode, 1uz);
        }
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_ge_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if) { stacktop_after_pop_n_no_fill_if_reachable(1uz); }
    else
    {
        stacktop_after_pop_n_if_reachable(bytecode, 1uz);
    }
#endif
    break;
}
case wasm1_code::i64_eqz:
{
    validate_numeric_unary(u8"i64.eqz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    // `i64.cmp -> i32` with disjoint i64/i32 rings pushes into the i32 ring (2D variant).
    // Ensure the i32 ring has a free slot before emitting the compare opfunc (spill happens here, not after).
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i64_eqz;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(1uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
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
    }
    break;
}
case wasm1_code::i64_eq:
{
    validate_numeric_binary(u8"i64.eq", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_ne:
{
    validate_numeric_binary(u8"i64.ne", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i64_ne;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::i64_lt_s:
{
    validate_numeric_binary(u8"i64.lt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_lt_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_lt_u:
{
    validate_numeric_binary(u8"i64.lt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i64_lt_u;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_lt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::i64_gt_s:
{
    validate_numeric_binary(u8"i64.gt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_gt_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_gt_u:
{
    validate_numeric_binary(u8"i64.gt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.kind = br_if_fuse_kind::i64_gt_u;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_gt_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::i64_le_s:
{
    validate_numeric_binary(u8"i64.le_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_le_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_le_u:
{
    validate_numeric_binary(u8"i64.le_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_le_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_ge_s:
{
    validate_numeric_binary(u8"i64.ge_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ge_s_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::i64_ge_u:
{
    validate_numeric_binary(u8"i64.ge_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i64_ge_u_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32))
    {
        stacktop_after_pop_n_retype_top_if_reachable(bytecode, 1uz, curr_operand_stack_value_type::i32);
    }
    else
    {
        stacktop_after_pop_n_push1_typed_if_reachable(bytecode, 2uz, curr_operand_stack_value_type::i32);
    }
    break;
}
case wasm1_code::f32_eq:
{
    validate_numeric_binary(u8"f32.eq", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.eq` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.eq(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_eq;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        // f32.cmp -> i32: output may land in the i32 ring (2D variant), so ensure the i32 ring has a free slot.
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_eq_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_eq_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f32_ne:
{
    validate_numeric_binary(u8"f32.ne", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.ne` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.ne(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_ne;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_ne_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ne_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f32_lt:
{
    validate_numeric_binary(u8"f32.lt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.lt` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.lt(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_lt;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_lt_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_lt_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_lt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_lt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f32_gt:
{
    validate_numeric_binary(u8"f32.gt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.gt` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.gt(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_gt;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_gt_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_gt_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_gt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_gt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f32_le:
{
    validate_numeric_binary(u8"f32.le", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.le` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.le(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_le;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_le_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_le_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_le_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_le_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f32_ge:
{
    validate_numeric_binary(u8"f32.ge", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f32); local.get(f32); f32.ge` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f32.ge(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f32) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f32_ge;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f32)
    {
        br_if_fuse.kind = br_if_fuse_kind::f32_ge_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ge_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ge_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f32_ge_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_eq:
{
    validate_numeric_binary(u8"f64.eq", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.eq` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.eq(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f64_eq;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_eq_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_eq_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_eq_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_ne:
{
    validate_numeric_binary(u8"f64.ne", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.ne` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.ne(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f64_ne;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_ne_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ne_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ne_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_lt:
{
    validate_numeric_binary(u8"f64.lt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_convert)
    {
        conbine_pending.kind = conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_cmp;
        break;
    }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.lt` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.lt(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }

    // Enable compare+br_if fusion for:
    // - `f64.lt; br_if` (branch on `lhs < rhs`), and
    // - `f64.lt; i32.eqz; br_if` (branch on `!(lhs < rhs)` — including NaN cases).
    //
    // The second form is common in LLVM loop codegen and cannot be rewritten into `f64.ge` due to NaN semantics.
    // We therefore skip the `i32.eqz` opcode and select a dedicated fused `br_if_f64_lt_eqz` opfunc.

    br_if_fuse.kind = br_if_fuse_kind::f64_lt;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;

    // NOTE: the `i32.eqz` skipping fusion is not applied when `local.get(rhs)` is delayed into the compare op.
    // In that case we fall back to the generic `i32.eqz` opcode for correctness (NaN semantics).
    if(!is_polymorphic && !(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64) &&
       code_curr != code_end)
    {
        wasm1_code op1{};  // init
        ::std::memcpy(::std::addressof(op1), code_curr, sizeof(op1));
        if(op1 == wasm1_code::i32_eqz)
        {
            auto const after_eqz{code_curr + 1};
            if(after_eqz != code_end)
            {
                wasm1_code op2{};  // init
                ::std::memcpy(::std::addressof(op2), after_eqz, sizeof(op2));
                if(op2 == wasm1_code::br_if)
                {
                    br_if_fuse.kind = br_if_fuse_kind::f64_lt_eqz;
                    code_curr = after_eqz;  // skip `i32.eqz`, re-enter at br_if
                }
            }
        }
    }
#endif

    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_lt_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_lt_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_lt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_lt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_gt:
{
    validate_numeric_binary(u8"f64.gt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.gt` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.gt(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f64_gt;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_gt_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_gt_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_gt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_gt_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_le:
{
    validate_numeric_binary(u8"f64.le", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.le` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.le(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f64_le;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_le_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_le_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_le_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_le_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
case wasm1_code::f64_ge:
{
    validate_numeric_binary(u8"f64.ge", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);
    bool localget_rhs_cmp{};
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    // `local.get(f64); local.get(f64); f64.ge` is allowed to continue in the conbine state machine,
    // but we don't have a dedicated `f64.ge(2localget)` opfunc. Flush the pending local.get(s) so the
    // generic compare sees the correct operands at runtime.
    if(conbine_pending.kind == conbine_pending_kind::local_get2 && conbine_pending.vt == curr_operand_stack_value_type::f64) { flush_conbine_pending(); }
    br_if_fuse.kind = br_if_fuse_kind::f64_ge;
    br_if_fuse.site = bytecode.size();
    br_if_fuse.stacktop_currpos_at_site = curr_stacktop;
#endif
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    if constexpr(stacktop_enabled_for_vt(curr_operand_stack_value_type::i32) &&
                 !stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
    {
        stacktop_prepare_push1_if_reachable(bytecode, curr_operand_stack_value_type::i32);
    }
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
    if(conbine_pending.kind == conbine_pending_kind::local_get && conbine_pending.vt == curr_operand_stack_value_type::f64)
    {
        br_if_fuse.kind = br_if_fuse_kind::f64_ge_localget_rhs;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ge_localget_rhs_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
        emit_imm_to(bytecode, conbine_pending.off1);
        conbine_pending.kind = conbine_pending_kind::none;
        br_if_fuse.end = bytecode.size();
        localget_rhs_cmp = true;
    }
    else
    {
        br_if_fuse.end = SIZE_MAX;
        emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ge_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    }
#else
    emit_opfunc_to(bytecode, translate::get_uwvmint_f64_ge_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    wasm1_code next_opbase{};  // init
    if(code_curr != code_end) { ::std::memcpy(::std::addressof(next_opbase), code_curr, sizeof(next_opbase)); }
    if(next_opbase == wasm1_code::br_if)
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_no_fill_if_reachable(localget_rhs_cmp ? 0uz : 1uz);
            if constexpr(stacktop_enabled)
            {
                if(!is_polymorphic) { codegen_stack_set_top(curr_operand_stack_value_type::i32); }
            }
        }
        else
        {
            stacktop_after_pop_n_push1_typed_no_fill_if_reachable(localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    else
    {
        if constexpr(stacktop_ranges_merged_for(curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32))
        {
            stacktop_after_pop_n_retype_top_if_reachable(bytecode, localget_rhs_cmp ? 0uz : 1uz, curr_operand_stack_value_type::i32);
        }
        else
        {
            stacktop_after_pop_n_push1_typed_if_reachable(bytecode, localget_rhs_cmp ? 1uz : 2uz, curr_operand_stack_value_type::i32);
        }
    }
    break;
}
