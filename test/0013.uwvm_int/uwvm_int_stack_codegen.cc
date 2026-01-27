#include <uwvm2/runtime/compiler/uwvm_int/optable/stack.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

using slot_i32_i64 = optable::wasm_stack_top_i32_with_i64_u;

template <typename T>
[[gnu::always_inline]] inline void codegen_keep(T const& v) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    using U = ::std::remove_cvref_t<T>;
    if constexpr(::std::is_integral_v<U> || ::std::is_pointer_v<U>) { asm volatile("" : : "r"(v)); }
    else
    {
        asm volatile("" : : "w"(v));
    }
#else
    (void)v;
#endif
}

using T0 = ::std::byte const*;
using T1 = ::std::byte*;
using T2 = ::std::byte*;

// Intended for manual/CI codegen inspection under -O3.
//
// Example:
// `clang++ --sysroot=$SYSROOT -O3 -S -fuse-ld=lld -std=c++26 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I
// third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_stack_codegen.cc -o /tmp/uwvm_int_stack_codegen.s`

using opfunc_drop_i32_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32>;
using opfunc_select_i64_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, slot_i32_i64, slot_i32_i64, slot_i32_i64>;
using opfunc_select_i64_disjoint_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i64, wasm_i64>;
using opfunc_select_i64_value_only_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i64, wasm_i64>;

[[gnu::noinline]] void end_drop_i32(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4, wasm_i32 r5)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(r3);
    codegen_keep(r4);
    codegen_keep(r5);
}

[[gnu::noinline]] void end_select_i64(T0 ip, T1 sp, T2 local_base, slot_i32_i64 s3, slot_i32_i64 s4, slot_i32_i64 s5)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(s3);
    codegen_keep(s4);
    codegen_keep(s5);
}

[[gnu::noinline]] void end_select_i64_disjoint(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i64 r4, wasm_i64 r5)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(r3);
    codegen_keep(r4);
    codegen_keep(r5);
}

[[gnu::noinline]] void end_select_i64_value_only(T0 ip, T1 sp, T2 local_base, wasm_i64 r3, wasm_i64 r4)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(r3);
    codegen_keep(r4);
}

[[gnu::noinline]] void codegen_drop_i32_cached(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4, wasm_i32 r5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
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

    optable::uwvmint_drop_i32<opt, SIZE_MAX, T0, T1, T2, wasm_i32, wasm_i32, wasm_i32>(ip, sp, local_base, r3, r4, r5);
}

[[gnu::noinline]] void codegen_select_i64_cached(T0 ip, T1 sp, T2 local_base, slot_i32_i64 s3, slot_i32_i64 s4, slot_i32_i64 s5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 6uz,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 6uz,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_select_i64<opt, 3uz, T0, T1, T2, slot_i32_i64, slot_i32_i64, slot_i32_i64>(ip, sp, local_base, s3, s4, s5);
}

[[gnu::noinline]] void codegen_select_i64_disjoint(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i64 r4, wasm_i64 r5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 4uz,
                                                               .i64_stack_top_begin_pos = 4uz,
                                                               .i64_stack_top_end_pos = 6uz,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_select_i64<opt, 3uz, 4uz, T0, T1, T2, wasm_i32, wasm_i64, wasm_i64>(ip, sp, local_base, r3, r4, r5);
}

[[gnu::noinline]] void codegen_select_i64_value_only(T0 ip, T1 sp, T2 local_base, wasm_i64 r3, wasm_i64 r4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = SIZE_MAX,
                                                               .i32_stack_top_end_pos = SIZE_MAX,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 5uz,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_select_i64<opt, 0uz, 3uz, T0, T1, T2, wasm_i64, wasm_i64>(ip, sp, local_base, r3, r4);
}

int main()
{
    opfunc_drop_i32_cached_t end_drop_fn = &end_drop_i32;
    opfunc_select_i64_cached_t end_select_fn = &end_select_i64;
    opfunc_select_i64_disjoint_t end_select_disjoint_fn = &end_select_i64_disjoint;
    opfunc_select_i64_value_only_t end_select_value_only_fn = &end_select_i64_value_only;
    codegen_keep(end_drop_fn);
    codegen_keep(end_select_fn);
    codegen_keep(end_select_disjoint_fn);
    codegen_keep(end_select_value_only_fn);
    return 0;
}
