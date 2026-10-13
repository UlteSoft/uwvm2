// Source-only keeper O3 assembly entry points for the real public data APIs.
// These wrappers preserve type/membership/full-range checks; they are not a
// hand-written decoder or an exposed private runtime bypass.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <cstddef>
#include <cstdint>
namespace data_codegen_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
#if defined(__GNUC__) || defined(__clang__)
# define GC_DATA_ASM_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
# define GC_DATA_ASM_NOINLINE __declspec(noinline)
#else
# define GC_DATA_ASM_NOINLINE
#endif
    GC_DATA_ASM_NOINLINE gc::gc_object_status checked_new(gc::gc_object_store& store,
        ::std::uint_least32_t type, ::std::byte const* bytes, ::std::size_t byte_count,
        ::std::size_t offset, ::std::size_t elements, gc::gc_reference& output) noexcept
    { return store.array_new_data(type, bytes, byte_count, offset, elements, output); }
    GC_DATA_ASM_NOINLINE gc::gc_object_status checked_init(gc::gc_object_store& store,
        ::std::uint_least32_t type, gc::gc_reference target, ::std::size_t destination,
        ::std::byte const* bytes, ::std::size_t byte_count, ::std::size_t offset,
        ::std::size_t elements) noexcept
    { return store.array_init_data(type, target, destination, bytes, byte_count, offset, elements); }
#undef GC_DATA_ASM_NOINLINE
}
