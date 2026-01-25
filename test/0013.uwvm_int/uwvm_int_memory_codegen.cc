#include <uwvm2/utils/macro/push_macros.h>

#include <uwvm2/runtime/compiler/uwvm_int/optable/memory.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using slot_scalar = optable::wasm_stack_top_i32_i64_f32_f64_u;

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
// third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_memory_codegen.cc -o /tmp/uwvm_int_memory_codegen.s`

using opfunc_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, slot_scalar, slot_scalar>;

[[gnu::noinline]] void end_sink(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(s3);
    codegen_keep(s4);
}

static constexpr optable::uwvm_interpreter_translate_option_t opt_scalar_cache{
    .is_tail_call = true,
    .i32_stack_top_begin_pos = 3uz,
    .i32_stack_top_end_pos = 5uz,
    .i64_stack_top_begin_pos = 3uz,
    .i64_stack_top_end_pos = 5uz,
    .f32_stack_top_begin_pos = 3uz,
    .f32_stack_top_end_pos = 5uz,
    .f64_stack_top_begin_pos = 3uz,
    .f64_stack_top_end_pos = 5uz,
    .v128_stack_top_begin_pos = SIZE_MAX,
    .v128_stack_top_end_pos = SIZE_MAX,
};

[[gnu::noinline]] void codegen_i32_load_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
#if defined(UWVM_SUPPORT_MMAP)
    optable::uwvmint_i32_load_mmap_full<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    optable::uwvmint_i32_load_multithread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# else
    optable::uwvmint_i32_load_singlethread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# endif
#endif
}

#if defined(UWVM_SUPPORT_MMAP)
[[gnu::noinline]] void codegen_i32_load_cached_mmap_path(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i32_load_mmap_path<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_load_cached_mmap_judge(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i32_load_mmap_judge<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}
#endif

[[gnu::noinline]] void codegen_i64_load_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
#if defined(UWVM_SUPPORT_MMAP)
    optable::uwvmint_i64_load_mmap_full<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    optable::uwvmint_i64_load_multithread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# else
    optable::uwvmint_i64_load_singlethread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# endif
#endif
}

#if defined(UWVM_SUPPORT_MMAP)
[[gnu::noinline]] void codegen_i64_load_cached_mmap_path(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i64_load_mmap_path<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i64_load_cached_mmap_judge(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i64_load_mmap_judge<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}
#endif

[[gnu::noinline]] void codegen_i64_store_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
#if defined(UWVM_SUPPORT_MMAP)
    optable::uwvmint_i64_store_mmap_full<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    optable::uwvmint_i64_store_multithread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# else
    optable::uwvmint_i64_store_singlethread_allocator<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
# endif
#endif
}

#if defined(UWVM_SUPPORT_MMAP)
[[gnu::noinline]] void codegen_i64_store_cached_mmap_path(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i64_store_mmap_path<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i64_store_cached_mmap_judge(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_i64_store_mmap_judge<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}
#endif

[[gnu::noinline]] void codegen_memory_size_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_memory_size<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_memory_grow_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    optable::uwvmint_memory_grow<opt_scalar_cache, 3uz, T0, T1, T2, slot_scalar, slot_scalar>(ip, sp, local_base, s3, s4);
}

int main()
{
    opfunc_cached_t end_fn = &end_sink;
    codegen_keep(end_fn);
    return 0;
}

#include <uwvm2/utils/macro/pop_macros.h>
