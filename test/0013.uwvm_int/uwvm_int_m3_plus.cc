#include <uwvm2/runtime/compiler/uwvm_int/optable/m3_plus.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace m3p = ::uwvm2::runtime::compiler::uwvm_int::optable::m3_plus;

using wasm_i32 = m3p::wasm_i32;
using wasm_f32 = m3p::wasm_f32;
using wasm_f64 = m3p::wasm_f64;
using wasm_v128 = m3p::wasm_v128;

template <typename T>
inline void write_slot(::std::byte* p, T const& v) noexcept
{ ::std::memcpy(p, ::std::addressof(v), sizeof(T)); }

template <typename T>
inline void emit_slot(::std::byte*& p, T const& v) noexcept
{
    write_slot(p, v);
    p += sizeof(T);
}

template <typename T>
inline T load_slot(::std::byte const* p) noexcept
{
    T out;  // no init
    ::std::memcpy(::std::addressof(out), p, sizeof(T));
    return out;
}

template <typename T>
inline void store_slot(::std::byte* p, T const& v) noexcept
{ ::std::memcpy(p, ::std::addressof(v), sizeof(T)); }

inline int g_hit{};
inline ::std::byte const* g_ip{};
inline ::std::byte* g_sp{};
inline ::std::byte* g_state{};
inline m3p::m3_register_t g_r0{};
inline m3p::m3_register_t g_r1{};

static void reset_observe() noexcept
{
    g_hit = 0;
    g_ip = nullptr;
    g_sp = nullptr;
    g_state = nullptr;
    g_r0 = {};
    g_r1 = {};
}

[[gnu::sysv_abi]] static void end0(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3p::m3_register_t r0, m3p::m3_register_t r1) noexcept
{
    g_hit = 10;
    g_ip = ip;
    g_sp = sp;
    g_state = state_raw;
    g_r0 = r0;
    g_r1 = r1;
}

[[gnu::sysv_abi]] static void end1(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3p::m3_register_t r0, m3p::m3_register_t r1) noexcept
{
    g_hit = 11;
    g_ip = ip;
    g_sp = sp;
    g_state = state_raw;
    g_r0 = r0;
    g_r1 = r1;
}

[[gnu::sysv_abi]] static void end2(::std::byte const* ip, ::std::byte* sp, ::std::byte* state_raw, m3p::m3_register_t r0, m3p::m3_register_t r1) noexcept
{
    g_hit = 12;
    g_ip = ip;
    g_sp = sp;
    g_state = state_raw;
    g_r0 = r0;
    g_r1 = r1;
}

[[nodiscard]] inline wasm_v128 make_f32x4(wasm_f32 a, wasm_f32 b, wasm_f32 c, wasm_f32 d) noexcept
{
    m3p::details::lane_array<wasm_f32, 4uz> lanes{
        {a, b, c, d}
    };
    return m3p::details::store_f32x4_lanes(lanes);
}

int main()
{
    using profile = m3p::hybrid_i2_fvring_profile;
    static_assert(!profile::has_integer_register_ring);
    static_assert(profile::theoretical_state_count == 8uz);

    using opfunc_t = m3p::opfunc_t;

    // I2 integer path: local-as-operand add, R0 immediate multiply, local sink, no integer ring state.
    {
        reset_observe();

        alignas(16)::std::byte locals[64]{};
        alignas(16)::std::byte operand_stack[64]{};
        m3p::state st{.local_base = locals, .operand_stack_base = operand_stack};
        store_slot(locals + 0, wasm_i32{3});
        store_slot(locals + 4, wasm_i32{4});

        alignas(16)::std::byte code[2uz * sizeof(opfunc_t) + 3uz * sizeof(m3p::local_offset_t) + sizeof(wasm_i32) + sizeof(opfunc_t)]{};

        opfunc_t add_ll = &m3p::uwvmint_m3p_i32_binop<profile, m3p::int_binop::add, m3p::int_src::local, m3p::int_src::local, m3p::int_dst::r0>;
        opfunc_t mul_ri_set = &m3p::uwvmint_m3p_i32_binop<profile, m3p::int_binop::mul, m3p::int_src::r0, m3p::int_src::immediate, m3p::int_dst::local>;
        opfunc_t end = &end0;
        auto* p{code};
        m3p::local_offset_t off0{};
        m3p::local_offset_t off4{4uz};
        m3p::local_offset_t off8{8uz};
        emit_slot(p, add_ll);
        emit_slot(p, off0);
        emit_slot(p, off4);
        emit_slot(p, mul_ri_set);
        emit_slot(p, wasm_i32{3});
        emit_slot(p, off8);
        emit_slot(p, end);

        ::std::byte* sp = operand_stack;
        add_ll(code, sp, reinterpret_cast<::std::byte*>(::std::addressof(st)), {}, {});

        if(g_hit != 10) { return 1; }
        if(load_slot<wasm_i32>(locals + 8) != 21) { return 2; }
        if(static_cast<wasm_i32>(g_r0) != 7) { return 3; }
        if(st.counters.local_load_count != 2u || st.counters.local_store_count != 1u) { return 4; }
        if(st.counters.fv_physical_rotate_count != 0u) { return 5; }
    }

    // Branch path: compare into R0, then br_if consumes R0 without integer ring state.
    {
        reset_observe();

        alignas(16)::std::byte locals[64]{};
        alignas(16)::std::byte operand_stack[64]{};
        m3p::state st{.local_base = locals, .operand_stack_base = operand_stack};
        store_slot(locals + 0, wasm_i32{3});
        store_slot(locals + 4, wasm_i32{9});

        alignas(16)::std::byte code[2uz * sizeof(opfunc_t) + 2uz * sizeof(m3p::local_offset_t) + sizeof(::std::byte const*) + sizeof(opfunc_t)]{};
        alignas(16)::std::byte true_slot[sizeof(opfunc_t)]{};

        opfunc_t cmp = &m3p::uwvmint_m3p_i32_binop<profile, m3p::int_binop::lt_u, m3p::int_src::local, m3p::int_src::local, m3p::int_dst::r0>;
        opfunc_t br = &m3p::uwvmint_m3p_br_if<profile, m3p::int_src::r0>;
        opfunc_t end_true = &end1;
        opfunc_t end_false = &end2;
        m3p::local_offset_t off0{};
        m3p::local_offset_t off4{4uz};
        auto* p{code};
        emit_slot(p, cmp);
        emit_slot(p, off0);
        emit_slot(p, off4);
        emit_slot(p, br);
        write_slot(true_slot, end_true);
        ::std::byte const* true_ip = true_slot;
        emit_slot(p, true_ip);
        emit_slot(p, end_false);

        ::std::byte* sp = operand_stack;
        cmp(code, sp, reinterpret_cast<::std::byte*>(::std::addressof(st)), {}, {});

        if(g_hit != 11) { return 6; }
        if(st.counters.branch_count != 1u || st.counters.branch_taken_count != 1u) { return 7; }
    }

    // FV f64 expression chain: (a*b) + (c*d) with head-index fill and top-update, no physical rotate.
    {
        reset_observe();

        alignas(16)::std::byte locals[128]{};
        alignas(16)::std::byte operand_stack[64]{};
        m3p::state st{.local_base = locals, .operand_stack_base = operand_stack};
        store_slot(locals + 0, wasm_f64{2.0});
        store_slot(locals + 8, wasm_f64{3.0});
        store_slot(locals + 16, wasm_f64{4.0});
        store_slot(locals + 24, wasm_f64{5.0});

        alignas(16)::std::byte code[7uz * sizeof(opfunc_t) + 5uz * sizeof(m3p::local_offset_t)]{};

        opfunc_t fill = &m3p::uwvmint_m3p_f64_fill_local<profile>;
        opfunc_t mul_local = &m3p::uwvmint_m3p_f64_binop_top_local<profile, m3p::float_binop::mul>;
        opfunc_t add_prev = &m3p::uwvmint_m3p_f64_binop_top_prev<profile, m3p::float_binop::add>;
        opfunc_t set_top = &m3p::uwvmint_m3p_f64_local_set_top<profile>;
        opfunc_t end = &end0;

        auto* p{code};
        m3p::local_offset_t off0{};
        m3p::local_offset_t off8{8uz};
        m3p::local_offset_t off16{16uz};
        m3p::local_offset_t off24{24uz};
        m3p::local_offset_t off32{32uz};
        emit_slot(p, fill);
        emit_slot(p, off0);
        emit_slot(p, mul_local);
        emit_slot(p, off8);
        emit_slot(p, fill);
        emit_slot(p, off16);
        emit_slot(p, mul_local);
        emit_slot(p, off24);
        emit_slot(p, add_prev);
        emit_slot(p, set_top);
        emit_slot(p, off32);
        emit_slot(p, end);

        ::std::byte* sp = operand_stack;
        fill(code, sp, reinterpret_cast<::std::byte*>(::std::addressof(st)), {}, {});

        if(g_hit != 10) { return 8; }
        if(load_slot<wasm_f64>(locals + 32) != wasm_f64{26.0}) { return 9; }
        if(st.fv_depth != 0u) { return 10; }
        if(st.counters.fv_ring_fill_count != 2u) { return 11; }
        if(st.counters.fv_top_update_count != 3u) { return 12; }
        if(st.counters.fv_physical_rotate_count != 0u) { return 13; }
    }

    // Mixed address + v128 FV path: I2 supplies addresses, f32x4 values stay in FV ring.
    {
        reset_observe();

        alignas(16)::std::byte locals[64]{};
        alignas(16)::std::byte operand_stack[64]{};
        alignas(16)::std::byte memory[64]{};
        m3p::state st{.local_base = locals, .operand_stack_base = operand_stack, .memory_begin = memory, .memory_size = sizeof(memory)};
        store_slot(locals + 0, wasm_i32{0});
        store_slot(locals + 4, wasm_i32{16});
        store_slot(locals + 8, wasm_i32{32});
        auto const lhs{make_f32x4(1.0f, 2.0f, 3.0f, 4.0f)};
        auto const rhs{make_f32x4(10.0f, 20.0f, 30.0f, 40.0f)};
        store_slot(memory + 0, lhs);
        store_slot(memory + 16, rhs);

        alignas(16)::std::byte code[5uz * sizeof(opfunc_t) + 3uz * sizeof(wasm_i32) + 3uz * sizeof(m3p::local_offset_t)]{};

        opfunc_t load_v = &m3p::uwvmint_m3p_v128_load<profile, m3p::int_src::local>;
        opfunc_t add_v = &m3p::uwvmint_m3p_v128_binop_top_prev<profile, m3p::v128_binop::f32x4_add>;
        opfunc_t store_v = &m3p::uwvmint_m3p_v128_store<profile, m3p::int_src::local>;
        opfunc_t end = &end0;

        auto* p{code};
        m3p::local_offset_t off0{};
        m3p::local_offset_t off4{4uz};
        m3p::local_offset_t off8{8uz};
        emit_slot(p, load_v);
        emit_slot(p, wasm_i32{0});
        emit_slot(p, off0);
        emit_slot(p, load_v);
        emit_slot(p, wasm_i32{0});
        emit_slot(p, off4);
        emit_slot(p, add_v);
        emit_slot(p, store_v);
        emit_slot(p, wasm_i32{0});
        emit_slot(p, off8);
        emit_slot(p, end);

        ::std::byte* sp = operand_stack;
        load_v(code, sp, reinterpret_cast<::std::byte*>(::std::addressof(st)), {}, {});

        if(g_hit != 10) { return 14; }
        auto const out{load_slot<wasm_v128>(memory + 32)};
        auto const lanes{m3p::details::load_f32x4_lanes(out)};
        if(lanes.lane[0] != 11.0f || lanes.lane[1] != 22.0f || lanes.lane[2] != 33.0f || lanes.lane[3] != 44.0f) { return 15; }
        if(st.counters.fv_ring_fill_count != 2u || st.counters.fv_physical_rotate_count != 0u) { return 16; }
    }

    return 0;
}
