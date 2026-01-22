#include <uwvm2/runtime/compiler/uwvm_int/optable/control.h>

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

template <typename T>
inline void push_operand(::std::byte*& sp, T v) noexcept
{
    ::std::memcpy(sp, ::std::addressof(v), sizeof(T));
    sp += sizeof(T);
}

inline int g_hit{};
inline ::std::byte const* g_ip{};
inline ::std::byte* g_sp{};
inline ::std::byte* g_local_base{};

static void reset_state() noexcept
{
    g_hit = 0;
    g_ip = nullptr;
    g_sp = nullptr;
    g_local_base = nullptr;
}

static void end0(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 10;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

static void end1(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 11;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

static void end2(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 12;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

static void end3(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 13;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

int main()
{
    using T0 = ::std::byte const*;
    using T1 = ::std::byte*;
    using T2 = ::std::byte*;

    using opfunc_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2>;

    // br: jumps to jmp_ip (slot holding next opfunc).
    {
        reset_state();

        alignas(16) ::std::byte slot_end[sizeof(opfunc_t)]{};
        opfunc_t end_fn = &end0;
        write_slot(slot_end, end_fn);

        alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(T0)]{};
        opfunc_t br_fn = &optable::uwvmint_br<optable::uwvm_interpreter_translate_option_t{.is_tail_call = true}, T0, T1, T2>;
        write_slot(instr, br_fn);

        T0 jmp_ip = slot_end;
        write_slot(instr + sizeof(opfunc_t), jmp_ip);

        alignas(16) ::std::byte mem[32]{};
        ::std::byte* sp = mem;
        ::std::byte* local_base = mem;

        br_fn(instr, sp, local_base);

        if(g_hit != 10) { return 1; }
        if(g_ip != slot_end) { return 2; }
        if(g_sp != mem) { return 3; }
        if(g_local_base != mem) { return 4; }
    }

    // br_if: cond == 0 -> fallthrough (next_op_false slot), cond != 0 -> jmp_ip slot.
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

        // False case
        {
            reset_state();

            alignas(16) ::std::byte slot_true[sizeof(opfunc_t)]{};
            opfunc_t end_true = &end1;
            write_slot(slot_true, end_true);

            alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(T0) + sizeof(opfunc_t)]{};
            opfunc_t br_if_fn = &optable::uwvmint_br_if<opt, 0uz, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_t), jmp_ip);

            opfunc_t end_false = &end2;
            write_slot(instr + sizeof(opfunc_t) + sizeof(T0), end_false);

            alignas(16) ::std::byte mem[32]{};
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, wasm_i32{0});

            br_if_fn(instr, sp, local_base);

            if(g_hit != 12) { return 5; }
            if(g_ip != (instr + sizeof(opfunc_t) + sizeof(T0))) { return 6; }
            if(g_sp != mem) { return 7; }  // popped i32 cond from local sp copy
        }

        // True case
        {
            reset_state();

            alignas(16) ::std::byte slot_true[sizeof(opfunc_t)]{};
            opfunc_t end_true = &end3;
            write_slot(slot_true, end_true);

            alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(T0) + sizeof(opfunc_t)]{};
            opfunc_t br_if_fn = &optable::uwvmint_br_if<opt, 0uz, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_t), jmp_ip);

            opfunc_t end_false = &end2;
            write_slot(instr + sizeof(opfunc_t) + sizeof(T0), end_false);

            alignas(16) ::std::byte mem[32]{};
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, wasm_i32{1});

            br_if_fn(instr, sp, local_base);

            if(g_hit != 13) { return 8; }
            if(g_ip != slot_true) { return 9; }
            if(g_sp != mem) { return 10; }
        }
    }

    // br_table: idx selects table[idx], out-of-range selects table[max_size] (default).
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

        alignas(16) ::std::byte slot0[sizeof(opfunc_t)]{};
        alignas(16) ::std::byte slot1[sizeof(opfunc_t)]{};
        alignas(16) ::std::byte slotd[sizeof(opfunc_t)]{};

        opfunc_t fn0 = &end0;
        opfunc_t fn1 = &end1;
        opfunc_t fnd = &end2;
        write_slot(slot0, fn0);
        write_slot(slot1, fn1);
        write_slot(slotd, fnd);

        constexpr ::std::size_t max_size = 2uz;  // two explicit targets + default at index 2

        alignas(16) ::std::byte instr[sizeof(opfunc_t) + sizeof(::std::size_t) + (max_size + 1uz) * sizeof(T0)]{};
        opfunc_t br_table_fn = &optable::uwvmint_br_table<opt, 0uz, T0, T1, T2>;
        write_slot(instr, br_table_fn);
        write_slot(instr + sizeof(opfunc_t), max_size);

        T0 const t0 = slot0;
        T0 const t1 = slot1;
        T0 const td = slotd;
        write_slot(instr + sizeof(opfunc_t) + sizeof(::std::size_t) + 0uz * sizeof(T0), t0);
        write_slot(instr + sizeof(opfunc_t) + sizeof(::std::size_t) + 1uz * sizeof(T0), t1);
        write_slot(instr + sizeof(opfunc_t) + sizeof(::std::size_t) + 2uz * sizeof(T0), td);

        auto run_case = [&](wasm_i32 idx, int expected_hit) noexcept -> int
        {
            reset_state();

            alignas(16) ::std::byte mem[32]{};
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, idx);

            br_table_fn(instr, sp, local_base);

            if(g_hit != expected_hit) { return 100; }
            if(g_sp != mem) { return 101; }
            return 0;
        };

        if(int e = run_case(wasm_i32{0}, 10); e) { return e + 20; }
        if(int e = run_case(wasm_i32{1}, 11); e) { return e + 30; }
        if(int e = run_case(wasm_i32{2}, 12); e) { return e + 40; }
        if(int e = run_case(wasm_i32{100}, 12); e) { return e + 50; }
        // Negative indexes behave like unsigned (uint32_t) for this implementation and fall into default.
        if(int e = run_case(wasm_i32{-1}, 12); e) { return e + 60; }
    }

    return 0;
}
