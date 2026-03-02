#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    struct xorshift32
    {
        ::std::uint32_t s;

        [[nodiscard]] ::std::uint32_t next() noexcept
        {
            ::std::uint32_t x = s;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            s = x;
            return x;
        }
    };

    enum class inst_kind : ::std::uint8_t
    {
        i32_const,
        local_get,
        local_set,
        local_tee,
        drop,
        nop,

        add,
        sub,
        mul,
        and_,
        or_,
        xor_,
        shl,
        shr_u,
        shr_s,
        rotl,
        rotr,

        eqz,
        clz,
        ctz,
        popcnt,

        eq,
        ne,
        lt_s,
        lt_u,
        gt_s,
        gt_u,
        le_s,
        le_u,
        ge_s,
        ge_u,

        select,
    };

    struct inst
    {
        inst_kind kind{};
        ::std::uint32_t imm{};  // const value or local index
    };

    using program = ::std::vector<inst>;

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        return (r == 0u) ? x : ((x << r) | (x >> (32u - r)));
    }

    [[nodiscard]] constexpr ::std::uint32_t rotr32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        return (r == 0u) ? x : ((x >> r) | (x << (32u - r)));
    }

    [[nodiscard]] constexpr ::std::uint32_t ashr32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        if(r == 0u) { return x; }
        ::std::uint32_t const ux = x >> r;
        if((x & 0x8000'0000u) == 0u) { return ux; }
        ::std::uint32_t const fill = 0xffff'ffffu << (32u - r);
        return ux | fill;
    }

    [[nodiscard]] ::std::uint32_t simulate(program const& p, ::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        ::std::array<::std::uint32_t, 7uz> locals{};
        locals[0] = static_cast<::std::uint32_t>(a);
        locals[1] = static_cast<::std::uint32_t>(b);
        locals[2] = static_cast<::std::uint32_t>(c);

        ::std::vector<::std::uint32_t> st{};
        st.reserve(32uz);

        auto pop = [&]() noexcept -> ::std::uint32_t
        {
            auto v = st.back();
            st.pop_back();
            return v;
        };
        auto push = [&](::std::uint32_t v) noexcept { st.push_back(v); };

        for(auto const ins : p)
        {
            switch(ins.kind)
            {
                case inst_kind::i32_const:
                    push(ins.imm);
                    break;
                case inst_kind::local_get:
                    push(locals[ins.imm]);
                    break;
                case inst_kind::local_set:
                    locals[ins.imm] = pop();
                    break;
                case inst_kind::local_tee:
                {
                    ::std::uint32_t const v = pop();
                    locals[ins.imm] = v;
                    push(v);
                    break;
                }
                case inst_kind::drop:
                    (void)pop();
                    break;
                case inst_kind::nop:
                    break;

                case inst_kind::add:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs + rhs);
                    break;
                }
                case inst_kind::sub:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs - rhs);
                    break;
                }
                case inst_kind::mul:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs * rhs);
                    break;
                }
                case inst_kind::and_:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs & rhs);
                    break;
                }
                case inst_kind::or_:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs | rhs);
                    break;
                }
                case inst_kind::xor_:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs ^ rhs);
                    break;
                }
                case inst_kind::shl:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs << (rhs & 31u));
                    break;
                }
                case inst_kind::shr_u:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs >> (rhs & 31u));
                    break;
                }
                case inst_kind::shr_s:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(ashr32(lhs, rhs));
                    break;
                }
                case inst_kind::rotl:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(rotl32(lhs, rhs));
                    break;
                }
                case inst_kind::rotr:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(rotr32(lhs, rhs));
                    break;
                }

                case inst_kind::eqz:
                {
                    ::std::uint32_t const v = pop();
                    push(v == 0u ? 1u : 0u);
                    break;
                }
                case inst_kind::clz:
                {
                    ::std::uint32_t const v = pop();
                    push(static_cast<::std::uint32_t>(::std::countl_zero(v)));
                    break;
                }
                case inst_kind::ctz:
                {
                    ::std::uint32_t const v = pop();
                    push(static_cast<::std::uint32_t>(::std::countr_zero(v)));
                    break;
                }
                case inst_kind::popcnt:
                {
                    ::std::uint32_t const v = pop();
                    push(static_cast<::std::uint32_t>(::std::popcount(v)));
                    break;
                }

                case inst_kind::eq:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs == rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::ne:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs != rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::lt_s:
                {
                    ::std::int32_t const rhs = static_cast<::std::int32_t>(pop());
                    ::std::int32_t const lhs = static_cast<::std::int32_t>(pop());
                    push(lhs < rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::lt_u:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs < rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::gt_s:
                {
                    ::std::int32_t const rhs = static_cast<::std::int32_t>(pop());
                    ::std::int32_t const lhs = static_cast<::std::int32_t>(pop());
                    push(lhs > rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::gt_u:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs > rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::le_s:
                {
                    ::std::int32_t const rhs = static_cast<::std::int32_t>(pop());
                    ::std::int32_t const lhs = static_cast<::std::int32_t>(pop());
                    push(lhs <= rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::le_u:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs <= rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::ge_s:
                {
                    ::std::int32_t const rhs = static_cast<::std::int32_t>(pop());
                    ::std::int32_t const lhs = static_cast<::std::int32_t>(pop());
                    push(lhs >= rhs ? 1u : 0u);
                    break;
                }
                case inst_kind::ge_u:
                {
                    ::std::uint32_t const rhs = pop();
                    ::std::uint32_t const lhs = pop();
                    push(lhs >= rhs ? 1u : 0u);
                    break;
                }

                case inst_kind::select:
                {
                    ::std::uint32_t const cond = pop();
                    ::std::uint32_t const v2 = pop();
                    ::std::uint32_t const v1 = pop();
                    push(cond != 0u ? v1 : v2);
                    break;
                }
            }
        }

        if(st.size() != 1uz) { ::fast_io::fast_terminate(); }
        return st.back();
    }

    [[nodiscard]] program make_program(::std::uint32_t seed)
    {
        xorshift32 rng{seed == 0u ? 0x1234abcdU : seed};

        program p{};
        p.reserve(96uz);

        auto emit = [&](inst_kind k, ::std::uint32_t imm = 0u) { p.push_back(inst{k, imm}); };

        int depth{};
        auto push_const = [&](::std::uint32_t v)
        {
            emit(inst_kind::i32_const, v);
            ++depth;
        };
        auto push_local = [&](::std::uint32_t idx)
        {
            emit(inst_kind::local_get, idx);
            ++depth;
        };
        auto do_drop = [&]()
        {
            emit(inst_kind::drop);
            --depth;
        };
        auto do_unop = [&](inst_kind k)
        {
            emit(k);
        };
        auto do_binop = [&](inst_kind k)
        {
            emit(k);
            --depth;
        };
        auto do_select = [&]()
        {
            emit(inst_kind::select);
            depth -= 2;
        };

        auto pick_local = [&]() noexcept -> ::std::uint32_t
        {
            return rng.next() % 7u;  // 0..6 (3 params + 4 locals)
        };

        // Prologue: force some structured patterns (select, and delay-local local.get+nop+local.get+add+local.tee).
        push_local(0u);
        push_local(1u);
        push_local(2u);
        do_select();  // depth: 1

        push_local(0u);           // depth: 2
        emit(inst_kind::nop);
        push_local(1u);           // depth: 3
        do_binop(inst_kind::add); // depth: 2
        emit(inst_kind::local_tee, 3u);  // local3
        do_drop();                       // depth: 1
        push_local(3u);                  // depth: 2

        // Random-ish body.
        constexpr int k_steps = 64;
        for(int i = 0; i < k_steps; ++i)
        {
            if(depth == 0)
            {
                if((rng.next() & 1u) != 0u) { push_local(pick_local()); }
                else { push_const(rng.next()); }
                continue;
            }

            ::std::uint32_t const r = rng.next();
            ::std::uint32_t const action = r % 100u;

            if(action < 10u)
            {
                emit(inst_kind::nop);
                continue;
            }

            if(action < 30u && depth < 12)
            {
                if((r & 1u) != 0u) { push_local(pick_local()); }
                else { push_const(rng.next()); }
                continue;
            }

            if(action < 45u)
            {
                // local.set / local.tee
                ::std::uint32_t const idx = pick_local();
                if((r & 1u) != 0u)
                {
                    emit(inst_kind::local_set, idx);
                    --depth;
                }
                else
                {
                    emit(inst_kind::local_tee, idx);
                }
                continue;
            }

            if(action < 55u)
            {
                // unary ops
                switch((r >> 8) % 4u)
                {
                    case 0u: do_unop(inst_kind::eqz); break;
                    case 1u: do_unop(inst_kind::clz); break;
                    case 2u: do_unop(inst_kind::ctz); break;
                    default: do_unop(inst_kind::popcnt); break;
                }
                continue;
            }

            if(action < 65u)
            {
                // drop (keep stack in check)
                do_drop();
                continue;
            }

            if(action < 75u && depth >= 3)
            {
                do_select();
                continue;
            }

            if(depth < 2)
            {
                // Need more operands.
                push_local(pick_local());
                continue;
            }

            // binary ops / comparisons
            switch((r >> 10) % 16u)
            {
                case 0u: do_binop(inst_kind::add); break;
                case 1u: do_binop(inst_kind::sub); break;
                case 2u: do_binop(inst_kind::mul); break;
                case 3u: do_binop(inst_kind::and_); break;
                case 4u: do_binop(inst_kind::or_); break;
                case 5u: do_binop(inst_kind::xor_); break;
                case 6u: do_binop(inst_kind::shl); break;
                case 7u: do_binop(inst_kind::shr_u); break;
                case 8u: do_binop(inst_kind::shr_s); break;
                case 9u: do_binop(inst_kind::rotl); break;
                case 10u: do_binop(inst_kind::rotr); break;
                case 11u: do_binop(inst_kind::eq); break;
                case 12u: do_binop(inst_kind::ne); break;
                case 13u: do_binop(inst_kind::lt_s); break;
                case 14u: do_binop(inst_kind::lt_u); break;
                default: do_binop(inst_kind::ge_u); break;
            }
        }

        if(depth == 0) { push_local(0u); }
        while(depth > 1)
        {
            do_binop(inst_kind::add);
        }
        return p;
    }

    inline void emit_wasm(program const& p, byte_vec& code)
    {
        auto op = [&](wasm_op o) { append_u8(code, u8(o)); };
        auto u32 = [&](::std::uint32_t v) { append_u32_leb(code, v); };
        auto i32 = [&](::std::int32_t v) { append_i32_leb(code, v); };

        for(auto const ins : p)
        {
            switch(ins.kind)
            {
                case inst_kind::i32_const:
                    op(wasm_op::i32_const);
                    i32(static_cast<::std::int32_t>(ins.imm));
                    break;
                case inst_kind::local_get:
                    op(wasm_op::local_get);
                    u32(ins.imm);
                    break;
                case inst_kind::local_set:
                    op(wasm_op::local_set);
                    u32(ins.imm);
                    break;
                case inst_kind::local_tee:
                    op(wasm_op::local_tee);
                    u32(ins.imm);
                    break;
                case inst_kind::drop:
                    op(wasm_op::drop);
                    break;
                case inst_kind::nop:
                    op(wasm_op::nop);
                    break;
                case inst_kind::add:
                    op(wasm_op::i32_add);
                    break;
                case inst_kind::sub:
                    op(wasm_op::i32_sub);
                    break;
                case inst_kind::mul:
                    op(wasm_op::i32_mul);
                    break;
                case inst_kind::and_:
                    op(wasm_op::i32_and);
                    break;
                case inst_kind::or_:
                    op(wasm_op::i32_or);
                    break;
                case inst_kind::xor_:
                    op(wasm_op::i32_xor);
                    break;
                case inst_kind::shl:
                    op(wasm_op::i32_shl);
                    break;
                case inst_kind::shr_u:
                    op(wasm_op::i32_shr_u);
                    break;
                case inst_kind::shr_s:
                    op(wasm_op::i32_shr_s);
                    break;
                case inst_kind::rotl:
                    op(wasm_op::i32_rotl);
                    break;
                case inst_kind::rotr:
                    op(wasm_op::i32_rotr);
                    break;
                case inst_kind::eqz:
                    op(wasm_op::i32_eqz);
                    break;
                case inst_kind::clz:
                    op(wasm_op::i32_clz);
                    break;
                case inst_kind::ctz:
                    op(wasm_op::i32_ctz);
                    break;
                case inst_kind::popcnt:
                    op(wasm_op::i32_popcnt);
                    break;
                case inst_kind::eq:
                    op(wasm_op::i32_eq);
                    break;
                case inst_kind::ne:
                    op(wasm_op::i32_ne);
                    break;
                case inst_kind::lt_s:
                    op(wasm_op::i32_lt_s);
                    break;
                case inst_kind::lt_u:
                    op(wasm_op::i32_lt_u);
                    break;
                case inst_kind::gt_s:
                    op(wasm_op::i32_gt_s);
                    break;
                case inst_kind::gt_u:
                    op(wasm_op::i32_gt_u);
                    break;
                case inst_kind::le_s:
                    op(wasm_op::i32_le_s);
                    break;
                case inst_kind::le_u:
                    op(wasm_op::i32_le_u);
                    break;
                case inst_kind::ge_s:
                    op(wasm_op::i32_ge_s);
                    break;
                case inst_kind::ge_u:
                    op(wasm_op::i32_ge_u);
                    break;
                case inst_kind::select:
                    op(wasm_op::select);
                    break;
            }
        }
    }

    struct func_spec
    {
        program prog{};
        ::std::array<::std::array<::std::int32_t, 3uz>, 3uz> params{};
        ::std::array<::std::uint32_t, 3uz> expected{};
    };

    [[nodiscard]] byte_vec pack_i32x3(::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_random_i32_module(::std::vector<func_spec> const& funcs)
    {
        module_builder mb{};

        for(auto const& f : funcs)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({4u, k_val_i32});  // local3..6
            emit_wasm(f.prog, fb.code);
            append_u8(fb.code, u8(wasm_op::end));
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_differential_suite(runtime_module_t const& rt, ::std::vector<func_spec> const& funcs) noexcept
    {
        if constexpr(Opt.is_tail_call)
        {
            static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>());
        }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        for(::std::size_t fi{}; fi < funcs.size(); ++fi)
        {
            for(::std::size_t ci{}; ci < funcs[fi].params.size(); ++ci)
            {
                auto const& p = funcs[fi].params[ci];
                auto rr = Runner::run(cm.local_funcs.index_unchecked(fi),
                                      rt.local_defined_function_vec_storage.index_unchecked(fi),
                                      pack_i32x3(p[0], p[1], p[2]),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == funcs[fi].expected[ci]);
            }
        }

        return 0;
    }

    [[nodiscard]] int test_translate_differential_random_i32() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        ::std::vector<func_spec> funcs{};
        funcs.reserve(24uz);

        for(::std::uint32_t i = 0; i < 24u; ++i)
        {
            ::std::uint32_t const seed = 0xC0DE0000u ^ (i * 0x9E37'79B9u);

            func_spec fs{};
            fs.prog = make_program(seed);

            xorshift32 prng{seed ^ 0xA5A5A5A5u};
            fs.params[0] = {static_cast<::std::int32_t>(seed),
                            static_cast<::std::int32_t>(seed ^ 0x1357'9BDFu),
                            0};  // select: false path
            fs.params[1] = {static_cast<::std::int32_t>(~seed),
                            static_cast<::std::int32_t>(seed + 0x2468'ACE0u),
                            1};  // select: true path
            fs.params[2] = {static_cast<::std::int32_t>(prng.next()),
                            static_cast<::std::int32_t>(prng.next()),
                            static_cast<::std::int32_t>(prng.next())};

            for(::std::size_t ci = 0; ci < fs.params.size(); ++ci)
            {
                auto const& p = fs.params[ci];
                fs.expected[ci] = simulate(fs.prog, p[0], p[1], p[2]);
            }

            funcs.push_back(::std::move(fs));
        }

        auto wasm = build_random_i32_module(funcs);
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_diff_random_i32");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_differential_suite<opt>(rt, funcs) == 0);
        }

        // tailcall (no cache)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_differential_suite<opt>(rt, funcs) == 0);
        }

        // tailcall + stacktop caching (small merged scalar4)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_differential_suite<opt>(rt, funcs) == 0);
        }

        // tailcall + stacktop caching (fully split rings)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 9uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_differential_suite<opt>(rt, funcs) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_differential_random_i32();
}
