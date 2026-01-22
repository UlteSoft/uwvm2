#include <uwvm2/runtime/compiler/uwvm_int/optable/register_ring.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

template <typename T>
inline T load(::std::byte const* p) noexcept
{
    T out{};
    ::std::memcpy(::std::addressof(out), p, sizeof(T));
    return out;
}

int main()
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

    // i32 ring: [3,5) => slots 3,4.
    // Spill StartPos=4 Count=2 => writes slot4 then slot3 back to operand stack.
    constexpr optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = SIZE_MAX,
        .i64_stack_top_end_pos = SIZE_MAX,
        .f32_stack_top_begin_pos = SIZE_MAX,
        .f32_stack_top_end_pos = SIZE_MAX,
        .f64_stack_top_begin_pos = SIZE_MAX,
        .f64_stack_top_end_pos = SIZE_MAX,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX};

    ::std::byte const* op{};
    alignas(16) ::std::byte mem[32]{};
    ::std::byte* sp{mem};
    ::std::byte* local_base{};

    wasm_i32 r3{0x11223344};
    wasm_i32 r4{0x55667788};

    optable::manipulate::spill_stacktop_to_operand_stack<opt, 4uz, 2uz>(op, sp, local_base, r3, r4);

    if(sp != mem + 8) { return 1; }
    // After spill, memory layout should be [r3][r4] in ascending addresses.
    if(load<wasm_i32>(mem) != r3) { return 2; }
    if(load<wasm_i32>(mem + 4) != r4) { return 3; }

    // i32/i64 merge ring: [3,5) => slots 3,4.
    // Spill StartPos=4 types=[i64,i32] => writes slot4(i64) then slot3(i32) back to operand stack.
    constexpr optable::uwvm_interpreter_translate_option_t opt_i32_i64_merge{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = SIZE_MAX,
        .f32_stack_top_end_pos = SIZE_MAX,
        .f64_stack_top_begin_pos = SIZE_MAX,
        .f64_stack_top_end_pos = SIZE_MAX,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX};

    ::std::byte const* op2{};
    alignas(16) ::std::byte mem2[32]{};
    ::std::byte* sp2{mem2};
    ::std::byte* local_base2{};

    optable::wasm_stack_top_i32_with_i64_u s3{};
    optable::wasm_stack_top_i32_with_i64_u s4{};
    s3.i32 = wasm_i32{0x11223344};
    s4.i64 = wasm_i64{0x1122334455667788};

    optable::manipulate::spill_stacktop_to_operand_stack<opt_i32_i64_merge, 4uz, wasm_i64, wasm_i32>(op2, sp2, local_base2, s3, s4);

    if(sp2 != mem2 + 12) { return 4; }
    if(load<wasm_i32>(mem2) != s3.i32) { return 5; }
    if(load<wasm_i64>(mem2 + 4) != s4.i64) { return 6; }

    // operand_stack_to_stacktop: inverse of spill, i32-only
    {
        ::std::byte const* op3{};
        alignas(16) ::std::byte mem3[32]{};
        ::std::byte* sp3{mem3};
        ::std::byte* local_base3{};

        ::std::memcpy(mem3, ::std::addressof(r3), sizeof(wasm_i32));
        ::std::memcpy(mem3 + 4, ::std::addressof(r4), sizeof(wasm_i32));
        sp3 += 8;

        wasm_i32 c3{};
        wasm_i32 c4{};
        optable::manipulate::operand_stack_to_stacktop<opt, 4uz, 2uz>(op3, sp3, local_base3, c3, c4);

        if(sp3 != mem3) { return 7; }
        if(c3 != r3 || c4 != r4) { return 8; }
    }

    // operand_stack_to_stacktop: inverse of spill, i32/i64 merge mixed types
    {
        ::std::byte const* op3{};
        alignas(16) ::std::byte mem3[32]{};
        ::std::byte* sp3{mem3};
        ::std::byte* local_base3{};

        wasm_i32 const v3{static_cast<wasm_i32>(0xAABBCCDDu)};
        wasm_i64 const v4{static_cast<wasm_i64>(0x1122334455667788ull)};
        ::std::memcpy(mem3, ::std::addressof(v3), sizeof(wasm_i32));
        ::std::memcpy(mem3 + 4, ::std::addressof(v4), sizeof(wasm_i64));
        sp3 += 12;

        optable::wasm_stack_top_i32_with_i64_u c3{};
        optable::wasm_stack_top_i32_with_i64_u c4{};
        optable::manipulate::operand_stack_to_stacktop<opt_i32_i64_merge, 4uz, wasm_i64, wasm_i32>(op3, sp3, local_base3, c3, c4);

        if(sp3 != mem3) { return 9; }
        if(c3.i32 != v3 || c4.i64 != v4) { return 10; }
    }

    // Compile-only: ensure the interpreter opfunc wrapper is instantiable.
    {
        constexpr optable::uwvm_interpreter_translate_option_t opt_tail{.is_tail_call = true,
                                                                        .i32_stack_top_begin_pos = 3uz,
                                                                        .i32_stack_top_end_pos = 5uz,
                                                                        .i64_stack_top_begin_pos = SIZE_MAX,
                                                                        .i64_stack_top_end_pos = SIZE_MAX,
                                                                        .f32_stack_top_begin_pos = SIZE_MAX,
                                                                        .f32_stack_top_end_pos = SIZE_MAX,
                                                                        .f64_stack_top_begin_pos = SIZE_MAX,
                                                                        .f64_stack_top_end_pos = SIZE_MAX,
                                                                        .v128_stack_top_begin_pos = SIZE_MAX,
                                                                        .v128_stack_top_end_pos = SIZE_MAX};

        using T0 = ::std::byte const*;
        using T1 = ::std::byte*;
        using T2 = ::std::byte*;
        using T3 = wasm_i32;
        using T4 = wasm_i32;
        using opfunc_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, T3, T4>;
        opfunc_t f = &optable::uwvmint_operand_stack_to_stacktop<opt_tail, 4uz, 2uz, T0, T1, T2, T3, T4>;
        (void)f;
    }

    return 0;
}
