namespace details
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
    using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

    using wasm_stack_top_i32_with_f32_u = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_with_f32_u;
    using wasm_stack_top_i32_with_i64_u = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_with_i64_u;
    using wasm_stack_top_i32_i64_f32_f64_u = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_i64_f32_f64_u;

    using trivial_call_inline_kind = ::uwvm2::runtime::compiler::uwvm_int::optable::trivial_defined_call_kind;

    struct trivial_call_inline_match
    {
        trivial_call_inline_kind kind{};
        wasm_i32 imm{};   // used by *_const_i32 patterns
        wasm_i32 imm2{};  // used by mul_add_const_i32
    };

    [[nodiscard]] UWVM_ALWAYS_INLINE inline trivial_call_inline_match
        match_trivial_call_inline_body(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t const* code_ptr) noexcept
    {
        using wasm1_code = ::uwvm2::runtime::compiler::uwvm_int::optable::wasm1_code;

        trivial_call_inline_match res{};
        if(code_ptr == nullptr) { return res; }

        auto curr{reinterpret_cast<::std::byte const*>(code_ptr->body.expr_begin)};
        auto const end{reinterpret_cast<::std::byte const*>(code_ptr->body.code_end)};

        auto const read_op{[&](wasm1_code& out) noexcept -> bool
                           {
                               if(curr == end) { return false; }
                               ::std::memcpy(::std::addressof(out), curr, sizeof(out));
                               ++curr;
                               return true;
                           }};

        auto const read_u32_leb{[&](::std::uint32_t& out) noexcept -> bool
                                {
                                    ::std::uint32_t v{};
                                    ::std::uint32_t shift{};
                                    for(::std::size_t i{}; i != 5uz; ++i)
                                    {
                                        if(curr == end) { return false; }
                                        auto const byte{::std::to_integer<::std::uint8_t>(*curr)};
                                        ++curr;
                                        v |= (static_cast<::std::uint32_t>(byte & 0x7fu) << shift);
                                        if((byte & 0x80u) == 0u)
                                        {
                                            out = v;
                                            return true;
                                        }
                                        shift += 7u;
                                    }
                                    return false;
                                }};

        auto const read_i32_leb{[&](wasm_i32& out) noexcept -> bool
                                {
                                    ::std::int32_t v{};
                                    ::std::uint32_t shift{};
                                    ::std::uint8_t byte{};
                                    for(::std::size_t i{}; i != 5uz; ++i)
                                    {
                                        if(curr == end) { return false; }
                                        byte = ::std::to_integer<::std::uint8_t>(*curr);
                                        ++curr;
                                        v |= (static_cast<::std::int32_t>(byte & 0x7fu) << shift);
                                        shift += 7u;
                                        if((byte & 0x80u) == 0u)
                                        {
                                            if(shift < 32u && (byte & 0x40u)) { v |= (-1) << shift; }
                                            out = static_cast<wasm_i32>(v);
                                            return true;
                                        }
                                    }
                                    return false;
                                }};

        // Pattern: xorshift32 (i32 -> i32)
        // local.get 0
        // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
        // local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
        // local.get 0 ; i32.const 5  ; i32.shl ; i32.xor ; local.set 0
        // local.get 0 ; end
        //
        // Notes:
        // - This is a common tiny PRNG step used in microbenchmarks and hash-like code.
        // - We match the exact canonical form (locals only, no extra ops) to avoid false positives.
        auto const match_xorshift32_i32{[&]() noexcept -> bool
                                        {
                                            auto const begin{curr};
                                            auto fail{[&]() noexcept
                                                      {
                                                          curr = begin;
                                                          return false;
                                                      }};

                                            auto expect_op{[&](wasm1_code expected) noexcept -> bool
                                                           {
                                                               wasm1_code op{};  // no init
                                                               if(!read_op(op) || op != expected) { return false; }
                                                               return true;
                                                           }};
                                            auto expect_local0{[&](wasm1_code op_kind) noexcept -> bool
                                                               {
                                                                   if(!expect_op(op_kind)) { return false; }
                                                                   ::std::uint32_t idx{};
                                                                   if(!read_u32_leb(idx) || idx != 0u) { return false; }
                                                                   return true;
                                                               }};
                                            auto expect_i32_const{[&](wasm_i32 expected) noexcept -> bool
                                                                  {
                                                                      if(!expect_op(wasm1_code::i32_const)) { return false; }
                                                                      wasm_i32 imm{};  // no init
                                                                      if(!read_i32_leb(imm) || imm != expected) { return false; }
                                                                      return true;
                                                                  }};

                                            // local.get 0
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }

                                            // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_i32_const(static_cast<wasm_i32>(13))) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_shl)) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_xor)) { return fail(); }
                                            if(!expect_local0(wasm1_code::local_set)) { return fail(); }

                                            // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_i32_const(static_cast<wasm_i32>(17))) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_shr_u)) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_xor)) { return fail(); }
                                            if(!expect_local0(wasm1_code::local_set)) { return fail(); }

                                            // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_i32_const(static_cast<wasm_i32>(5))) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_shl)) { return fail(); }
                                            if(!expect_op(wasm1_code::i32_xor)) { return fail(); }
                                            if(!expect_local0(wasm1_code::local_set)) { return fail(); }

                                            // local.get 0 ; end
                                            if(!expect_local0(wasm1_code::local_get)) { return fail(); }
                                            if(!expect_op(wasm1_code::end)) { return fail(); }
                                            if(curr != end) { return fail(); }
                                            return true;
                                        }};

        if(match_xorshift32_i32())
        {
            res.kind = trivial_call_inline_kind::xorshift32_i32;
            return res;
        }

        wasm1_code op0{};
        if(!read_op(op0)) { return res; }
        if(op0 == wasm1_code::end && curr == end)
        {
            res.kind = trivial_call_inline_kind::nop_void;
            return res;
        }
        if(op0 == wasm1_code::i32_const)
        {
            wasm_i32 imm;  // no init
            if(!read_i32_leb(imm)) { return res; }

            wasm1_code op1{};
            if(!read_op(op1)) { return res; }

            if(op1 == wasm1_code::end && curr == end)
            {
                res.kind = trivial_call_inline_kind::const_i32;
                res.imm = imm;
                return res;
            }
            if(op1 == wasm1_code::drop)
            {
                wasm1_code op2{};
                if(!read_op(op2) || op2 != wasm1_code::end) { return res; }
                if(curr != end) { return res; }

                res.kind = trivial_call_inline_kind::nop_void;
                return res;
            }

            return res;
        }
        if(op0 != wasm1_code::local_get) { return res; }
        ::std::uint32_t idx0{};
        if(!read_u32_leb(idx0)) { return res; }

        wasm1_code op1{};
        if(!read_op(op1)) { return res; }
        if(op1 == wasm1_code::end && curr == end)
        {
            if(idx0 == 0u)
            {
                res.kind = trivial_call_inline_kind::param0_i32;
                return res;
            }
            return res;
        }
        if(op1 == wasm1_code::drop)
        {
            wasm1_code op2{};
            if(!read_op(op2) || op2 != wasm1_code::end) { return res; }
            if(curr != end) { return res; }

            res.kind = trivial_call_inline_kind::nop_void;
            return res;
        }

        // Pattern C': local.get 1 ; i32.const IMM ; i32.xor ; local.get 0 ; i32.add ; end
        // This is semantically equivalent to Pattern C (param0 + (param1 ^ IMM)), but uses a different stack order.
        if(op1 == wasm1_code::i32_const)
        {
            // Pattern A': local.get 0 ; i32.const IMM ; i32.add ; end
            // Pattern B': local.get 0 ; i32.const MUL ; i32.mul ; i32.const ADD ; i32.add ; end
            if(idx0 == 0u)
            {
                wasm_i32 imm0;  // no init
                if(!read_i32_leb(imm0)) { return res; }

                wasm1_code op2{};
                if(!read_op(op2)) { return res; }

                if(op2 == wasm1_code::i32_add)
                {
                    wasm1_code op3{};
                    if(!read_op(op3) || op3 != wasm1_code::end) { return res; }
                    if(curr != end) { return res; }

                    res.kind = trivial_call_inline_kind::add_const_i32;
                    res.imm = imm0;
                    return res;
                }
                else if(op2 == wasm1_code::i32_xor)
                {
                    wasm1_code op3{};
                    if(!read_op(op3) || op3 != wasm1_code::end) { return res; }
                    if(curr != end) { return res; }

                    res.kind = trivial_call_inline_kind::xor_const_i32;
                    res.imm = imm0;
                    return res;
                }
                else if(op2 == wasm1_code::i32_mul)
                {
                    wasm1_code op3{};
                    if(!read_op(op3)) { return res; }
                    if(op3 == wasm1_code::end && curr == end)
                    {
                        res.kind = trivial_call_inline_kind::mul_const_i32;
                        res.imm = imm0;
                        return res;
                    }
                    if(op3 != wasm1_code::i32_const) { return res; }
                    wasm_i32 imm1;  // no init
                    if(!read_i32_leb(imm1)) { return res; }

                    wasm1_code op4{};
                    wasm1_code op5{};
                    if(!read_op(op4) || op4 != wasm1_code::i32_add) { return res; }
                    if(!read_op(op5) || op5 != wasm1_code::end) { return res; }
                    if(curr != end) { return res; }

                    res.kind = trivial_call_inline_kind::mul_add_const_i32;
                    res.imm = imm0;
                    res.imm2 = imm1;
                    return res;
                }
                else if(op2 == wasm1_code::i32_rotr)
                {
                    wasm1_code op3{};
                    if(!read_op(op3) || op3 != wasm1_code::i32_const) { return res; }
                    wasm_i32 imm1;  // no init
                    if(!read_i32_leb(imm1)) { return res; }

                    wasm1_code op4{};
                    wasm1_code op5{};
                    if(!read_op(op4) || op4 != wasm1_code::i32_add) { return res; }
                    if(!read_op(op5) || op5 != wasm1_code::end) { return res; }
                    if(curr != end) { return res; }

                    res.kind = trivial_call_inline_kind::rotr_add_const_i32;
                    res.imm = imm1;
                    res.imm2 = imm0;
                    return res;
                }

                return res;
            }

            if(idx0 != 1u) { return res; }

            wasm_i32 imm;  // no init
            if(!read_i32_leb(imm)) { return res; }

            wasm1_code op2{};
            if(!read_op(op2) || op2 != wasm1_code::i32_xor) { return res; }

            wasm1_code op3{};
            if(!read_op(op3) || op3 != wasm1_code::local_get) { return res; }
            ::std::uint32_t idx1{};
            if(!read_u32_leb(idx1) || idx1 != 0u) { return res; }

            wasm1_code op4{};
            wasm1_code op5{};
            if(!read_op(op4) || op4 != wasm1_code::i32_add) { return res; }
            if(!read_op(op5) || op5 != wasm1_code::end) { return res; }
            if(curr != end) { return res; }

            res.kind = trivial_call_inline_kind::xor_add_const_i32;
            res.imm = imm;
            return res;
        }

        if(op1 != wasm1_code::local_get) { return res; }
        ::std::uint32_t idx1{};
        if(!read_u32_leb(idx1)) { return res; }

        wasm1_code op2{};
        if(!read_op(op2)) { return res; }

        if(op2 == wasm1_code::i32_add)
        {
            // Pattern E: sum8 xor const
            // local.get 0 ; local.get 1 ; i32.add ;
            // local.get 2 ; local.get 3 ; i32.add ; i32.add ;
            // local.get 4 ; local.get 5 ; i32.add ;
            // local.get 6 ; local.get 7 ; i32.add ; i32.add ; i32.add ;
            // i32.const IMM ; i32.xor ; end
            if(idx0 == 0u && idx1 == 1u)
            {
                wasm1_code op3{};
                if(!read_op(op3) || op3 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx2{};
                if(!read_u32_leb(idx2) || idx2 != 2u) { return res; }

                wasm1_code op4{};
                if(!read_op(op4) || op4 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx3{};
                if(!read_u32_leb(idx3) || idx3 != 3u) { return res; }

                wasm1_code op5{};
                wasm1_code op6{};
                if(!read_op(op5) || op5 != wasm1_code::i32_add) { return res; }
                if(!read_op(op6) || op6 != wasm1_code::i32_add) { return res; }

                wasm1_code op7{};
                if(!read_op(op7) || op7 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx4{};
                if(!read_u32_leb(idx4) || idx4 != 4u) { return res; }

                wasm1_code op8{};
                if(!read_op(op8) || op8 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx5{};
                if(!read_u32_leb(idx5) || idx5 != 5u) { return res; }

                wasm1_code op9{};
                if(!read_op(op9) || op9 != wasm1_code::i32_add) { return res; }

                wasm1_code op10{};
                if(!read_op(op10) || op10 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx6{};
                if(!read_u32_leb(idx6) || idx6 != 6u) { return res; }

                wasm1_code op11{};
                if(!read_op(op11) || op11 != wasm1_code::local_get) { return res; }
                ::std::uint32_t idx7{};
                if(!read_u32_leb(idx7) || idx7 != 7u) { return res; }

                wasm1_code op12{};
                wasm1_code op13{};
                wasm1_code op14{};
                if(!read_op(op12) || op12 != wasm1_code::i32_add) { return res; }
                if(!read_op(op13) || op13 != wasm1_code::i32_add) { return res; }
                if(!read_op(op14) || op14 != wasm1_code::i32_add) { return res; }

                wasm1_code op15{};
                if(!read_op(op15) || op15 != wasm1_code::i32_const) { return res; }
                wasm_i32 imm;  // no init
                if(!read_i32_leb(imm)) { return res; }

                wasm1_code op16{};
                wasm1_code op17{};
                if(!read_op(op16) || op16 != wasm1_code::i32_xor) { return res; }
                if(!read_op(op17) || op17 != wasm1_code::end) { return res; }
                if(curr != end) { return res; }

                res.kind = trivial_call_inline_kind::sum8_xor_const_i32;
                res.imm = imm;
                return res;
            }

            return res;
        }

        if(op2 == wasm1_code::i32_xor)
        {
            if(!((idx0 == 0u && idx1 == 1u) || (idx0 == 1u && idx1 == 0u))) { return res; }

            wasm1_code op3{};
            if(!read_op(op3)) { return res; }

            // Pattern B: local.get 0/1 ; local.get 1/0 ; i32.xor ; end
            if(op3 == wasm1_code::end && curr == end)
            {
                res.kind = trivial_call_inline_kind::xor_i32;
                return res;
            }

            // Pattern D: local.get 0/1 ; local.get 1/0 ; i32.xor ; local.get K ; i32.xor ; end
            // If K is a non-parameter local and the body contains no writes, local[K] is always the zero-initialized default (0),
            // so this reduces to `param0 xor param1`.
            if(op3 != wasm1_code::local_get) { return res; }
            ::std::uint32_t idxk{};
            if(!read_u32_leb(idxk) || idxk < 2u) { return res; }

            wasm1_code op4{};
            wasm1_code op5{};
            if(!read_op(op4) || op4 != wasm1_code::i32_xor) { return res; }
            if(!read_op(op5) || op5 != wasm1_code::end) { return res; }
            if(curr != end) { return res; }
            res.kind = trivial_call_inline_kind::xor_i32;
            return res;
        }

        if(op2 == wasm1_code::i32_const)
        {
            if(!(idx0 == 0u && idx1 == 1u)) { return res; }

            wasm_i32 imm;  // no init
            if(!read_i32_leb(imm)) { return res; }

            wasm1_code op3{};
            wasm1_code op4{};
            wasm1_code op5{};
            if(!read_op(op3) || !read_op(op4)) { return res; }
            if(!read_op(op5) || op5 != wasm1_code::end) { return res; }
            if(curr != end) { return res; }

            if(op3 == wasm1_code::i32_xor && op4 == wasm1_code::i32_add)
            {
                res.kind = trivial_call_inline_kind::xor_add_const_i32;
                res.imm = imm;
                return res;
            }
            if(op3 == wasm1_code::i32_or && op4 == wasm1_code::i32_sub)
            {
                res.kind = trivial_call_inline_kind::sub_or_const_i32;
                res.imm = imm;
                return res;
            }
            return res;
        }

        return res;
    }

    inline consteval bool range_enabled(::std::size_t begin_pos, ::std::size_t end_pos) noexcept { return begin_pos != end_pos; }

    inline consteval bool in_range(::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
    { return range_enabled(begin_pos, end_pos) && begin_pos <= pos && pos < end_pos; }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline consteval ::std::size_t interpreter_tuple_size() noexcept
    {
        ::std::size_t max_end{3uz};

        if constexpr(range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos))
        {
            if(CompileOption.i32_stack_top_end_pos > max_end) { max_end = CompileOption.i32_stack_top_end_pos; }
        }
        if constexpr(range_enabled(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos))
        {
            if(CompileOption.i64_stack_top_end_pos > max_end) { max_end = CompileOption.i64_stack_top_end_pos; }
        }
        if constexpr(range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos))
        {
            if(CompileOption.f32_stack_top_end_pos > max_end) { max_end = CompileOption.f32_stack_top_end_pos; }
        }
        if constexpr(range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos))
        {
            if(CompileOption.f64_stack_top_end_pos > max_end) { max_end = CompileOption.f64_stack_top_end_pos; }
        }
        if constexpr(range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos))
        {
            if(CompileOption.v128_stack_top_end_pos > max_end) { max_end = CompileOption.v128_stack_top_end_pos; }
        }

        return max_end;
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline consteval bool interpreter_tuple_has_no_holes() noexcept
    {
        constexpr ::std::size_t end_pos{interpreter_tuple_size<CompileOption>()};
        for(::std::size_t pos{3uz}; pos < end_pos; ++pos)
        {
            bool const hit{in_range(pos, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos) ||
                           in_range(pos, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos) ||
                           in_range(pos, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos) ||
                           in_range(pos, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos) ||
                           in_range(pos, CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)};
            if(!hit) { return false; }
        }
        return true;
    }

    template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    struct interpreter_tuple_arg
    {
        inline static consteval auto pick() noexcept
        {
            if constexpr(I == 0uz) { return ::std::type_identity<::std::byte const*>{}; }
            else if constexpr(I == 1uz) { return ::std::type_identity<::std::byte*>{}; }
            else if constexpr(I == 2uz) { return ::std::type_identity<::std::byte*>{}; }
            else
            {
                constexpr bool i32_hit{in_range(I, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos)};
                constexpr bool i64_hit{in_range(I, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos)};
                constexpr bool f32_hit{in_range(I, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos)};
                constexpr bool f64_hit{in_range(I, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos)};
                constexpr bool v128_hit{in_range(I, CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)};

                constexpr ::std::size_t hit_count{static_cast<::std::size_t>(i32_hit) + static_cast<::std::size_t>(i64_hit) +
                                                  static_cast<::std::size_t>(f32_hit) + static_cast<::std::size_t>(f64_hit) +
                                                  static_cast<::std::size_t>(v128_hit)};

                if constexpr(hit_count == 0uz) { return ::std::type_identity<::std::byte*>{}; }
                else if constexpr(hit_count == 1uz)
                {
                    if constexpr(i32_hit) { return ::std::type_identity<wasm_i32>{}; }
                    else if constexpr(i64_hit) { return ::std::type_identity<wasm_i64>{}; }
                    else if constexpr(f32_hit) { return ::std::type_identity<wasm_f32>{}; }
                    else if constexpr(f64_hit) { return ::std::type_identity<wasm_f64>{}; }
                    else
                    {
                        return ::std::type_identity<wasm_v128>{};
                    }
                }
                else
                {
                    // Merge layouts must be expressed as *fully overlapping* ranges (same begin/end). Partial overlaps are invalid.
                    constexpr bool i32_i64_merge{range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos) &&
                                                 range_enabled(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos) &&
                                                 CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                 CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                    constexpr bool i32_f32_merge{range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos) &&
                                                 range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos) &&
                                                 CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                 CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                    constexpr bool f32_f64_merge{range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos) &&
                                                 range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos) &&
                                                 CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                 CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                    constexpr bool f32_v128_merge{range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos) &&
                                                  range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos) &&
                                                  CompileOption.f32_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                  CompileOption.f32_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};
                    constexpr bool f64_v128_merge{range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos) &&
                                                  range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos) &&
                                                  CompileOption.f64_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                  CompileOption.f64_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};

                    constexpr bool scalar4_merge{i32_i64_merge && i32_f32_merge && f32_f64_merge};
                    constexpr bool f32_f64_v128_merge{f32_f64_merge && f32_v128_merge && f64_v128_merge};

                    if constexpr(i32_hit && i64_hit && f32_hit && f64_hit)
                    {
                        static_assert(scalar4_merge, "i32/i64/f32/f64 overlap must be a fully merged scalar range (same begin/end).");
                        static_assert(!v128_hit, "scalar4 merged range cannot also overlap v128 (unsupported slot layout).");
                        return ::std::type_identity<wasm_stack_top_i32_i64_f32_f64_u>{};
                    }
                    else if constexpr(f32_hit && f64_hit && v128_hit)
                    {
                        static_assert(f32_f64_v128_merge, "f32/f64/v128 overlap must be a fully merged f32/f64/v128 range (same begin/end).");
                        static_assert(!(i32_hit || i64_hit), "f32/f64/v128 merged range must not overlap i32/i64 (unsupported slot layout).");
                        return ::std::type_identity<wasm_v128>{};
                    }
                    else if constexpr(i32_hit && i64_hit)
                    {
                        static_assert(i32_i64_merge, "i32/i64 overlap must be a fully merged i32/i64 range (same begin/end).");
                        static_assert(!(f32_hit || f64_hit || v128_hit), "i32/i64 merged range must not overlap f32/f64/v128 (unsupported slot layout).");
                        return ::std::type_identity<wasm_stack_top_i32_with_i64_u>{};
                    }
                    else if constexpr(i32_hit && f32_hit)
                    {
                        static_assert(i32_f32_merge, "i32/f32 overlap must be a fully merged i32/f32 range (same begin/end).");
                        static_assert(!(i64_hit || f64_hit || v128_hit), "i32/f32 merged range must not overlap i64/f64/v128 (unsupported slot layout).");
                        return ::std::type_identity<wasm_stack_top_i32_with_f32_u>{};
                    }
                    else if constexpr(f32_hit && f64_hit)
                    {
                        static_assert(f32_f64_merge, "f32/f64 overlap must be a fully merged f32/f64 range (same begin/end).");
                        static_assert(!v128_hit, "f32/f64 merged range cannot also overlap v128 unless all three are merged.");
                        static_assert(!(i32_hit || i64_hit), "f32/f64 merged range must not overlap i32/i64 (unsupported slot layout).");
                        return ::std::type_identity<wasm_f64>{};
                    }
                    else
                    {
                        static_assert(hit_count == 0uz, "unsupported stack-top slot overlap/merge layout at this position.");
                        return ::std::type_identity<::std::byte*>{};
                    }
                }
            }
        }

        using type = typename decltype(pick())::type;
    };

    template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    using interpreter_tuple_arg_t = typename interpreter_tuple_arg<I, CompileOption>::type;

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
    consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<interpreter_tuple_arg_t<Is, CompileOption>...>
        interpreter_tailcall_opfunc_ptr(::std::index_sequence<Is...>) noexcept;

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
    consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<interpreter_tuple_arg_t<Is, CompileOption>...>
        interpreter_byref_opfunc_ptr(::std::index_sequence<Is...>) noexcept;

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    using interpreter_tailcall_opfunc_ptr_t =
        decltype(interpreter_tailcall_opfunc_ptr<CompileOption>(::std::make_index_sequence<interpreter_tuple_size<CompileOption>()>{}));

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    using interpreter_byref_opfunc_ptr_t =
        decltype(interpreter_byref_opfunc_ptr<CompileOption>(::std::make_index_sequence<interpreter_tuple_size<CompileOption>()>{}));

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    using interpreter_expected_opfunc_ptr_t =
        ::std::conditional_t<CompileOption.is_tail_call, interpreter_tailcall_opfunc_ptr_t<CompileOption>, interpreter_byref_opfunc_ptr_t<CompileOption>>;

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
    inline consteval ::uwvm2::utils::container::tuple<interpreter_tuple_arg_t<Is, CompileOption>...>
        make_interpreter_tuple(::std::index_sequence<Is...>) noexcept
    { return {}; }
}  // namespace details

