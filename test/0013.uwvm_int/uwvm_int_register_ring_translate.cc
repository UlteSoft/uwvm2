#include <uwvm2/runtime/compiler/uwvm_int/optable/register_ring.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

template <typename T>
inline void write_slot(::std::byte* p, T const& v) noexcept
{
    ::std::memcpy(p, ::std::addressof(v), sizeof(T));
}

inline int g_hit{};
inline ::std::byte const* g_ip{};
inline ::std::byte* g_sp{};
inline wasm_i32 g_r3{};
inline wasm_i32 g_r4{};
inline wasm_i32 g_r5{};

static void reset_state() noexcept
{
    g_hit = 0;
    g_ip = nullptr;
    g_sp = nullptr;
    g_r3 = wasm_i32{};
    g_r4 = wasm_i32{};
    g_r5 = wasm_i32{};
}

static void end_capture(::std::byte const* ip, ::std::byte* sp, ::std::byte*, wasm_i32 r3, wasm_i32 r4, wasm_i32 r5) noexcept
{
    g_hit = 1;
    g_ip = ip;
    g_sp = sp;
    g_r3 = r3;
    g_r4 = r4;
    g_r5 = r5;
}

int main()
{
    using T0 = ::std::byte const*;
    using T1 = ::std::byte*;
    using T2 = ::std::byte*;

    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 6uz};

    using opfunc_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32>;

    optable::uwvm_interpreter_stacktop_currpos_t curr{};
    optable::uwvm_interpreter_stacktop_remain_size_t remain{};

    ::uwvm2::utils::container::tuple<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32> tup{};

    // translate: StartPos=5, Count=3 -> spill in ring order: 5,3,4 (top->deep).
    {
        curr.i32_stack_top_curr_pos = 5uz;
        remain.i32_stack_top_remain_size = 3uz;

        opfunc_t got =
            optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<opt, wasm_i32>(curr, remain, tup);
        opfunc_t exp = &optable::uwvmint_stacktop_to_operand_stack<opt, 5uz, 3uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32>;
        if(got != exp) { return 1; }

        reset_state();

        alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(opfunc_t)]{};
        write_slot(instr, got);
        opfunc_t end_fn = &end_capture;
        write_slot(instr + sizeof(opfunc_t), end_fn);

        alignas(16) ::std::byte mem[32]{};
        ::std::byte* sp = mem;
        ::std::byte* local_base = mem;

        wasm_i32 r3{0x11111111};
        wasm_i32 r4{0x22222222};
        wasm_i32 r5{0x33333333};

        got(instr, sp, local_base, r3, r4, r5);

        if(g_hit != 1) { return 2; }
        if(g_ip != instr + sizeof(opfunc_t)) { return 3; }
        if(g_sp != mem + 12) { return 4; }
        if(g_r3 != r3 || g_r4 != r4 || g_r5 != r5) { return 5; }

        wasm_i32 m0{};
        wasm_i32 m1{};
        wasm_i32 m2{};
        ::std::memcpy(::std::addressof(m0), mem + 0, sizeof(wasm_i32));
        ::std::memcpy(::std::addressof(m1), mem + 4, sizeof(wasm_i32));
        ::std::memcpy(::std::addressof(m2), mem + 8, sizeof(wasm_i32));
        // Operand stack memory is deep->top: [slot4][slot3][slot5].
        if(m0 != r4 || m1 != r3 || m2 != r5) { return 6; }
    }

    // translate: StartPos=5, Count=3 -> load in ring order: 5,3,4 (top->deep).
    {
        curr.i32_stack_top_curr_pos = 5uz;
        remain.i32_stack_top_remain_size = 3uz;

        opfunc_t got =
            optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<opt, wasm_i32>(curr, remain, tup);
        opfunc_t exp = &optable::uwvmint_operand_stack_to_stacktop<opt, 5uz, 3uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32>;
        if(got != exp) { return 7; }

        reset_state();

        alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(opfunc_t)]{};
        write_slot(instr, got);
        opfunc_t end_fn = &end_capture;
        write_slot(instr + sizeof(opfunc_t), end_fn);

        alignas(16) ::std::byte mem[32]{};
        wasm_i32 const v3{0x01020304};
        wasm_i32 const v4{0x05060708};
        wasm_i32 const v5{0x090A0B0C};
        // Operand stack memory is deep->top: [slot4][slot3][slot5].
        ::std::memcpy(mem + 0, ::std::addressof(v4), sizeof(wasm_i32));
        ::std::memcpy(mem + 4, ::std::addressof(v3), sizeof(wasm_i32));
        ::std::memcpy(mem + 8, ::std::addressof(v5), sizeof(wasm_i32));

        ::std::byte* sp = mem + 12;
        ::std::byte* local_base = mem;

        got(instr, sp, local_base, wasm_i32{}, wasm_i32{}, wasm_i32{});

        if(g_hit != 1) { return 8; }
        if(g_ip != instr + sizeof(opfunc_t)) { return 9; }
        if(g_sp != mem) { return 10; }
        if(g_r3 != v3 || g_r4 != v4 || g_r5 != v5) { return 11; }
    }

    return 0;
}
