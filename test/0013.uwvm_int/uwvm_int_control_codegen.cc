#include <uwvm2/runtime/compiler/uwvm_int/optable/control.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
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
using opfunc_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2>;

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

[[gnu::noinline]] void codegen_end(T0 ip, T1 sp, T2 local_base) noexcept
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
}

// Intended for manual/CI codegen inspection under -O3:
//
// Example:
// `clang++ --sysroot=$SYSROOT -O3 -S -fuse-ld=lld -std=c++26 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I
// third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_control_codegen.cc -o /tmp/uwvm_int_control_codegen.s`
//
// Expectations:
// - No libcalls to memcpy/memmove; loads/stores should be inlined.
// - br: load jmp_ip and indirect tail-branch to next opfunc.
// - br_if: load jmp_ip, pop i32 cond, conditional select between jmp_ip and fallthrough slot, then indirect tail-branch.
// - br_table: pop i32 idx, clamp via min(max_size, idx), load table[idx], then indirect tail-branch.

[[gnu::noinline]] void codegen_br(T0 ip, T1 sp, T2 local_base)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
    optable::uwvmint_br<opt, T0, T1, T2>(ip, sp, local_base);
}

[[gnu::noinline]] void codegen_br_if(T0 ip, T1 sp, T2 local_base)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
    optable::uwvmint_br_if<opt, 0uz, T0, T1, T2>(ip, sp, local_base);
}

[[gnu::noinline]] void codegen_br_table(T0 ip, T1 sp, T2 local_base)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
    optable::uwvmint_br_table<opt, 0uz, T0, T1, T2>(ip, sp, local_base);
}

int main()
{
    // Force emission of the templates for inspection in the generated assembly.
    opfunc_t f0 = &optable::uwvmint_br<optable::uwvm_interpreter_translate_option_t{.is_tail_call = true}, T0, T1, T2>;
    opfunc_t f1 = &optable::uwvmint_br_if<optable::uwvm_interpreter_translate_option_t{.is_tail_call = true}, 0uz, T0, T1, T2>;
    opfunc_t f2 = &optable::uwvmint_br_table<optable::uwvm_interpreter_translate_option_t{.is_tail_call = true}, 0uz, T0, T1, T2>;
    codegen_keep(f0);
    codegen_keep(f1);
    codegen_keep(f2);
    return 0;
}
