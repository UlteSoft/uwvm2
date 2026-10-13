// Exact native field hot-path wrappers for the 16fcee and one-block headers.
// Disassemble and normalize these four real implementations; neither wrapper
// replaces the store with a mock or treats a guest token as a native address.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>

namespace gc = ::uwvm2::uwvm::runtime::storage;

extern "C" [[gnu::noinline]] gc::gc_object_status block_struct_get(
    gc::gc_object_store& store, gc::gc_reference reference, ::std::size_t field,
    gc::gc_object_value& result) noexcept
{ return store.struct_get(reference, field, false, result); }

extern "C" [[gnu::noinline]] gc::gc_object_status block_struct_set(
    gc::gc_object_store& store, gc::gc_reference reference, ::std::size_t field,
    gc::gc_object_value value) noexcept
{ return store.struct_set(reference, field, value); }

extern "C" [[gnu::noinline]] gc::gc_object_status block_array_get(
    gc::gc_object_store& store, gc::gc_reference reference, ::std::size_t index,
    gc::gc_object_value& result) noexcept
{ return store.array_get(reference, index, false, result); }

extern "C" [[gnu::noinline]] gc::gc_object_status block_array_set(
    gc::gc_object_store& store, gc::gc_reference reference, ::std::size_t index,
    gc::gc_object_value value) noexcept
{ return store.array_set(reference, index, value); }
