#include <uwvm2/runtime/compiler/uwvm_int/optable/define.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace uwvm_int_optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

template <typename T>
inline void push_operand(::std::byte*& sp, T v) noexcept
{
    ::std::memcpy(sp, ::std::addressof(v), sizeof(T));
    sp += sizeof(T);
}

template <typename T>
inline bool memeq(T const& a, T const& b) noexcept
{
    return ::std::memcmp(::std::addressof(a), ::std::addressof(b), sizeof(T)) == 0;
}

consteval uwvm_int_optable::uwvm_interpreter_stacktop_remain_size_t remain_i32_only() noexcept
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 6uz,
        .i64_stack_top_begin_pos = SIZE_MAX,
        .i64_stack_top_end_pos = SIZE_MAX,
        .f32_stack_top_begin_pos = SIZE_MAX,
        .f32_stack_top_end_pos = SIZE_MAX,
        .f64_stack_top_begin_pos = SIZE_MAX,
        .f64_stack_top_end_pos = SIZE_MAX,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX};

    constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
        .i32_stack_top_curr_pos = 4uz,
        .i64_stack_top_curr_pos = SIZE_MAX,
        .f32_stack_top_curr_pos = SIZE_MAX,
        .f64_stack_top_curr_pos = SIZE_MAX,
        .v128_stack_top_curr_pos = SIZE_MAX};

    ::std::byte const* op{};
    ::std::byte* sp{};
    ::std::byte* local_base{};
    wasm_i32 r3{};
    wasm_i32 r4{};
    wasm_i32 r5{};

    return uwvm_int_optable::get_remain_size_from_operand_stack<opt,
                                                                curr,
                                                                ::fast_io::tuple<wasm_i32, wasm_i32, wasm_i32>>(op, sp, local_base, r3, r4, r5);
}

static_assert(remain_i32_only().i32_stack_top_remain_size == 0uz);

int main()
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
    using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

    // Case 1: i32 stacktop only, [3,6), curr=4 => 4,5,3 order.
    {
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = false,
            .i32_stack_top_begin_pos = 3uz,
            .i32_stack_top_end_pos = 6uz,
            .i64_stack_top_begin_pos = SIZE_MAX,
            .i64_stack_top_end_pos = SIZE_MAX,
            .f32_stack_top_begin_pos = SIZE_MAX,
            .f32_stack_top_end_pos = SIZE_MAX,
            .f64_stack_top_begin_pos = SIZE_MAX,
            .f64_stack_top_end_pos = SIZE_MAX,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX};

        constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = SIZE_MAX,
            .f32_stack_top_curr_pos = SIZE_MAX,
            .f64_stack_top_curr_pos = SIZE_MAX,
            .v128_stack_top_curr_pos = SIZE_MAX};

        ::std::byte const* op{};
        alignas(16) ::std::byte mem[64]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        wasm_i32 r3{11};
        wasm_i32 r4{22};
        wasm_i32 r5{33};

        auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                        curr,
                                                                        ::fast_io::tuple<wasm_i32, wasm_i32, wasm_i32>>(op, sp, local_base, r3, r4, r5);

        if(::fast_io::get<0>(vals) != 22 || ::fast_io::get<1>(vals) != 33 || ::fast_io::get<2>(vals) != 11) { return 1; }
        if(sp != mem) { return 2; }
    }

    // Case 2: i32 stacktop exhausted => fallback to operand stack.
    {
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = false,
            .i32_stack_top_begin_pos = 3uz,
            .i32_stack_top_end_pos = 6uz,
            .i64_stack_top_begin_pos = SIZE_MAX,
            .i64_stack_top_end_pos = SIZE_MAX,
            .f32_stack_top_begin_pos = SIZE_MAX,
            .f32_stack_top_end_pos = SIZE_MAX,
            .f64_stack_top_begin_pos = SIZE_MAX,
            .f64_stack_top_end_pos = SIZE_MAX,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX};

        constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = SIZE_MAX,
            .f32_stack_top_curr_pos = SIZE_MAX,
            .f64_stack_top_curr_pos = SIZE_MAX,
            .v128_stack_top_curr_pos = SIZE_MAX};

        ::std::byte const* op{};
        alignas(16) ::std::byte mem[64]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        // memory has 1 deeper i32
        push_operand(sp, wasm_i32{44});

        wasm_i32 r3{11};
        wasm_i32 r4{22};
        wasm_i32 r5{33};

        auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                        curr,
                                                                        ::fast_io::tuple<wasm_i32, wasm_i32, wasm_i32, wasm_i32>>(op, sp, local_base, r3, r4, r5);

        if(::fast_io::get<0>(vals) != 22 || ::fast_io::get<1>(vals) != 33 || ::fast_io::get<2>(vals) != 11 || ::fast_io::get<3>(vals) != 44)
        {
            return 3;
        }
        if(sp != mem) { return 4; }
    }

    // Case 3: i32/i64 stacktop merge [3,5), curr=3 => i32 from 3, i64 from 4.
    {
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
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

        constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = SIZE_MAX,
            .f64_stack_top_curr_pos = SIZE_MAX,
            .v128_stack_top_curr_pos = SIZE_MAX};

        ::std::byte const* op{};
        alignas(16) ::std::byte mem[64]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        uwvm_int_optable::wasm_stack_top_i32_with_i64_u s3{};
        uwvm_int_optable::wasm_stack_top_i32_with_i64_u s4{};
        s3.i32 = 111;
        s4.i64 = 222;

        auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                        curr,
                                                                        ::fast_io::tuple<wasm_i32, wasm_i64>>(op, sp, local_base, s3, s4);
        if(::fast_io::get<0>(vals) != 111 || ::fast_io::get<1>(vals) != 222) { return 5; }
        if(sp != mem) { return 6; }

        // overflow to memory after 2 pops
        push_operand(sp, wasm_i32{333});
        s3.i32 = 111;
        s4.i64 = 222;
        auto const vals2 = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                         curr,
                                                                         ::fast_io::tuple<wasm_i32, wasm_i64, wasm_i32>>(op, sp, local_base, s3, s4);
        if(::fast_io::get<0>(vals2) != 111 || ::fast_io::get<1>(vals2) != 222 || ::fast_io::get<2>(vals2) != 333) { return 7; }
        if(sp != mem) { return 8; }
    }

    // Case 4: f32/f64/v128 stacktop merge [3,5), curr=4 => f32 from 4, f64 from 3.
    {
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = false,
            .i32_stack_top_begin_pos = SIZE_MAX,
            .i32_stack_top_end_pos = SIZE_MAX,
            .i64_stack_top_begin_pos = SIZE_MAX,
            .i64_stack_top_end_pos = SIZE_MAX,
            .f32_stack_top_begin_pos = 3uz,
            .f32_stack_top_end_pos = 5uz,
            .f64_stack_top_begin_pos = 3uz,
            .f64_stack_top_end_pos = 5uz,
            .v128_stack_top_begin_pos = 3uz,
            .v128_stack_top_end_pos = 5uz};

        constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
            .i32_stack_top_curr_pos = SIZE_MAX,
            .i64_stack_top_curr_pos = SIZE_MAX,
            .f32_stack_top_curr_pos = 4uz,
            .f64_stack_top_curr_pos = 4uz,
            .v128_stack_top_curr_pos = 4uz};

        ::std::byte const* op{};
        alignas(16) ::std::byte mem[128]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        uwvm_int_optable::wasm_stack_top_f32_f64_v128 f3{};
        uwvm_int_optable::wasm_stack_top_f32_f64_v128 f4{};
        f3.f64 = 1.25;
        f4.f32 = 2.5f;

        auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                        curr,
                                                                        ::fast_io::tuple<wasm_f32, wasm_f64>>(op, sp, local_base, f3, f4);
        if(::fast_io::get<0>(vals) != 2.5f || ::fast_io::get<1>(vals) != 1.25) { return 9; }
        if(sp != mem) { return 10; }

        // overflow to memory after 2 pops
        push_operand(sp, wasm_f32{3.5f});
        f3.f64 = 1.25;
        f4.f32 = 2.5f;
        auto const vals2 = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                         curr,
                                                                         ::fast_io::tuple<wasm_f32, wasm_f64, wasm_f32>>(op, sp, local_base, f3, f4);
        if(::fast_io::get<0>(vals2) != 2.5f || ::fast_io::get<1>(vals2) != 1.25 || ::fast_io::get<2>(vals2) != 3.5f) { return 11; }
        if(sp != mem) { return 12; }
    }

    // Case 5: no stacktop => all from operand stack (cache path).
    {
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{};
        constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{};

        ::std::byte const* op{};
        alignas(16) ::std::byte mem[256]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        wasm_v128 v128{};
        auto const v128_bytes = ::std::bit_cast<::uwvm2::utils::container::array<char, 16u>>(v128);
        (void)v128_bytes;

        // push bottom -> top, so the first requested value is on top
        push_operand(sp, wasm_v128{});
        push_operand(sp, wasm_f64{9.0});
        push_operand(sp, wasm_f32{8.0f});
        push_operand(sp, wasm_i64{7});
        push_operand(sp, wasm_i32{6});

        auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                        curr,
                                                                        ::fast_io::tuple<wasm_i32, wasm_i64, wasm_f32, wasm_f64, wasm_v128>>(op, sp, local_base);
        if(::fast_io::get<0>(vals) != 6 || ::fast_io::get<1>(vals) != 7 || ::fast_io::get<2>(vals) != 8.0f || ::fast_io::get<3>(vals) != 9.0)
        {
            return 13;
        }
        if(!memeq(::fast_io::get<4>(vals), wasm_v128{})) { return 14; }
        if(sp != mem) { return 15; }
    }

    // Direct helpers: cache and no-cache top.
    {
        // cache pop
        ::std::byte const* op{};
        alignas(16) ::std::byte mem[16]{};
        ::std::byte* sp{mem};
        ::std::byte* local_base{};

        push_operand(sp, wasm_i32{123});

        wasm_i32 const a = uwvm_int_optable::get_curr_val_from_operand_stack_cache<wasm_i32>(op, sp, local_base);
        if(a != 123) { return 16; }
        if(sp != mem) { return 17; }

        // top pop with no stacktop range
        constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{};
        push_operand(sp, wasm_i32{456});
        wasm_i32 const b = uwvm_int_optable::get_curr_val_from_operand_stack_top<opt, wasm_i32, 0uz>(op, sp, local_base);
        if(b != 456) { return 18; }
        if(sp != mem) { return 19; }
    }

    return 0;
}
