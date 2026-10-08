// Compile this tiny wrapper at -O3 with the frozen baseline and striped GC
// headers, then compare actual x86-64 normal-path disassembly. No benchmark
// timing is inferred from instruction counts alone.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>

namespace r = ::uwvm2::uwvm::runtime::storage;

extern "C" [[gnu::noinline]] r::gc_object_status gc_local_get(
    r::gc_object_store& store, r::gc_reference reference,
    r::gc_object_value& result) noexcept
{
    return store.struct_get(reference, 0u, false, result);
}

extern "C" [[gnu::noinline]] r::gc_object_status gc_local_set(
    r::gc_object_store& store, r::gc_reference reference) noexcept
{
    return store.struct_set(reference, 0u, r::gc_object_value::i32(7u));
}
