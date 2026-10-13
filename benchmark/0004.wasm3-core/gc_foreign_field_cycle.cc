// Release-blocker probe: two modules' GC objects embed references to one
// another. Expected until a real collector/lease redesign: return 2 because
// the store-owned object value leases keep both otherwise dead arenas alive.
#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace r = ::uwvm2::uwvm::runtime::storage;

[[nodiscard]] static t::recursive_type_section one_reference_struct()
{
    t::recursive_type_section section{};
    section.type_count = 1u;
    t::recursive_group group{};
    group.first_type_index = 0u;
    t::sub_type structure{};
    structure.kind = t::composite_kind::struct_;
    t::field_type field{};
    field.storage.value.kind = t::value_kind::reference;
    field.storage.value.heap = t::heap_type{
        static_cast<::std::int_least64_t>(t::abstract_heap_type::struct_)};
    field.storage.value.nullable = true;
    field.mutable_ = true;
    structure.fields.push_back(field);
    group.types.push_back(::std::move(structure));
    section.groups.push_back(::std::move(group));
    return section;
}

// The module lease owners themselves are independent of the stores. Dropping
// them here isolates object-owned `value_leases` from ordinary module roots.
static int mutual_cycle()
{
    auto section{one_reference_struct()};
    auto a_roots{::std::make_shared<r::gc_lease_owner>()};
    auto b_roots{::std::make_shared<r::gc_lease_owner>()};
    auto a{::std::make_shared<r::gc_object_store>(section, a_roots)};
    auto b{::std::make_shared<r::gc_object_store>(section, b_roots)};
    if(!a->valid() || !b->valid()) { ::fast_io::fast_terminate(); }
    r::gc_reference a_object{}, b_object{};
    if(a->struct_new_default(0u, a_object) != r::gc_object_status::ok ||
       b->struct_new_default(0u, b_object) != r::gc_object_status::ok ||
       a->struct_set(a_object, 0u, r::gc_object_value::reference(b_object)) != r::gc_object_status::ok ||
       b->struct_set(b_object, 0u, r::gc_object_value::reference(a_object)) != r::gc_object_status::ok)
    { ::fast_io::fast_terminate(); }
    r::gc_object_value observed{};
    if(a->struct_get(a_object, 0u, false, observed) != r::gc_object_status::ok ||
       observed.as<r::gc_reference>().storage.ptr != b_object.storage.ptr ||
       b->struct_get(b_object, 0u, false, observed) != r::gc_object_status::ok ||
       observed.as<r::gc_reference>().storage.ptr != a_object.storage.ptr)
    { ::fast_io::fast_terminate(); }
    ::std::weak_ptr<r::gc_object_store> a_weak{a}, b_weak{b};
    a.reset();
    b.reset();
    a_roots.reset();
    b_roots.reset();
    if(a_weak.expired() && b_weak.expired()) { return 0; }
    return !a_weak.expired() && !b_weak.expired() ? 2 : 3;
}

static int overwritten_field()
{
    auto section{one_reference_struct()};
    auto a_roots{::std::make_shared<r::gc_lease_owner>()};
    auto b_roots{::std::make_shared<r::gc_lease_owner>()};
    auto a{::std::make_shared<r::gc_object_store>(section, a_roots)};
    auto b{::std::make_shared<r::gc_object_store>(section, b_roots)};
    if(!a->valid() || !b->valid()) { ::fast_io::fast_terminate(); }
    r::gc_reference a_object{}, b_object{};
    if(a->struct_new_default(0u, a_object) != r::gc_object_status::ok ||
       b->struct_new_default(0u, b_object) != r::gc_object_status::ok ||
       a->struct_set(a_object, 0u, r::gc_object_value::reference(b_object)) != r::gc_object_status::ok)
    { ::fast_io::fast_terminate(); }
    r::gc_reference null_ref{};
    null_ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
    if(a->struct_set(a_object, 0u, r::gc_object_value::reference(null_ref)) != r::gc_object_status::ok)
    { ::fast_io::fast_terminate(); }
    ::std::weak_ptr<r::gc_object_store> b_weak{b};
    b.reset();
    b_roots.reset();
    a_roots.reset();
    if(b_weak.expired()) { return 0; }
    a.reset();
    // If B dies only when A's object store dies, the overwritten value lease
    // (not a remaining reference field or module root) retained B needlessly.
    return b_weak.expired() ? 4 : 5;
}

static int mutual_bridge()
{
    auto section{one_reference_struct()};
    auto a_roots{::std::make_shared<r::gc_lease_owner>()};
    auto b_roots{::std::make_shared<r::gc_lease_owner>()};
    auto a{::std::make_shared<r::gc_object_store>(section, a_roots)};
    auto b{::std::make_shared<r::gc_object_store>(section, b_roots)};
    if(!a->valid() || !b->valid()) { ::fast_io::fast_terminate(); }
    r::gc_reference a_object{}, b_object{}, a_bridge{}, b_bridge{}, observed{};
    if(a->struct_new_default(0u, a_object) != r::gc_object_status::ok ||
       b->struct_new_default(0u, b_object) != r::gc_object_status::ok ||
       a->extern_convert_any(b_object, a_bridge) != r::gc_object_status::ok ||
       b->extern_convert_any(a_object, b_bridge) != r::gc_object_status::ok ||
       a->any_convert_extern(a_bridge, observed) != r::gc_object_status::ok ||
       observed.storage.ptr != b_object.storage.ptr ||
       b->any_convert_extern(b_bridge, observed) != r::gc_object_status::ok ||
       observed.storage.ptr != a_object.storage.ptr)
    { ::fast_io::fast_terminate(); }
    ::std::weak_ptr<r::gc_object_store> a_weak{a}, b_weak{b};
    a.reset();
    b.reset();
    a_roots.reset();
    b_roots.reset();
    if(a_weak.expired() && b_weak.expired()) { return 0; }
    return !a_weak.expired() && !b_weak.expired() ? 6 : 7;
}

int main(int argc, char const* const* argv)
{
    if(argc != 2) { return 64; }
    if(::std::string_view{argv[1]} == "mutual-cycle") { return mutual_cycle(); }
    if(::std::string_view{argv[1]} == "overwritten-field") { return overwritten_field(); }
    if(::std::string_view{argv[1]} == "mutual-bridge") { return mutual_bridge(); }
    return 64;
}
