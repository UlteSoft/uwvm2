// Keeper-only O3 assembly witness. No native executable is produced or run on
// the development host. Inspect source-bound macro-off and macro-on assembly.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <cstddef>
#include <cstdint>

namespace publication_codegen_20261003
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
#if defined(__GNUC__) || defined(__clang__)
# define GC_PUBLICATION_CODEGEN_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
# define GC_PUBLICATION_CODEGEN_NOINLINE __declspec(noinline)
#else
# define GC_PUBLICATION_CODEGEN_NOINLINE
#endif
    // Four complete initialized inputs are supplied by the real native caller;
    // ordinary struct_new still checks the concrete layout, fields and leases.
    // The schema is the mixed packed/SIMD/typed-reference handoff witness.
    GC_PUBLICATION_CODEGEN_NOINLINE
    [[nodiscard]] gc::gc_object_status allocate(gc::gc_object_store& store,
        gc::gc_object_value const* complete_inputs, gc::gc_reference& result) noexcept
    { return store.struct_new(0u, complete_inputs, 4uz, result); }

    // A real opaque token passes the unchanged acquire membership reader and
    // packed-field/object-mutation checks. This is not a proved raw pointer.
    GC_PUBLICATION_CODEGEN_NOINLINE
    [[nodiscard]] ::std::uint64_t read_packed(gc::gc_object_store const& store,
        gc::gc_reference reference) noexcept
    { return store.struct_get32<false>(reference, 1uz); }

    GC_PUBLICATION_CODEGEN_NOINLINE
    [[nodiscard]] gc::gc_object_status write_reference(gc::gc_object_store& store,
        gc::gc_reference destination, gc::gc_reference source) noexcept
    { return store.struct_set(destination, 3uz, gc::gc_object_value::reference(source)); }
#undef GC_PUBLICATION_CODEGEN_NOINLINE
}
