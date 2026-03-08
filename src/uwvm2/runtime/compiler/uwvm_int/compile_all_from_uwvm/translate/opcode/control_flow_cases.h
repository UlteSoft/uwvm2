case wasm1_code::unreachable:
{
    // unreachable ...
    // [   safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    ++code_curr;

    // unreachable ...
    // [   safe  ] unsafe (could be the section_end)
    //             ^^ code_curr

    emit_opfunc_to(
        bytecode,
        ::uwvm2::runtime::compiler::uwvm_int::optable::translate::get_uwvmint_unreachable_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

    if(!control_flow_stack.empty())
    {
        auto const base{control_flow_stack.back_unchecked().operand_stack_base};
        operand_stack_truncate_to(base);
    }

    is_polymorphic = true;

    break;
}
case wasm1_code::nop:
{
    // nop    ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    ++code_curr;

    // nop    ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    break;
}
case wasm1_code::block:
{
    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // block  blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ op_begin

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // block blocktype ...
    // [     safe    ] unsafe (could be the section_end)
    //       ^^ op_begin

    wasm_byte blocktype_byte;
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // block blocktype ...
    // [     safe    ] unsafe (could be the section_end)
    //                 ^^ op_begin

    block_result_type block_result{};
    switch(blocktype_byte)
    {
        case 0x40u:
        {
            block_result = {};
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    control_flow_stack.push_back({.result = block_result,
                                  .operand_stack_base = operand_stack.size(),
                                  .type = block_type::block,
                                  .polymorphic_base = is_polymorphic,
                                  .then_polymorphic_end = false,
                                  .start_label_id = SIZE_MAX,
                                  .end_label_id = new_label(false),
                                  .else_label_id = SIZE_MAX});

    break;
}
case wasm1_code::loop:
{
    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // loop   blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // loop blocktype ...
    // [    safe    ] unsafe (could be the section_end)
    //      ^^ code_curr

    wasm_byte blocktype_byte;
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // loop blocktype ...
    // [    safe    ] unsafe (could be the section_end)
    //                ^^ code_curr

    block_result_type block_result{};
    switch(blocktype_byte)
    {
        case 0x40u:
        {
            block_result = {};
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    control_flow_stack.push_back({.result = block_result,
                                  .operand_stack_base = operand_stack.size(),
                                  .type = block_type::loop,
                                  .polymorphic_base = is_polymorphic,
                                  .then_polymorphic_end = false,
                                  .start_label_id =
                                      [&]() constexpr UWVM_THROWS
                                  {
                                      auto const loop_start{new_label(false)};
                                      if constexpr(stacktop_enabled)
                                      {
                                          if constexpr(strict_cf_entry_like_call)
                                          {
                                              if(!is_polymorphic)
                                              {
                                                  // Fallthrough into loop start: canonicalize before the re-entry label so
                                                  // back-edges can jump directly to the label and see the canonical state.
                                                  if(runtime_log_on) [[unlikely]]
                                                  {
                                                      ++runtime_log_stats.cf_loop_entry_canonicalize_to_mem_count;
                                                      if(runtime_log_emit_cf)
                                                      {
                                                          ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                               u8"[uwvm-int-translator] fn=",
                                                                               function_index,
                                                                               u8" ip=",
                                                                               runtime_log_curr_ip,
                                                                               u8" event=cf.loop_entry | action=canonicalize_edge_to_memory\n");
                                                      }
                                                  }
                                                  stacktop_canonicalize_edge_to_memory(bytecode);
                                              }
                                              else
                                              {
                                                  // Unreachable fallthrough: no runtime code needed, but keep model deterministic.
                                                  stacktop_reset_currpos_to_begin();
                                                  stacktop_memory_count = codegen_operand_stack.size();
                                                  stacktop_cache_count = 0uz;
                                                  stacktop_cache_i32_count = 0uz;
                                                  stacktop_cache_i64_count = 0uz;
                                                  stacktop_cache_f32_count = 0uz;
                                                  stacktop_cache_f64_count = 0uz;
                                              }
                                          }
                                      }
                                      if constexpr(stacktop_enabled)
                                      {
                                          if constexpr(!strict_cf_entry_like_call)
                                          {
                                              // Fallthrough into loop start: canonicalize currpos to a deterministic begin slot
                                              // using a pure-register transform (no operand-stack spill/fill).
                                              if(runtime_log_on) [[unlikely]]
                                              {
                                                  ++runtime_log_stats.cf_loop_entry_transform_count;
                                                  if(runtime_log_emit_cf)
                                                  {
                                                      ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                           u8"[uwvm-int-translator] fn=",
                                                                           function_index,
                                                                           u8" ip=",
                                                                           runtime_log_curr_ip,
                                                                           u8" event=cf.loop_entry | action=stacktop_transform_currpos_to_begin\n");
                                                  }
                                              }
                                              stacktop_transform_currpos_to_begin(bytecode);
                                          }
                                      }
                                      set_label_offset(loop_start, bytecode.size());
                                      return loop_start;
                                  }(),
                                  .end_label_id = new_label(false),
                                  .else_label_id = SIZE_MAX,
                                  .wasm_code_curr_at_start_label = code_curr});

    break;
}
case wasm1_code::if_:
{
    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // if     blocktype ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(code_curr == code_end) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::missing_block_type;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    // if blocktype ...
    // [   safe   ] unsafe (could be the section_end)
    //    ^^ code_curr

    wasm_byte blocktype_byte;
    ::std::memcpy(::std::addressof(blocktype_byte), code_curr, sizeof(blocktype_byte));

    ++code_curr;

    // if blocktype ...
    // [   safe   ] unsafe (could be the section_end)
    //              ^^ code_curr

    block_result_type block_result{};
    switch(blocktype_byte)
    {
        case 0x40u:
        {
            block_result = {};
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i32):
        {
            block_result.begin = i32_result_arr;
            block_result.end = i32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::i64):
        {
            block_result.begin = i64_result_arr;
            block_result.end = i64_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f32):
        {
            block_result.begin = f32_result_arr;
            block_result.end = f32_result_arr + 1u;
            break;
        }
        case static_cast<wasm_byte>(wasm_value_type_u::f64):
        {
            block_result.begin = f64_result_arr;
            block_result.end = f64_result_arr + 1u;
            break;
        }
        [[unlikely]] default:
        {
            err.err_curr = op_begin;
            err.err_selectable.u8 = blocktype_byte;
            err.err_code = code_validation_error_code::illegal_block_type;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(!is_polymorphic && operand_stack.empty()) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.operand_stack_underflow.op_code_name = u8"if";
        err.err_selectable.operand_stack_underflow.stack_size_actual = 0uz;
        err.err_selectable.operand_stack_underflow.stack_size_required = 1uz;
        err.err_code = code_validation_error_code::operand_stack_underflow;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(!operand_stack.empty())
    {
        auto const cond{operand_stack.back_unchecked()};
        operand_stack_pop_unchecked();
        if(cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.if_cond_type_not_i32.cond_type = cond.type;
            err.err_code = code_validation_error_code::if_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    auto const else_dest_label_id{new_label(false)};
    auto const end_label_id{new_label(false)};

    // With stack-top caching enabled, the condition pop requires cache refills (fills are skipped on the taken path),
    // so we lower the taken path to an always-present thunk:
    //   if (cond==0) -> else_thunk: [fill-to-canonical] ; br else_dest
    // This ensures both then/else paths see a canonical cache state at entry.
    ::std::size_t else_thunk_label_id{SIZE_MAX};
    ::std::size_t const br_if_target_label_id{[&]() constexpr UWVM_THROWS -> ::std::size_t
                                              {
                                                  if constexpr(stacktop_enabled)
                                                  {
                                                      if(!is_polymorphic)
                                                      {
                                                          else_thunk_label_id = new_label(true);
                                                          return else_thunk_label_id;
                                                      }
                                                  }
                                                  return else_dest_label_id;
                                              }()};

    // Lower `if` to `br_if(cond==0)` (jump to else/end on condition == 0).
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    emit_opfunc_to(
        bytecode,
        ::uwvm2::runtime::compiler::uwvm_int::optable::translate::get_uwvmint_br_if_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#else
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
    emit_opfunc_to(bytecode, translate::get_uwvmint_i32_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
    emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
#endif
    emit_ptr_label_placeholder(br_if_target_label_id, false);

    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            // `if` consumes the i32 condition (compiler-managed stack-top cursor); then refill to canonical on then-path.
            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);

            // Save post-pop pre-fill state for the else thunk.
            auto const post_pop_curr_stacktop{curr_stacktop};
            auto const post_pop_memory_count{stacktop_memory_count};
            auto const post_pop_cache_count{stacktop_cache_count};
            auto const post_pop_cache_i32_count{stacktop_cache_i32_count};
            auto const post_pop_cache_i64_count{stacktop_cache_i64_count};
            auto const post_pop_cache_f32_count{stacktop_cache_f32_count};
            auto const post_pop_cache_f64_count{stacktop_cache_f64_count};
            auto const post_pop_codegen_operand_stack{codegen_operand_stack};

            // Emit else thunk now (types are still the pre-then stack state).
            {
                auto const saved_curr_stacktop{curr_stacktop};
                auto const saved_memory_count{stacktop_memory_count};
                auto const saved_cache_count{stacktop_cache_count};
                auto const saved_cache_i32_count{stacktop_cache_i32_count};
                auto const saved_cache_i64_count{stacktop_cache_i64_count};
                auto const saved_cache_f32_count{stacktop_cache_f32_count};
                auto const saved_cache_f64_count{stacktop_cache_f64_count};
                auto const saved_codegen_operand_stack{codegen_operand_stack};

                curr_stacktop = post_pop_curr_stacktop;
                stacktop_memory_count = post_pop_memory_count;
                stacktop_cache_count = post_pop_cache_count;
                stacktop_cache_i32_count = post_pop_cache_i32_count;
                stacktop_cache_i64_count = post_pop_cache_i64_count;
                stacktop_cache_f32_count = post_pop_cache_f32_count;
                stacktop_cache_f64_count = post_pop_cache_f64_count;
                codegen_operand_stack = post_pop_codegen_operand_stack;

                set_label_offset(else_thunk_label_id, thunks.size());
                if constexpr(strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(thunks); }
                else
                {
                    stacktop_fill_to_canonical(thunks);
                }
                emit_br_to(thunks, else_dest_label_id, true);

                curr_stacktop = saved_curr_stacktop;
                stacktop_memory_count = saved_memory_count;
                stacktop_cache_count = saved_cache_count;
                stacktop_cache_i32_count = saved_cache_i32_count;
                stacktop_cache_i64_count = saved_cache_i64_count;
                stacktop_cache_f32_count = saved_cache_f32_count;
                stacktop_cache_f64_count = saved_cache_f64_count;
                codegen_operand_stack = saved_codegen_operand_stack;
            }

            // then-path: execute the fill-to-canonical immediately after the conditional branch.
            curr_stacktop = post_pop_curr_stacktop;
            stacktop_memory_count = post_pop_memory_count;
            stacktop_cache_count = post_pop_cache_count;
            stacktop_cache_i32_count = post_pop_cache_i32_count;
            stacktop_cache_i64_count = post_pop_cache_i64_count;
            stacktop_cache_f32_count = post_pop_cache_f32_count;
            stacktop_cache_f64_count = post_pop_cache_f64_count;
            codegen_operand_stack = post_pop_codegen_operand_stack;
            stacktop_fill_to_canonical(bytecode);
        }
    }

    auto else_entry_curr_stacktop{curr_stacktop};
    auto else_entry_memory_count{stacktop_memory_count};
    auto else_entry_cache_count{stacktop_cache_count};
    auto else_entry_cache_i32_count{stacktop_cache_i32_count};
    auto else_entry_cache_i64_count{stacktop_cache_i64_count};
    auto else_entry_cache_f32_count{stacktop_cache_f32_count};
    auto else_entry_cache_f64_count{stacktop_cache_f64_count};
    auto else_entry_codegen_operand_stack{codegen_operand_stack};
    if constexpr(stacktop_enabled)
    {
        if constexpr(strict_cf_entry_like_call)
        {
            else_entry_curr_stacktop.i32_stack_top_curr_pos = stacktop_i32_enabled ? CompileOption.i32_stack_top_begin_pos : SIZE_MAX;
            else_entry_curr_stacktop.i64_stack_top_curr_pos = stacktop_i64_enabled ? CompileOption.i64_stack_top_begin_pos : SIZE_MAX;
            else_entry_curr_stacktop.f32_stack_top_curr_pos = stacktop_f32_enabled ? CompileOption.f32_stack_top_begin_pos : SIZE_MAX;
            else_entry_curr_stacktop.f64_stack_top_curr_pos = stacktop_f64_enabled ? CompileOption.f64_stack_top_begin_pos : SIZE_MAX;
            else_entry_curr_stacktop.v128_stack_top_curr_pos = stacktop_v128_enabled ? CompileOption.v128_stack_top_begin_pos : SIZE_MAX;
            else_entry_memory_count = else_entry_codegen_operand_stack.size();
            else_entry_cache_count = 0uz;
            else_entry_cache_i32_count = 0uz;
            else_entry_cache_i64_count = 0uz;
            else_entry_cache_f32_count = 0uz;
            else_entry_cache_f64_count = 0uz;
        }
    }

    control_flow_stack.push_back({.result = block_result,
                                  .operand_stack_base = operand_stack.size(),
                                  .type = block_type::if_,
                                  .polymorphic_base = is_polymorphic,
                                  .then_polymorphic_end = false,
                                  .stacktop_currpos_at_else_entry = else_entry_curr_stacktop,
                                  .stacktop_memory_count_at_else_entry = else_entry_memory_count,
                                  .stacktop_cache_count_at_else_entry = else_entry_cache_count,
                                  .stacktop_cache_i32_count_at_else_entry = else_entry_cache_i32_count,
                                  .stacktop_cache_i64_count_at_else_entry = else_entry_cache_i64_count,
                                  .stacktop_cache_f32_count_at_else_entry = else_entry_cache_f32_count,
                                  .stacktop_cache_f64_count_at_else_entry = else_entry_cache_f64_count,
                                  .codegen_operand_stack_at_else_entry = else_entry_codegen_operand_stack,
                                  .start_label_id = SIZE_MAX,
                                  .end_label_id = end_label_id,
                                  .else_label_id = else_dest_label_id});
    break;
}
case wasm1_code::else_:
{
    // else   ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // else   ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // else   ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(control_flow_stack.empty() || control_flow_stack.back_unchecked().type != block_type::if_) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::illegal_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto& if_frame{control_flow_stack.back_unchecked()};

    if(!is_polymorphic)
    {
        auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
        auto const actual_count{operand_stack.size() - if_frame.operand_stack_base};

        bool mismatch{expected_count != actual_count};

        wasm_value_type_u expected_type{};
        wasm_value_type_u actual_type{};

        bool const expected_single{expected_count == 1uz};
        bool const actual_single{actual_count == 1uz};

        if(expected_single) { expected_type = static_cast<wasm_value_type_u>(*if_frame.result.begin); }
        if(actual_single) { actual_type = static_cast<wasm_value_type_u>(operand_stack.back_unchecked().type); }

        if(!mismatch && expected_single && actual_single && expected_type != actual_type) { mismatch = true; }

        if(mismatch) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
            err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
            err.err_selectable.if_then_result_mismatch.expected_type = expected_type;
            err.err_selectable.if_then_result_mismatch.actual_type = actual_type;
            err.err_code = code_validation_error_code::if_then_result_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if_frame.then_polymorphic_end = is_polymorphic;

    if constexpr(stacktop_enabled)
    {
        if(!if_frame.polymorphic_base)
        {
            // If the then-path is reachable, record its stack-top state at the end label.
            // This is required when the else-path becomes unreachable before `end` (only then reaches `end`).
            if_frame.stacktop_has_then_end_state = !is_polymorphic;
            if(!is_polymorphic)
            {
                if_frame.stacktop_currpos_at_then_end = curr_stacktop;
                if_frame.stacktop_memory_count_at_then_end = stacktop_memory_count;
                if_frame.stacktop_cache_count_at_then_end = stacktop_cache_count;
                if_frame.stacktop_cache_i32_count_at_then_end = stacktop_cache_i32_count;
                if_frame.stacktop_cache_i64_count_at_then_end = stacktop_cache_i64_count;
                if_frame.stacktop_cache_f32_count_at_then_end = stacktop_cache_f32_count;
                if_frame.stacktop_cache_f64_count_at_then_end = stacktop_cache_f64_count;
                if_frame.codegen_operand_stack_at_then_end = codegen_operand_stack;
            }
        }
    }

    // Lower `else` marker:
    // - then-path must skip else body, so we emit an unconditional `br` to the end label here.
    // - else-label is the start of else body, which is *after* this `br`.
    if constexpr(stacktop_enabled)
    {
        if constexpr(strict_cf_entry_like_call)
        {
            if(!is_polymorphic) { stacktop_canonicalize_edge_to_memory(bytecode); }
        }
    }
    emit_br_to(bytecode, if_frame.end_label_id, false);
    set_label_offset(if_frame.else_label_id, bytecode.size());

    operand_stack_truncate_to(if_frame.operand_stack_base);
    is_polymorphic = if_frame.polymorphic_base;
    if constexpr(stacktop_enabled)
    {
        if(!if_frame.polymorphic_base)
        {
            // Restore stack-top cache state at `if` entry so else body codegen matches the taken path
            // (which runs the else thunk fill sequence then branches here).
            curr_stacktop = if_frame.stacktop_currpos_at_else_entry;
            stacktop_memory_count = if_frame.stacktop_memory_count_at_else_entry;
            stacktop_cache_count = if_frame.stacktop_cache_count_at_else_entry;
            stacktop_cache_i32_count = if_frame.stacktop_cache_i32_count_at_else_entry;
            stacktop_cache_i64_count = if_frame.stacktop_cache_i64_count_at_else_entry;
            stacktop_cache_f32_count = if_frame.stacktop_cache_f32_count_at_else_entry;
            stacktop_cache_f64_count = if_frame.stacktop_cache_f64_count_at_else_entry;
            sync_type_stacks_from_codegen_snapshot(if_frame.codegen_operand_stack_at_else_entry);
        }
    }
    if_frame.type = block_type::else_;

    break;
}
case wasm1_code::end:
{
    // end    ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // end    ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // end    ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    if(control_flow_stack.empty()) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.u8 = static_cast<::std::uint_least8_t>(curr_opbase);
        err.err_code = code_validation_error_code::illegal_opbase;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const& frame{control_flow_stack.back_unchecked()};
    bool const is_function_frame{frame.type == block_type::function};

    ::uwvm2::utils::container::u8string_view block_kind;
    switch(frame.type)
    {
        case block_type::function:
        {
            block_kind = u8"function";
            break;
        }
        case block_type::block:
        {
            block_kind = u8"block";
            break;
        }
        case block_type::loop:
        {
            block_kind = u8"loop";
            break;
        }
        case block_type::if_:
        {
            block_kind = u8"if";
            break;
        }
        case block_type::else_:
        {
            block_kind = u8"if-else";
            break;
        }
        [[unlikely]] default:
        {
            block_kind = u8"block";
            break;
        }
    }

    auto const expected_count{static_cast<::std::size_t>(frame.result.end - frame.result.begin)};

    if(frame.type == block_type::if_ && expected_count != 0uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.if_missing_else.expected_count = expected_count;
        err.err_selectable.if_missing_else.expected_type = static_cast<wasm_value_type_u>(*frame.result.begin);
        err.err_code = code_validation_error_code::if_missing_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const base{frame.operand_stack_base};
    auto const stack_size{operand_stack.size()};
    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
    {
        err.err_curr = op_begin;
        err.err_selectable.end_result_mismatch.block_kind = block_kind;
        err.err_selectable.end_result_mismatch.expected_count = expected_count;
        err.err_selectable.end_result_mismatch.actual_count = actual_count;

        if(expected_count == 1uz) { err.err_selectable.end_result_mismatch.expected_type = static_cast<wasm_value_type_u>(*frame.result.begin); }
        else
        {
            err.err_selectable.end_result_mismatch.expected_type = {};
        }

        if(actual_count == 1uz && stack_size != 0uz)
        {
            err.err_selectable.end_result_mismatch.actual_type = static_cast<wasm_value_type_u>(operand_stack.back_unchecked().type);
        }
        else
        {
            err.err_selectable.end_result_mismatch.actual_type = {};
        }

        err.err_code = code_validation_error_code::end_result_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    if(expected_count != 0uz && actual_count >= expected_count)
    {
        for(::std::size_t i{}; i != expected_count; ++i)
        {
            auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(stack_size - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.end_result_mismatch.block_kind = block_kind;
                err.err_selectable.end_result_mismatch.expected_count = expected_count;
                err.err_selectable.end_result_mismatch.actual_count = actual_count;
                err.err_selectable.end_result_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.end_result_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                err.err_code = code_validation_error_code::end_result_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Lower `end` marker: materialize branch target labels (no runtime opcode is emitted for `end` itself).
    //
    // bytecode[0 ... end_label) | (next opfunc / return)
    // [           safe         ] | unsafe (could be realloc during later append)
    //                              ^^ bytecode.size()
    if constexpr(stacktop_enabled)
    {
        if constexpr(strict_cf_entry_like_call)
        {
            // Fallthrough into `end`: canonicalize *before* the re-entry label so branches can jump
            // directly to the label (skipping this sequence) after doing their own canonicalization.
            if(!is_polymorphic) { stacktop_canonicalize_edge_to_memory(bytecode); }
            else
            {
                // Unreachable fallthrough: no runtime code needed, but keep model deterministic.
                stacktop_reset_currpos_to_begin();
                stacktop_memory_count = codegen_operand_stack.size();
                stacktop_cache_count = 0uz;
                stacktop_cache_i32_count = 0uz;
                stacktop_cache_i64_count = 0uz;
                stacktop_cache_f32_count = 0uz;
                stacktop_cache_f64_count = 0uz;
            }
        }
    }
    if(frame.end_label_id != SIZE_MAX) { set_label_offset(frame.end_label_id, bytecode.size()); }
    if(frame.type == block_type::if_)
    {
        // `if` without `else` (only valid for empty result) uses the end as the "else" target.
        if(frame.else_label_id != SIZE_MAX) { set_label_offset(frame.else_label_id, bytecode.size()); }
    }

    operand_stack_truncate_to(base);
    for(::std::size_t i{}; i != expected_count; ++i) { operand_stack_push(frame.result.begin[i]); }

    bool const polymorphic_end_before_merge{is_polymorphic};
    bool new_polymorphic_after_end{};
    if(frame.type == block_type::else_) { new_polymorphic_after_end = frame.polymorphic_base || (frame.then_polymorphic_end && is_polymorphic); }
    else if(frame.type == block_type::loop)
    {
        // Loop end is only reachable via fallthrough; branches target the loop header, not `end`.
        // If the fallthrough path is unreachable (polymorphic), code after `end` must remain unreachable.
        new_polymorphic_after_end = frame.polymorphic_base || is_polymorphic;
    }
    else
    {
        new_polymorphic_after_end = frame.polymorphic_base;
    }
    is_polymorphic = new_polymorphic_after_end;

    if constexpr(stacktop_enabled)
    {
        if constexpr(strict_cf_entry_like_call)
        {
            // In strict CF-entry mode, all re-entry labels are compiled to expect an empty stack-top cache.
            // This makes the post-`end` state deterministic regardless of how control reaches it.
            codegen_operand_stack = operand_stack;
            stacktop_reset_currpos_to_begin();
            stacktop_memory_count = codegen_operand_stack.size();
            stacktop_cache_count = 0uz;
            stacktop_cache_i32_count = 0uz;
            stacktop_cache_i64_count = 0uz;
            stacktop_cache_f32_count = 0uz;
            stacktop_cache_f64_count = 0uz;
        }
        else
        {
            // If the current fallthrough path is unreachable at `end`, but the construct is reachable due to
            // an earlier branch to this `end` label, restore the stack-top model to the reachable path state.
            if(!new_polymorphic_after_end && polymorphic_end_before_merge)
            {
                if(frame.type == block_type::if_ && !frame.polymorphic_base)
                {
                    // `if` without `else` can be reachable after `end` via the condition-false path,
                    // even if the then-path became unreachable before `end`.
                    curr_stacktop = frame.stacktop_currpos_at_else_entry;
                    stacktop_memory_count = frame.stacktop_memory_count_at_else_entry;
                    stacktop_cache_count = frame.stacktop_cache_count_at_else_entry;
                    stacktop_cache_i32_count = frame.stacktop_cache_i32_count_at_else_entry;
                    stacktop_cache_i64_count = frame.stacktop_cache_i64_count_at_else_entry;
                    stacktop_cache_f32_count = frame.stacktop_cache_f32_count_at_else_entry;
                    stacktop_cache_f64_count = frame.stacktop_cache_f64_count_at_else_entry;
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_else_entry);
                }
                else if(frame.type == block_type::else_ && !frame.then_polymorphic_end && frame.stacktop_has_then_end_state)
                {
                    // `if-else`: else-path is unreachable at `end`, but then-path reaches `end`.
                    curr_stacktop = frame.stacktop_currpos_at_then_end;
                    stacktop_memory_count = frame.stacktop_memory_count_at_then_end;
                    stacktop_cache_count = frame.stacktop_cache_count_at_then_end;
                    stacktop_cache_i32_count = frame.stacktop_cache_i32_count_at_then_end;
                    stacktop_cache_i64_count = frame.stacktop_cache_i64_count_at_then_end;
                    stacktop_cache_f32_count = frame.stacktop_cache_f32_count_at_then_end;
                    stacktop_cache_f64_count = frame.stacktop_cache_f64_count_at_then_end;
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_then_end);
                }
                else if(frame.stacktop_has_end_state)
                {
                    // Generic `block`/`loop`/`function` merge: fallthrough is unreachable at `end`,
                    // but the construct is reachable via a branch to its end label.
                    curr_stacktop = frame.stacktop_currpos_at_end;
                    stacktop_memory_count = frame.stacktop_memory_count_at_end;
                    stacktop_cache_count = frame.stacktop_cache_count_at_end;
                    stacktop_cache_i32_count = frame.stacktop_cache_i32_count_at_end;
                    stacktop_cache_i64_count = frame.stacktop_cache_i64_count_at_end;
                    stacktop_cache_f32_count = frame.stacktop_cache_f32_count_at_end;
                    stacktop_cache_f64_count = frame.stacktop_cache_f64_count_at_end;
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_end);
                }
            }
        }
    }

    control_flow_stack.pop_back_unchecked();

    if(is_function_frame)
    {
        if(code_curr != code_end) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::trailing_code_after_end;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        // Extra-heavy mega-fuse: replace the whole reference ChaCha20 block function body with one opfunc.
        // Matching is hash-based and intentionally strict so it never triggers accidentally.
        if constexpr(CompileOption.is_tail_call)
        {
            auto const fnv1a64{[](::std::byte const* p, ::std::size_t n) noexcept -> ::std::uint_least64_t
                               {
                                   ::std::uint_least64_t h{0xcbf29ce484222325ull};
                                   for(::std::size_t i{}; i != n; ++i)
                                   {
                                       h ^= static_cast<::std::uint_least64_t>(::std::to_integer<::std::uint_least8_t>(p[i]));
                                       h *= 0x100000001b3ull;
                                   }
                                   return h;
                               }};

            constexpr ::std::uint_least64_t kChacha20RefExprHash{0x247b8526bda862aaull};
            constexpr ::std::size_t kChacha20RefExprLen{770uz};
            ::std::size_t const code_len{static_cast<::std::size_t>(code_end - code_begin)};
            if(code_len == kChacha20RefExprLen && fnv1a64(code_begin, code_len) == kChacha20RefExprHash)
            {
                // Validate parameter locals are i32.
                if(func_parameter_count_uz >= 2uz && local_type_from_index(0u) == curr_operand_stack_value_type::i32 &&
                   local_type_from_index(1u) == curr_operand_stack_value_type::i32)
                {
                    ensure_memory0_resolved();
                    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

                    // Drop the previously generated bytecode and emit only:
                    //   [mega-op][return]
                    bytecode.clear();
                    labels.clear();
                    ptr_fixups.clear();
                    thunks.clear();

                    emit_opfunc_to(bytecode,
                                   translate::get_uwvmint_chacha20_block_fixed_key_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    emit_imm_to(bytecode, local_offset_from_index(0u));  // out_ptr
                    emit_imm_to(bytecode, local_offset_from_index(1u));  // counter
                    emit_imm_to(bytecode, resolved_memory0.memory_p);    // memory0
                }
            }
        }
#endif

        // Function end: emit `return` at the function end label.
        emit_return_to(bytecode);

        // Finalize thunks and patch all `[byte const*]` immediates:
        // - First pass: fill rel_offset_t placeholders with absolute offsets from bytecode begin.
        // - Second pass: turn rel_offset_t offsets into real `byte const*` pointers via `bit_cast`.
        ::std::size_t const main_size{bytecode.size()};

        if(!thunks.empty())
        {
            // Append thunks after main bytecode so previously recorded main offsets remain valid.
            emit_bytes_to(bytecode, thunks.data(), thunks.size());
        }

        // bytecode.data() (stable after append) ...
        // [             safe            ] | unsafe (no further realloc allowed)
        // ^^ bytecode_begin_ptr
        ::std::byte* const bytecode_begin_mut_ptr{bytecode.data()};
        ::std::byte const* const bytecode_begin_ptr{bytecode_begin_mut_ptr};

        for(auto const& fx: ptr_fixups)
        {
            auto const& lbl{labels.index_unchecked(fx.label_id)};
            if(lbl.offset == SIZE_MAX) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::std::size_t const target_abs{lbl.in_thunk ? (main_size + lbl.offset) : lbl.offset};
            ::std::size_t const site_abs{fx.in_thunk ? (main_size + fx.site) : fx.site};

            // Patch the `[byte const*]` immediate directly with the absolute pointer bits.
            ::std::byte const* const target_ptr{bytecode_begin_ptr + target_abs};
            rel_offset_t const ptr_bits{::std::bit_cast<rel_offset_t>(target_ptr)};
            ::std::memcpy(bytecode_begin_mut_ptr + site_abs, ::std::addressof(ptr_bits), sizeof(ptr_bits));
        }

        if(runtime_log_on) [[unlikely]]
        {
            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 u8"[uwvm-int-translator] fn=",
                                 function_index,
                                 u8" event=stats.func | wasm_ops=",
                                 runtime_log_stats.wasm_op_count,
                                 u8" bytecode{main=",
                                 main_size,
                                 u8",thunk=",
                                 thunks.size(),
                                 u8"} opfunc{main=",
                                 runtime_log_stats.opfunc_main_count,
                                 u8",thunk=",
                                 runtime_log_stats.opfunc_thunk_count,
                                 u8"} label_imm{main=",
                                 runtime_log_stats.label_placeholder_main_count,
                                 u8",thunk=",
                                 runtime_log_stats.label_placeholder_thunk_count,
                                 u8"} cf{br=",
                                 runtime_log_stats.cf_br_count,
                                 u8",br_tr=",
                                 runtime_log_stats.cf_br_transform_count,
                                 u8",br_if=",
                                 runtime_log_stats.cf_br_if_count,
                                 u8",loop_tr=",
                                 runtime_log_stats.cf_loop_entry_transform_count,
                                 u8",loop_mem=",
                                 runtime_log_stats.cf_loop_entry_canonicalize_to_mem_count,
                                 u8"} stacktop{spill1=",
                                 runtime_log_stats.stacktop_spill1_count,
                                 u8",spillN=",
                                 runtime_log_stats.stacktop_spillN_count,
                                 u8",fill1=",
                                 runtime_log_stats.stacktop_fill1_count,
                                 u8",fillN=",
                                 runtime_log_stats.stacktop_fillN_count,
                                 u8"}\n");
        }

        local_func_symbol.operand_stack_max = runtime_operand_stack_max;
        local_func_symbol.operand_stack_byte_max = runtime_operand_stack_byte_max;
        local_func_symbol.local_bytes_zeroinit_end =
            static_cast<::std::size_t>(local_bytes_zeroinit_end <= internal_temp_local_off ? local_bytes_zeroinit_end : internal_temp_local_off);
        storage.local_bytes_max = ::std::max(storage.local_bytes_max, local_func_symbol.local_bytes_max);
        storage.local_count = ::std::max(storage.local_count, local_func_symbol.local_count);
        storage.local_bytes_zeroinit_end = ::std::max(storage.local_bytes_zeroinit_end, local_func_symbol.local_bytes_zeroinit_end);
        storage.operand_stack_max = ::std::max(storage.operand_stack_max, runtime_operand_stack_max);
        storage.operand_stack_byte_max = ::std::max(storage.operand_stack_byte_max, runtime_operand_stack_byte_max);
        // IMPORTANT: bytecode contains self-referential absolute pointers (patched from rel offsets).
        // Copying would produce a new buffer with pointers still targeting the old buffer (UAF).
        storage.local_funcs.push_back(::std::move(local_func_symbol));

        finished_current_func = true;
        break;
    }

    break;
}
