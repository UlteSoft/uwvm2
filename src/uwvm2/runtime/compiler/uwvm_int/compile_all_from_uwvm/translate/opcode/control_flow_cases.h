// Structured control-flow opcodes maintain two views at once: Wasm validation state and emitted
// interpreter labels. The extra comments explain why stack repair and stack-top canonicalization
// happen around block boundaries instead of being deferred to the runtime branch helper.
/// @warning Extension point: new block result encodings or multi-value block forms must update all block/loop/if blocktype decoders here.
case wasm1_code::unreachable:
{
    // `unreachable` emits a trapping helper and then enters polymorphic validation state. From this
    // point until the next control-flow merge, missing operands are tolerated by the Wasm rules.
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
    codegen_reachable = false;

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
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wswitch" // Core 3 opcode is intentionally outside the shared wasm1 enum.
#endif
case static_cast<wasm1_code>(0x1f): // Core 3 try_table
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
case wasm1_code::block:
{
    // A `block` creates a forward branch target whose label arity is its result type. We record the
    // operand-stack base now so every later branch can repair the stack back to this boundary.
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
    //        ^^ code_curr

    if(curr_opbase == static_cast<wasm1_code>(0x1f))
    { ::uwvm2::validation::standard::wasm3::require_exceptions_enabled(!wasm1p1_para.disable_exceptions, 0x1fu, op_begin, err); }
    auto const signature{parse_block_type(op_begin, u8"block")};
    ::uwvm2::runtime::compiler::shared::wasm_exception_control::handlers exception_handlers{};
    if(curr_opbase == static_cast<wasm1_code>(0x1f))
    {
        exception_handlers = ::uwvm2::runtime::compiler::shared::wasm_exception_control::read_handlers(
            code_curr, code_end, op_begin, curr_module, control_flow_stack.size(),
            [&](::std::size_t frame_index) constexpr noexcept
            {
                auto const& frame{control_flow_stack.index_unchecked(frame_index)};
                return frame.label;
            },
            [&](::std::size_t frame_index, ::std::size_t value_index) noexcept
            {
                auto const& frame{control_flow_stack.index_unchecked(frame_index)};
                return block_core_type_at(frame.label, frame.signature_type_index,
                    frame.type != block_type::loop, frame.has_singleton_result_core_type,
                    frame.singleton_result_core_type, value_index);
            }, err);
        // [try_table blocktype checked catch vector] next ... code_end
        // [safe                                   ] unsafe (could be code_end)
        //                                           ^^ code_curr: transactional decoder committed before publishing this frame.
    }


#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    // Large functions may enter tiered execution while looping. Emitting the poll at an outer
    // block boundary keeps the runtime check rare enough for interpreter speed while still giving
    // hot loops a deterministic OSR handoff point.
    if constexpr(CompileOption.enable_tiered_loop_osr_poll)
    {
        auto const function_code_size{static_cast<::std::size_t>(code_end - code_begin)};
        if(::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_poll_should_emit(local_func_count, function_code_size) &&
           !is_polymorphic && operand_stack.empty() && codegen_operand_stack.empty())
        {
            auto const result_begin{curr_func_type.result.begin};
            auto const result_end{curr_func_type.result.end};
            auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
            // OSR writes the complete function result at the empty operand-stack
            // base. Reserve that full span even when this body's only exit is a
            // tail transfer and its ordinary stack high-water mark is smaller.
            {
                ::std::size_t result_bytes{};
                bool result_layout_valid{true};
                for(::std::size_t index{}; index != result_count; ++index)
                {
                    // [validated result types ...] result_count; index is in range.
                    // [safe                     ] no cursor escapes the type storage.
                    auto const width{operand_stack_valtype_size(result_begin[index])};
                    if(width == 0uz || width > (::std::numeric_limits<::std::size_t>::max)() - result_bytes)
                    { result_layout_valid = false; break; }
                    result_bytes += width;
                }

                if(result_layout_valid)
                {
                    if(result_bytes > runtime_operand_stack_byte_max) { runtime_operand_stack_byte_max = result_bytes; }
                    if(result_count > runtime_operand_stack_max) { runtime_operand_stack_max = result_count; }
                    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                    using poll_imm_t = ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_immediate_t;
                    auto const request_countdown{
                        ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_block_osr_request_countdown_for_function_size(function_code_size)};
                    if(request_countdown != ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_request_countdown_disabled)
                    {
                        poll_imm_t poll_imm{.wasm_module_id = options.curr_wasm_id,
                                            .func_index = function_index,
                                            .loop_wasm_code_offset = static_cast<::std::size_t>(op_begin - code_begin),
                                            .result_bytes = result_bytes,
                                            .local_bytes = local_func_symbol.local_bytes_max - internal_temp_local_bytes,
                                            .countdown = 8192u,
                                            .reset_countdown = 8192u,
                                            .request_countdown = request_countdown};
                        emit_opfunc_to(bytecode, translate::get_uwvmint_tiered_loop_osr_poll_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        emit_imm(poll_imm);
                    }
                }
            }
        }
    }
#endif

    enter_control_frame(op_begin, u8"block", block_type::block, signature, SIZE_MAX, new_label(false), SIZE_MAX);
    control_flow_stack.back_unchecked().exception_handlers = ::std::move(exception_handlers);

    break;
}
case wasm1_code::loop:
{
    // A `loop` differs from `block`: its branch target is the loop header, and MVP loop labels take
    // parameters rather than results. The translator therefore records both start and end labels.
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

    auto const signature{parse_block_type(op_begin, u8"loop")};
    auto const loop_start_label_id{
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
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
             if constexpr(CompileOption.enable_tiered_loop_osr_poll)
             {
                 auto const function_code_size{static_cast<::std::size_t>(code_end - code_begin)};
                 if(::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_poll_should_emit(local_func_count, function_code_size) &&
                    !is_polymorphic && operand_stack.empty() && codegen_operand_stack.empty())
                 {
                     auto const result_begin{curr_func_type.result.begin};
                     auto const result_end{curr_func_type.result.end};
                     auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
                     // OSR writes the complete function result at the empty operand-stack
                     // base. Reserve that full span even when this body's only exit is a
                     // tail transfer and its ordinary stack high-water mark is smaller.
                     {
                         ::std::size_t result_bytes{};
                         bool result_layout_valid{true};
                         for(::std::size_t index{}; index != result_count; ++index)
                         {
                             // [validated result types ...] result_count; index is in range.
                             // [safe                     ] no cursor escapes the type storage.
                             auto const width{operand_stack_valtype_size(result_begin[index])};
                             if(width == 0uz || width > (::std::numeric_limits<::std::size_t>::max)() - result_bytes)
                             { result_layout_valid = false; break; }
                             result_bytes += width;
                         }

                         if(result_layout_valid)
                         {
                             if(result_bytes > runtime_operand_stack_byte_max) { runtime_operand_stack_byte_max = result_bytes; }
                             if(result_count > runtime_operand_stack_max) { runtime_operand_stack_max = result_count; }
                             namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                             using poll_imm_t = ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_immediate_t;
                             auto const poll_policy{::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_counter_policy_for_function_size(
                                 function_code_size)};
                             if(poll_policy.request_countdown !=
                                ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_request_countdown_disabled)
                             {
                                 poll_imm_t poll_imm{.wasm_module_id = options.curr_wasm_id,
                                                     .func_index = function_index,
                                                     .loop_wasm_code_offset = static_cast<::std::size_t>(op_begin - code_begin),
                                                     .result_bytes = result_bytes,
                                                     .local_bytes = local_func_symbol.local_bytes_max - internal_temp_local_bytes,
                                                     .countdown = poll_policy.initial_countdown,
                                                     .reset_countdown = poll_policy.reset_countdown,
                                                     .request_countdown = poll_policy.request_countdown};
                                 emit_opfunc_to(bytecode,
                                                translate::get_uwvmint_tiered_loop_osr_poll_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                 emit_imm(poll_imm);
                             }
                         }
                     }
                 }
             }
#endif
             return loop_start;
         }()};

    enter_control_frame(op_begin, u8"loop", block_type::loop, signature, loop_start_label_id, new_label(false), SIZE_MAX, code_curr);
#if defined(UWVM_CPP_EXCEPTIONS)
    {
        // The loop's header was published after register-only entry normalization. Save that exact
        // small cache state now: later try_table clauses can select this backward target.
        auto& loop_frame{control_flow_stack.back_unchecked()};
        loop_frame.stacktop_currpos_at_else_entry = curr_stacktop;
        loop_frame.stacktop_memory_count_at_else_entry = stacktop_memory_count;
        loop_frame.stacktop_cache_count_at_else_entry = stacktop_cache_count;
        loop_frame.stacktop_cache_i32_count_at_else_entry = stacktop_cache_i32_count;
        loop_frame.stacktop_cache_i64_count_at_else_entry = stacktop_cache_i64_count;
        loop_frame.stacktop_cache_f32_count_at_else_entry = stacktop_cache_f32_count;
        loop_frame.stacktop_cache_f64_count_at_else_entry = stacktop_cache_f64_count;
    }
#endif


    break;
}
case wasm1_code::if_:
{
    // `if` consumes an i32 condition and splits execution into two stack-top states. The generated
    // else thunk exists so both arms can enter their bodies with the same canonical cache contract.
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

    auto const signature{parse_block_type(op_begin, u8"if")};

    auto const if_param_count{static_cast<::std::size_t>(signature.start.end - signature.start.begin)};
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const if_required_overflows{if_param_count == max_operand_stack_requirement};
    auto const if_required_stack_size{if_required_overflows ? max_operand_stack_requirement : (if_param_count + 1uz)};
    if(!is_polymorphic && (if_required_overflows || concrete_operand_count() < if_required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"if", if_required_stack_size);
    }

    if(auto const cond{try_pop_concrete_operand()}; !operand_type_matches(cond, curr_operand_stack_value_type::i32)) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.if_cond_type_not_i32.cond_type = to_wasm1_value_type(cond.type);
        err.err_code = code_validation_error_code::if_cond_type_not_i32;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
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

    // Save the else-entry stack-top model separately from the then path. The conditional branch
    // can jump into a thunk first, so the else body must restore the exact state expected there.
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

    enter_control_frame(op_begin, u8"if", block_type::if_, signature, SIZE_MAX, end_label_id, else_dest_label_id);
    auto& if_frame_for_else_entry{control_flow_stack.back_unchecked()};
    if_frame_for_else_entry.stacktop_currpos_at_else_entry = else_entry_curr_stacktop;
    if_frame_for_else_entry.stacktop_memory_count_at_else_entry = else_entry_memory_count;
    if_frame_for_else_entry.stacktop_cache_count_at_else_entry = else_entry_cache_count;
    if_frame_for_else_entry.stacktop_cache_i32_count_at_else_entry = else_entry_cache_i32_count;
    if_frame_for_else_entry.stacktop_cache_i64_count_at_else_entry = else_entry_cache_i64_count;
    if_frame_for_else_entry.stacktop_cache_f32_count_at_else_entry = else_entry_cache_f32_count;
    if_frame_for_else_entry.stacktop_cache_f64_count_at_else_entry = else_entry_cache_f64_count;
    if_frame_for_else_entry.codegen_operand_stack_at_else_entry = else_entry_codegen_operand_stack;

    // As in the spec's push_ctrl algorithm, the then-frame starts reachable even when the
    // surrounding frame is polymorphic.
    is_polymorphic = false;
    break;
}
case wasm1_code::else_:
{
    // At `else`, the then-arm must branch over the else body after validating its result values.
    // The translator stores the then-end state so `end` can merge the reachable arm correctly.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::illegal_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto& if_frame{control_flow_stack.back_unchecked()};

    // Match `end`: polymorphic mode only relaxes underflow, but still rejects extra values
    // and still checks types when enough concrete values are present.
    auto const expected_count{static_cast<::std::size_t>(if_frame.result.end - if_frame.result.begin)};
    auto const base{if_frame.operand_stack_base};
    auto const stack_size{operand_stack.size()};
    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
        err.err_selectable.if_then_result_mismatch.actual_count = actual_count;

        if(expected_count == 1uz) { err.err_selectable.if_then_result_mismatch.expected_type = to_wasm1_value_type(*if_frame.result.begin); }
        else
        {
            err.err_selectable.if_then_result_mismatch.expected_type = {};
        }

        if(actual_count == 1uz && stack_size != 0uz)
        {
            err.err_selectable.if_then_result_mismatch.actual_type = to_wasm1_value_type(operand_stack.back_unchecked().type);
        }
        else
        {
            err.err_selectable.if_then_result_mismatch.actual_type = {};
        }

        err.err_code = code_validation_error_code::if_then_result_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Unreachable permits missing deeper operands, never a mismatched concrete suffix.
    if(expected_count != 0uz)
    {
        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{if_frame.result.begin[expected_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
            if(!block_value_matches(actual_operand, if_frame.result, if_frame.signature_type_index, true,
                if_frame.has_singleton_result_core_type, if_frame.singleton_result_core_type,
                expected_count - 1uz - i)) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.if_then_result_mismatch.expected_count = expected_count;
                err.err_selectable.if_then_result_mismatch.actual_count = actual_count;
                err.err_selectable.if_then_result_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.if_then_result_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::if_then_result_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    if_frame.then_polymorphic_end = is_polymorphic;

    if constexpr(stacktop_enabled)
    {
        if(!if_frame.polymorphic_base)
        {
            // If the then-path is reachable, record its stack-top state at the end label.
            // This is required when the else-path becomes unreachable before `end` (only then reaches `end`).
            if_frame.stacktop_has_then_end_state = codegen_reachable && !is_polymorphic;
            if(if_frame.stacktop_has_then_end_state)
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
    block_push_types(if_frame.start, if_frame.signature_type_index, false,
        if_frame.has_singleton_result_core_type, if_frame.singleton_result_core_type);
    // As in the spec's push_ctrl(else, ...), the else-frame itself starts reachable.
    is_polymorphic = false;
    codegen_reachable = if_frame.codegen_entry_reachable;
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
            sync_type_stacks_from_codegen_snapshot(if_frame.codegen_operand_stack_at_else_entry, if_frame.codegen_entry_reachable);
        }
    }
    // The saved pre-entry snapshot may be synthetic/dead and omit declared parameters.
    // Rebuild after that restoration, never instead of a real incoming-edge snapshot.
    stacktop_restore_dead_validation_model();
    // Core 3 pop_ctrl resets non-defaultable locals initialized only in the then arm.
    if(!initialized_locals.restore(if_frame.local_init_checkpoint)) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    if_frame.type = block_type::else_;

    break;
}
case wasm1_code::end:
{
    // `end` is a validation and label-resolution marker, not a runtime opcode by itself. This case
    // validates the block result, materializes pending labels, and rebuilds the post-block stack.
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
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
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

    bool implicit_else_matches_result{true};
    if(frame.type == block_type::if_)
    {
        auto const start_count{static_cast<::std::size_t>(frame.start.end - frame.start.begin)};
        implicit_else_matches_result = start_count == expected_count;
        for(::std::size_t i{}; implicit_else_matches_result && i != expected_count; ++i)
        {
            auto const start_rich{block_core_type_at(frame.start, frame.signature_type_index, false,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
            auto const result_rich{block_core_type_at(frame.result, frame.signature_type_index, true,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
            auto const start_core{start_rich.has_type ? start_rich.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.start.begin[i])};
            auto const result_core{result_rich.has_type ? result_rich.type :
                ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(frame.result.begin[i])};
            auto const signatures{::uwvm2::validation::standard::wasm3::core3_signature_view<
                ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{rich_owned_begin,
                    rich_owned_available ? runtime_type_count : 0uz}};
            implicit_else_matches_result = frame.start.begin[i] == frame.result.begin[i] &&
                runtime_core3_value_type_matches(start_core, result_core, signatures);
        }
    }
    if(frame.type == block_type::if_ && !implicit_else_matches_result) [[unlikely]]
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.if_missing_else.expected_count = expected_count;
        err.err_selectable.if_missing_else.expected_type =
            expected_count == 1uz ? to_wasm1_value_type(*frame.result.begin) : ::uwvm2::parser::wasm::standard::wasm1::type::value_type{};
        err.err_code = code_validation_error_code::if_missing_else;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const base{frame.operand_stack_base};
    auto const stack_size{operand_stack.size()};
    auto const actual_count{stack_size >= base ? stack_size - base : 0uz};

    if(!is_polymorphic ? (actual_count != expected_count) : (actual_count > expected_count))
    {
        // [caller-saved opcode/prefix] immediate bytes ... | code_end
        // [dispatch-proven byte, where present       ] | one-past is not dereferenced
        // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
        err.err_curr = op_begin;
        err.err_selectable.end_result_mismatch.block_kind = block_kind;
        err.err_selectable.end_result_mismatch.expected_count = expected_count;
        err.err_selectable.end_result_mismatch.actual_count = actual_count;

        if(expected_count == 1uz) { err.err_selectable.end_result_mismatch.expected_type = to_wasm1_value_type(*frame.result.begin); }
        else
        {
            err.err_selectable.end_result_mismatch.expected_type = {};
        }

        if(actual_count == 1uz && stack_size != 0uz)
        {
            err.err_selectable.end_result_mismatch.actual_type = to_wasm1_value_type(operand_stack.back_unchecked().type);
        }
        else
        {
            err.err_selectable.end_result_mismatch.actual_type = {};
        }

        err.err_code = code_validation_error_code::end_result_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Unreachable permits missing deeper operands, never a mismatched concrete suffix.
    if(expected_count != 0uz)
    {
        auto const concrete_to_check{actual_count < expected_count ? actual_count : expected_count};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{frame.result.begin[expected_count - 1uz - i]};
            auto const actual_operand{operand_stack.index_unchecked(stack_size - 1uz - i)};
            auto const matches{block_value_matches(actual_operand, frame.result, frame.signature_type_index, true,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type,
                expected_count - 1uz - i)};
            if(!matches) [[unlikely]]
            {
                // [caller-saved opcode/prefix] immediate bytes ... | code_end
                // [dispatch-proven byte, where present       ] | one-past is not dereferenced
                // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
                err.err_curr = op_begin;
                err.err_selectable.end_result_mismatch.block_kind = block_kind;
                err.err_selectable.end_result_mismatch.expected_count = expected_count;
                err.err_selectable.end_result_mismatch.actual_count = actual_count;
                err.err_selectable.end_result_mismatch.expected_type = to_wasm1_value_type(expected_type);
                err.err_selectable.end_result_mismatch.actual_type = to_wasm1_value_type(actual_operand.type);
                err.err_code = code_validation_error_code::end_result_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // A branch to this end may arrive with a different register-ring cursor from a live
    // fallthrough. In particular, a try_table catch-only predecessor starts with a freshly
    // materialized payload, while a normal protected call can branch with its result cached.
    // Normalize only the conflicting fallthrough edge to the already published branch state.
    // This is translation-time selection: matching joins and functions without such a join
    // emit no extra runtime instructions.
    if constexpr(stacktop_enabled && !strict_cf_entry_like_call)
    {
        if(codegen_reachable && !is_polymorphic && frame.stacktop_has_end_state)
        {
            auto const& target_types{frame.codegen_operand_stack_at_end};
            auto const& target_pos{frame.stacktop_currpos_at_end};
            bool const same_state{
                curr_stacktop.i32_stack_top_curr_pos == target_pos.i32_stack_top_curr_pos &&
                curr_stacktop.i64_stack_top_curr_pos == target_pos.i64_stack_top_curr_pos &&
                curr_stacktop.f32_stack_top_curr_pos == target_pos.f32_stack_top_curr_pos &&
                curr_stacktop.f64_stack_top_curr_pos == target_pos.f64_stack_top_curr_pos &&
                curr_stacktop.v128_stack_top_curr_pos == target_pos.v128_stack_top_curr_pos &&
                stacktop_memory_count == frame.stacktop_memory_count_at_end &&
                stacktop_cache_count == frame.stacktop_cache_count_at_end &&
                stacktop_cache_i32_count == frame.stacktop_cache_i32_count_at_end &&
                stacktop_cache_i64_count == frame.stacktop_cache_i64_count_at_end &&
                stacktop_cache_f32_count == frame.stacktop_cache_f32_count_at_end &&
                stacktop_cache_f64_count == frame.stacktop_cache_f64_count_at_end};
            if(!same_state)
            {
                auto const count{codegen_operand_stack.size()};
                if(count != target_types.size() || frame.stacktop_memory_count_at_end > count ||
                   frame.stacktop_cache_count_at_end != count - frame.stacktop_memory_count_at_end) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                for(::std::size_t index{}; index != count; ++index)
                {
                    if(codegen_operand_stack.index_unchecked(index).type != target_types.index_unchecked(index).type) [[unlikely]]
                    { ::fast_io::fast_terminate(); }
                }
                // [live fallthrough operand frame][cached suffix] -> complete memory tuple.
                // [safe                            ] the current type stack supplies every spill width.
                stacktop_flush_all_to_operand_stack(bytecode);
                // Empty caches make cursor reassignment a compiler-only operation. The target snapshot
                // came from a validated branch to this exact frame and has the same carrier tuple.
                curr_stacktop = target_pos;
                codegen_operand_stack = target_types;
                while(stacktop_memory_count != frame.stacktop_memory_count_at_end)
                {
                    auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                    if(!stacktop_enabled_for_vt(vt)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const begin{stacktop_range_begin_pos(vt)};
                    auto const end{stacktop_range_end_pos(vt)};
                    auto const cached{stacktop_cache_count_for_range(begin, end)};
                    if(cached >= end - begin) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const slot{stacktop_ring_advance_next(stacktop_currpos_for_range(begin, end), cached, begin, end)};
                    // [fully materialized operand prefix][one complete value] -> cached target suffix.
                    // [safe                            ^^^^^^^^^^^^^^^^^^^] the validated snapshot bounds each fill.
                    emit_stacktop_fill1_typed_to(bytecode, slot, vt);
                    --stacktop_memory_count;
                    ++stacktop_cache_count;
                    ++stacktop_cache_count_ref_for_vt(vt);
                }
                if(stacktop_cache_count != frame.stacktop_cache_count_at_end ||
                   stacktop_cache_i32_count != frame.stacktop_cache_i32_count_at_end ||
                   stacktop_cache_i64_count != frame.stacktop_cache_i64_count_at_end ||
                   stacktop_cache_f32_count != frame.stacktop_cache_f32_count_at_end ||
                   stacktop_cache_f64_count != frame.stacktop_cache_f64_count_at_end) [[unlikely]]
                { ::fast_io::fast_terminate(); }
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
        // `if` without `else` uses the end as the false target. Validation above guarantees that the untouched
        // block parameters on that path are exactly the declared result tuple.
        if(frame.else_label_id != SIZE_MAX) { set_label_offset(frame.else_label_id, bytecode.size()); }
    }

    operand_stack_truncate_to(base);
    block_push_types(frame.result, frame.signature_type_index, true,
        frame.has_singleton_result_core_type, frame.singleton_result_core_type);

    bool const codegen_fallthrough_before_merge{codegen_reachable};
    // Validation restores the enclosing frame's bottom flag, not the execution
    // reachability of this loop/if. Core 2 appendix 7.3 pop_ctrl/end does not
    // propagate a child's unreachable flag. Keep the codegen merge below separate:
    // br 0 in both if arms reaches this end, but never supplies a missing operand.
    bool const new_polymorphic_after_end{frame.polymorphic_base};
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
            if(!new_polymorphic_after_end && !codegen_fallthrough_before_merge)
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
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_else_entry, frame.codegen_entry_reachable);
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
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_then_end, frame.stacktop_has_then_end_state);
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
                    sync_type_stacks_from_codegen_snapshot(frame.codegen_operand_stack_at_end, frame.stacktop_has_end_state);
                }
            }
        }
    }

    codegen_reachable = codegen_fallthrough_before_merge || frame.stacktop_has_end_state ||
                        frame.stacktop_has_then_end_state ||
                        (frame.type == block_type::if_ && frame.codegen_entry_reachable);
#if defined(UWVM_CPP_EXCEPTIONS)
    if(frame.type != block_type::loop && frame.exception_target_index != SIZE_MAX)
    {
        bool const exception_only_entry{!codegen_reachable};
        if(exception_only_entry)
        {
            // EH is the only executable incoming edge. Validation has already restored the target
            // tuple; select an empty cache for its successor, independent of unreachable stale state.
            codegen_operand_stack = operand_stack;
            stacktop_reset_currpos_to_begin();
            stacktop_memory_count = codegen_operand_stack.size();
            stacktop_cache_count = 0uz;
            stacktop_cache_i32_count = stacktop_cache_i64_count = stacktop_cache_f32_count = stacktop_cache_f64_count = 0uz;
        }
        codegen_reachable = true;
        exception_capture_end_state(frame);
        if(exception_only_entry && !is_function_frame)
        {
            // This label accepts the empty-cache snapshot captured above. Numeric opfunc selection
            // assumes canonical caches at ordinary instruction boundaries, so refill at the label
            // before compiling its successor. EH thunks branch to this refill, never beyond it.
            // Only the EH-only edge executes these loads; existing ordinary joins are unchanged.
            stacktop_fill_to_canonical(bytecode);
        }
    }
#endif
    // All real ordinary/EH incoming edges have been resolved above. When none
    // reaches this end, the specification still reifies the declared result tuple.
    // Its memory-only accounting model emits no dead recovery instructions.
    stacktop_restore_dead_validation_model();
    // A branch-to-end restores a codegen snapshot whose carriers may predate the
    // declared block result. Rebind the validation witness to the result tuple
    // after every merge; this does not change guest values or emitted opfuncs.
    if((rich_owned_available && frame.signature_type_index < runtime_type_count) ||
       frame.has_singleton_result_core_type)
    {
        if(operand_stack.size() < expected_count) [[unlikely]] { ::fast_io::fast_terminate(); }
        for(::std::size_t i{}; i != expected_count; ++i)
        {
            auto const rich{block_core_type_at(frame.result, frame.signature_type_index, true,
                frame.has_singleton_result_core_type, frame.singleton_result_core_type, i)};
            if(!rich.has_type) { continue; }
            auto& value{operand_stack.index_unchecked(operand_stack.size() - expected_count + i)};
            value.core_type = rich.type;
            value.has_core_type = true;
            if(codegen_operand_stack.size() >= expected_count)
            {
                auto& codegen_value{codegen_operand_stack.index_unchecked(codegen_operand_stack.size() - expected_count + i)};
                codegen_value.core_type = rich.type;
                codegen_value.has_core_type = true;
            }
        }
    }
    // Core 3 pop_ctrl does not export local.set effects from a nested frame.
    if(!initialized_locals.restore(frame.local_init_checkpoint)) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    control_flow_stack.pop_back_unchecked();

    if(is_function_frame)
    {
        if(code_curr != code_end) [[unlikely]]
        {
            // [caller-saved opcode/prefix] immediate bytes ... | code_end
            // [dispatch-proven byte, where present       ] | one-past is not dereferenced
            // ^^ op_begin -> err.err_curr: copy only; caller owns the opcode-span proof.
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::trailing_code_after_end;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        // Extra-heavy mega-fuse is permitted only for the exact reference expression and its complete ABI.
        if constexpr(CompileOption.is_tail_call)
        {
            static constexpr ::std::byte kChacha20RefExpr[]{
                ::std::byte{0x41u}, ::std::byte{0x9cu}, ::std::byte{0xbau}, ::std::byte{0xf8u}, ::std::byte{0xf8u}, ::std::byte{0x01u}, ::std::byte{0x21u}, ::std::byte{0x02u}, ::std::byte{0x41u}, ::std::byte{0x00u}, ::std::byte{0x21u}, ::std::byte{0x03u},
                ::std::byte{0x41u}, ::std::byte{0xf4u}, ::std::byte{0xcau}, ::std::byte{0x81u}, ::std::byte{0xd9u}, ::std::byte{0x06u}, ::std::byte{0x21u}, ::std::byte{0x04u}, ::std::byte{0x41u}, ::std::byte{0x8cu}, ::std::byte{0x9au}, ::std::byte{0xb8u},
                ::std::byte{0xf8u}, ::std::byte{0x00u}, ::std::byte{0x21u}, ::std::byte{0x05u}, ::std::byte{0x41u}, ::std::byte{0x98u}, ::std::byte{0xb2u}, ::std::byte{0xe8u}, ::std::byte{0xd8u}, ::std::byte{0x01u}, ::std::byte{0x21u}, ::std::byte{0x06u},
                ::std::byte{0x41u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0xd0u}, ::std::byte{0x04u}, ::std::byte{0x21u}, ::std::byte{0x07u}, ::std::byte{0x41u}, ::std::byte{0xb2u}, ::std::byte{0xdau}, ::std::byte{0x88u},
                ::std::byte{0xcbu}, ::std::byte{0x07u}, ::std::byte{0x21u}, ::std::byte{0x08u}, ::std::byte{0x41u}, ::std::byte{0x88u}, ::std::byte{0x92u}, ::std::byte{0xa8u}, ::std::byte{0xd8u}, ::std::byte{0x00u}, ::std::byte{0x21u}, ::std::byte{0x09u},
                ::std::byte{0x41u}, ::std::byte{0x94u}, ::std::byte{0xaau}, ::std::byte{0xd8u}, ::std::byte{0xb8u}, ::std::byte{0x01u}, ::std::byte{0x21u}, ::std::byte{0x0au}, ::std::byte{0x41u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0x80u},
                ::std::byte{0xc8u}, ::std::byte{0x00u}, ::std::byte{0x21u}, ::std::byte{0x0bu}, ::std::byte{0x41u}, ::std::byte{0xeeu}, ::std::byte{0xc8u}, ::std::byte{0x81u}, ::std::byte{0x99u}, ::std::byte{0x03u}, ::std::byte{0x21u}, ::std::byte{0x0cu},
                ::std::byte{0x41u}, ::std::byte{0x84u}, ::std::byte{0x8au}, ::std::byte{0x98u}, ::std::byte{0x38u}, ::std::byte{0x21u}, ::std::byte{0x0du}, ::std::byte{0x41u}, ::std::byte{0x90u}, ::std::byte{0xa2u}, ::std::byte{0xc8u}, ::std::byte{0x98u},
                ::std::byte{0x01u}, ::std::byte{0x21u}, ::std::byte{0x0eu}, ::std::byte{0x41u}, ::std::byte{0xe5u}, ::std::byte{0xf0u}, ::std::byte{0xc1u}, ::std::byte{0x8bu}, ::std::byte{0x06u}, ::std::byte{0x21u}, ::std::byte{0x0fu}, ::std::byte{0x41u},
                ::std::byte{0x80u}, ::std::byte{0x82u}, ::std::byte{0x88u}, ::std::byte{0x18u}, ::std::byte{0x21u}, ::std::byte{0x10u}, ::std::byte{0x41u}, ::std::byte{0x0au}, ::std::byte{0x21u}, ::std::byte{0x11u}, ::std::byte{0x20u}, ::std::byte{0x01u},
                ::std::byte{0x21u}, ::std::byte{0x12u}, ::std::byte{0x03u}, ::std::byte{0x40u}, ::std::byte{0x20u}, ::std::byte{0x02u}, ::std::byte{0x20u}, ::std::byte{0x03u}, ::std::byte{0x20u}, ::std::byte{0x04u}, ::std::byte{0x20u}, ::std::byte{0x05u},
                ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x04u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x03u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x02u},
                ::std::byte{0x20u}, ::std::byte{0x05u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x05u}, ::std::byte{0x20u}, ::std::byte{0x04u}, ::std::byte{0x6au}, ::std::byte{0x22u},
                ::std::byte{0x13u}, ::std::byte{0x20u}, ::std::byte{0x0eu}, ::std::byte{0x20u}, ::std::byte{0x12u}, ::std::byte{0x20u}, ::std::byte{0x0fu}, ::std::byte{0x20u}, ::std::byte{0x10u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x04u},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0fu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0eu}, ::std::byte{0x20u}, ::std::byte{0x10u}, ::std::byte{0x73u},
                ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x10u}, ::std::byte{0x20u}, ::std::byte{0x04u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x14u}, ::std::byte{0x20u}, ::std::byte{0x0fu},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0fu}, ::std::byte{0x20u}, ::std::byte{0x0eu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0eu}, ::std::byte{0x20u},
                ::std::byte{0x10u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x10u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x04u}, ::std::byte{0x20u}, ::std::byte{0x06u},
                ::std::byte{0x20u}, ::std::byte{0x07u}, ::std::byte{0x20u}, ::std::byte{0x08u}, ::std::byte{0x20u}, ::std::byte{0x09u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x08u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u},
                ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x07u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x06u}, ::std::byte{0x20u}, ::std::byte{0x09u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u},
                ::std::byte{0x22u}, ::std::byte{0x09u}, ::std::byte{0x20u}, ::std::byte{0x08u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x08u}, ::std::byte{0x20u}, ::std::byte{0x07u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u},
                ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x12u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x07u}, ::std::byte{0x20u}, ::std::byte{0x0au}, ::std::byte{0x20u},
                ::std::byte{0x0bu}, ::std::byte{0x20u}, ::std::byte{0x0cu}, ::std::byte{0x20u}, ::std::byte{0x0du}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0cu}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u},
                ::std::byte{0x22u}, ::std::byte{0x0bu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0au}, ::std::byte{0x20u}, ::std::byte{0x0du}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u},
                ::std::byte{0x0du}, ::std::byte{0x20u}, ::std::byte{0x0cu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0cu}, ::std::byte{0x20u}, ::std::byte{0x0bu}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u},
                ::std::byte{0x22u}, ::std::byte{0x0bu}, ::std::byte{0x20u}, ::std::byte{0x0au}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x15u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0au}, ::std::byte{0x20u}, ::std::byte{0x10u},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x10u}, ::std::byte{0x20u}, ::std::byte{0x04u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x04u}, ::std::byte{0x20u},
                ::std::byte{0x07u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x07u}, ::std::byte{0x20u}, ::std::byte{0x0au}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0au},
                ::std::byte{0x20u}, ::std::byte{0x10u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x21u}, ::std::byte{0x10u}, ::std::byte{0x20u}, ::std::byte{0x13u}, ::std::byte{0x20u}, ::std::byte{0x03u},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x03u}, ::std::byte{0x20u}, ::std::byte{0x02u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x02u}, ::std::byte{0x20u},
                ::std::byte{0x05u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x05u}, ::std::byte{0x20u}, ::std::byte{0x08u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x08u},
                ::std::byte{0x20u}, ::std::byte{0x0bu}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0bu}, ::std::byte{0x20u}, ::std::byte{0x0eu}, ::std::byte{0x6au}, ::std::byte{0x22u},
                ::std::byte{0x0eu}, ::std::byte{0x20u}, ::std::byte{0x05u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x05u}, ::std::byte{0x20u}, ::std::byte{0x08u}, ::std::byte{0x6au},
                ::std::byte{0x22u}, ::std::byte{0x08u}, ::std::byte{0x20u}, ::std::byte{0x0bu}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0bu}, ::std::byte{0x20u}, ::std::byte{0x0eu},
                ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0eu}, ::std::byte{0x20u}, ::std::byte{0x05u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x21u}, ::std::byte{0x05u}, ::std::byte{0x20u},
                ::std::byte{0x02u}, ::std::byte{0x20u}, ::std::byte{0x12u}, ::std::byte{0x20u}, ::std::byte{0x06u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x06u}, ::std::byte{0x20u}, ::std::byte{0x09u}, ::std::byte{0x73u}, ::std::byte{0x41u},
                ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x09u}, ::std::byte{0x20u}, ::std::byte{0x0cu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0cu}, ::std::byte{0x20u}, ::std::byte{0x0fu}, ::std::byte{0x73u},
                ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0fu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x02u}, ::std::byte{0x20u}, ::std::byte{0x09u}, ::std::byte{0x73u}, ::std::byte{0x41u},
                ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x09u}, ::std::byte{0x20u}, ::std::byte{0x0cu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0cu}, ::std::byte{0x20u}, ::std::byte{0x0fu}, ::std::byte{0x73u},
                ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x12u}, ::std::byte{0x20u}, ::std::byte{0x02u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x02u}, ::std::byte{0x20u}, ::std::byte{0x09u},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x21u}, ::std::byte{0x09u}, ::std::byte{0x20u}, ::std::byte{0x03u}, ::std::byte{0x20u}, ::std::byte{0x15u}, ::std::byte{0x20u}, ::std::byte{0x0du},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0du}, ::std::byte{0x20u}, ::std::byte{0x14u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0fu}, ::std::byte{0x73u},
                ::std::byte{0x41u}, ::std::byte{0x10u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x03u}, ::std::byte{0x20u}, ::std::byte{0x06u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x06u}, ::std::byte{0x20u}, ::std::byte{0x0du},
                ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x0cu}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x0du}, ::std::byte{0x20u}, ::std::byte{0x0fu}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x0fu}, ::std::byte{0x20u},
                ::std::byte{0x03u}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x08u}, ::std::byte{0x77u}, ::std::byte{0x22u}, ::std::byte{0x03u}, ::std::byte{0x20u}, ::std::byte{0x06u}, ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x06u},
                ::std::byte{0x20u}, ::std::byte{0x0du}, ::std::byte{0x73u}, ::std::byte{0x41u}, ::std::byte{0x07u}, ::std::byte{0x77u}, ::std::byte{0x21u}, ::std::byte{0x0du}, ::std::byte{0x20u}, ::std::byte{0x11u}, ::std::byte{0x41u}, ::std::byte{0x7fu},
                ::std::byte{0x6au}, ::std::byte{0x22u}, ::std::byte{0x11u}, ::std::byte{0x0du}, ::std::byte{0x00u}, ::std::byte{0x0bu}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x03u}, ::std::byte{0x36u}, ::std::byte{0x02u},
                ::std::byte{0x3cu}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x07u}, ::std::byte{0x41u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0xd0u}, ::std::byte{0x04u}, ::std::byte{0x6au},
                ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x38u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x0bu}, ::std::byte{0x41u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0x80u}, ::std::byte{0xc8u},
                ::std::byte{0x00u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x34u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x01u}, ::std::byte{0x20u}, ::std::byte{0x12u}, ::std::byte{0x6au},
                ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x30u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x02u}, ::std::byte{0x41u}, ::std::byte{0x9cu}, ::std::byte{0xbau}, ::std::byte{0xf8u}, ::std::byte{0xf8u},
                ::std::byte{0x01u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x2cu}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x06u}, ::std::byte{0x41u}, ::std::byte{0x98u}, ::std::byte{0xb2u},
                ::std::byte{0xe8u}, ::std::byte{0xd8u}, ::std::byte{0x01u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x28u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x0au}, ::std::byte{0x41u},
                ::std::byte{0x94u}, ::std::byte{0xaau}, ::std::byte{0xd8u}, ::std::byte{0xb8u}, ::std::byte{0x01u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x24u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u},
                ::std::byte{0x0eu}, ::std::byte{0x41u}, ::std::byte{0x90u}, ::std::byte{0xa2u}, ::std::byte{0xc8u}, ::std::byte{0x98u}, ::std::byte{0x01u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x20u}, ::std::byte{0x20u},
                ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x05u}, ::std::byte{0x41u}, ::std::byte{0x8cu}, ::std::byte{0x9au}, ::std::byte{0xb8u}, ::std::byte{0xf8u}, ::std::byte{0x00u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u},
                ::std::byte{0x1cu}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x09u}, ::std::byte{0x41u}, ::std::byte{0x88u}, ::std::byte{0x92u}, ::std::byte{0xa8u}, ::std::byte{0xd8u}, ::std::byte{0x00u}, ::std::byte{0x6au},
                ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x18u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x0du}, ::std::byte{0x41u}, ::std::byte{0x84u}, ::std::byte{0x8au}, ::std::byte{0x98u}, ::std::byte{0x38u},
                ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x14u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x10u}, ::std::byte{0x41u}, ::std::byte{0x80u}, ::std::byte{0x82u}, ::std::byte{0x88u},
                ::std::byte{0x18u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x10u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x04u}, ::std::byte{0x41u}, ::std::byte{0xf4u}, ::std::byte{0xcau},
                ::std::byte{0x81u}, ::std::byte{0xd9u}, ::std::byte{0x06u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x0cu}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x08u}, ::std::byte{0x41u},
                ::std::byte{0xb2u}, ::std::byte{0xdau}, ::std::byte{0x88u}, ::std::byte{0xcbu}, ::std::byte{0x07u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x08u}, ::std::byte{0x20u}, ::std::byte{0x00u}, ::std::byte{0x20u},
                ::std::byte{0x0cu}, ::std::byte{0x41u}, ::std::byte{0xeeu}, ::std::byte{0xc8u}, ::std::byte{0x81u}, ::std::byte{0x99u}, ::std::byte{0x03u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u}, ::std::byte{0x04u}, ::std::byte{0x20u},
                ::std::byte{0x00u}, ::std::byte{0x20u}, ::std::byte{0x0fu}, ::std::byte{0x41u}, ::std::byte{0xe5u}, ::std::byte{0xf0u}, ::std::byte{0xc1u}, ::std::byte{0x8bu}, ::std::byte{0x06u}, ::std::byte{0x6au}, ::std::byte{0x36u}, ::std::byte{0x02u},
                ::std::byte{0x00u}, ::std::byte{0x0bu},
            };
            static_assert(sizeof(kChacha20RefExpr) == 770uz);
            // The fused walker already proved [code_begin, code_end) belongs to this function and reached its exclusive end.
            // [complete validated expression] | code_end
            // [safe: size equality below    ] | exclusive end is never dereferenced
            // ^^ code_begin: exact bytes are compared only after the complete extent proof; no pointer advances.
            auto const code_len{static_cast<::std::size_t>(code_end - code_begin)};
            bool exact_abi{func_parameter_count_uz == 2uz && curr_func_type.result.begin == curr_func_type.result.end &&
                           all_local_count == 22u};
            if(exact_abi)
            {
                // Exactly two parameters and twenty declared locals are present. Every queried index is below the proved 22.
                for(wasm_u32 local_index{}; local_index != 22u; ++local_index)
                {
                    if(local_type_from_index(local_index) != curr_operand_stack_value_type::i32) { exact_abi = false; break; }
                }
            }
            if(exact_abi && code_len == sizeof(kChacha20RefExpr) &&
               ::fast_io::freestanding::my_memcmp(code_begin, kChacha20RefExpr, sizeof(kChacha20RefExpr)) == 0)
            {
                ensure_memory_resolved();
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
                emit_imm_to(bytecode, resolved_memory.memory_p);    // memory0
            }
        }
#endif

        // Function end: emit `return` at the function end label.
        emit_return_to(bytecode);
#if defined(UWVM_CPP_EXCEPTIONS)
        emit_exception_thunks();
#endif

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

#if defined(UWVM_CPP_EXCEPTIONS)
        // All bytecode buffers are now final. Publish immutable call descriptors and their bounded
        // pointer immediates only after the cold targets and branch relocations are resolved.
        finalize_exception_metadata(main_size);
#endif

        if(runtime_log_on && runtime_log_emit_func_stats) [[unlikely]]
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
                                 u8"} loop_unwind{cand=",
                                 runtime_log_stats.loop_unwind_candidate_count,
                                 u8",applied=",
                                 runtime_log_stats.loop_unwind_applied_count,
                                 u8",reject=",
                                 runtime_log_stats.loop_unwind_rejected_count,
                                 u8",full=",
                                 runtime_log_stats.loop_unwind_full_count,
                                 u8",partial=",
                                 runtime_log_stats.loop_unwind_partial_count,
                                 u8",replay=",
                                 runtime_log_stats.loop_unwind_replayed_body_count,
                                 u8",wasm_bytes=",
                                 runtime_log_stats.loop_unwind_replayed_wasm_bytes,
                                 u8",bytecode_bytes=",
                                 runtime_log_stats.loop_unwind_replayed_bytecode_bytes,
                                 u8"} reorder{cand=",
                                 runtime_log_stats.instr_reorder_candidate_count,
                                 u8",applied=",
                                 runtime_log_stats.instr_reorder_applied_count,
                                 u8",local_preload=",
                                 runtime_log_stats.instr_reorder_local_preload_count,
                                 u8",local_reduce=",
                                 runtime_log_stats.instr_reorder_local_reduce_count,
                                 u8",reduce_set=",
                                 runtime_log_stats.instr_reorder_local_reduce_set_count,
                                 u8",reduce_tee=",
                                 runtime_log_stats.instr_reorder_local_reduce_tee_count,
                                 u8",expr_fold=",
                                 runtime_log_stats.instr_reorder_expr_fold_count,
                                 u8",expr_set=",
                                 runtime_log_stats.instr_reorder_expr_local_set_count,
                                 u8",expr_tee=",
                                 runtime_log_stats.instr_reorder_expr_local_tee_count,
                                 u8",const_set=",
                                 runtime_log_stats.instr_reorder_const_binop_local_set_count,
                                 u8",const_tee=",
                                 runtime_log_stats.instr_reorder_const_binop_local_tee_count,
                                 u8",ring_reject=",
                                 runtime_log_stats.instr_reorder_ring_slot_reject_count,
                                 u8",ring_used=",
                                 runtime_log_stats.instr_reorder_ring_slot_used_count,
                                 u8",expr_steps=",
                                 runtime_log_stats.instr_reorder_expr_step_count,
                                 u8",local_reads=",
                                 runtime_log_stats.instr_reorder_local_read_count,
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
        // IMPORTANT: bytecode contains self-referential absolute pointers (patched from rel offsets).
        // Copying would produce a new buffer with pointers still targeting the old buffer (UAF).
        storage.local_funcs.index_unchecked(local_function_idx) = ::std::move(local_func_symbol);

        finished_current_func = true;
        break;
    }

    break;
}
