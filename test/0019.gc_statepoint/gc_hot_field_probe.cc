// Assembly comparison only. These wrappers do not allocate, collect or stop
// the VM. Compile identical bytes against the original and collector headers.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
namespace gc = ::uwvm2::uwvm::runtime::storage;

extern "C" [[gnu::noinline]] gc::gc_object_status uwvm_gc_probe_struct_get(
    gc::gc_object_store const* store, gc::gc_reference const* reference,
    ::std::size_t field, gc::gc_object_value* output) noexcept
{
    if(store == nullptr || reference == nullptr || output == nullptr)
    { return gc::gc_object_status::invalid_value; }
    return store->struct_get(*reference, field, false, *output);
}
extern "C" [[gnu::noinline]] gc::gc_object_status uwvm_gc_probe_struct_set(
    gc::gc_object_store* store, gc::gc_reference const* reference,
    ::std::size_t field, ::std::uint32_t value) noexcept
{
    if(store == nullptr || reference == nullptr)
    { return gc::gc_object_status::invalid_value; }
    return store->struct_set(*reference, field, gc::gc_object_value::i32(value));
}
extern "C" [[gnu::noinline]] gc::gc_object_status uwvm_gc_probe_array_get(
    gc::gc_object_store const* store, gc::gc_reference const* reference,
    ::std::size_t index, gc::gc_object_value* output) noexcept
{
    if(store == nullptr || reference == nullptr || output == nullptr)
    { return gc::gc_object_status::invalid_value; }
    return store->array_get(*reference, index, false, *output);
}
extern "C" [[gnu::noinline]] gc::gc_object_status uwvm_gc_probe_array_set(
    gc::gc_object_store* store, gc::gc_reference const* reference,
    ::std::size_t index, ::std::uint32_t value) noexcept
{
    if(store == nullptr || reference == nullptr)
    { return gc::gc_object_status::invalid_value; }
    return store->array_set(*reference, index, gc::gc_object_value::i32(value));
}
