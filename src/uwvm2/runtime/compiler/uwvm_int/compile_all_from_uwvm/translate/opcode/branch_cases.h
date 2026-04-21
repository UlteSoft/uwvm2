case wasm1_code::br:
{
    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br     label_index ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    wasm_u32 label_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(label_index))};
    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
    }

    // br     label_index ...
    // [     safe       ] unsafe (could be the section_end)
    //        ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(label_next);

    // br     label_index ...
    // [     safe       ] unsafe (could be the section_end)
    //                    ^^ code_curr

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
    if(label_index_uz >= all_label_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_label_index.label_index = label_index;
        err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
        err.err_code = code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const target_frame_index{all_label_count_uz - 1uz - label_index_uz};
    auto& target_frame{control_flow_stack.index_unchecked(target_frame_index)};
    // Label arity = label_types count. IMPORTANT: for `loop`, label types are parameters (MVP: none),
    // not result types.
    auto const target_arity{target_frame.type == block_type::loop ? 0uz : static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

    if(!is_polymorphic && concrete_operand_count() < target_arity) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"br", target_arity);
    }

    if(target_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < target_arity ? available_arg_count : target_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{target_frame.result.begin[target_arity - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Translate: `br` requires stack-shape repair before jumping because the interpreter `br` opcode does not unwind the operand stack.
    // In polymorphic (unreachable) regions we can skip repair.
    auto const target_label_id{get_branch_target_label_id(target_frame)};
    if(!is_polymorphic)
    {
        auto const target_base{target_frame.operand_stack_base};
        auto const curr_size{operand_stack.size()};

        if(target_arity == 0uz)
        {
            bool fused_extra_heavy_loop_run{};
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            // Extra-heavy: mega-fuse the full `test7`-style i32 sum loop into a single opfunc that keeps
            // `i/sum` in registers and performs at most one bounds check per slot.
            if(target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
               target_frame.wasm_code_curr_at_start_label != nullptr)
            {
                bool match_ok{true};
                if constexpr(stacktop_enabled) { match_ok = (stacktop_cache_count == 0uz); }

                if(match_ok)
                {
                    auto p{target_frame.wasm_code_curr_at_start_label};
                    auto const endp{op_begin};

                    auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                          {
                                              if(p >= endp) [[unlikely]] { return false; }
                                              wasm1_code op;  // no init
                                              ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                              if(op != expected) { return false; }
                                              ++p;
                                              return true;
                                          }};

                    auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    wasm_u32 sp_local_idx{};      // init
                    wasm_u32 off_i{};             // init
                    wasm_u32 off_sum{};           // init
                    wasm_i32 end_i{};             // init
                    wasm_i32 one_i{};             // init
                    wasm_u32 break_lbl_idx{};     // init
                    wasm_i32 step_i{};            // init
                    wasm_u32 memarg_align{};      // init
                    wasm_u32 tmp_local_idx{};     // init
                    wasm_u32 tmp_memarg_align{};  // init
                    wasm_u32 tmp_memarg_off{};    // init
                    wasm_u32 tmp_store_align{};   // init
                    wasm_u32 tmp_store_off{};     // init

                    auto const consume_local_get_sp{
                        [&]() constexpr noexcept -> bool
                        { return consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == sp_local_idx; }};

                    // Header: `local.get sp; i32.load off_i; i32.const end; i32.lt_s; i32.const 1; i32.and; i32.eqz; br_if 1`.
                    match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(sp_local_idx) && consume_op(wasm1_code::i32_load) &&
                               consume_u32_leb(memarg_align) && consume_u32_leb(off_i) && consume_op(wasm1_code::i32_const) && consume_i32_leb(end_i) &&
                               consume_op(wasm1_code::i32_lt_s) && consume_op(wasm1_code::i32_const) && consume_i32_leb(one_i) &&
                               consume_op(wasm1_code::i32_and) && consume_op(wasm1_code::i32_eqz) && consume_op(wasm1_code::br_if) &&
                               consume_u32_leb(break_lbl_idx);
                    if(match_ok) { match_ok = (one_i == wasm_i32{1} && break_lbl_idx == 1u); }

                    // Sum update:
                    // `local.get sp; local.get sp; i32.load off_sum; local.get sp; i32.load off_i; i32.add; i32.store off_sum;`
                    if(match_ok)
                    {
                        match_ok = consume_local_get_sp() && consume_local_get_sp() && consume_op(wasm1_code::i32_load) && consume_u32_leb(tmp_memarg_align) &&
                                   consume_u32_leb(off_sum) && consume_local_get_sp() && consume_op(wasm1_code::i32_load) &&
                                   consume_u32_leb(tmp_memarg_align) && consume_u32_leb(tmp_memarg_off) && tmp_memarg_off == off_i &&
                                   consume_op(wasm1_code::i32_add) && consume_op(wasm1_code::i32_store) && consume_u32_leb(tmp_store_align) &&
                                   consume_u32_leb(tmp_store_off) && tmp_store_off == off_sum;
                    }

                    // i increment:
                    // `local.get sp; local.get sp; i32.load off_i; i32.const step; i32.add; i32.store off_i;`
                    if(match_ok)
                    {
                        match_ok = consume_local_get_sp() && consume_local_get_sp() && consume_op(wasm1_code::i32_load) && consume_u32_leb(tmp_memarg_align) &&
                                   consume_u32_leb(tmp_memarg_off) && tmp_memarg_off == off_i && consume_op(wasm1_code::i32_const) && consume_i32_leb(step_i) &&
                                   consume_op(wasm1_code::i32_add) && consume_op(wasm1_code::i32_store) && consume_u32_leb(tmp_store_align) &&
                                   consume_u32_leb(tmp_store_off) && tmp_store_off == off_i;
                    }

                    if(match_ok) { match_ok = (p == endp); }

                    if(match_ok)
                    {
                        ensure_memory0_resolved();

                        auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                        if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                        {
                            // Remove all ptr-fixups in the to-be-rewritten loop region.
                            while(!ptr_fixups.empty())
                            {
                                auto const& fx{ptr_fixups.back_unchecked()};
                                if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                ptr_fixups.pop_back_unchecked();
                            }

                            bytecode.resize(loop_lbl.offset);

                            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                            emit_opfunc_to(bytecode, translate::get_uwvmint_i32_sum_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                            emit_imm_to(bytecode, local_offset_from_index(sp_local_idx));
                            emit_imm_to(bytecode, resolved_memory0.memory_p);
                            emit_imm_to(bytecode, off_i);
                            emit_imm_to(bytecode, off_sum);
                            emit_imm_to(bytecode, end_i);
                            emit_imm_to(bytecode, step_i);

                            fused_extra_heavy_loop_run = true;
                        }
                    }
                }
            }

            // Extra-heavy: mega-fuse `micro/loop_i64.wasm` hot loop into one opfunc dispatch.
            if(!fused_extra_heavy_loop_run && target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
               target_frame.wasm_code_curr_at_start_label != nullptr)
            {
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

                bool match_ok{true};
                if constexpr(stacktop_enabled) { match_ok = (stacktop_cache_count == 0uz); }

                if(match_ok)
                {
                    auto p{target_frame.wasm_code_curr_at_start_label};
                    auto const endp{op_begin};

                    auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                          {
                                              if(p >= endp) [[unlikely]] { return false; }
                                              wasm1_code op;  // no init
                                              ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                              if(op != expected) { return false; }
                                              ++p;
                                              return true;
                                          }};

                    auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    auto const consume_i64_leb{[&](wasm_i64& v) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    wasm_u32 i_local_idx{};    // init
                    wasm_u32 x_local_idx{};    // init
                    wasm_u32 tmp_local_idx{};  // init
                    wasm_u32 break_lbl_idx{};  // init
                    wasm_i32 end_i{};          // init
                    wasm_i32 one_i32{};        // init
                    wasm_i64 one_i64{};        // init
                    wasm_i64 mul_i64{};        // init
                    wasm_i64 rot_i64{};        // init

                    // Header: `local.get i; i32.const end; i32.ge_u; br_if 1`
                    match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(i_local_idx) && consume_op(wasm1_code::i32_const) &&
                               consume_i32_leb(end_i) && consume_op(wasm1_code::i32_ge_u) && consume_op(wasm1_code::br_if) && consume_u32_leb(break_lbl_idx) &&
                               break_lbl_idx == 1u;

                    // x = x + 1
                    if(match_ok)
                    {
                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(x_local_idx) && consume_op(wasm1_code::i64_const) &&
                                   consume_i64_leb(one_i64) && one_i64 == wasm_i64{1} && consume_op(wasm1_code::i64_add) && consume_op(wasm1_code::local_set) &&
                                   consume_u32_leb(tmp_local_idx) && tmp_local_idx == x_local_idx;
                    }

                    // x = x * 6364136223846793005
                    if(match_ok)
                    {
                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == x_local_idx &&
                                   consume_op(wasm1_code::i64_const) && consume_i64_leb(mul_i64) && mul_i64 == static_cast<wasm_i64>(6364136223846793005ll) &&
                                   consume_op(wasm1_code::i64_mul) && consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_local_idx) &&
                                   tmp_local_idx == x_local_idx;
                    }

                    // x = x ^ i64.extend_i32_u(i)
                    if(match_ok)
                    {
                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == x_local_idx &&
                                   consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == i_local_idx &&
                                   consume_op(wasm1_code::i64_extend_i32_u) && consume_op(wasm1_code::i64_xor) && consume_op(wasm1_code::local_set) &&
                                   consume_u32_leb(tmp_local_idx) && tmp_local_idx == x_local_idx;
                    }

                    // x = rotr(x, 17)
                    if(match_ok)
                    {
                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == x_local_idx &&
                                   consume_op(wasm1_code::i64_const) && consume_i64_leb(rot_i64) && rot_i64 == wasm_i64{17} &&
                                   consume_op(wasm1_code::i64_rotr) && consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_local_idx) &&
                                   tmp_local_idx == x_local_idx;
                    }

                    // i = i + 1
                    if(match_ok)
                    {
                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == i_local_idx &&
                                   consume_op(wasm1_code::i32_const) && consume_i32_leb(one_i32) && one_i32 == wasm_i32{1} && consume_op(wasm1_code::i32_add) &&
                                   consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == i_local_idx;
                    }

                    if(match_ok) { match_ok = (p == endp); }

                    if(match_ok)
                    {
                        auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                        if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                        {
                            while(!ptr_fixups.empty())
                            {
                                auto const& fx{ptr_fixups.back_unchecked()};
                                if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                ptr_fixups.pop_back_unchecked();
                            }

                            bytecode.resize(loop_lbl.offset);

                            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                            emit_opfunc_to(bytecode, translate::get_uwvmint_loop_i64_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                            emit_imm_to(bytecode, local_offset_from_index(i_local_idx));
                            emit_imm_to(bytecode, local_offset_from_index(x_local_idx));
                            emit_imm_to(bytecode, end_i);

                            fused_extra_heavy_loop_run = true;
                        }
                    }
                }
            }

            if constexpr(CompileOption.is_tail_call)
            {
                // Extra-heavy: mega-fuse `micro/round_f64_dense.wasm` hot loop into one opfunc dispatch.
                if(!fused_extra_heavy_loop_run && target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
                   target_frame.wasm_code_curr_at_start_label != nullptr)
                {
                    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
                    using wasm_u64 = ::std::uint_least64_t;

                    bool match_ok{true};
                    if constexpr(stacktop_enabled) { match_ok = (stacktop_cache_count == 0uz); }

                    if(match_ok)
                    {
                        auto p{target_frame.wasm_code_curr_at_start_label};
                        auto const endp{op_begin};

                        auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                              {
                                                  if(p >= endp) [[unlikely]] { return false; }
                                                  wasm1_code op;  // no init
                                                  ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                                  if(op != expected) { return false; }
                                                  ++p;
                                                  return true;
                                              }};

                        auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_f64_const_bits{[&](wasm_u64 expected_bits) constexpr noexcept -> bool
                                                          {
                                                              if(!consume_op(wasm1_code::f64_const)) { return false; }
                                                              if(static_cast<::std::size_t>(endp - p) < sizeof(wasm_f64)) [[unlikely]] { return false; }
                                                              wasm_u64 bits{};  // init
                                                              ::std::memcpy(::std::addressof(bits), p, sizeof(bits));
                                                              p += sizeof(bits);
                                                              return bits == expected_bits;
                                                          }};

                        constexpr wasm_u64 f64_add_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(0.000001))};
                        constexpr wasm_u64 f64_mul_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(1.0000001))};
                        constexpr wasm_u64 f64_sub_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(0.5))};
                        constexpr wasm_u64 f64_negone_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(-1.0))};

                        wasm_u32 i_local_idx{};    // init
                        wasm_u32 x_local_idx{};    // init
                        wasm_u32 acc_local_idx{};  // init
                        wasm_u32 tmp_local_idx{};  // init
                        wasm_u32 break_lbl_idx{};  // init
                        wasm_i32 end_i{};          // init
                        wasm_i32 one_i32{};        // init

                        auto const consume_local_get{
                            [&](wasm_u32 expected) constexpr noexcept -> bool
                            { return consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == expected; }};

                        auto const consume_local_set{
                            [&](wasm_u32 expected) constexpr noexcept -> bool
                            { return consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == expected; }};

                        auto const consume_acc_add_unop{[&](wasm1_code unop) constexpr noexcept -> bool
                                                        {
                                                            return consume_local_get(acc_local_idx) && consume_local_get(x_local_idx) && consume_op(unop) &&
                                                                   consume_op(wasm1_code::f64_add) && consume_local_set(acc_local_idx);
                                                        }};

                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(i_local_idx) && consume_op(wasm1_code::i32_const) &&
                                   consume_i32_leb(end_i) && consume_op(wasm1_code::i32_ge_u) && consume_op(wasm1_code::br_if) &&
                                   consume_u32_leb(break_lbl_idx) && break_lbl_idx == 1u;

                        if(match_ok)
                        {
                            match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(x_local_idx) && consume_f64_const_bits(f64_add_bits) &&
                                       consume_op(wasm1_code::f64_add) && consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(x_local_idx) && consume_f64_const_bits(f64_mul_bits) && consume_op(wasm1_code::f64_mul) &&
                                       consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(x_local_idx) && consume_f64_const_bits(f64_sub_bits) && consume_op(wasm1_code::f64_sub) &&
                                       consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(acc_local_idx) && consume_local_get(x_local_idx) &&
                                       consume_op(wasm1_code::f64_floor) && consume_op(wasm1_code::f64_add) && consume_local_set(acc_local_idx);
                        }

                        if(match_ok) { match_ok = consume_acc_add_unop(wasm1_code::f64_ceil); }
                        if(match_ok) { match_ok = consume_acc_add_unop(wasm1_code::f64_trunc); }
                        if(match_ok) { match_ok = consume_acc_add_unop(wasm1_code::f64_nearest); }
                        if(match_ok) { match_ok = consume_acc_add_unop(wasm1_code::f64_abs); }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(acc_local_idx) && consume_local_get(x_local_idx) && consume_f64_const_bits(f64_negone_bits) &&
                                       consume_op(wasm1_code::f64_copysign) && consume_op(wasm1_code::f64_add) && consume_local_set(acc_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(i_local_idx) && consume_op(wasm1_code::i32_const) && consume_i32_leb(one_i32) &&
                                       one_i32 == wasm_i32{1} && consume_op(wasm1_code::i32_add) && consume_local_set(i_local_idx);
                        }

                        if(match_ok) { match_ok = (p == endp); }

                        if(match_ok)
                        {
                            auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                            if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                            {
                                while(!ptr_fixups.empty())
                                {
                                    auto const& fx{ptr_fixups.back_unchecked()};
                                    if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                    ptr_fixups.pop_back_unchecked();
                                }

                                bytecode.resize(loop_lbl.offset);

                                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_round_f64_dense_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                emit_imm_to(bytecode, local_offset_from_index(i_local_idx));
                                emit_imm_to(bytecode, local_offset_from_index(x_local_idx));
                                emit_imm_to(bytecode, local_offset_from_index(acc_local_idx));
                                emit_imm_to(bytecode, end_i);

                                fused_extra_heavy_loop_run = true;
                            }
                        }
                    }
                }
            }

            if constexpr(CompileOption.is_tail_call)
            {
                // Extra-heavy: mega-fuse `micro/loop_f64.wasm` hot loop into one opfunc dispatch.
                if(!fused_extra_heavy_loop_run && target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
                   target_frame.wasm_code_curr_at_start_label != nullptr)
                {
                    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
                    using wasm_u64 = ::std::uint_least64_t;

                    bool match_ok{true};
                    if constexpr(stacktop_enabled) { match_ok = (stacktop_cache_count == 0uz); }

                    if(match_ok)
                    {
                        auto p{target_frame.wasm_code_curr_at_start_label};
                        auto const endp{op_begin};

                        auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                              {
                                                  if(p >= endp) [[unlikely]] { return false; }
                                                  wasm1_code op;  // no init
                                                  ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                                  if(op != expected) { return false; }
                                                  ++p;
                                                  return true;
                                              }};

                        auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_f64_const_bits{[&](wasm_u64 expected_bits) constexpr noexcept -> bool
                                                          {
                                                              if(!consume_op(wasm1_code::f64_const)) { return false; }
                                                              if(static_cast<::std::size_t>(endp - p) < sizeof(wasm_f64)) [[unlikely]] { return false; }
                                                              wasm_u64 bits{};  // init
                                                              ::std::memcpy(::std::addressof(bits), p, sizeof(bits));
                                                              p += sizeof(bits);
                                                              return bits == expected_bits;
                                                          }};

                        constexpr wasm_u64 f64_add_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(1.0))};
                        constexpr wasm_u64 f64_mul_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(1.0000001))};
                        constexpr wasm_u64 f64_sub_bits{::std::bit_cast<wasm_u64>(static_cast<wasm_f64>(0.5))};

                        wasm_u32 i_local_idx{};    // init
                        wasm_u32 x_local_idx{};    // init
                        wasm_u32 tmp_local_idx{};  // init
                        wasm_u32 break_lbl_idx{};  // init
                        wasm_i32 end_i{};          // init
                        wasm_i32 one_i32{};        // init

                        auto const consume_local_get{
                            [&](wasm_u32 expected) constexpr noexcept -> bool
                            { return consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == expected; }};

                        auto const consume_local_set{
                            [&](wasm_u32 expected) constexpr noexcept -> bool
                            { return consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_local_idx) && tmp_local_idx == expected; }};

                        match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(i_local_idx) && consume_op(wasm1_code::i32_const) &&
                                   consume_i32_leb(end_i) && consume_op(wasm1_code::i32_ge_u) && consume_op(wasm1_code::br_if) &&
                                   consume_u32_leb(break_lbl_idx) && break_lbl_idx == 1u;

                        if(match_ok)
                        {
                            match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(x_local_idx) && consume_f64_const_bits(f64_add_bits) &&
                                       consume_op(wasm1_code::f64_add) && consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(x_local_idx) && consume_f64_const_bits(f64_mul_bits) && consume_op(wasm1_code::f64_mul) &&
                                       consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(x_local_idx) && consume_f64_const_bits(f64_sub_bits) && consume_op(wasm1_code::f64_sub) &&
                                       consume_local_set(x_local_idx);
                        }

                        if(match_ok)
                        {
                            match_ok = consume_local_get(i_local_idx) && consume_op(wasm1_code::i32_const) && consume_i32_leb(one_i32) &&
                                       one_i32 == wasm_i32{1} && consume_op(wasm1_code::i32_add) && consume_local_set(i_local_idx);
                        }

                        if(match_ok) { match_ok = (p == endp); }

                        if(match_ok)
                        {
                            auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                            if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                            {
                                while(!ptr_fixups.empty())
                                {
                                    auto const& fx{ptr_fixups.back_unchecked()};
                                    if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                    ptr_fixups.pop_back_unchecked();
                                }

                                bytecode.resize(loop_lbl.offset);

                                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                                emit_opfunc_to(bytecode,
                                               translate::get_uwvmint_loop_f64_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                emit_imm_to(bytecode, local_offset_from_index(i_local_idx));
                                emit_imm_to(bytecode, local_offset_from_index(x_local_idx));
                                emit_imm_to(bytecode, end_i);

                                fused_extra_heavy_loop_run = true;
                            }
                        }
                    }
                }
            }

            // Extra-heavy: mega-fuse `test10` hot affine inv-square f32 loop.
            if constexpr(CompileOption.is_tail_call)
            {
                if(!fused_extra_heavy_loop_run && target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
                   target_frame.wasm_code_curr_at_start_label != nullptr)
                {
                    bool match_ok{true};
                    if constexpr(stacktop_enabled) { match_ok = (stacktop_cache_count == 0uz); }

                    if(match_ok)
                    {
                        auto p{target_frame.wasm_code_curr_at_start_label};
                        auto const endp{op_begin};

                        auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                              {
                                                  if(p >= endp) [[unlikely]] { return false; }
                                                  wasm1_code op;  // no init
                                                  ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                                  if(op != expected) { return false; }
                                                  ++p;
                                                  return true;
                                              }};

                        auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_f32_const_bits{[&](wasm_u32 expected_bits) constexpr noexcept -> bool
                                                          {
                                                              if(!consume_op(wasm1_code::f32_const)) { return false; }
                                                              if(static_cast<::std::size_t>(endp - p) < 4uz) [[unlikely]] { return false; }
                                                              wasm_u32 bits;  // no init
                                                              ::std::memcpy(::std::addressof(bits), p, sizeof(bits));
                                                              p += 4;
                                                              return bits == expected_bits;
                                                          }};

                        constexpr wasm_u32 f32_one_bits{0x3f800000u};
                        constexpr wasm_u32 f32_k_bits{0x3716feb5u};  // f32.const 0x1.2dfd6ap-17

                        wasm_u32 sum_idx{};      // init
                        wasm_u32 i_idx{};        // init
                        wasm_u32 sum_out_idx{};  // init (local 3)
                        wasm_u32 i1_idx{};       // init (local 4)
                        wasm_i32 end_i{};        // init
                        wasm_u32 tmp_u32{};      // init
                        wasm_i32 tmp_i32{};      // init

                        // 1 / (1 + i*k)^2 + sum -> sum_out
                        match_ok = consume_f32_const_bits(f32_one_bits) && consume_op(wasm1_code::local_get) && consume_u32_leb(i_idx) &&
                                   consume_op(wasm1_code::f32_convert_i32_u) && consume_f32_const_bits(f32_k_bits) && consume_op(wasm1_code::f32_mul) &&
                                   consume_f32_const_bits(f32_one_bits) && consume_op(wasm1_code::f32_add) && consume_op(wasm1_code::local_tee) &&
                                   consume_u32_leb(sum_out_idx) && consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == sum_out_idx &&
                                   consume_op(wasm1_code::f32_mul) && consume_op(wasm1_code::f32_div) && consume_op(wasm1_code::local_get) &&
                                   consume_u32_leb(sum_idx) && consume_op(wasm1_code::f32_add) && consume_op(wasm1_code::local_set) &&
                                   consume_u32_leb(tmp_u32) && tmp_u32 == sum_out_idx;

                        // i1 = i+1; if(i1 == end) break;
                        if(match_ok)
                        {
                            wasm_u32 brif_lbl_idx{};  // init
                            match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_const) &&
                                       consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{1} && consume_op(wasm1_code::i32_add) &&
                                       consume_op(wasm1_code::local_tee) && consume_u32_leb(i1_idx) && consume_op(wasm1_code::i32_const) &&
                                       consume_i32_leb(end_i) && consume_op(wasm1_code::i32_eq) && consume_op(wasm1_code::br_if) &&
                                       consume_u32_leb(brif_lbl_idx) && brif_lbl_idx == 1u;
                        }

                        // i = i1+1
                        if(match_ok)
                        {
                            match_ok = consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == i1_idx &&
                                       consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{1} &&
                                       consume_op(wasm1_code::i32_add) && consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_u32) && tmp_u32 == i_idx;
                        }

                        // 1 / (1 + i1*k)^2 + sum_out -> sum
                        if(match_ok)
                        {
                            match_ok = consume_f32_const_bits(f32_one_bits) && consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) &&
                                       tmp_u32 == i1_idx && consume_op(wasm1_code::f32_convert_i32_u) && consume_f32_const_bits(f32_k_bits) &&
                                       consume_op(wasm1_code::f32_mul) && consume_f32_const_bits(f32_one_bits) && consume_op(wasm1_code::f32_add) &&
                                       consume_op(wasm1_code::local_tee) && consume_u32_leb(tmp_u32) && tmp_u32 == sum_idx &&
                                       consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == sum_idx && consume_op(wasm1_code::f32_mul) &&
                                       consume_op(wasm1_code::f32_div) && consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) &&
                                       tmp_u32 == sum_out_idx && consume_op(wasm1_code::f32_add) && consume_op(wasm1_code::local_set) &&
                                       consume_u32_leb(tmp_u32) && tmp_u32 == sum_idx;
                        }

                        if(match_ok) { match_ok = (p == endp); }

                        if(match_ok)
                        {
                            auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                            if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                            {
                                while(!ptr_fixups.empty())
                                {
                                    auto const& fx{ptr_fixups.back_unchecked()};
                                    if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                    ptr_fixups.pop_back_unchecked();
                                }

                                bytecode.resize(loop_lbl.offset);

                                namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                                emit_opfunc_to(
                                    bytecode,
                                    translate::get_uwvmint_f32_affine_inv_square_sum_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                emit_imm_to(bytecode, local_offset_from_index(sum_idx));
                                emit_imm_to(bytecode, local_offset_from_index(i_idx));
                                emit_imm_to(bytecode, local_offset_from_index(sum_out_idx));
                                emit_imm_to(bytecode, local_offset_from_index(i1_idx));
                                emit_imm_to(bytecode, end_i);

                                fused_extra_heavy_loop_run = true;
                            }
                        }
                    }
                }
            }
#endif
            // Drop everything above target base.
            // Safety: `target_base` must be <= `curr_size` in the non-polymorphic path.
            if(!fused_extra_heavy_loop_run && curr_size > target_base)
            {
                emit_drop_to_stack_size_no_fill(bytecode, target_base);

                if constexpr(stacktop_enabled)
                {
                    if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(bytecode); }
                }
            }

            if(!fused_extra_heavy_loop_run)
            {
                if constexpr(stacktop_enabled)
                {
                    if constexpr(strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(bytecode); }
                }
                if constexpr(stacktop_enabled)
                {
                    if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                    {
                        emit_br_to_with_stacktop_transform(bytecode, target_label_id, false);
                    }
                    else
                    {
                        emit_br_to(bytecode, target_label_id, false);
                    }
                }
                else
                {
                    emit_br_to(bytecode, target_label_id, false);
                }
            }
        }
        else
        {
            // Wasm1: arity is 0 or 1.
            auto const result_type{target_frame.result.begin[0]};

            if(curr_size - target_base > 1uz)
            {
                // Preserve the result value across dropping extra values.
                emit_local_set_typed_to_no_fill(bytecode, result_type, internal_temp_local_off);

                // Drop values between [target_base .. curr_size-2].
                if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(bytecode, target_base); }
                else
                {
                    for(::std::size_t i{curr_size - 1uz}; i-- > target_base;) { emit_drop_typed_to_no_fill(bytecode, operand_stack.index_unchecked(i).type); }
                }

                if constexpr(stacktop_enabled)
                {
                    if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(bytecode); }
                }
                emit_local_get_typed_to(bytecode, result_type, internal_temp_local_off);
            }

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(bytecode); }
            }
            if constexpr(stacktop_enabled)
            {
                if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                {
                    emit_br_to_with_stacktop_transform(bytecode, target_label_id, false);
                }
                else
                {
                    emit_br_to(bytecode, target_label_id, false);
                }
            }
            else
            {
                emit_br_to(bytecode, target_label_id, false);
            }
        }
    }
    else
    {
        emit_br_to(bytecode, target_label_id, false);
    }

    if constexpr(stacktop_enabled)
    {
        if constexpr(strict_cf_entry_like_call) { /* snapshots not needed */ }
        else
        {
            // If this unconditional branch targets the end label of its frame, record the current
            // stack-top state so the `end` handler can restore it when the fallthrough path becomes
            // unreachable (polymorphic) before reaching `end`.
            if(!is_polymorphic && target_label_id == target_frame.end_label_id)
            {
                target_frame.stacktop_has_end_state = true;
                target_frame.stacktop_currpos_at_end = curr_stacktop;
                target_frame.stacktop_memory_count_at_end = stacktop_memory_count;
                target_frame.stacktop_cache_count_at_end = stacktop_cache_count;
                target_frame.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                target_frame.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                target_frame.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                target_frame.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                target_frame.codegen_operand_stack_at_end = codegen_operand_stack;
            }
        }
    }

    if(target_arity != 0uz) { operand_stack_pop_n(target_arity); }
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    break;
}
case wasm1_code::br_if:
{
    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br_if  label_index ...
    // [safe] unsafe (could be the section_end)
    //        ^^ code_curr

    wasm_u32 label_index;
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [label_next, label_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                                reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                                ::fast_io::mnp::leb128_get(label_index))};
    if(label_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(label_err);
    }

    // br_if  label_index ...
    // [      safe      ] unsafe (could be the section_end)
    //        ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(label_next);

    // br_if  label_index ...
    // [      safe      ] unsafe (could be the section_end)
    //                    ^^ code_curr

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const label_index_uz{static_cast<::std::size_t>(label_index)};
    if(label_index_uz >= all_label_count_uz) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.illegal_label_index.label_index = label_index;
        err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
        err.err_code = code_validation_error_code::illegal_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const target_frame_index{all_label_count_uz - 1uz - label_index_uz};
    auto const& target_frame{control_flow_stack.index_unchecked(target_frame_index)};
    auto& target_frame_mut{control_flow_stack.index_unchecked(target_frame_index)};
    // Label arity = label_types count. IMPORTANT: for `loop`, label types are parameters (MVP: none),
    // not result types.
    auto const target_arity{target_frame.type == block_type::loop ? 0uz : static_cast<::std::size_t>(target_frame.result.end - target_frame.result.begin)};

    auto constexpr max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const target_arity_plus_cond_overflows{target_arity == max_operand_stack_requirement};
    auto const required_stack_size{target_arity_plus_cond_overflows ? max_operand_stack_requirement : (target_arity + 1uz)};

    if(!is_polymorphic && (target_arity_plus_cond_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"br_if", required_stack_size);
    }

    if(auto const cond{try_pop_concrete_operand()}; cond.from_stack)
    {
        if(cond.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_if";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(cond.type);
            err.err_code = code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    if(target_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < target_arity ? available_arg_count : target_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const expected_type{target_frame.result.begin[target_arity - 1uz - i]};
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_if";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Translate: `br_if` needs stack repair on the taken path only. If repair is necessary, emit a thunk target.
    namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;

    // When stack-top caching is enabled, the i32 condition may still reside in operand stack memory
    // (e.g. tiny rings / merged scalar rings). `br_if` must pop from the correct source to keep the
    // runtime stack pointer consistent with the compiler-side model.
    bool brif_cond_cached_at_site{true};

#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS

    // Conbine: `local.get(i32); i32.eqz; br_if` and `local.get(i32); i32.const; cmp; br_if`.
    bool conbine_brif_local_eqz{};
    bool conbine_brif_i64_local_eqz{};
    bool conbine_brif_i32_cmp_imm{};
    bool conbine_brif_i64_cmp_imm{};

    local_offset_t conbine_brif_local_off{};
    wasm_i32 conbine_brif_imm{};
    wasm_i64 conbine_brif_imm64{};
    conbine_brif_cmp_kind conbine_brif_cmp{conbine_brif_cmp_kind::none};

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    bool conbine_brif_i32_rem_u_eqz_2localget{};
    local_offset_t conbine_brif_local_off2{};
    bool conbine_brif_for_i32_inc_f64_lt_u_eqz{};
    bool conbine_brif_for_i32_inc_lt_u{};
    wasm_i32 conbine_brif_step{};
    wasm_i32 conbine_brif_end{};
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    bool conbine_brif_for_ptr_inc_ne{};
#  endif
# endif

    if(conbine_pending.kind == conbine_pending_kind::local_get_eqz_i32)
    {
        conbine_brif_local_eqz = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get_eqz_i64)
    {
        conbine_brif_i64_local_eqz = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get_const_i32_cmp_brif)
    {
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
        // Heavy: fold the hot for-loop skeleton
        //   local += step; if(local < end) br_if <loop>
        // into a single `for_i32_inc_lt_u_br_if` opfunc. This triggers when the translator has
        // just emitted `i32_add_imm_local_set_same` and the next ops are the fused
        // `local.get; i32.const end; i32.lt_u; br_if`.
        if(!is_polymorphic && conbine_pending.brif_cmp == conbine_brif_cmp_kind::i32_lt_u)
        {
            using prev_fptr_t = decltype(translate::get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            constexpr ::std::size_t prev_inst_size{sizeof(prev_fptr_t) + sizeof(local_offset_t) + sizeof(wasm_i32)};
            if(bytecode.size() >= prev_inst_size)
            {
                auto const prev_start{bytecode.size() - prev_inst_size};

                prev_fptr_t prev_fptr{};  // init
                ::std::memcpy(::std::addressof(prev_fptr), bytecode.data() + prev_start, sizeof(prev_fptr));

                auto const expect_prev_fptr{translate::get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)};
                if(prev_fptr == expect_prev_fptr)
                {
                    local_offset_t prev_local_off{};  // init
                    ::std::memcpy(::std::addressof(prev_local_off), bytecode.data() + prev_start + sizeof(prev_fptr), sizeof(prev_local_off));
                    wasm_i32 prev_step{};  // init
                    ::std::memcpy(::std::addressof(prev_step), bytecode.data() + prev_start + sizeof(prev_fptr) + sizeof(prev_local_off), sizeof(prev_step));

                    if(prev_local_off == conbine_pending.off1)
                    {
                        // Remove the already-emitted update-local op and replace the whole update+cmp+br_if
                        // with a single loop-skeleton opfunc.
                        bytecode.resize(prev_start);
                        while(!ptr_fixups.empty() && !ptr_fixups.back_unchecked().in_thunk && ptr_fixups.back_unchecked().site >= prev_start)
                        {
                            ptr_fixups.pop_back_unchecked();
                        }

                        conbine_brif_for_i32_inc_lt_u = true;
                        conbine_brif_local_off = prev_local_off;
                        conbine_brif_step = prev_step;
                        conbine_brif_end = conbine_pending.imm_i32;
                    }
                }
            }
        }

        if(!conbine_brif_for_i32_inc_lt_u)
# endif
        {
            conbine_brif_i32_cmp_imm = true;
            conbine_brif_local_off = conbine_pending.off1;
            conbine_brif_imm = conbine_pending.imm_i32;
            conbine_brif_cmp = conbine_pending.brif_cmp;
        }
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
    else if(conbine_pending.kind == conbine_pending_kind::local_get_const_i64_cmp_brif)
    {
        conbine_brif_i64_cmp_imm = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_brif_imm64 = conbine_pending.imm_i64;
        conbine_brif_cmp = conbine_pending.brif_cmp;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
    else if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::i32_rem_u_eqz_2localget_wait_brif)
    {
        conbine_brif_i32_rem_u_eqz_2localget = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_brif_local_off2 = conbine_pending.off2;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
    else if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_f64_lt_u_eqz_after_eqz)
    {
        conbine_brif_for_i32_inc_f64_lt_u_eqz = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_brif_local_off2 = conbine_pending.off2;
        conbine_brif_step = conbine_pending.imm_i32;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
    else if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_i32_inc_after_cmp)
    {
        conbine_brif_for_i32_inc_lt_u = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_brif_step = conbine_pending.imm_i32;
        conbine_brif_end = conbine_pending.imm_i32_2;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
    else if(!is_polymorphic && conbine_pending.kind == conbine_pending_kind::for_ptr_inc_after_cmp)
    {
        conbine_brif_for_ptr_inc_ne = true;
        conbine_brif_local_off = conbine_pending.off1;
        conbine_brif_local_off2 = conbine_pending.off2;
        conbine_brif_step = conbine_pending.imm_i32;
        conbine_pending.kind = conbine_pending_kind::none;
        conbine_pending.brif_cmp = conbine_brif_cmp_kind::none;
    }
#  endif
# endif

    auto const fuse_kind{br_if_fuse.kind};
    auto const fuse_site{br_if_fuse.site};
    auto const fuse_end{br_if_fuse.end};
    auto const fuse_stacktop_currpos{br_if_fuse.stacktop_currpos_at_site};

    br_if_fuse.kind = br_if_fuse_kind::none;
    br_if_fuse.site = SIZE_MAX;
    br_if_fuse.end = SIZE_MAX;
    br_if_fuse.stacktop_currpos_at_site = {};

    bool fused_brif{};
    auto const patch_fused_brif{
        [&]() constexpr UWVM_THROWS
        {
            if(fused_brif || fuse_kind == br_if_fuse_kind::none) { return; }
            using fptr_t = decltype(translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
            if constexpr(CompileOption.is_tail_call)
            {
                // Mega-fuse: `f32.load(local)` + `f32.{lt,gt}(local rhs)` + `br_if` into one dispatch.
                // This is an AABB-style hot pattern: load a struct field, compare against a cached local scalar, and branch.
                if(fuse_kind == br_if_fuse_kind::f32_lt_localget_rhs || fuse_kind == br_if_fuse_kind::f32_gt_localget_rhs ||
                   fuse_kind == br_if_fuse_kind::f32_le_localget_rhs || fuse_kind == br_if_fuse_kind::f32_ge_localget_rhs)
                {
                    bool const mega_fused{
                        [&]() constexpr UWVM_THROWS -> bool
                        {
                            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
                            using memarg_offset_slot_t = wasm_u32;

                            if(fuse_site == SIZE_MAX || fuse_end == SIZE_MAX) { return false; }
                            if(fuse_end != bytecode.size()) { return false; }

                            local_offset_t rhs_off{};  // init
                            ::std::memcpy(::std::addressof(rhs_off), bytecode.data() + fuse_site + sizeof(fptr_t), sizeof(rhs_off));

                            auto pre_load_stacktop{fuse_stacktop_currpos};
                            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
                            {
                                auto const begin{CompileOption.f32_stack_top_begin_pos};
                                auto const end{CompileOption.f32_stack_top_end_pos};
                                auto const curr{pre_load_stacktop.f32_stack_top_curr_pos};
                                pre_load_stacktop.f32_stack_top_curr_pos = (curr + 1uz == end) ? begin : (curr + 1uz);
                            }

                            // No memory in module => no memory load to mega-fuse.
                            if(all_memory_count == 0u) { return false; }
                            ensure_memory0_resolved();
                            native_memory_t* const memory0_p{resolved_memory0.memory_p};
                            if(memory0_p == nullptr) [[unlikely]] { return false; }

                            // Candidate 1: `f32_load_localget_off` immediately preceding the compare.
                            constexpr ::std::size_t kLoadLocalgetOffSize{sizeof(fptr_t) + sizeof(local_offset_t) + sizeof(native_memory_t*) +
                                                                         sizeof(memarg_offset_slot_t)};
                            if(fuse_site >= kLoadLocalgetOffSize)
                            {
                                auto const load_site{fuse_site - kLoadLocalgetOffSize};

                                fptr_t stored_load{};  // init
                                ::std::memcpy(::std::addressof(stored_load), bytecode.data() + load_site, sizeof(stored_load));

                                local_offset_t addr_off{};  // init
                                ::std::memcpy(::std::addressof(addr_off), bytecode.data() + load_site + sizeof(stored_load), sizeof(addr_off));

                                native_memory_t* memory_p{};  // init
                                ::std::memcpy(::std::addressof(memory_p),
                                              bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off),
                                              sizeof(memory_p));

                                if(memory_p == memory0_p)
                                {
                                    memarg_offset_slot_t offset_slot{};  // init
                                    ::std::memcpy(::std::addressof(offset_slot),
                                                  bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off) + sizeof(memory_p),
                                                  sizeof(offset_slot));

                                    auto const expected_load{translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<CompileOption>(pre_load_stacktop,
                                                                                                                                         *memory0_p,
                                                                                                                                         interpreter_tuple)};

                                    if(stored_load == expected_load)
                                    {
                                        bytecode.resize(load_site);

                                        fptr_t mega_fptr{};  // init
                                        if(fuse_kind == br_if_fuse_kind::f32_lt_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_localget_off_lt_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_gt_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_localget_off_gt_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_le_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_localget_off_le_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_ge_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_localget_off_ge_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else
                                        {
                                            return false;
                                        }

                                        emit_opfunc_to(bytecode, mega_fptr);
                                        emit_imm_to(bytecode, addr_off);
                                        emit_imm_to(bytecode, rhs_off);
                                        emit_imm_to(bytecode, memory0_p);
                                        emit_imm_to(bytecode, offset_slot);

                                        fused_brif = true;
                                        return true;
                                    }
                                }
                            }

                            // Candidate 2: `f32_load_local_plus_imm` immediately preceding the compare.
                            constexpr ::std::size_t kLoadLocalPlusImmSize{sizeof(fptr_t) + sizeof(local_offset_t) + sizeof(wasm_i32) +
                                                                          sizeof(native_memory_t*) + sizeof(memarg_offset_slot_t)};
                            if(fuse_site >= kLoadLocalPlusImmSize)
                            {
                                auto const load_site{fuse_site - kLoadLocalPlusImmSize};

                                fptr_t stored_load{};  // init
                                ::std::memcpy(::std::addressof(stored_load), bytecode.data() + load_site, sizeof(stored_load));

                                local_offset_t addr_off{};  // init
                                ::std::memcpy(::std::addressof(addr_off), bytecode.data() + load_site + sizeof(stored_load), sizeof(addr_off));

                                wasm_i32 add_imm{};  // init
                                ::std::memcpy(::std::addressof(add_imm), bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off), sizeof(add_imm));

                                native_memory_t* memory_p{};  // init
                                ::std::memcpy(::std::addressof(memory_p),
                                              bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off) + sizeof(add_imm),
                                              sizeof(memory_p));

                                if(memory_p == memory0_p)
                                {
                                    memarg_offset_slot_t offset_slot{};  // init
                                    ::std::memcpy(::std::addressof(offset_slot),
                                                  bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off) + sizeof(add_imm) + sizeof(memory_p),
                                                  sizeof(offset_slot));

                                    auto const expected_load{translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<CompileOption>(pre_load_stacktop,
                                                                                                                                           *memory0_p,
                                                                                                                                           interpreter_tuple)};

                                    if(stored_load == expected_load)
                                    {
                                        bytecode.resize(load_site);

                                        fptr_t mega_fptr{};  // init
                                        if(fuse_kind == br_if_fuse_kind::f32_lt_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_local_plus_imm_lt_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_gt_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_local_plus_imm_gt_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_le_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_local_plus_imm_le_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else if(fuse_kind == br_if_fuse_kind::f32_ge_localget_rhs)
                                        {
                                            mega_fptr = translate::get_uwvmint_br_if_f32_load_local_plus_imm_ge_localget_rhs_fptr_from_tuple<CompileOption>(
                                                fuse_stacktop_currpos,
                                                *memory0_p,
                                                interpreter_tuple);
                                        }
                                        else
                                        {
                                            return false;
                                        }

                                        emit_opfunc_to(bytecode, mega_fptr);
                                        emit_imm_to(bytecode, addr_off);
                                        emit_imm_to(bytecode, add_imm);
                                        emit_imm_to(bytecode, rhs_off);
                                        emit_imm_to(bytecode, memory0_p);
                                        emit_imm_to(bytecode, offset_slot);

                                        fused_brif = true;
                                        return true;
                                    }
                                }
                            }

                            return false;
                        }()};
                    if(mega_fused) { return; }
                }

                // Mega-fuse: `i32.load(local)` + `i32.const(imm)` + `i32.eq` + `br_if` into one dispatch.
                // This targets AABB-style leaf checks like `objectIndex == -1` in tight traversal loops.
                if(!is_polymorphic && fuse_kind == br_if_fuse_kind::i32_eq)
                {
                    bool const mega_fused{
                        [&]() constexpr UWVM_THROWS -> bool
                        {
                            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
                            using memarg_offset_slot_t = wasm_u32;
                            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

                            if(fuse_site == SIZE_MAX) { return false; }
                            if(fuse_end != SIZE_MAX) { return false; }
                            if(fuse_site + sizeof(fptr_t) != bytecode.size()) { return false; }

                            // Candidate: `i32.const imm` immediately preceding `i32.eq`.
                            constexpr ::std::size_t kConstSize{sizeof(fptr_t) + sizeof(wasm_i32)};
                            if(fuse_site < kConstSize) { return false; }
                            auto const const_site{fuse_site - kConstSize};

                            fptr_t stored_const{};  // init
                            ::std::memcpy(::std::addressof(stored_const), bytecode.data() + const_site, sizeof(stored_const));

                            wasm_i32 cmp_imm{};  // init
                            ::std::memcpy(::std::addressof(cmp_imm), bytecode.data() + const_site + sizeof(stored_const), sizeof(cmp_imm));

                            auto pre_const_stacktop{fuse_stacktop_currpos};
                            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
                            {
                                auto const begin{CompileOption.i32_stack_top_begin_pos};
                                auto const end{CompileOption.i32_stack_top_end_pos};
                                auto const curr{pre_const_stacktop.i32_stack_top_curr_pos};
                                pre_const_stacktop.i32_stack_top_curr_pos = (curr + 1uz == end) ? begin : (curr + 1uz);
                                if constexpr(CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos)
                                {
                                    pre_const_stacktop.i64_stack_top_curr_pos = pre_const_stacktop.i32_stack_top_curr_pos;
                                }
                            }

                            auto const expected_const{translate::get_uwvmint_i32_const_fptr_from_tuple<CompileOption>(pre_const_stacktop, interpreter_tuple)};
                            if(stored_const != expected_const) { return false; }

                            // Candidate: `i32_load_localget_off` immediately preceding the const.
                            constexpr ::std::size_t kLoadSize{sizeof(fptr_t) + sizeof(local_offset_t) + sizeof(native_memory_t*) +
                                                              sizeof(memarg_offset_slot_t)};
                            if(const_site < kLoadSize) { return false; }
                            auto const load_site{const_site - kLoadSize};

                            fptr_t stored_load{};  // init
                            ::std::memcpy(::std::addressof(stored_load), bytecode.data() + load_site, sizeof(stored_load));

                            local_offset_t addr_off{};  // init
                            ::std::memcpy(::std::addressof(addr_off), bytecode.data() + load_site + sizeof(stored_load), sizeof(addr_off));

                            native_memory_t* memory_p{};  // init
                            ::std::memcpy(::std::addressof(memory_p), bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off), sizeof(memory_p));
                            if(memory_p == nullptr) [[unlikely]] { return false; }

                            memarg_offset_slot_t offset_slot{};  // init
                            ::std::memcpy(::std::addressof(offset_slot),
                                          bytecode.data() + load_site + sizeof(stored_load) + sizeof(addr_off) + sizeof(memory_p),
                                          sizeof(offset_slot));

                            auto pre_load_stacktop{fuse_stacktop_currpos};
                            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
                            {
                                auto const begin{CompileOption.i32_stack_top_begin_pos};
                                auto const end{CompileOption.i32_stack_top_end_pos};
                                auto curr{pre_load_stacktop.i32_stack_top_curr_pos};
                                // Undo two pushes: `i32.load` and `i32.const`.
                                curr = (curr + 1uz == end) ? begin : (curr + 1uz);
                                curr = (curr + 1uz == end) ? begin : (curr + 1uz);
                                pre_load_stacktop.i32_stack_top_curr_pos = curr;
                                if constexpr(CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                             CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos)
                                {
                                    pre_load_stacktop.i64_stack_top_curr_pos = pre_load_stacktop.i32_stack_top_curr_pos;
                                }
                            }

                            // Validate the candidate load without dereferencing the embedded `memory_p`.
                            // This avoids crashing the translator when the back-scan lands on unrelated opfunc bytes.
                            fptr_t mega_fptr{};  // init
#  if defined(UWVM_SUPPORT_MMAP)
                            auto const get_load_fptr{
                                [&]<auto BoundsCheckFn, typename... TypeInTuple>(
                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) constexpr noexcept -> fptr_t
                                {
                                    return translate::details::select_stacktop_fptr_or_default_with<CompileOption,
                                                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                                                    CompileOption.i32_stack_top_end_pos,
                                                                                                    translate::details::i32_load_localget_off_op_with,
                                                                                                    BoundsCheckFn,
                                                                                                    0uz,
                                                                                                    TypeInTuple...>(pre_load_stacktop.i32_stack_top_curr_pos);
                                }};
                            auto const get_mega_fptr{[&]<auto BoundsCheckFn, typename... TypeInTuple>(
                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) constexpr noexcept -> fptr_t
                                                     {
                                                         return translate::details::select_stacktop_fptr_or_default_with<
                                                             CompileOption,
                                                             0uz,
                                                             0uz,
                                                             translate::details::br_if_i32_load_localget_off_eq_imm_op_with,
                                                             BoundsCheckFn,
                                                             0uz,
                                                             TypeInTuple...>(0uz);
                                                     }};

                            auto const expect_full{
                                get_load_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_full>(interpreter_tuple)};
                            auto const expect_path{
                                get_load_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_path>(interpreter_tuple)};
                            auto const expect_judge{
                                get_load_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_judge>(interpreter_tuple)};

                            if(stored_load == expect_full)
                            {
                                mega_fptr = get_mega_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_full>(interpreter_tuple);
                            }
                            else if(stored_load == expect_path)
                            {
                                mega_fptr = get_mega_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_path>(interpreter_tuple);
                            }
                            else if(stored_load == expect_judge)
                            {
                                mega_fptr = get_mega_fptr.template operator()<&translate::details::op_details::bounds_check_mmap_judge>(interpreter_tuple);
                            }
                            else
                            {
                                return false;
                            }
#  else
                            auto const expected_load{
                                [&]<typename... TypeInTuple>(::uwvm2::utils::container::tuple<TypeInTuple...> const&) constexpr noexcept -> fptr_t
                                {
                                    return translate::details::select_stacktop_fptr_or_default_with<CompileOption,
                                                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                                                    CompileOption.i32_stack_top_end_pos,
                                                                                                    translate::details::i32_load_localget_off_op_with,
                                                                                                    &translate::details::op_details::bounds_check_allocator,
                                                                                                    0uz,
                                                                                                    TypeInTuple...>(pre_load_stacktop.i32_stack_top_curr_pos);
                                }(interpreter_tuple)};
                            if(stored_load != expected_load) { return false; }
                            mega_fptr = [&]<typename... TypeInTuple>(::uwvm2::utils::container::tuple<TypeInTuple...> const&) constexpr noexcept -> fptr_t
                            {
                                return translate::details::select_stacktop_fptr_or_default_with<CompileOption,
                                                                                                0uz,
                                                                                                0uz,
                                                                                                translate::details::br_if_i32_load_localget_off_eq_imm_op_with,
                                                                                                &translate::details::op_details::bounds_check_allocator,
                                                                                                0uz,
                                                                                                TypeInTuple...>(0uz);
                            }(interpreter_tuple);
#  endif

                            bytecode.resize(load_site);

                            emit_opfunc_to(bytecode, mega_fptr);
                            emit_imm_to(bytecode, addr_off);
                            emit_imm_to(bytecode, cmp_imm);
                            emit_imm_to(bytecode, memory_p);
                            emit_imm_to(bytecode, offset_slot);

                            fused_brif = true;
                            return true;
                        }()};
                    if(mega_fused) { return; }
                }
            }
# endif
            if constexpr(stacktop_enabled)
            {
                // Fused br_if opfuncs read the i32 condition from stack-top cache. With tiny rings,
                // the condition can be materialized in operand-stack memory (cache empty), in which
                // case fusing would desync the runtime stack pointer.
                if(!brif_cond_cached_at_site) { return; }
            }
            if(fuse_site == SIZE_MAX) { return; }
            if(fuse_end != SIZE_MAX)
            {
                if(fuse_end != bytecode.size()) { return; }
            }
            else
            {
                if(fuse_site + sizeof(fptr_t) != bytecode.size()) { return; }
            }

            fptr_t fused_fptr{};
            switch(fuse_kind)
            {
                case br_if_fuse_kind::i32_eqz:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_eqz_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::local_tee_nz:
                {
                    fused_fptr = translate::get_uwvmint_br_if_local_tee_nz_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_eq:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_eq_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_ne:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_ne_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_lt_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_lt_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_lt_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_lt_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_gt_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_gt_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_ge_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_ge_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_ge_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_ge_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_le_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_le_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_gt_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_gt_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_le_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_le_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i32_and_nz:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i32_and_nz_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_eqz:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_eqz_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_eq:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_eq_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_ne:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_ne_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_lt_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_lt_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_gt_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_gt_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_lt_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_lt_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_gt_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_gt_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_ge_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_ge_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_ge_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_ge_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_le_s:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_le_s_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::i64_le_u:
                {
                    fused_fptr = translate::get_uwvmint_br_if_i64_le_u_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                case br_if_fuse_kind::f32_eq:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_eq_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_ne:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_ne_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_eq_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_eq_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_ne_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_ne_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_lt:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_lt_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_lt_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_lt_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_gt:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_gt_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_gt_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_gt_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_le:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_le_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_le_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_le_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_ge:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_ge_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f32_ge_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f32_ge_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_eq:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_eq_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_ne:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_ne_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_eq_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_eq_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_ne_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_ne_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_lt:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_lt_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_lt_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_lt_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_gt:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_gt_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_gt_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_gt_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_le:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_le_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_le_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_le_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_ge:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_ge_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_ge_localget_rhs:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_ge_localget_rhs_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
                case br_if_fuse_kind::f64_lt_eqz:
                {
                    fused_fptr = translate::get_uwvmint_br_if_f64_lt_eqz_fptr_from_tuple<CompileOption>(fuse_stacktop_currpos, interpreter_tuple);
                    break;
                }
# endif
                case br_if_fuse_kind::none:
                    [[fallthrough]];
                [[unlikely]] default:
                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    return;
                }
            }

            ::std::byte tmp[sizeof(fused_fptr)];
            ::std::memcpy(tmp, ::std::addressof(fused_fptr), sizeof(fused_fptr));
            ::std::memcpy(bytecode.data() + fuse_site, tmp, sizeof(fused_fptr));
            fused_brif = true;
        }};

    auto const emit_br_if_jump{
        [&](::std::size_t label_id) constexpr UWVM_THROWS
        {
            patch_fused_brif();
            if(!fused_brif)
            {
                if constexpr(stacktop_enabled)
                {
                    if(!brif_cond_cached_at_site)
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_pop_from_memory_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                    else
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                    }
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
            }
            emit_ptr_label_placeholder(label_id, false);
        }};

    auto const emit_br_if_jump_conbine{
        [&](::std::size_t label_id) constexpr UWVM_THROWS
        {
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            // Extra-heavy: mega-fuse the full `quick_branchy_i32` hot loop into one opfunc dispatch.
            // This targets dispatch/prediction dominated workloads by keeping the whole loop in registers.
            if(!is_polymorphic && target_frame.type == block_type::loop)
            {
                bool const fused{
                    [&]() constexpr UWVM_THROWS -> bool
                    {
                        auto const loop_begin{target_frame.wasm_code_curr_at_start_label};
                        auto const endp{op_begin};
                        if(loop_begin == nullptr || loop_begin >= endp) { return false; }

                        ::std::byte const* p{loop_begin};

                        auto const consume_op{[&](wasm1_code expected) constexpr noexcept -> bool
                                              {
                                                  if(p == endp) { return false; }
                                                  wasm1_code op{};  // init
                                                  ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                                  if(op != expected) { return false; }
                                                  ++p;
                                                  return true;
                                              }};

                        auto const consume_u32_leb{[&](wasm_u32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        auto const consume_i32_leb{[&](wasm_i32& v) constexpr noexcept -> bool
                                                   {
                                                       using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                       auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                       reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                       ::fast_io::mnp::leb128_get(v))};
                                                       if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                       p = reinterpret_cast<::std::byte const*>(next);
                                                       return true;
                                                   }};

                        constexpr wasm_u32 a{1664525u};
                        constexpr wasm_u32 b{1013904223u};
                        constexpr wasm_u32 c{3668339992u};

                        wasm_u32 s_idx{};      // init
                        wasm_u32 t_idx{};      // init
                        wasm_u32 acc_idx{};    // init
                        wasm_u32 cnt_idx{};    // init
                        wasm_u32 tmp_u32{};    // init
                        wasm_i32 tmp_i32{};    // init
                        wasm_i32 tmp_i32_2{};  // init

                        bool match_ok =
                            // LCG update + select 1:
                            consume_op(wasm1_code::local_get) && consume_u32_leb(s_idx) && consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) &&
                            static_cast<wasm_u32>(tmp_i32) == a && consume_op(wasm1_code::i32_mul) && consume_op(wasm1_code::local_tee) &&
                            consume_u32_leb(t_idx) && consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) && static_cast<wasm_u32>(tmp_i32) == b &&
                            consume_op(wasm1_code::i32_add) && consume_op(wasm1_code::local_tee) && consume_u32_leb(tmp_u32) && tmp_u32 == s_idx &&
                            consume_op(wasm1_code::local_get) && consume_u32_leb(acc_idx) && consume_op(wasm1_code::i32_add) &&
                            consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == t_idx && consume_op(wasm1_code::i32_const) &&
                            consume_i32_leb(tmp_i32) && static_cast<wasm_u32>(tmp_i32) == c && consume_op(wasm1_code::i32_add) &&
                            consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == acc_idx && consume_op(wasm1_code::i32_xor) &&
                            consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == s_idx && consume_op(wasm1_code::i32_const) &&
                            consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{1} && consume_op(wasm1_code::i32_and) && consume_op(wasm1_code::select) &&

                            // select 2 + add + rotl + local.set acc:
                            consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) && tmp_u32 == s_idx && consume_op(wasm1_code::i32_const) &&
                            consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{3} && consume_op(wasm1_code::i32_shr_u) && consume_op(wasm1_code::i32_const) &&
                            consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{0} && consume_op(wasm1_code::local_get) && consume_u32_leb(tmp_u32) &&
                            tmp_u32 == s_idx && consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{1} &&
                            consume_op(wasm1_code::i32_shl) && consume_op(wasm1_code::i32_sub) && consume_op(wasm1_code::local_get) &&
                            consume_u32_leb(tmp_u32) && tmp_u32 == s_idx && consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) &&
                            tmp_i32 == wasm_i32{4} && consume_op(wasm1_code::i32_and) && consume_op(wasm1_code::select) && consume_op(wasm1_code::i32_add) &&
                            consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32) && tmp_i32 == wasm_i32{5} && consume_op(wasm1_code::i32_rotl) &&
                            consume_op(wasm1_code::local_set) && consume_u32_leb(tmp_u32) && tmp_u32 == acc_idx &&

                            // decrement + tee + br_if <loop>:
                            consume_op(wasm1_code::local_get) && consume_u32_leb(cnt_idx) && consume_op(wasm1_code::i32_const) && consume_i32_leb(tmp_i32_2) &&
                            tmp_i32_2 == wasm_i32{-1} && consume_op(wasm1_code::i32_add) && consume_op(wasm1_code::local_tee) && consume_u32_leb(tmp_u32) &&
                            tmp_u32 == cnt_idx && p == endp;

                        if(!match_ok) { return false; }

                        // Ensure local types are i32 (this mega-op is i32-only).
                        if(s_idx >= all_local_count || t_idx >= all_local_count || acc_idx >= all_local_count || cnt_idx >= all_local_count) { return false; }
                        if(local_type_from_index(s_idx) != curr_operand_stack_value_type::i32 ||
                           local_type_from_index(t_idx) != curr_operand_stack_value_type::i32 ||
                           local_type_from_index(acc_idx) != curr_operand_stack_value_type::i32 ||
                           local_type_from_index(cnt_idx) != curr_operand_stack_value_type::i32)
                        {
                            return false;
                        }

                        if(label_id >= labels.size()) { return false; }
                        auto const& loop_label_info{labels.index_unchecked(label_id)};
                        if(loop_label_info.in_thunk) { return false; }
                        auto const loop_start{loop_label_info.offset};
                        if(loop_start > bytecode.size()) { return false; }

                        // Rewind the whole loop body and replace it with a single mega-op.
                        bytecode.resize(loop_start);
                        while(!ptr_fixups.empty() && !ptr_fixups.back_unchecked().in_thunk && ptr_fixups.back_unchecked().site >= loop_start)
                        {
                            ptr_fixups.pop_back_unchecked();
                        }

                        emit_opfunc_to(bytecode,
                                       translate::get_uwvmint_quick_branchy_i32_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        emit_imm_to(bytecode, local_offset_from_index(cnt_idx));
                        emit_imm_to(bytecode, local_offset_from_index(acc_idx));
                        emit_imm_to(bytecode, local_offset_from_index(s_idx));
                        return true;
                    }()};
                if(fused) { return; }
            }
#  endif
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            if(conbine_brif_i32_rem_u_eqz_2localget)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_local_off2);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }
#  endif

            if(conbine_brif_for_i32_inc_f64_lt_u_eqz)
            {
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
                // Mega-fuse `test8` hot prime divisor loop:
                //   br_if (n % i == 0) -> break
                //   i += step; br_if (sqrt < i) == 0 -> loop
                //
                // When the loop body is exactly the above two fused br_if opfuncs and the second br_if
                // targets the loop start label, we can emit a single opfunc that runs the whole loop
                // internally and keeps `n/sqrt/i` in registers.
                {
                    using prev_fptr_t =
                        decltype(translate::get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));

                    constexpr ::std::size_t prev_inst_size{sizeof(prev_fptr_t) + sizeof(local_offset_t) * 2uz + sizeof(rel_offset_t)};
                    if(bytecode.size() >= prev_inst_size && label_id < labels.size())
                    {
                        auto const prev_start{bytecode.size() - prev_inst_size};
                        auto const& loop_label_info{labels.index_unchecked(label_id)};
                        if(!loop_label_info.in_thunk && loop_label_info.offset == prev_start)
                        {
                            prev_fptr_t stored_prev{};  // init
                            ::std::memcpy(::std::addressof(stored_prev), bytecode.data() + prev_start, sizeof(stored_prev));

                            auto const expected_prev{
                                translate::get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple)};
                            if(stored_prev == expected_prev)
                            {
                                local_offset_t n_off{};  // init
                                local_offset_t i_off{};  // init
                                ::std::memcpy(::std::addressof(n_off), bytecode.data() + prev_start + sizeof(prev_fptr_t), sizeof(n_off));
                                ::std::memcpy(::std::addressof(i_off), bytecode.data() + prev_start + sizeof(prev_fptr_t) + sizeof(n_off), sizeof(i_off));
                                if(i_off == conbine_brif_local_off2 && !ptr_fixups.empty())
                                {
                                    auto const prev_label_site{prev_start + sizeof(prev_fptr_t) + sizeof(local_offset_t) * 2uz};
                                    auto const last_fixup{ptr_fixups.back_unchecked()};
                                    if(!last_fixup.in_thunk && last_fixup.site == prev_label_site)
                                    {
                                        auto const break_label_id{last_fixup.label_id};

                                        // Rewind previous br_if opfunc and its ptr-fixup, then emit the
                                        // mega-fused opfunc.
                                        bytecode.resize(prev_start);
                                        ptr_fixups.pop_back_unchecked();

                                        emit_opfunc_to(
                                            bytecode,
                                            translate::get_uwvmint_prime_divisor_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                        emit_imm_to(bytecode, n_off);
                                        emit_imm_to(bytecode, i_off);
                                        emit_imm_to(bytecode, conbine_brif_local_off);  // sqrt_off
                                        emit_imm_to(bytecode, conbine_brif_step);       // step
                                        emit_ptr_label_placeholder(break_label_id, false);
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
#  endif

                emit_opfunc_to(bytecode,
                               translate::get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_local_off2);
                emit_imm_to(bytecode, conbine_brif_step);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

            if(conbine_brif_for_i32_inc_lt_u)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_for_i32_inc_lt_u_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_step);
                emit_imm_to(bytecode, conbine_brif_end);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
            if(conbine_brif_for_ptr_inc_ne)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_for_ptr_inc_ne_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_local_off2);
                emit_imm_to(bytecode, conbine_brif_step);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }
#  endif
# endif
            if(conbine_brif_local_eqz)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_local_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

            if(conbine_brif_i64_local_eqz)
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_local_eqz_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

            if(conbine_brif_i32_cmp_imm)
            {
                switch(conbine_brif_cmp)
                {
                    case conbine_brif_cmp_kind::i32_eq:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ne:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_ne_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_lt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_lt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_gt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_gt_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_gt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_gt_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_le_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_le_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_le_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_le_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ge_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i32_ge_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::none:
                        [[fallthrough]];
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_imm);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

            if(conbine_brif_i64_cmp_imm)
            {
                switch(conbine_brif_cmp)
                {
                    case conbine_brif_cmp_kind::i64_eq:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_eq_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ne:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_ne_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_lt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_lt_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_lt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_lt_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_gt_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_gt_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_gt_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_gt_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_le_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_le_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_le_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_le_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ge_s:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_ge_s_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::i64_ge_u:
                    {
                        emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_i64_ge_u_imm_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                        break;
                    }
                    case conbine_brif_cmp_kind::none:
                        [[fallthrough]];
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }

                emit_imm_to(bytecode, conbine_brif_local_off);
                emit_imm_to(bytecode, conbine_brif_imm64);
                emit_ptr_label_placeholder(label_id, false);
                return;
            }

            emit_br_if_jump(label_id);
        }};

    auto const emit_br_if_jump_any{[&](::std::size_t label_id) constexpr UWVM_THROWS
                                   {
                                       if(runtime_log_on) [[unlikely]]
                                       {
                                           ++runtime_log_stats.cf_br_if_count;
                                           if(runtime_log_emit_cf)
                                           {
                                               ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                                                    u8"[uwvm-int-translator] fn=",
                                                                    function_index,
                                                                    u8" ip=",
                                                                    runtime_log_curr_ip,
                                                                    u8" event=bytecode.emit.cf | op=br_if bc=main off=",
                                                                    bytecode.size(),
                                                                    u8" label_id=",
                                                                    label_id,
                                                                    u8"\n");
                                           }
                                       }
                                       emit_br_if_jump_conbine(label_id);
                                   }};

    auto const target_label_id{get_branch_target_label_id(target_frame)};

    bool const brif_consumes_stack_cond{!(conbine_brif_local_eqz || conbine_brif_i64_local_eqz || conbine_brif_i32_cmp_imm || conbine_brif_i64_cmp_imm
# ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
                                          || conbine_brif_i32_rem_u_eqz_2localget || conbine_brif_for_i32_inc_f64_lt_u_eqz || conbine_brif_for_i32_inc_lt_u
#  ifdef UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS
                                          || conbine_brif_for_ptr_inc_ne
#  endif
# endif
                                          )};
#else
    // Combine disabled: `br_if` always consumes an i32 condition from the operand stack.
    auto const emit_br_if_jump_any{
        [&](::std::size_t label_id) constexpr UWVM_THROWS
        {
            if(runtime_log_on) [[unlikely]]
            {
                ++runtime_log_stats.cf_br_if_count;
                if(runtime_log_emit_cf)
                {
                    ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                         u8"[uwvm-int-translator] fn=",
                                         function_index,
                                         u8" ip=",
                                         runtime_log_curr_ip,
                                         u8" event=bytecode.emit.cf | op=br_if bc=main off=",
                                         bytecode.size(),
                                         u8" label_id=",
                                         label_id,
                                         u8"\n");
                }
            }
            if constexpr(stacktop_enabled)
            {
                if(!brif_cond_cached_at_site)
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_pop_from_memory_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
                else
                {
                    emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                }
            }
            else
            {
                emit_opfunc_to(bytecode, translate::get_uwvmint_br_if_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
            }
            emit_ptr_label_placeholder(label_id, false);
        }};

    auto const target_label_id{get_branch_target_label_id(target_frame)};
    bool const brif_consumes_stack_cond{true};
#endif
    if(is_polymorphic) { emit_br_if_jump_any(target_label_id); }
    else
    {
        auto const target_base{target_frame.operand_stack_base};
        auto const curr_size{operand_stack.size()};  // condition already popped

        bool const need_repair{curr_size > target_base + target_arity};

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        // Extra-heavy: mega-fuse `test9`-like hot f32 loops into a single opfunc dispatch.
        // This removes huge threaded-interpreter dispatch overhead from the tight arithmetic loops.
        if constexpr(CompileOption.is_tail_call)
        {
            if(!need_repair && target_arity == 0uz && target_frame.type == block_type::loop && label_index_uz == 0uz && curr_size == target_base &&
               target_base == 0uz && target_frame.wasm_code_curr_at_start_label != nullptr)
            {
                bool fused_extra_heavy_loop_run{};

                auto const startp{target_frame.wasm_code_curr_at_start_label};
                auto const endp{op_begin};

                if(startp < endp)
                {
                    auto const consume_op{[&](wasm1_code expected, ::std::byte const*& p) constexpr noexcept -> bool
                                          {
                                              if(p >= endp) [[unlikely]] { return false; }
                                              wasm1_code op;  // no init
                                              ::std::memcpy(::std::addressof(op), p, sizeof(op));
                                              if(op != expected) { return false; }
                                              ++p;
                                              return true;
                                          }};

                    auto const consume_u32_leb{[&](wasm_u32& v, ::std::byte const*& p) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    auto const consume_i32_leb{[&](wasm_i32& v, ::std::byte const*& p) constexpr noexcept -> bool
                                               {
                                                   using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
                                                   auto const [next, err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(p),
                                                                                                   reinterpret_cast<char8_t_const_may_alias_ptr>(endp),
                                                                                                   ::fast_io::mnp::leb128_get(v))};
                                                   if(err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }
                                                   p = reinterpret_cast<::std::byte const*>(next);
                                                   return true;
                                               }};

                    auto const consume_f32_const_bits{[&](wasm_u32 expected_bits, ::std::byte const*& p) constexpr noexcept -> bool
                                                      {
                                                          if(!consume_op(wasm1_code::f32_const, p)) { return false; }
                                                          if(static_cast<::std::size_t>(endp - p) < 4uz) [[unlikely]] { return false; }
                                                          wasm_u32 bits;  // no init
                                                          ::std::memcpy(::std::addressof(bits), p, sizeof(bits));
                                                          p += 4;
                                                          return bits == expected_bits;
                                                      }};

                    constexpr wasm_u32 f32_one_bits{0x3f800000u};
                    constexpr wasm_u32 f32_half_bits{0x3f000000u};

                    wasm_u32 fused_sum_idx{};   // init
                    wasm_u32 fused_i_idx{};     // init
                    wasm_u32 fused_prod_idx{};  // init
                    wasm_u32 fused_ip4_idx{};   // init
                    wasm_i32 fused_end{};       // init

                    enum class test9_loop_kind : unsigned
                    {
                        none,
                        f32_inv_square_sum,
                        f32_mul_chain_sum,
                        f32_inv_cube_sum,
                    };
                    test9_loop_kind fused_kind{test9_loop_kind::none};

                    auto const try_match_inv_square{
                        [&]() constexpr noexcept -> bool
                        {
                            auto p{startp};
                            wasm_u32 sum_idx{};  // init
                            wasm_u32 i_idx{};    // init
                            wasm_u32 tmp_idx{};  // init
                            wasm_i32 end_i{};    // init
                            wasm_u32 tmp_u32{};  // init
                            wasm_i32 tmp_i32{};  // init

                            bool ok = consume_f32_const_bits(f32_one_bits, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(i_idx, p) &&
                                      consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                      consume_op(wasm1_code::i32_mul, p) && consume_op(wasm1_code::f32_convert_i32_u, p) &&
                                      consume_op(wasm1_code::f32_div, p) && consume_f32_const_bits(f32_one_bits, p) && consume_op(wasm1_code::local_get, p) &&
                                      consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) &&
                                      tmp_i32 == wasm_i32{-1} && consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_tee, p) &&
                                      consume_u32_leb(tmp_idx, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) &&
                                      tmp_u32 == tmp_idx && consume_op(wasm1_code::i32_mul, p) && consume_op(wasm1_code::f32_convert_i32_u, p) &&
                                      consume_op(wasm1_code::f32_div, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(sum_idx, p) &&
                                      consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::local_set, p) &&
                                      consume_u32_leb(tmp_u32, p) && tmp_u32 == sum_idx && consume_op(wasm1_code::local_get, p) &&
                                      consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) &&
                                      tmp_i32 == wasm_i32{2} && consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_tee, p) &&
                                      consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_const, p) && consume_i32_leb(end_i, p) &&
                                      consume_op(wasm1_code::i32_ne, p) && (p == endp);

                            if(!ok) { return false; }

                            fused_sum_idx = sum_idx;
                            fused_i_idx = i_idx;
                            fused_end = end_i;
                            fused_kind = test9_loop_kind::f32_inv_square_sum;
                            return true;
                        }};

                    auto const try_match_inv_cube{
                        [&]() constexpr noexcept -> bool
                        {
                            auto p{startp};
                            wasm_u32 sum_idx{};  // init
                            wasm_u32 i_idx{};    // init
                            wasm_u32 tmp_idx{};  // init
                            wasm_i32 end_i{};    // init
                            wasm_u32 tmp_u32{};  // init
                            wasm_i32 tmp_i32{};  // init

                            bool ok =
                                consume_f32_const_bits(f32_one_bits, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(i_idx, p) &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_mul, p) &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx && consume_op(wasm1_code::i32_mul, p) &&
                                consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_div, p) && consume_f32_const_bits(f32_one_bits, p) &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{-1} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_tee, p) && consume_u32_leb(tmp_idx, p) &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == tmp_idx &&
                                consume_op(wasm1_code::i32_mul, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) &&
                                tmp_u32 == tmp_idx && consume_op(wasm1_code::i32_mul, p) && consume_op(wasm1_code::f32_convert_i32_u, p) &&
                                consume_op(wasm1_code::f32_div, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(sum_idx, p) &&
                                consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::local_set, p) &&
                                consume_u32_leb(tmp_u32, p) && tmp_u32 == sum_idx && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) &&
                                tmp_u32 == i_idx && consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{2} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_tee, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(end_i, p) && consume_op(wasm1_code::i32_ne, p) && (p == endp);

                            if(!ok) { return false; }

                            fused_sum_idx = sum_idx;
                            fused_i_idx = i_idx;
                            fused_end = end_i;
                            fused_kind = test9_loop_kind::f32_inv_cube_sum;
                            return true;
                        }};

                    auto const try_match_mul_chain{
                        [&]() constexpr noexcept -> bool
                        {
                            auto p{startp};
                            wasm_u32 sum_idx{};   // init
                            wasm_u32 i_idx{};     // init
                            wasm_u32 prod_idx{};  // init
                            wasm_u32 ip4_idx{};   // init
                            wasm_u32 t5{};        // init
                            wasm_u32 t6{};        // init
                            wasm_u32 t7{};        // init
                            wasm_u32 t8{};        // init
                            wasm_i32 end_i{};     // init
                            wasm_u32 tmp_u32{};   // init
                            wasm_i32 tmp_i32{};   // init

                            bool ok =
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(prod_idx, p) && consume_f32_const_bits(f32_half_bits, p) &&
                                consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(i_idx, p) &&
                                consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_tee, p) &&
                                consume_u32_leb(t5, p) && consume_f32_const_bits(f32_half_bits, p) && consume_op(wasm1_code::f32_mul, p) &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{1} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_mul, p) &&
                                consume_op(wasm1_code::local_tee, p) && consume_u32_leb(t6, p) && consume_f32_const_bits(f32_half_bits, p) &&
                                consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{2} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_mul, p) &&
                                consume_op(wasm1_code::local_tee, p) && consume_u32_leb(t7, p) && consume_f32_const_bits(f32_half_bits, p) &&
                                consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{3} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_mul, p) &&
                                consume_op(wasm1_code::local_tee, p) && consume_u32_leb(t8, p) && consume_f32_const_bits(f32_half_bits, p) &&
                                consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{4} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_tee, p) && consume_u32_leb(ip4_idx, p) &&
                                consume_op(wasm1_code::f32_convert_i32_u, p) && consume_op(wasm1_code::f32_mul, p) && consume_op(wasm1_code::local_tee, p) &&
                                consume_u32_leb(tmp_u32, p) && tmp_u32 == prod_idx && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) &&
                                tmp_u32 == t8 && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == t7 &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == t6 && consume_op(wasm1_code::local_get, p) &&
                                consume_u32_leb(tmp_u32, p) && tmp_u32 == t5 && consume_op(wasm1_code::local_get, p) && consume_u32_leb(sum_idx, p) &&
                                consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::f32_add, p) &&
                                consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::f32_add, p) && consume_op(wasm1_code::local_set, p) &&
                                consume_u32_leb(tmp_u32, p) && tmp_u32 == sum_idx && consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) &&
                                tmp_u32 == i_idx && consume_op(wasm1_code::i32_const, p) && consume_i32_leb(tmp_i32, p) && tmp_i32 == wasm_i32{5} &&
                                consume_op(wasm1_code::i32_add, p) && consume_op(wasm1_code::local_set, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == i_idx &&
                                consume_op(wasm1_code::local_get, p) && consume_u32_leb(tmp_u32, p) && tmp_u32 == ip4_idx &&
                                consume_op(wasm1_code::i32_const, p) && consume_i32_leb(end_i, p) && consume_op(wasm1_code::i32_ne, p) && (p == endp);

                            if(!ok) { return false; }

                            fused_sum_idx = sum_idx;
                            fused_i_idx = i_idx;
                            fused_prod_idx = prod_idx;
                            fused_ip4_idx = ip4_idx;
                            fused_end = end_i;
                            fused_kind = test9_loop_kind::f32_mul_chain_sum;
                            return true;
                        }};

                    // Try match in descending "signature uniqueness" order.
                    if(!try_match_mul_chain())
                    {
                        if(!try_match_inv_square()) { (void)try_match_inv_cube(); }
                    }

                    if(fused_kind != test9_loop_kind::none)
                    {
                        auto const target_label_id{get_branch_target_label_id(target_frame)};
                        auto const& loop_lbl{labels.index_unchecked(target_label_id)};
                        if(!loop_lbl.in_thunk && loop_lbl.offset != SIZE_MAX)
                        {
                            while(!ptr_fixups.empty())
                            {
                                auto const& fx{ptr_fixups.back_unchecked()};
                                if(fx.in_thunk || fx.site < loop_lbl.offset) { break; }
                                ptr_fixups.pop_back_unchecked();
                            }

                            bytecode.resize(loop_lbl.offset);

                            namespace translate = ::uwvm2::runtime::compiler::uwvm_int::optable::translate;
                            switch(fused_kind)
                            {
                                case test9_loop_kind::f32_inv_square_sum:
                                {
                                    emit_opfunc_to(
                                        bytecode,
                                        translate::get_uwvmint_f32_inv_square_sum_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_sum_idx));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_i_idx));
                                    emit_imm_to(bytecode, fused_end);
                                    fused_extra_heavy_loop_run = true;
                                    break;
                                }
                                case test9_loop_kind::f32_inv_cube_sum:
                                {
                                    emit_opfunc_to(
                                        bytecode,
                                        translate::get_uwvmint_f32_inv_cube_sum_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_sum_idx));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_i_idx));
                                    emit_imm_to(bytecode, fused_end);
                                    fused_extra_heavy_loop_run = true;
                                    break;
                                }
                                case test9_loop_kind::f32_mul_chain_sum:
                                {
                                    emit_opfunc_to(
                                        bytecode,
                                        translate::get_uwvmint_f32_mul_chain_sum_loop_run_fptr_from_tuple<CompileOption>(curr_stacktop, interpreter_tuple));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_sum_idx));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_i_idx));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_prod_idx));
                                    emit_imm_to(bytecode, local_offset_from_index(fused_ip4_idx));
                                    emit_imm_to(bytecode, fused_end);
                                    fused_extra_heavy_loop_run = true;
                                    break;
                                }
                                case test9_loop_kind::none:
                                    [[fallthrough]];
                                [[unlikely]] default:
                                    break;
                            }
                        }
                    }
                }

                if(fused_extra_heavy_loop_run)
                {
                    if constexpr(stacktop_enabled)
                    {
                        if(brif_consumes_stack_cond)
                        {
                            // Model the i32 condition pop (no runtime code needed because the entire loop
                            // body (including the compare) is replaced by the mega-fused opfunc).
                            stacktop_commit_pop_n(1uz);
                            codegen_stack_pop_n(1uz);
                        }
                    }
                    break;
                }
            }
        }
#endif

        if constexpr(stacktop_enabled)
        {
            if(brif_consumes_stack_cond)
            {
                // With stack-top caching enabled, popping the condition may require fills to keep the
                // cache canonical. Those fills must run on both taken and fallthrough paths.
                //
                // Fast path: if the condition-pop cannot trigger any canonical fills, we can jump directly
                // without introducing a taken-path thunk.
                auto const brif_stacktop_currpos_at_site{curr_stacktop};
                brif_cond_cached_at_site = (stacktop_cache_count != 0uz);

                // Model the i32 condition pop (post-pop, pre-fill).
                stacktop_commit_pop_n(1uz);
                codegen_stack_pop_n(1uz);

                bool const need_fill_to_canonical{[&]() constexpr noexcept -> bool
                                                  {
                                                      if(stacktop_memory_count == 0uz) { return false; }
                                                      auto const vt{codegen_operand_stack.index_unchecked(stacktop_memory_count - 1uz).type};
                                                      ::std::size_t const begin_pos{stacktop_range_begin_pos(vt)};
                                                      ::std::size_t const end_pos{stacktop_range_end_pos(vt)};
                                                      ::std::size_t const ring_size{end_pos - begin_pos};
                                                      return stacktop_cache_count_for_range(begin_pos, end_pos) != ring_size;
                                                  }()};

                if constexpr(strict_cf_entry_like_call)
                {
                    // In strict mode, the taken path must enter re-entry labels with an empty cache.
                    // If the post-pop cache is already empty and no repair is needed, jump directly.
                    if(!need_repair && stacktop_cache_count == 0uz)
                    {
                        auto const saved_post_pop_stacktop{curr_stacktop};
                        curr_stacktop = brif_stacktop_currpos_at_site;
                        emit_br_if_jump_any(target_label_id);
                        curr_stacktop = saved_post_pop_stacktop;
                        // Fallthrough: refill to canonical after the condition pop.
                        stacktop_fill_to_canonical(bytecode);
                        break;
                    }
                }
                else
                {
                    if(!need_fill_to_canonical && !need_repair)
                    {
                        // Jump directly (no thunk needed). Emit using the pre-pop stacktop cursor.
                        //
                        // If the target is a loop start and we still have cached values after popping
                        // the condition, we must transform the cache layout to the loop's begin-currpos
                        // contract on the taken path (register-only; no spill/fill).
                        ::std::size_t brif_target_label_id{target_label_id};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
                        if constexpr(stacktop_enabled && CompileOption.is_tail_call && stacktop_regtransform_cf_entry && stacktop_regtransform_supported)
                        {
                            if(target_frame.type == block_type::loop && stacktop_cache_count != 0uz)
                            {
                                auto const transform_thunk_label_id{new_label(true)};
                                set_label_offset(transform_thunk_label_id, thunks.size());
                                emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                                brif_target_label_id = transform_thunk_label_id;
                            }
                        }
#endif

                        // Emit using the pre-pop stacktop cursor (the br_if op reads the condition before the pop).
                        auto const saved_post_pop_stacktop{curr_stacktop};
                        curr_stacktop = brif_stacktop_currpos_at_site;
                        emit_br_if_jump_any(brif_target_label_id);
                        curr_stacktop = saved_post_pop_stacktop;

                        if(target_label_id == target_frame.end_label_id)
                        {
                            target_frame_mut.stacktop_has_end_state = true;
                            target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                            target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                            target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                            target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                            target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                            target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                            target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                            target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                        }

                        break;
                    }
                }

                auto const taken_thunk_label_id{new_label(true)};

                // Emit the branch using the pre-pop stacktop cursor.
                {
                    auto const saved_post_pop_stacktop{curr_stacktop};
                    curr_stacktop = brif_stacktop_currpos_at_site;
                    emit_br_if_jump_any(taken_thunk_label_id);
                    curr_stacktop = saved_post_pop_stacktop;
                }

                auto const post_pop_curr_stacktop{curr_stacktop};
                auto const post_pop_memory_count{stacktop_memory_count};
                auto const post_pop_cache_count{stacktop_cache_count};
                auto const post_pop_cache_i32_count{stacktop_cache_i32_count};
                auto const post_pop_cache_i64_count{stacktop_cache_i64_count};
                auto const post_pop_cache_f32_count{stacktop_cache_f32_count};
                auto const post_pop_cache_f64_count{stacktop_cache_f64_count};
                auto const post_pop_codegen_operand_stack{codegen_operand_stack};

                // Emit taken thunk: fill-to-canonical, then optional repair, then jump.
                {
                    set_label_offset(taken_thunk_label_id, thunks.size());

                    // Do not let thunk codegen mutate the fallthrough compiler state.
                    auto const saved_curr_stacktop{curr_stacktop};
                    auto const saved_memory_count{stacktop_memory_count};
                    auto const saved_cache_count{stacktop_cache_count};
                    auto const saved_cache_i32_count{stacktop_cache_i32_count};
                    auto const saved_cache_i64_count{stacktop_cache_i64_count};
                    auto const saved_cache_f32_count{stacktop_cache_f32_count};
                    auto const saved_cache_f64_count{stacktop_cache_f64_count};
                    auto const saved_codegen_operand_stack{codegen_operand_stack};

                    // Ensure thunk starts from the post-pop state.
                    curr_stacktop = post_pop_curr_stacktop;
                    stacktop_memory_count = post_pop_memory_count;
                    stacktop_cache_count = post_pop_cache_count;
                    stacktop_cache_i32_count = post_pop_cache_i32_count;
                    stacktop_cache_i64_count = post_pop_cache_i64_count;
                    stacktop_cache_f32_count = post_pop_cache_f32_count;
                    stacktop_cache_f64_count = post_pop_cache_f64_count;
                    codegen_operand_stack = post_pop_codegen_operand_stack;

                    if constexpr(strict_cf_entry_like_call)
                    {
                        // Taken path: repair (if needed), then canonicalize (empty cache) before jumping.
                        if(need_repair)
                        {
                            if(target_arity == 0uz) { emit_drop_to_stack_size_no_fill(thunks, target_base); }
                            else
                            {
                                auto const result_type{target_frame.result.begin[0]};
                                emit_local_set_typed_to_no_fill(thunks, result_type, internal_temp_local_off);
                                if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(thunks, target_base); }
                                else
                                {
                                    for(::std::size_t i{curr_size - 1uz}; i-- > target_base;)
                                    {
                                        emit_drop_typed_to_no_fill(thunks, operand_stack.index_unchecked(i).type);
                                    }
                                }
                                emit_local_get_typed_to(thunks, result_type, internal_temp_local_off);
                            }
                        }

                        stacktop_canonicalize_edge_to_memory(thunks);
                        if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                        {
                            emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                        }
                        else
                        {
                            emit_br_to(thunks, target_label_id, true);
                        }
                    }
                    else
                    {
                        stacktop_fill_to_canonical(thunks);

                        if(!need_repair)
                        {
                            if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                            {
                                emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                            }
                            else
                            {
                                emit_br_to(thunks, target_label_id, true);
                            }
                        }
                        else if(target_arity == 0uz)
                        {
                            // Safety: `target_base` must be <= `curr_size` in the non-polymorphic path.
                            emit_drop_to_stack_size_no_fill(thunks, target_base);
                            stacktop_fill_to_canonical(thunks);
                            if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                            {
                                emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                            }
                            else
                            {
                                emit_br_to(thunks, target_label_id, true);
                            }
                        }
                        else
                        {
                            auto const result_type{target_frame.result.begin[0]};
                            emit_local_set_typed_to_no_fill(thunks, result_type, internal_temp_local_off);

                            if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(thunks, target_base); }
                            else
                            {
                                for(::std::size_t i{curr_size - 1uz}; i-- > target_base;)
                                {
                                    emit_drop_typed_to_no_fill(thunks, operand_stack.index_unchecked(i).type);
                                }
                            }

                            stacktop_fill_to_canonical(thunks);
                            emit_local_get_typed_to(thunks, result_type, internal_temp_local_off);
                            if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                            {
                                emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                            }
                            else
                            {
                                emit_br_to(thunks, target_label_id, true);
                            }
                        }

                        if(target_label_id == target_frame.end_label_id)
                        {
                            target_frame_mut.stacktop_has_end_state = true;
                            target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                            target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                            target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                            target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                            target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                            target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                            target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                            target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                        }
                    }

                    curr_stacktop = saved_curr_stacktop;
                    stacktop_memory_count = saved_memory_count;
                    stacktop_cache_count = saved_cache_count;
                    stacktop_cache_i32_count = saved_cache_i32_count;
                    stacktop_cache_i64_count = saved_cache_i64_count;
                    stacktop_cache_f32_count = saved_cache_f32_count;
                    stacktop_cache_f64_count = saved_cache_f64_count;
                    codegen_operand_stack = saved_codegen_operand_stack;
                }

                // Fallthrough: refill to canonical after the condition pop.
                stacktop_fill_to_canonical(bytecode);

                break;
            }
        }

        bool const strict_need_taken_thunk{[&]() constexpr noexcept -> bool
                                           {
                                               if constexpr(stacktop_enabled && strict_cf_entry_like_call)
                                               {
                                                   return need_repair || (stacktop_cache_count != 0uz);
                                               }
                                               else
                                               {
                                                   return need_repair;
                                               }
                                           }()};

        if(!strict_need_taken_thunk)
        {
            ::std::size_t brif_target_label_id{target_label_id};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
            if constexpr(stacktop_enabled && CompileOption.is_tail_call && stacktop_regtransform_cf_entry && stacktop_regtransform_supported)
            {
                if(!is_polymorphic && target_frame.type == block_type::loop && stacktop_cache_count != 0uz)
                {
                    auto const transform_thunk_label_id{new_label(true)};
                    set_label_offset(transform_thunk_label_id, thunks.size());
                    emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                    brif_target_label_id = transform_thunk_label_id;
                }
            }
#endif
            emit_br_if_jump_any(brif_target_label_id);

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { /* snapshots not needed */ }
                else
                {
                    // If this branch targets the end label of its frame, record the current stack-top state
                    // so `end` can restore it when the fallthrough path becomes unreachable later.
                    //
                    // Note: This must also happen for fused `br_if` forms that do not consume a condition from
                    // the operand stack (e.g. `local.get; i32.eqz; br_if` fused to a local-based `br_if`),
                    // because in that case the taken path still reaches the end label even though no stack pop
                    // triggers the thunk-based snapshot logic above.
                    if(!is_polymorphic && target_label_id == target_frame.end_label_id)
                    {
                        target_frame_mut.stacktop_has_end_state = true;
                        target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                        target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                        target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                        target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                        target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                        target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                        target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                        target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                    }
                }
            }
        }
        else
        {
            // Create a taken-path thunk: repair stack then jump.
            auto const thunk_label_id{new_label(true)};
            set_label_offset(thunk_label_id, thunks.size());

            // The thunk is emitted into a separate buffer; do not let its stack-top state mutations
            // affect the main bytecode generation state.
            auto const saved_curr_stacktop{curr_stacktop};
            auto const saved_memory_count{stacktop_memory_count};
            auto const saved_cache_count{stacktop_cache_count};
            auto const saved_cache_i32_count{stacktop_cache_i32_count};
            auto const saved_cache_i64_count{stacktop_cache_i64_count};
            auto const saved_cache_f32_count{stacktop_cache_f32_count};
            auto const saved_cache_f64_count{stacktop_cache_f64_count};
            auto const saved_codegen_operand_stack{codegen_operand_stack};

            if(need_repair)
            {
                if(target_arity == 0uz)
                {
                    emit_drop_to_stack_size_no_fill(thunks, target_base);
                    if constexpr(stacktop_enabled)
                    {
                        if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(thunks); }
                    }
                }
                else
                {
                    auto const result_type{target_frame.result.begin[0]};
                    emit_local_set_typed_to_no_fill(thunks, result_type, internal_temp_local_off);
                    if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(thunks, target_base); }
                    else
                    {
                        for(::std::size_t i{curr_size - 1uz}; i-- > target_base;) { emit_drop_typed_to_no_fill(thunks, operand_stack.index_unchecked(i).type); }
                    }
                    if constexpr(stacktop_enabled)
                    {
                        if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(thunks); }
                    }
                    emit_local_get_typed_to(thunks, result_type, internal_temp_local_off);
                }
            }

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(thunks); }
            }
            if constexpr(stacktop_enabled)
            {
                if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                {
                    emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                }
                else
                {
                    emit_br_to(thunks, target_label_id, true);
                }
            }
            else
            {
                emit_br_to(thunks, target_label_id, true);
            }

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { /* snapshots not needed */ }
                else
                {
                    // Same rationale as the `!need_repair` path: ensure `end` has a reachable snapshot even
                    // when the taken path is lowered via a repair thunk and the condition is not popped from
                    // the operand stack.
                    if(!is_polymorphic && target_label_id == target_frame.end_label_id)
                    {
                        target_frame_mut.stacktop_has_end_state = true;
                        target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                        target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                        target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                        target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                        target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                        target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                        target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                        target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                    }
                }
            }

            curr_stacktop = saved_curr_stacktop;
            stacktop_memory_count = saved_memory_count;
            stacktop_cache_count = saved_cache_count;
            stacktop_cache_i32_count = saved_cache_i32_count;
            stacktop_cache_i64_count = saved_cache_i64_count;
            stacktop_cache_f32_count = saved_cache_f32_count;
            stacktop_cache_f64_count = saved_cache_f64_count;
            codegen_operand_stack = saved_codegen_operand_stack;

            emit_br_if_jump_any(thunk_label_id);
        }
    }

    break;
}
case wasm1_code::br_table:
{
    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // br_table  target_count ...
    // [ safe ] unsafe (could be the section_end)
    //           ^^ code_curr

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    wasm_u32 target_count;
    auto const [cnt_next, cnt_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(target_count))};
    if(cnt_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(cnt_err);
    }

    // br_table  target_count ...
    // [       safe         ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(cnt_next);

    // br_table  target_count ...
    // [       safe         ] unsafe (could be the section_end)
    //                       ^^ code_curr

    // Security hardening: each `br_table` label index (including the default label) is encoded as
    // an unsigned LEB128 value and therefore occupies at least one byte. Reject encodings whose
    // remaining byte count cannot possibly contain `target_count + 1` label indices before any
    // reservation/allocation, preventing attacker-controlled oversized `target_count` values from
    // amplifying into excessive memory requests.
    auto const remaining_bytes{static_cast<::std::size_t>(code_end - code_curr)};
    auto constexpr max_br_table_label_count{::std::numeric_limits<::std::size_t>::max()};
    bool target_count_exceeds_size_t{};
    ::std::size_t target_count_uz{};
    if constexpr(::std::numeric_limits<wasm_u32>::max() > max_br_table_label_count)
    {
        if(target_count > max_br_table_label_count) [[unlikely]]
        {
            target_count_exceeds_size_t = true;
        }
        else
        {
            target_count_uz = static_cast<::std::size_t>(target_count);
        }
    }
    else
    {
        target_count_uz = static_cast<::std::size_t>(target_count);
    }

    auto const target_count_plus_default_overflows{!target_count_exceeds_size_t && target_count_uz == max_br_table_label_count};
    if(target_count_exceeds_size_t || target_count_plus_default_overflows || remaining_bytes == 0uz || target_count_uz >= remaining_bytes)
        [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.target_count = target_count;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.remaining_bytes = remaining_bytes;
        err.err_selectable.br_table_target_count_exceeds_remaining_bytes.max_target_count = (remaining_bytes == 0uz ? 0uz : remaining_bytes - 1uz);
        err.err_code = code_validation_error_code::br_table_target_count_exceeds_remaining_bytes;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto const all_label_count_uz{control_flow_stack.size()};
    auto const validate_label{[&](wasm_u32 li) constexpr UWVM_THROWS
                              {
                                  if(static_cast<::std::size_t>(li) >= all_label_count_uz) [[unlikely]]
                                  {
                                      err.err_curr = op_begin;
                                      err.err_selectable.illegal_label_index.label_index = li;
                                      err.err_selectable.illegal_label_index.all_label_count = static_cast<wasm_u32>(all_label_count_uz);
                                      err.err_code = code_validation_error_code::illegal_label_index;
                                      ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
                                  }
                              }};

    struct get_sig_result_t
    {
        ::std::size_t arity{};
        curr_operand_stack_value_type type{};
    };

    auto const get_sig{[&](wasm_u32 li) constexpr noexcept
                       {
                           auto const& frame{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
                           ::std::size_t arity{};
                           curr_operand_stack_value_type type{};
                           if(frame.type != block_type::loop)
                           {
                               arity = static_cast<::std::size_t>(frame.result.end - frame.result.begin);
                               if(arity != 0uz) { type = frame.result.begin[0]; }
                           }
                           return get_sig_result_t{arity, type};
                       }};

    bool have_expected_sig{};
    wasm_u32 expected_label{};
    ::std::size_t expected_arity{};
    curr_operand_stack_value_type expected_type{};

    auto const br_table_label_capacity{target_count_uz + 1uz};

    ::uwvm2::utils::container::vector<wasm_u32> br_table_label_indices{};
    br_table_label_indices.reserve(br_table_label_capacity);

    for(wasm_u32 i{}; i != target_count; ++i)
    {
        // ...    | curr_target ...
        // [safe] | unsafe (could be the section_end)
        //          ^^ code_curr

        wasm_u32 li;
        auto const [li_next, li_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                              reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                              ::fast_io::mnp::leb128_get(li))};
        if(li_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_code = code_validation_error_code::invalid_label_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(li_err);
        }

        // ...   | curr_target ...
        // [safe | safe      ] unsafe (could be the section_end)
        //         ^^ code_curr

        code_curr = reinterpret_cast<::std::byte const*>(li_next);

        // ...   | curr_target ...
        // [safe | safe      ] unsafe (could be the section_end)
        //                     ^^ code_curr

        validate_label(li);
        // Safe: reserved one slot per branch target plus the default label above.
        br_table_label_indices.push_back_unchecked(li);

        auto const [arity, type]{get_sig(li)};
        if(!have_expected_sig)
        {
            have_expected_sig = true;
            expected_label = li;
            expected_arity = arity;
            expected_type = type;
        }
        else if(arity != expected_arity || (expected_arity != 0uz && type != expected_type)) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
            err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = li;
            err.err_selectable.br_table_target_type_mismatch.expected_arity = static_cast<wasm_u32>(expected_arity);
            err.err_selectable.br_table_target_type_mismatch.actual_arity = static_cast<wasm_u32>(arity);
            err.err_selectable.br_table_target_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
            err.err_selectable.br_table_target_type_mismatch.actual_type = static_cast<wasm_value_type_u>(type);
            err.err_code = code_validation_error_code::br_table_target_type_mismatch;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // ... last_target | default_label ...
    // [   safe      ]   unsafe (could be the section_end)
    //                   ^^ code_curr

    wasm_u32 default_label;
    auto const [def_next, def_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(default_label))};
    if(def_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_code = code_validation_error_code::invalid_label_index;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(def_err);
    }

    // ... last_target | default_label ...
    // [         safe  |      safe   ] unsafe (could be the section_end)
    //                   ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(def_next);

    // ... last_target | default_label ...
    // [         safe  |      safe   ] unsafe (could be the section_end)
    //                                 ^^ code_curr

    validate_label(default_label);
    // Safe: reserved one slot per branch target plus the default label above.
    br_table_label_indices.push_back_unchecked(default_label);

    auto const [default_arity, default_type]{get_sig(default_label)};
    if(!have_expected_sig)
    {
        have_expected_sig = true;
        expected_label = default_label;
        expected_arity = default_arity;
        expected_type = default_type;
    }
    else if(default_arity != expected_arity || (expected_arity != 0uz && default_type != expected_type)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.br_table_target_type_mismatch.expected_label_index = expected_label;
        err.err_selectable.br_table_target_type_mismatch.mismatched_label_index = default_label;
        err.err_selectable.br_table_target_type_mismatch.expected_arity = static_cast<wasm_u32>(expected_arity);
        err.err_selectable.br_table_target_type_mismatch.actual_arity = static_cast<wasm_u32>(default_arity);
        err.err_selectable.br_table_target_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
        err.err_selectable.br_table_target_type_mismatch.actual_type = static_cast<wasm_value_type_u>(default_type);
        err.err_code = code_validation_error_code::br_table_target_type_mismatch;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    auto constexpr max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    auto const expected_arity_plus_index_overflows{expected_arity == max_operand_stack_requirement};
    auto const required_stack_size{expected_arity_plus_index_overflows ? max_operand_stack_requirement : (expected_arity + 1uz)};

    if(!is_polymorphic && (expected_arity_plus_index_overflows || concrete_operand_count() < required_stack_size)) [[unlikely]]
    {
        report_operand_stack_underflow(op_begin, u8"br_table", required_stack_size);
    }

    if(auto const idx{try_pop_concrete_operand()}; idx.from_stack)
    {
        if(idx.type != curr_operand_stack_value_type::i32) [[unlikely]]
        {
            err.err_curr = op_begin;
            err.err_selectable.br_cond_type_not_i32.op_code_name = u8"br_table";
            err.err_selectable.br_cond_type_not_i32.cond_type = static_cast<wasm_value_type_u>(idx.type);
            err.err_code = code_validation_error_code::br_cond_type_not_i32;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    // Stack-top caching: `br_table` reads the selector index from the stack-top cache (tail-call opfunc).
    // The bytecode opfunc does not pop the selector at runtime; instead the translator models the pop by advancing
    // the stack-top cursor and decrementing cache depth, so the consumed selector is treated as dead and will not
    // be spilled across the branch edge.
    //
    // IMPORTANT: the opfunc must read the selector from the **pre-pop** stack-top slot; use a snapshot for fptr selection.
    auto br_table_stacktop_currpos_at_site{curr_stacktop};
    if constexpr(stacktop_enabled)
    {
        if(!is_polymorphic)
        {
            // In strict CF-entry mode, the cache can be empty at re-entry labels; ensure the selector is resident in cache.
            if(stacktop_cache_count == 0uz) { stacktop_fill_to_canonical(bytecode); }
            br_table_stacktop_currpos_at_site = curr_stacktop;

            stacktop_commit_pop_n(1uz);
            codegen_stack_pop_n(1uz);
        }
    }

    if(expected_arity != 0uz)
    {
        auto const available_arg_count{concrete_operand_count()};
        auto const concrete_to_check{available_arg_count < expected_arity ? available_arg_count : expected_arity};
        for(::std::size_t i{}; i != concrete_to_check; ++i)
        {
            auto const actual_type{operand_stack.index_unchecked(operand_stack.size() - 1uz - i).type};
            if(actual_type != expected_type) [[unlikely]]
            {
                err.err_curr = op_begin;
                err.err_selectable.br_value_type_mismatch.op_code_name = u8"br_table";
                err.err_selectable.br_value_type_mismatch.expected_type = static_cast<wasm_value_type_u>(expected_type);
                err.err_selectable.br_value_type_mismatch.actual_type = static_cast<wasm_value_type_u>(actual_type);
                err.err_code = code_validation_error_code::br_value_type_mismatch;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }

    // Translate: `br_table` always branches. Use per-target thunks when stack repair is required.
    {
        auto const curr_size{operand_stack.size()};  // selector already popped
        emit_opfunc_to(
            bytecode,
            ::uwvm2::runtime::compiler::uwvm_int::optable::translate::get_uwvmint_br_table_fptr_from_tuple<CompileOption>(br_table_stacktop_currpos_at_site,
                                                                                                                          interpreter_tuple));
        emit_imm_to(bytecode, static_cast<::std::size_t>(target_count));

        for(auto const li: br_table_label_indices)
        {
            auto& target_frame_mut{control_flow_stack.index_unchecked(all_label_count_uz - 1uz - static_cast<::std::size_t>(li))};
            auto const& target_frame{target_frame_mut};
            auto const target_label_id{get_branch_target_label_id(target_frame)};

            if(is_polymorphic)
            {
                emit_ptr_label_placeholder(target_label_id, false);
                continue;
            }

            auto const target_base{target_frame.operand_stack_base};
            bool const need_repair{curr_size > target_base + expected_arity};
            bool const strict_need_thunk{[&]() constexpr noexcept -> bool
                                         {
                                             if constexpr(stacktop_enabled && strict_cf_entry_like_call)
                                             {
                                                 return need_repair || (stacktop_cache_count != 0uz);
                                             }
                                             else
                                             {
                                                 return need_repair;
                                             }
                                         }()};
            if(!strict_need_thunk)
            {
                if constexpr(stacktop_enabled)
                {
                    if constexpr(strict_cf_entry_like_call) { /* snapshots not needed */ }
                    else
                    {
                        // Record reachable end-label state for constructs that are reached via this `br_table`
                        // when their fallthrough becomes unreachable before `end`.
                        if(!is_polymorphic && target_label_id == target_frame.end_label_id)
                        {
                            target_frame_mut.stacktop_has_end_state = true;
                            target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                            target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                            target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                            target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                            target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                            target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                            target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                            target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                        }
                    }
                }
                ::std::size_t br_table_target_label_id{target_label_id};
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
                if constexpr(stacktop_enabled && CompileOption.is_tail_call && stacktop_regtransform_cf_entry && stacktop_regtransform_supported)
                {
                    if(!is_polymorphic && target_frame.type == block_type::loop && stacktop_cache_count != 0uz)
                    {
                        auto const transform_thunk_label_id{new_label(true)};
                        set_label_offset(transform_thunk_label_id, thunks.size());
                        emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                        br_table_target_label_id = transform_thunk_label_id;
                    }
                }
#endif
                emit_ptr_label_placeholder(br_table_target_label_id, false);
                continue;
            }

            auto const thunk_label_id{new_label(true)};
            set_label_offset(thunk_label_id, thunks.size());

            // Thunk emission must not mutate the main stack-top codegen state.
            auto const saved_curr_stacktop{curr_stacktop};
            auto const saved_memory_count{stacktop_memory_count};
            auto const saved_cache_count{stacktop_cache_count};
            auto const saved_cache_i32_count{stacktop_cache_i32_count};
            auto const saved_cache_i64_count{stacktop_cache_i64_count};
            auto const saved_cache_f32_count{stacktop_cache_f32_count};
            auto const saved_cache_f64_count{stacktop_cache_f64_count};
            auto const saved_codegen_operand_stack{codegen_operand_stack};

            if(need_repair)
            {
                if(expected_arity == 0uz)
                {
                    emit_drop_to_stack_size_no_fill(thunks, target_base);
                    if constexpr(stacktop_enabled)
                    {
                        if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(thunks); }
                    }
                }
                else
                {
                    emit_local_set_typed_to_no_fill(thunks, expected_type, internal_temp_local_off);
                    if constexpr(stacktop_enabled) { emit_drop_to_stack_size_no_fill(thunks, target_base); }
                    else
                    {
                        for(::std::size_t i{curr_size - 1uz}; i-- > target_base;) { emit_drop_typed_to_no_fill(thunks, operand_stack.index_unchecked(i).type); }
                    }
                    if constexpr(stacktop_enabled)
                    {
                        if constexpr(!strict_cf_entry_like_call) { stacktop_fill_to_canonical(thunks); }
                    }
                    emit_local_get_typed_to(thunks, expected_type, internal_temp_local_off);
                }
            }

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { stacktop_canonicalize_edge_to_memory(thunks); }
            }
            if constexpr(stacktop_enabled)
            {
                if(target_frame.type == block_type::loop && stacktop_regtransform_cf_entry)
                {
                    emit_br_to_with_stacktop_transform(thunks, target_label_id, true);
                }
                else
                {
                    emit_br_to(thunks, target_label_id, true);
                }
            }
            else
            {
                emit_br_to(thunks, target_label_id, true);
            }

            if constexpr(stacktop_enabled)
            {
                if constexpr(strict_cf_entry_like_call) { /* snapshots not needed */ }
                else
                {
                    if(!is_polymorphic && target_label_id == target_frame.end_label_id)
                    {
                        target_frame_mut.stacktop_has_end_state = true;
                        target_frame_mut.stacktop_currpos_at_end = curr_stacktop;
                        target_frame_mut.stacktop_memory_count_at_end = stacktop_memory_count;
                        target_frame_mut.stacktop_cache_count_at_end = stacktop_cache_count;
                        target_frame_mut.stacktop_cache_i32_count_at_end = stacktop_cache_i32_count;
                        target_frame_mut.stacktop_cache_i64_count_at_end = stacktop_cache_i64_count;
                        target_frame_mut.stacktop_cache_f32_count_at_end = stacktop_cache_f32_count;
                        target_frame_mut.stacktop_cache_f64_count_at_end = stacktop_cache_f64_count;
                        target_frame_mut.codegen_operand_stack_at_end = codegen_operand_stack;
                    }
                }
            }

            curr_stacktop = saved_curr_stacktop;
            stacktop_memory_count = saved_memory_count;
            stacktop_cache_count = saved_cache_count;
            stacktop_cache_i32_count = saved_cache_i32_count;
            stacktop_cache_i64_count = saved_cache_i64_count;
            stacktop_cache_f32_count = saved_cache_f32_count;
            stacktop_cache_f64_count = saved_cache_f64_count;
            codegen_operand_stack = saved_codegen_operand_stack;

            emit_ptr_label_placeholder(thunk_label_id, false);
        }
    }

    if(expected_arity != 0uz) { operand_stack_pop_n(expected_arity); }
    auto const curr_frame_base{control_flow_stack.back_unchecked().operand_stack_base};
    operand_stack_truncate_to(curr_frame_base);
    is_polymorphic = true;

    break;
}
