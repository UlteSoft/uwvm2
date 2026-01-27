#include <uwvm2/runtime/compiler/uwvm_int/optable/variable.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

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
// third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_variable_codegen.cc -o /tmp/uwvm_int_variable_codegen.s`

using opfunc_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32>;

[[gnu::noinline]] void end_sink(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(r3);
    codegen_keep(r4);
}

[[gnu::noinline]] void codegen_local_get_i32_cached(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
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

    optable::uwvmint_local_get_i32<opt, 3uz, T0, T1, T2, wasm_i32, wasm_i32>(ip, sp, local_base, r3, r4);
}

[[gnu::noinline]] void codegen_local_set_i32_cached(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
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

    optable::uwvmint_local_set_i32<opt, 3uz, T0, T1, T2, wasm_i32, wasm_i32>(ip, sp, local_base, r3, r4);
}

[[gnu::noinline]] void codegen_global_get_i32_cached(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
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

    optable::uwvmint_global_get_i32<opt, 3uz, T0, T1, T2, wasm_i32, wasm_i32>(ip, sp, local_base, r3, r4);
}

[[gnu::noinline]] void codegen_global_set_i32_cached(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_i32 r4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
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

    optable::uwvmint_global_set_i32<opt, 3uz, T0, T1, T2, wasm_i32, wasm_i32>(ip, sp, local_base, r3, r4);
}

int main()
{
    opfunc_cached_t end_fn = &end_sink;
    codegen_keep(end_fn);
    return 0;
}

