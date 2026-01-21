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

    optable::spill_stacktop_to_operand_stack<opt, 4uz, 2uz>(op, sp, local_base, r3, r4);

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

    optable::spill_stacktop_to_operand_stack<opt_i32_i64_merge, 4uz, wasm_i64, wasm_i32>(op2, sp2, local_base2, s3, s4);

    if(sp2 != mem2 + 12) { return 4; }
    if(load<wasm_i32>(mem2) != s3.i32) { return 5; }
    if(load<wasm_i64>(mem2 + 4) != s4.i64) { return 6; }

    return 0;
}
