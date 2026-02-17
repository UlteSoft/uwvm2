#include <uwvm2/runtime/compiler/uwvm_int/optable/control.h>

#include <cstddef>
#include <cstring>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

template <typename T>
inline void write_slot(::std::byte* p, T const& v) noexcept
{ ::std::memcpy(p, ::std::addressof(v), sizeof(T)); }

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

[[gnu::sysv_abi]] static void end0(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 10;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end1(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 11;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end2(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 12;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end3(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    g_hit = 13;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end0_ref(::std::byte const*& ip, ::std::byte*& sp, ::std::byte*& local_base) noexcept
{
    g_hit = 20;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end1_ref(::std::byte const*& ip, ::std::byte*& sp, ::std::byte*& local_base) noexcept
{
    g_hit = 21;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

[[gnu::sysv_abi]] static void end2_ref(::std::byte const*& ip, ::std::byte*& sp, ::std::byte*& local_base) noexcept
{
    g_hit = 22;
    g_ip = ip;
    g_sp = sp;
    g_local_base = local_base;
}

inline constexpr optable::uwvm_interpreter_translate_option_t opt_i32_cache{.is_tail_call = true, .i32_stack_top_begin_pos = 5uz, .i32_stack_top_end_pos = 8uz};

int main()
{
    using T0 = ::std::byte const*;
    using T1 = ::std::byte*;
    using T2 = ::std::byte*;

    using opfunc_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2>;
    using opfunc_ref_t = optable::uwvm_interpreter_opfunc_byref_t<T0, T1, T2>;

    // br: jumps to jmp_ip (slot holding next opfunc).
    {
        reset_state();

        alignas(16)::std::byte slot_end[sizeof(opfunc_t)]{};
        opfunc_t end_fn = &end0;
        write_slot(slot_end, end_fn);

        alignas(16)::std::byte instr[sizeof(opfunc_t) + sizeof(T0)]{};
        opfunc_t br_fn = &optable::uwvmint_br<optable::uwvm_interpreter_translate_option_t{.is_tail_call = true}, T0, T1, T2>;
        write_slot(instr, br_fn);

        T0 jmp_ip = slot_end;
        write_slot(instr + sizeof(opfunc_t), jmp_ip);

        alignas(16)::std::byte mem[32]{};
        ::std::byte* sp = mem;
        ::std::byte* local_base = mem;

        br_fn(instr, sp, local_base);

        if(g_hit != 10) { return 1; }
        if(g_ip != slot_end) { return 2; }
        if(g_sp != mem) { return 3; }
        if(g_local_base != mem) { return 4; }
    }

    // return (tailcall): returns from interpreter without executing following opfuncs.
    {
        reset_state();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

        alignas(16)::std::byte return_ip[sizeof(opfunc_t) + sizeof(opfunc_t)]{};
        opfunc_t return_fn = &optable::uwvmint_return<opt, T0, T1, T2>;
        write_slot(return_ip, return_fn);

        // If uwvmint_return incorrectly tail-calls the next opfunc, this would be executed.
        opfunc_t after_return = &end0;
        write_slot(return_ip + sizeof(opfunc_t), after_return);

        alignas(16)::std::byte instr[sizeof(opfunc_t) + sizeof(T0)]{};
        opfunc_t br_fn = &optable::uwvmint_br<opt, T0, T1, T2>;
        write_slot(instr, br_fn);

        T0 jmp_ip = return_ip;
        write_slot(instr + sizeof(opfunc_t), jmp_ip);

        alignas(16)::std::byte mem[32]{};
        ::std::byte* sp = mem;
        ::std::byte* local_base = mem;

        br_fn(instr, sp, local_base);

        if(g_hit != 0) { return 24; }
        if(g_ip != nullptr) { return 25; }
        if(g_sp != nullptr) { return 26; }
        if(g_local_base != nullptr) { return 27; }
    }

    // Non-tailcall br/br_if/br_table: update ip/sp via references and return to a higher-level loop.
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};

        // br: updates ip -> slot_target, no call to next.
        {
            reset_state();

            alignas(16)::std::byte slot_target[sizeof(opfunc_ref_t)]{};
            opfunc_ref_t end_fn = &end0_ref;
            write_slot(slot_target, end_fn);

            alignas(16)::std::byte instr[sizeof(opfunc_ref_t) + sizeof(T0)]{};
            opfunc_ref_t br_fn = &optable::uwvmint_br<opt, T0, T1, T2>;
            write_slot(instr, br_fn);

            T0 jmp_ip = slot_target;
            write_slot(instr + sizeof(opfunc_ref_t), jmp_ip);

            alignas(16)::std::byte mem[32]{};
            T0 ip = instr;
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;

            br_fn(ip, sp, local_base);
            if(ip != slot_target) { return 11; }
            if(sp != mem) { return 12; }

            opfunc_ref_t next_fn{};
            ::std::memcpy(::std::addressof(next_fn), ip, sizeof(next_fn));
            next_fn(ip, sp, local_base);

            if(g_hit != 20) { return 13; }
            if(g_ip != slot_target) { return 14; }
        }

        // br_if: cond==0 uses fallthrough slot; cond!=0 uses jmp_ip slot.
        {
            // False case
            reset_state();

            alignas(16)::std::byte slot_true[sizeof(opfunc_ref_t)]{};
            opfunc_ref_t end_true = &end1_ref;
            write_slot(slot_true, end_true);

            alignas(16)::std::byte instr[sizeof(opfunc_ref_t) + sizeof(T0) + sizeof(opfunc_ref_t)]{};
            opfunc_ref_t br_if_fn = &optable::uwvmint_br_if<opt, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_ref_t), jmp_ip);

            opfunc_ref_t end_false = &end2_ref;
            write_slot(instr + sizeof(opfunc_ref_t) + sizeof(T0), end_false);

            alignas(16)::std::byte mem[32]{};
            T0 ip = instr;
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, wasm_i32{0});

            br_if_fn(ip, sp, local_base);
            if(ip != (instr + sizeof(opfunc_ref_t) + sizeof(T0))) { return 15; }
            if(sp != mem) { return 16; }

            opfunc_ref_t next_fn{};
            ::std::memcpy(::std::addressof(next_fn), ip, sizeof(next_fn));
            next_fn(ip, sp, local_base);
            if(g_hit != 22) { return 17; }
        }

        {
            // True case
            reset_state();

            alignas(16)::std::byte slot_true[sizeof(opfunc_ref_t)]{};
            opfunc_ref_t end_true = &end1_ref;
            write_slot(slot_true, end_true);

            alignas(16)::std::byte instr[sizeof(opfunc_ref_t) + sizeof(T0) + sizeof(opfunc_ref_t)]{};
            opfunc_ref_t br_if_fn = &optable::uwvmint_br_if<opt, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_ref_t), jmp_ip);

            opfunc_ref_t end_false = &end2_ref;
            write_slot(instr + sizeof(opfunc_ref_t) + sizeof(T0), end_false);

            alignas(16)::std::byte mem[32]{};
            T0 ip = instr;
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, wasm_i32{1});

            br_if_fn(ip, sp, local_base);
            if(ip != slot_true) { return 18; }
            if(sp != mem) { return 19; }

            opfunc_ref_t next_fn{};
            ::std::memcpy(::std::addressof(next_fn), ip, sizeof(next_fn));
            next_fn(ip, sp, local_base);
            if(g_hit != 21) { return 20; }
        }

        // br_table: idx selects table[idx], out-of-range selects default (table[max_size]).
        {
            reset_state();

            alignas(16)::std::byte slot0[sizeof(opfunc_ref_t)]{};
            alignas(16)::std::byte slot1[sizeof(opfunc_ref_t)]{};
            alignas(16)::std::byte slotd[sizeof(opfunc_ref_t)]{};

            opfunc_ref_t fn0 = &end0_ref;
            opfunc_ref_t fn1 = &end1_ref;
            opfunc_ref_t fnd = &end2_ref;
            write_slot(slot0, fn0);
            write_slot(slot1, fn1);
            write_slot(slotd, fnd);

            constexpr ::std::size_t max_size = 2uz;
            alignas(16)::std::byte instr[sizeof(opfunc_ref_t) + sizeof(::std::size_t) + (max_size + 1uz) * sizeof(T0)]{};
            opfunc_ref_t br_table_fn = &optable::uwvmint_br_table<opt, T0, T1, T2>;
            write_slot(instr, br_table_fn);
            write_slot(instr + sizeof(opfunc_ref_t), max_size);

            T0 const t0 = slot0;
            T0 const t1 = slot1;
            T0 const td = slotd;
            write_slot(instr + sizeof(opfunc_ref_t) + sizeof(::std::size_t) + 0uz * sizeof(T0), t0);
            write_slot(instr + sizeof(opfunc_ref_t) + sizeof(::std::size_t) + 1uz * sizeof(T0), t1);
            write_slot(instr + sizeof(opfunc_ref_t) + sizeof(::std::size_t) + 2uz * sizeof(T0), td);

            alignas(16)::std::byte mem[32]{};
            T0 ip = instr;
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;
            push_operand(sp, wasm_i32{100});

            br_table_fn(ip, sp, local_base);
            if(ip != slotd) { return 21; }
            if(sp != mem) { return 22; }

            opfunc_ref_t next_fn{};
            ::std::memcpy(::std::addressof(next_fn), ip, sizeof(next_fn));
            next_fn(ip, sp, local_base);
            if(g_hit != 22) { return 23; }
        }

        // return: sets ip=nullptr to stop the outer interpreter loop.
        {
            reset_state();

            alignas(16)::std::byte instr[sizeof(opfunc_ref_t)]{};
            opfunc_ref_t ret_fn = &optable::uwvmint_return<opt, T0, T1, T2>;
            write_slot(instr, ret_fn);

            T0 ip = instr;
            alignas(16)::std::byte mem[32]{};
            ::std::byte* sp = mem;
            ::std::byte* local_base = mem;

            opfunc_ref_t curr{};
            ::std::memcpy(::std::addressof(curr), ip, sizeof(curr));
            curr(ip, sp, local_base);

            if(ip != nullptr) { return 28; }
            if(sp != mem) { return 29; }
            if(g_hit != 0) { return 30; }
        }
    }

    // br_if: cond == 0 -> fallthrough (next_op_false slot), cond != 0 -> jmp_ip slot.
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

        // False case
        {
            reset_state();

            alignas(16)::std::byte slot_true[sizeof(opfunc_t)]{};
            opfunc_t end_true = &end1;
            write_slot(slot_true, end_true);

            alignas(16)::std::byte instr[sizeof(opfunc_t) + sizeof(T0) + sizeof(opfunc_t)]{};
            opfunc_t br_if_fn = &optable::uwvmint_br_if<opt, 0uz, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_t), jmp_ip);

            opfunc_t end_false = &end2;
            write_slot(instr + sizeof(opfunc_t) + sizeof(T0), end_false);

            alignas(16)::std::byte mem[32]{};
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

            alignas(16)::std::byte slot_true[sizeof(opfunc_t)]{};
            opfunc_t end_true = &end3;
            write_slot(slot_true, end_true);

            alignas(16)::std::byte instr[sizeof(opfunc_t) + sizeof(T0) + sizeof(opfunc_t)]{};
            opfunc_t br_if_fn = &optable::uwvmint_br_if<opt, 0uz, T0, T1, T2>;
            write_slot(instr, br_if_fn);

            T0 jmp_ip = slot_true;
            write_slot(instr + sizeof(opfunc_t), jmp_ip);

            opfunc_t end_false = &end2;
            write_slot(instr + sizeof(opfunc_t) + sizeof(T0), end_false);

            alignas(16)::std::byte mem[32]{};
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

        alignas(16)::std::byte slot0[sizeof(opfunc_t)]{};
        alignas(16)::std::byte slot1[sizeof(opfunc_t)]{};
        alignas(16)::std::byte slotd[sizeof(opfunc_t)]{};

        opfunc_t fn0 = &end0;
        opfunc_t fn1 = &end1;
        opfunc_t fnd = &end2;
        write_slot(slot0, fn0);
        write_slot(slot1, fn1);
        write_slot(slotd, fnd);

        constexpr ::std::size_t max_size = 2uz;  // two explicit targets + default at index 2

        alignas(16)::std::byte instr[sizeof(opfunc_t) + sizeof(::std::size_t) + (max_size + 1uz) * sizeof(T0)]{};
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

            alignas(16)::std::byte mem[32]{};
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

    // translate::get_uwvmint_br_if_fptr: select uwvmint_br_if specialization by i32 curr-pos (no lambdas; template recursion).
    {
        using op_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        optable::uwvm_interpreter_stacktop_currpos_t curr{};

        curr.i32_stack_top_curr_pos = 5uz;
        op_cached_t got0 = optable::translate::get_uwvmint_br_if_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp0 = &optable::uwvmint_br_if<opt_i32_cache, 5uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got0 != exp0) { return 200; }

        curr.i32_stack_top_curr_pos = 6uz;
        op_cached_t got1 = optable::translate::get_uwvmint_br_if_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp1 = &optable::uwvmint_br_if<opt_i32_cache, 6uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got1 != exp1) { return 201; }

        curr.i32_stack_top_curr_pos = 7uz;
        op_cached_t got2 = optable::translate::get_uwvmint_br_if_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp2 = &optable::uwvmint_br_if<opt_i32_cache, 7uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got2 != exp2) { return 202; }
    }

    // translate::get_uwvmint_br_table_fptr: select uwvmint_br_table specialization by i32 curr-pos (no lambdas; template recursion).
    {
        using op_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        optable::uwvm_interpreter_stacktop_currpos_t curr{};

        curr.i32_stack_top_curr_pos = 5uz;
        op_cached_t got0 = optable::translate::get_uwvmint_br_table_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp0 = &optable::uwvmint_br_table<opt_i32_cache, 5uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got0 != exp0) { return 210; }

        curr.i32_stack_top_curr_pos = 6uz;
        op_cached_t got1 = optable::translate::get_uwvmint_br_table_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp1 = &optable::uwvmint_br_table<opt_i32_cache, 6uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got1 != exp1) { return 211; }

        curr.i32_stack_top_curr_pos = 7uz;
        op_cached_t got2 = optable::translate::get_uwvmint_br_table_fptr<opt_i32_cache, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>(curr);
        op_cached_t exp2 = &optable::uwvmint_br_table<opt_i32_cache, 7uz, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;
        if(got2 != exp2) { return 212; }
    }

    // translate::get_uwvmint_return_fptr: no stacktop dependency, single version.
    {
        optable::uwvm_interpreter_stacktop_currpos_t curr{};

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            opfunc_t got = optable::translate::get_uwvmint_return_fptr<opt, T0, T1, T2>(curr);
            opfunc_t exp = &optable::uwvmint_return<opt, T0, T1, T2>;
            if(got != exp) { return 220; }

            ::uwvm2::utils::container::tuple<T0, T1, T2> tup{};
            opfunc_t got2 = optable::translate::get_uwvmint_return_fptr_from_tuple<opt>(curr, tup);
            if(got2 != exp) { return 221; }
        }

        // non-tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            opfunc_ref_t got = optable::translate::get_uwvmint_return_fptr<opt, T0, T1, T2>(curr);
            opfunc_ref_t exp = &optable::uwvmint_return<opt, T0, T1, T2>;
            if(got != exp) { return 222; }

            ::uwvm2::utils::container::tuple<T0, T1, T2> tup{};
            opfunc_ref_t got2 = optable::translate::get_uwvmint_return_fptr_from_tuple<opt>(curr, tup);
            if(got2 != exp) { return 223; }
        }
    }

    return 0;
}
