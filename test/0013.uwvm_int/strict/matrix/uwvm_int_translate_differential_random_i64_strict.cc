#include "../uwvm_int_translate_strict_common.h"

#include <cstdlib>

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
        i64_const,
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

        clz,
        ctz,
        popcnt,

        // i64 comparisons: push i64(0/1) by emitting `i64.cmp` + `i64.extend_i32_u`.
        eqz,
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

        // i64 -> i32.wrap_i64 -> i64.extend_i32_{s,u} (keeps stack type i64).
        wrap_extend_s,
        wrap_extend_u,

        // i64 select: consumes v1,v2,cond_src and selects based on (cond_src != 0).
        select,
    };

    struct inst
    {
        inst_kind kind{};
        ::std::uint64_t imm{};  // i64.const bits or local index
    };

    using program = ::std::vector<inst>;

    [[nodiscard]] bool is_coverage_run() noexcept
    {
        return ::std::getenv("LLVM_PROFILE_FILE") != nullptr;
    }

    [[nodiscard]] constexpr ::std::uint64_t rotl64(::std::uint64_t x, ::std::uint64_t r) noexcept
    {
        r &= 63u;
        return (r == 0u) ? x : ((x << r) | (x >> (64u - r)));
    }

    [[nodiscard]] constexpr ::std::uint64_t rotr64(::std::uint64_t x, ::std::uint64_t r) noexcept
    {
        r &= 63u;
        return (r == 0u) ? x : ((x >> r) | (x << (64u - r)));
    }

    [[nodiscard]] constexpr ::std::uint64_t ashr64(::std::uint64_t x, ::std::uint64_t r) noexcept
    {
        r &= 63u;
        if(r == 0u) { return x; }
        ::std::uint64_t const ux = x >> r;
        if((x & 0x8000'0000'0000'0000ull) == 0ull) { return ux; }
        ::std::uint64_t const fill = 0xffff'ffff'ffff'ffffull << (64u - r);
        return ux | fill;
    }

    [[nodiscard]] ::std::uint64_t simulate(program const& p, ::std::int64_t a, ::std::int64_t b, ::std::int64_t c)
    {
        ::std::array<::std::uint64_t, 7uz> locals{};
        locals[0] = ::std::bit_cast<::std::uint64_t>(a);
        locals[1] = ::std::bit_cast<::std::uint64_t>(b);
        locals[2] = ::std::bit_cast<::std::uint64_t>(c);

        ::std::vector<::std::uint64_t> st{};
        st.reserve(32uz);

        auto pop = [&]() noexcept -> ::std::uint64_t
        {
            auto v = st.back();
            st.pop_back();
            return v;
        };
        auto push = [&](::std::uint64_t v) noexcept { st.push_back(v); };

        auto clz = [&](::std::uint64_t x) noexcept -> ::std::uint64_t
        {
            if(x == 0ull) { return 64ull; }
            return static_cast<::std::uint64_t>(__builtin_clzll(x));
        };
        auto ctz = [&](::std::uint64_t x) noexcept -> ::std::uint64_t
        {
            if(x == 0ull) { return 64ull; }
            return static_cast<::std::uint64_t>(__builtin_ctzll(x));
        };
        auto popcnt = [&](::std::uint64_t x) noexcept -> ::std::uint64_t
        { return static_cast<::std::uint64_t>(__builtin_popcountll(x)); };

        auto cmp_s = [&](::std::uint64_t x, ::std::uint64_t y) noexcept -> ::std::pair<::std::int64_t, ::std::int64_t>
        {
            return {::std::bit_cast<::std::int64_t>(x), ::std::bit_cast<::std::int64_t>(y)};
        };

        for(auto const ins : p)
        {
            switch(ins.kind)
            {
                case inst_kind::i64_const:
                    push(ins.imm);
                    break;
                case inst_kind::local_get:
                    push(locals[static_cast<::std::size_t>(ins.imm % 7ull)]);
                    break;
                case inst_kind::local_set:
                {
                    locals[static_cast<::std::size_t>(ins.imm % 7ull)] = pop();
                    break;
                }
                case inst_kind::local_tee:
                {
                    ::std::uint64_t const v = st.back();
                    locals[static_cast<::std::size_t>(ins.imm % 7ull)] = v;
                    break;
                }
                case inst_kind::drop:
                    (void)pop();
                    break;
                case inst_kind::nop:
                    break;

                case inst_kind::add:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 + b2);
                    break;
                }
                case inst_kind::sub:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 - b2);
                    break;
                }
                case inst_kind::mul:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 * b2);
                    break;
                }
                case inst_kind::and_:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 & b2);
                    break;
                }
                case inst_kind::or_:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 | b2);
                    break;
                }
                case inst_kind::xor_:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 ^ b2);
                    break;
                }
                case inst_kind::shl:
                {
                    ::std::uint64_t const r = pop();
                    ::std::uint64_t const x = pop();
                    push(x << (r & 63u));
                    break;
                }
                case inst_kind::shr_u:
                {
                    ::std::uint64_t const r = pop();
                    ::std::uint64_t const x = pop();
                    push(x >> (r & 63u));
                    break;
                }
                case inst_kind::shr_s:
                {
                    ::std::uint64_t const r = pop();
                    ::std::uint64_t const x = pop();
                    push(ashr64(x, r));
                    break;
                }
                case inst_kind::rotl:
                {
                    ::std::uint64_t const r = pop();
                    ::std::uint64_t const x = pop();
                    push(rotl64(x, r));
                    break;
                }
                case inst_kind::rotr:
                {
                    ::std::uint64_t const r = pop();
                    ::std::uint64_t const x = pop();
                    push(rotr64(x, r));
                    break;
                }

                case inst_kind::clz:
                {
                    ::std::uint64_t const x = pop();
                    push(clz(x));
                    break;
                }
                case inst_kind::ctz:
                {
                    ::std::uint64_t const x = pop();
                    push(ctz(x));
                    break;
                }
                case inst_kind::popcnt:
                {
                    ::std::uint64_t const x = pop();
                    push(popcnt(x));
                    break;
                }

                case inst_kind::eqz:
                {
                    ::std::uint64_t const x = pop();
                    push(x == 0ull ? 1ull : 0ull);
                    break;
                }
                case inst_kind::eq:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 == b2 ? 1ull : 0ull);
                    break;
                }
                case inst_kind::ne:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 != b2 ? 1ull : 0ull);
                    break;
                }
                case inst_kind::lt_s:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    auto const [as, bs] = cmp_s(a2, b2);
                    push(as < bs ? 1ull : 0ull);
                    break;
                }
                case inst_kind::lt_u:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 < b2 ? 1ull : 0ull);
                    break;
                }
                case inst_kind::gt_s:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    auto const [as, bs] = cmp_s(a2, b2);
                    push(as > bs ? 1ull : 0ull);
                    break;
                }
                case inst_kind::gt_u:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 > b2 ? 1ull : 0ull);
                    break;
                }
                case inst_kind::le_s:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    auto const [as, bs] = cmp_s(a2, b2);
                    push(as <= bs ? 1ull : 0ull);
                    break;
                }
                case inst_kind::le_u:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 <= b2 ? 1ull : 0ull);
                    break;
                }
                case inst_kind::ge_s:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    auto const [as, bs] = cmp_s(a2, b2);
                    push(as >= bs ? 1ull : 0ull);
                    break;
                }
                case inst_kind::ge_u:
                {
                    ::std::uint64_t const b2 = pop();
                    ::std::uint64_t const a2 = pop();
                    push(a2 >= b2 ? 1ull : 0ull);
                    break;
                }

                case inst_kind::wrap_extend_s:
                {
                    ::std::uint64_t const x = pop();
                    auto const lo_u32 = static_cast<::std::uint32_t>(x);
                    auto const lo_i32 = ::std::bit_cast<::std::int32_t>(lo_u32);
                    push(::std::bit_cast<::std::uint64_t>(static_cast<::std::int64_t>(lo_i32)));
                    break;
                }
                case inst_kind::wrap_extend_u:
                {
                    ::std::uint64_t const x = pop();
                    push(static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(x)));
                    break;
                }

                case inst_kind::select:
                {
                    ::std::uint64_t const cond_src = pop();
                    ::std::uint64_t const v2 = pop();
                    ::std::uint64_t const v1 = pop();
                    push(cond_src != 0ull ? v1 : v2);
                    break;
                }
            }
        }

        if(st.size() != 1uz) { ::fast_io::fast_terminate(); }
        return st.back();
    }

    [[nodiscard]] program make_program(::std::uint32_t seed, int steps)
    {
        xorshift32 rng{seed == 0u ? 0x1234abcdU : seed};

        if(steps < 1) { steps = 1; }
        program p{};
        p.reserve(static_cast<::std::size_t>(steps) + 64uz);

        auto emit = [&](inst_kind k, ::std::uint64_t imm = 0u) { p.push_back(inst{k, imm}); };

        int depth{};
        auto push_const = [&]()
        {
            ::std::uint64_t const u = (static_cast<::std::uint64_t>(rng.next()) << 32) | static_cast<::std::uint64_t>(rng.next());
            emit(inst_kind::i64_const, u);
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

        // Prologue: force a select and a delay-local shape: local.get+nop+local.get+add+local.tee.
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

        for(int i = 0; i < steps; ++i)
        {
            if(depth == 0)
            {
                if((rng.next() & 1u) != 0u) { push_local(pick_local()); }
                else { push_const(); }
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
                else { push_const(); }
                continue;
            }

            if(action < 45u)
            {
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

            if(action < 58u)
            {
                switch((r >> 8) % 6u)
                {
                    case 0u: do_unop(inst_kind::eqz); break;
                    case 1u: do_unop(inst_kind::clz); break;
                    case 2u: do_unop(inst_kind::ctz); break;
                    case 3u: do_unop(inst_kind::popcnt); break;
                    case 4u: do_unop(inst_kind::wrap_extend_s); break;
                    default: do_unop(inst_kind::wrap_extend_u); break;
                }
                continue;
            }

            if(action < 68u)
            {
                do_drop();
                continue;
            }

            if(action < 78u && depth >= 3)
            {
                do_select();
                continue;
            }

            if(depth < 2)
            {
                push_local(pick_local());
                continue;
            }

            switch((r >> 10) % 22u)
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
                case 15u: do_binop(inst_kind::gt_s); break;
                case 16u: do_binop(inst_kind::gt_u); break;
                case 17u: do_binop(inst_kind::le_s); break;
                case 18u: do_binop(inst_kind::le_u); break;
                case 19u: do_binop(inst_kind::ge_s); break;
                case 20u: do_binop(inst_kind::ge_u); break;
                default: do_binop(inst_kind::add); break;
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
        auto i64 = [&](::std::int64_t v) { append_i64_leb(code, v); };

        for(auto const ins : p)
        {
            switch(ins.kind)
            {
                case inst_kind::i64_const:
                    op(wasm_op::i64_const);
                    i64(::std::bit_cast<::std::int64_t>(ins.imm));
                    break;
                case inst_kind::local_get:
                    op(wasm_op::local_get);
                    u32(static_cast<::std::uint32_t>(ins.imm));
                    break;
                case inst_kind::local_set:
                    op(wasm_op::local_set);
                    u32(static_cast<::std::uint32_t>(ins.imm));
                    break;
                case inst_kind::local_tee:
                    op(wasm_op::local_tee);
                    u32(static_cast<::std::uint32_t>(ins.imm));
                    break;
                case inst_kind::drop:
                    op(wasm_op::drop);
                    break;
                case inst_kind::nop:
                    op(wasm_op::nop);
                    break;

                case inst_kind::add:
                    op(wasm_op::i64_add);
                    break;
                case inst_kind::sub:
                    op(wasm_op::i64_sub);
                    break;
                case inst_kind::mul:
                    op(wasm_op::i64_mul);
                    break;
                case inst_kind::and_:
                    op(wasm_op::i64_and);
                    break;
                case inst_kind::or_:
                    op(wasm_op::i64_or);
                    break;
                case inst_kind::xor_:
                    op(wasm_op::i64_xor);
                    break;
                case inst_kind::shl:
                    op(wasm_op::i64_shl);
                    break;
                case inst_kind::shr_u:
                    op(wasm_op::i64_shr_u);
                    break;
                case inst_kind::shr_s:
                    op(wasm_op::i64_shr_s);
                    break;
                case inst_kind::rotl:
                    op(wasm_op::i64_rotl);
                    break;
                case inst_kind::rotr:
                    op(wasm_op::i64_rotr);
                    break;

                case inst_kind::clz:
                    op(wasm_op::i64_clz);
                    break;
                case inst_kind::ctz:
                    op(wasm_op::i64_ctz);
                    break;
                case inst_kind::popcnt:
                    op(wasm_op::i64_popcnt);
                    break;

                case inst_kind::eqz:
                    op(wasm_op::i64_eqz);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::eq:
                    op(wasm_op::i64_eq);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::ne:
                    op(wasm_op::i64_ne);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::lt_s:
                    op(wasm_op::i64_lt_s);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::lt_u:
                    op(wasm_op::i64_lt_u);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::gt_s:
                    op(wasm_op::i64_gt_s);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::gt_u:
                    op(wasm_op::i64_gt_u);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::le_s:
                    op(wasm_op::i64_le_s);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::le_u:
                    op(wasm_op::i64_le_u);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::ge_s:
                    op(wasm_op::i64_ge_s);
                    op(wasm_op::i64_extend_i32_u);
                    break;
                case inst_kind::ge_u:
                    op(wasm_op::i64_ge_u);
                    op(wasm_op::i64_extend_i32_u);
                    break;

                case inst_kind::wrap_extend_s:
                    op(wasm_op::i32_wrap_i64);
                    op(wasm_op::i64_extend_i32_s);
                    break;
                case inst_kind::wrap_extend_u:
                    op(wasm_op::i32_wrap_i64);
                    op(wasm_op::i64_extend_i32_u);
                    break;

                case inst_kind::select:
                    op(wasm_op::i64_eqz);
                    op(wasm_op::i32_eqz);
                    op(wasm_op::select);
                    break;
            }
        }
    }

    struct func_spec
    {
        ::std::uint32_t seed{};
        program prog{};
        ::std::array<::std::array<::std::int64_t, 3uz>, 3uz> params{};
        ::std::array<::std::uint64_t, 3uz> expected{};
    };

    [[nodiscard]] char const* name(inst_kind k) noexcept
    {
        switch(k)
        {
            case inst_kind::i64_const: return "i64_const";
            case inst_kind::local_get: return "local_get";
            case inst_kind::local_set: return "local_set";
            case inst_kind::local_tee: return "local_tee";
            case inst_kind::drop: return "drop";
            case inst_kind::nop: return "nop";

            case inst_kind::add: return "add";
            case inst_kind::sub: return "sub";
            case inst_kind::mul: return "mul";
            case inst_kind::and_: return "and";
            case inst_kind::or_: return "or";
            case inst_kind::xor_: return "xor";
            case inst_kind::shl: return "shl";
            case inst_kind::shr_u: return "shr_u";
            case inst_kind::shr_s: return "shr_s";
            case inst_kind::rotl: return "rotl";
            case inst_kind::rotr: return "rotr";

            case inst_kind::clz: return "clz";
            case inst_kind::ctz: return "ctz";
            case inst_kind::popcnt: return "popcnt";

            case inst_kind::eqz: return "eqz";
            case inst_kind::eq: return "eq";
            case inst_kind::ne: return "ne";
            case inst_kind::lt_s: return "lt_s";
            case inst_kind::lt_u: return "lt_u";
            case inst_kind::gt_s: return "gt_s";
            case inst_kind::gt_u: return "gt_u";
            case inst_kind::le_s: return "le_s";
            case inst_kind::le_u: return "le_u";
            case inst_kind::ge_s: return "ge_s";
            case inst_kind::ge_u: return "ge_u";

            case inst_kind::wrap_extend_s: return "wrap_ext_s";
            case inst_kind::wrap_extend_u: return "wrap_ext_u";

            case inst_kind::select: return "select";
        }
        return "?";
    }

    [[nodiscard]] constexpr ::std::size_t opcalls_for(inst_kind k) noexcept
    {
        switch(k)
        {
            case inst_kind::eqz:
            case inst_kind::eq:
            case inst_kind::ne:
            case inst_kind::lt_s:
            case inst_kind::lt_u:
            case inst_kind::gt_s:
            case inst_kind::gt_u:
            case inst_kind::le_s:
            case inst_kind::le_u:
            case inst_kind::ge_s:
            case inst_kind::ge_u:
            case inst_kind::wrap_extend_s:
            case inst_kind::wrap_extend_u:
                return 2uz;
            case inst_kind::select:
                return 3uz;
            default:
                return 1uz;
        }
    }

    struct sim_state
    {
        ::std::array<::std::uint64_t, 7uz> locals{};
        ::std::vector<::std::uint64_t> st{};
    };

    static void sim_step(sim_state& s, inst const ins) noexcept
    {
        auto pop = [&]() noexcept -> ::std::uint64_t
        {
            auto v = s.st.back();
            s.st.pop_back();
            return v;
        };
        auto push = [&](::std::uint64_t v) noexcept { s.st.push_back(v); };

        auto cmp_s = [&](::std::uint64_t x, ::std::uint64_t y) noexcept -> ::std::pair<::std::int64_t, ::std::int64_t>
        {
            return {::std::bit_cast<::std::int64_t>(x), ::std::bit_cast<::std::int64_t>(y)};
        };

        switch(ins.kind)
        {
            case inst_kind::i64_const:
                push(ins.imm);
                break;
            case inst_kind::local_get:
                push(s.locals[static_cast<::std::size_t>(ins.imm % 7ull)]);
                break;
            case inst_kind::local_set:
                s.locals[static_cast<::std::size_t>(ins.imm % 7ull)] = pop();
                break;
            case inst_kind::local_tee:
                s.locals[static_cast<::std::size_t>(ins.imm % 7ull)] = s.st.back();
                break;
            case inst_kind::drop:
                (void)pop();
                break;
            case inst_kind::nop:
                break;

            case inst_kind::add:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 + b2);
                break;
            }
            case inst_kind::sub:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 - b2);
                break;
            }
            case inst_kind::mul:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 * b2);
                break;
            }
            case inst_kind::and_:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 & b2);
                break;
            }
            case inst_kind::or_:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 | b2);
                break;
            }
            case inst_kind::xor_:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 ^ b2);
                break;
            }
            case inst_kind::shl:
            {
                ::std::uint64_t const r = pop();
                ::std::uint64_t const x = pop();
                push(x << (r & 63u));
                break;
            }
            case inst_kind::shr_u:
            {
                ::std::uint64_t const r = pop();
                ::std::uint64_t const x = pop();
                push(x >> (r & 63u));
                break;
            }
            case inst_kind::shr_s:
            {
                ::std::uint64_t const r = pop();
                ::std::uint64_t const x = pop();
                push(ashr64(x, r));
                break;
            }
            case inst_kind::rotl:
            {
                ::std::uint64_t const r = pop();
                ::std::uint64_t const x = pop();
                push(rotl64(x, r));
                break;
            }
            case inst_kind::rotr:
            {
                ::std::uint64_t const r = pop();
                ::std::uint64_t const x = pop();
                push(rotr64(x, r));
                break;
            }

            case inst_kind::clz:
            {
                ::std::uint64_t const x = pop();
                ::std::uint64_t const out = (x == 0ull) ? 64ull : static_cast<::std::uint64_t>(__builtin_clzll(x));
                push(out);
                break;
            }
            case inst_kind::ctz:
            {
                ::std::uint64_t const x = pop();
                ::std::uint64_t const out = (x == 0ull) ? 64ull : static_cast<::std::uint64_t>(__builtin_ctzll(x));
                push(out);
                break;
            }
            case inst_kind::popcnt:
            {
                ::std::uint64_t const x = pop();
                push(static_cast<::std::uint64_t>(__builtin_popcountll(x)));
                break;
            }

            case inst_kind::eqz:
            {
                ::std::uint64_t const x = pop();
                push(x == 0ull ? 1ull : 0ull);
                break;
            }
            case inst_kind::eq:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 == b2 ? 1ull : 0ull);
                break;
            }
            case inst_kind::ne:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 != b2 ? 1ull : 0ull);
                break;
            }
            case inst_kind::lt_s:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                auto const [as, bs] = cmp_s(a2, b2);
                push(as < bs ? 1ull : 0ull);
                break;
            }
            case inst_kind::lt_u:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 < b2 ? 1ull : 0ull);
                break;
            }
            case inst_kind::gt_s:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                auto const [as, bs] = cmp_s(a2, b2);
                push(as > bs ? 1ull : 0ull);
                break;
            }
            case inst_kind::gt_u:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 > b2 ? 1ull : 0ull);
                break;
            }
            case inst_kind::le_s:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                auto const [as, bs] = cmp_s(a2, b2);
                push(as <= bs ? 1ull : 0ull);
                break;
            }
            case inst_kind::le_u:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 <= b2 ? 1ull : 0ull);
                break;
            }
            case inst_kind::ge_s:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                auto const [as, bs] = cmp_s(a2, b2);
                push(as >= bs ? 1ull : 0ull);
                break;
            }
            case inst_kind::ge_u:
            {
                ::std::uint64_t const b2 = pop();
                ::std::uint64_t const a2 = pop();
                push(a2 >= b2 ? 1ull : 0ull);
                break;
            }

            case inst_kind::wrap_extend_s:
            {
                ::std::uint64_t const x = pop();
                auto const lo_u32 = static_cast<::std::uint32_t>(x);
                auto const lo_i32 = ::std::bit_cast<::std::int32_t>(lo_u32);
                push(::std::bit_cast<::std::uint64_t>(static_cast<::std::int64_t>(lo_i32)));
                break;
            }
            case inst_kind::wrap_extend_u:
            {
                ::std::uint64_t const x = pop();
                push(static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(x)));
                break;
            }

            case inst_kind::select:
            {
                ::std::uint64_t const cond_src = pop();
                ::std::uint64_t const v2 = pop();
                ::std::uint64_t const v1 = pop();
                push(cond_src != 0ull ? v1 : v2);
                break;
            }
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    static void diagnose_mismatch_byref(compiled_local_func_t const& fn,
                                        runtime_local_func_t const& rt_fn,
                                        byte_vec const& packed_params,
                                        program const& prog) noexcept
    {
        if constexpr(Opt.is_tail_call) { return; }

        auto const* const ft = rt_fn.function_type_ptr;
        if(ft == nullptr) { ::fast_io::fast_terminate(); }

        auto const param_bytes = abi_total_bytes(ft->parameter.begin, ft->parameter.end);
        if(param_bytes != packed_params.size()) { ::fast_io::fast_terminate(); }

        constexpr ::std::size_t k_align = 16uz;
        auto align_up = [](byte_vec& buf) noexcept -> ::std::byte*
        {
            auto const p = reinterpret_cast<::std::uintptr_t>(buf.data());
            auto const a = (p + (k_align - 1uz)) & ~(k_align - 1uz);
            return reinterpret_cast<::std::byte*>(a);
        };

        byte_vec local_buf(fn.local_bytes_max + k_align);
        ::std::memset(local_buf.data(), 0xCD, local_buf.size());
        ::std::byte* local_base = align_up(local_buf);
        if(fn.local_bytes_zeroinit_end > fn.local_bytes_max) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(fn.local_bytes_zeroinit_end != 0uz) { ::std::memset(local_base, 0, fn.local_bytes_zeroinit_end); }
        if(param_bytes != 0uz) { ::std::memcpy(local_base, packed_params.data(), param_bytes); }

        byte_vec stack_buf((fn.operand_stack_byte_max == 0uz ? 64uz : fn.operand_stack_byte_max) + k_align);
        ::std::memset(stack_buf.data(), 0xCC, stack_buf.size());
        ::std::byte* operand_base = align_up(stack_buf);

        using Runner = interpreter_runner<Opt>;
        auto args = Runner::make_args(::std::make_index_sequence<Runner::tuple_size>{}, fn.op.operands.data(), operand_base, local_base);

        sim_state sim{};
        sim.st.reserve(64uz);
        ::std::memcpy(sim.locals.data(), packed_params.data(), ::std::min(param_bytes, sim.locals.size() * sizeof(::std::uint64_t)));

        ::std::size_t inst_idx{};
        ::std::size_t inst_rem{prog.empty() ? 0uz : opcalls_for(prog[0].kind)};

        auto check_state = [&](::std::size_t at_inst) noexcept -> bool
        {
            auto const* const sp = ::std::get<1>(args);
            auto const bytes = static_cast<::std::size_t>(sp - operand_base);
            if((bytes % 8uz) != 0uz)
            {
                ::std::fprintf(stderr, "  diag: unaligned sp bytes=%zu at_inst=%zu\n", bytes, at_inst);
                return false;
            }
            auto const depth = bytes / 8uz;
            if(depth != sim.st.size())
            {
                ::std::fprintf(stderr, "  diag: depth mismatch at_inst=%zu got=%zu exp=%zu\n", at_inst, depth, sim.st.size());
                return false;
            }
            for(::std::size_t i{}; i < depth; ++i)
            {
                ::std::uint64_t v{};
                ::std::memcpy(::std::addressof(v), operand_base + i * 8uz, 8uz);
                if(v != sim.st[i])
                {
                    ::std::fprintf(stderr,
                                   "  diag: stack[%zu] mismatch at_inst=%zu got=0x%016llx exp=0x%016llx\n",
                                   i,
                                   at_inst,
                                   static_cast<unsigned long long>(v),
                                   static_cast<unsigned long long>(sim.st[i]));
                    return false;
                }
            }
            for(::std::size_t li{}; li < 7uz; ++li)
            {
                ::std::uint64_t v{};
                ::std::memcpy(::std::addressof(v), local_base + li * 8uz, 8uz);
                if(v != sim.locals[li])
                {
                    ::std::fprintf(stderr,
                                   "  diag: local[%zu] mismatch at_inst=%zu got=0x%016llx exp=0x%016llx\n",
                                   li,
                                   at_inst,
                                   static_cast<unsigned long long>(v),
                                   static_cast<unsigned long long>(sim.locals[li]));
                    return false;
                }
            }
            return true;
        };

        // Execute and check after each inst boundary (inst expands to 1/2/3 wasm opcodes).
        for(; inst_idx < prog.size();)
        {
            auto const* const ip = ::std::get<0>(args);
            if(ip == nullptr)
            {
                ::std::fprintf(stderr, "  diag: ip became nullptr early at_inst=%zu\n", inst_idx);
                return;
            }

            using opfunc_ptr_t = typename Runner::opfunc_ptr_t;
            opfunc_ptr_t fptr{};
            ::std::memcpy(::std::addressof(fptr), ip, sizeof(fptr));
            ::std::apply([&](auto&... a) { fptr(a...); }, args);

            if(inst_rem == 0uz) { ::fast_io::fast_terminate(); }
            --inst_rem;
            if(inst_rem == 0uz)
            {
                sim_step(sim, prog[inst_idx]);
                if(!check_state(inst_idx))
                {
                    ::std::fprintf(stderr, "  diag: first divergence at inst %zu (%s)\n", inst_idx, name(prog[inst_idx].kind));
                    return;
                }
                ++inst_idx;
                if(inst_idx < prog.size()) { inst_rem = opcalls_for(prog[inst_idx].kind); }
            }
        }
    }

    [[nodiscard]] byte_vec pack_i64x3(::std::int64_t a, ::std::int64_t b, ::std::int64_t c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_random_i64_module(::std::vector<func_spec> const& funcs)
    {
        module_builder mb{};

        for(auto const& f : funcs)
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({4u, k_val_i64});  // local3..6
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
                                      pack_i64x3(p[0], p[1], p[2]),
                                      nullptr,
                                      nullptr);
                auto const got = static_cast<::std::uint64_t>(load_i64(rr.results));
                auto const exp = funcs[fi].expected[ci];
                if(got != exp) [[unlikely]]
                {
                    ::std::fprintf(stderr,
                                   "uwvm2test diff_i64 mismatch: fi=%zu seed=0x%08x case=%zu got=0x%016llx exp=0x%016llx\n",
                                   fi,
                                   funcs[fi].seed,
                                   ci,
                                   static_cast<unsigned long long>(got),
                                   static_cast<unsigned long long>(exp));
                    ::std::fprintf(stderr,
                                   "  params: a=0x%016llx b=0x%016llx c=0x%016llx\n",
                                   static_cast<unsigned long long>(::std::bit_cast<::std::uint64_t>(p[0])),
                                   static_cast<unsigned long long>(::std::bit_cast<::std::uint64_t>(p[1])),
                                   static_cast<unsigned long long>(::std::bit_cast<::std::uint64_t>(p[2])));
                    diagnose_mismatch_byref<Opt>(cm.local_funcs.index_unchecked(fi),
                                                 rt.local_defined_function_vec_storage.index_unchecked(fi),
                                                 pack_i64x3(p[0], p[1], p[2]),
                                                 funcs[fi].prog);
                    ::std::fprintf(stderr, "  program:\n");
                    for(::std::size_t pi{}; pi < funcs[fi].prog.size(); ++pi)
                    {
                        auto const ins = funcs[fi].prog[pi];
                        ::std::fprintf(stderr,
                                       "    %4zu: %-12s imm=0x%016llx\n",
                                       pi,
                                       name(ins.kind),
                                       static_cast<unsigned long long>(ins.imm));
                    }
                    return fail(__LINE__, "diff_i64 mismatch");
                }
            }
        }

        return 0;
    }

    [[nodiscard]] int test_translate_differential_random_i64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        ::std::vector<func_spec> funcs{};
        bool const coverage = is_coverage_run();
        ::std::uint32_t const program_count = coverage ? 96u : 20u;
        int const steps = coverage ? 128 : 56;
        funcs.reserve(program_count);

        for(::std::uint32_t i = 0; i < program_count; ++i)
        {
            ::std::uint32_t const seed = 0xC0DE1000u ^ (i * 0x9E37'79B9u);

            func_spec fs{};
            fs.seed = seed;
            fs.prog = make_program(seed, steps);

            xorshift32 prng{seed ^ 0x5A5A'5A5Au};
            fs.params[0] = {static_cast<::std::int64_t>(static_cast<::std::uint64_t>(seed) << 32),
                            static_cast<::std::int64_t>(static_cast<::std::uint64_t>(seed ^ 0x1357'9BDFu)),
                            0};  // select: false path
            fs.params[1] = {static_cast<::std::int64_t>(~static_cast<::std::uint64_t>(seed)),
                            static_cast<::std::int64_t>(static_cast<::std::uint64_t>(seed) * 3ull + 7ull),
                            1};  // select: true path
            fs.params[2] = {::std::bit_cast<::std::int64_t>((static_cast<::std::uint64_t>(prng.next()) << 32) | prng.next()),
                            ::std::bit_cast<::std::int64_t>((static_cast<::std::uint64_t>(prng.next()) << 32) | prng.next()),
                            ::std::bit_cast<::std::int64_t>((static_cast<::std::uint64_t>(prng.next()) << 32) | prng.next())};

            for(::std::size_t ci = 0; ci < fs.params.size(); ++ci)
            {
                auto const& p = fs.params[ci];
                fs.expected[ci] = simulate(fs.prog, p[0], p[1], p[2]);
            }

            funcs.push_back(::std::move(fs));
        }

        auto wasm = build_random_i64_module(funcs);
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_diff_random_i64");
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
    return test_translate_differential_random_i64();
}
