// Exact array boundaries and ordered aggregate visitation under real exclusive
// admission. Native component correctness; this is not a Wasm timing result.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <memory>
#include <utility>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace g = ::uwvm2::object::global;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;
namespace
{
    ::std::size_t checks{}, collections{}, cases{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_array_scan_batches line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::recursive_type_section declarations()
    {
        t::recursive_type_section section{}; section.type_count = 3u;
        t::recursive_group group{};
        t::field_type number{}; number.storage.value.kind = t::value_kind::i32; number.mutable_ = true;
        t::field_type edge{}; edge.storage.value.kind = t::value_kind::reference; edge.mutable_ = true;
        edge.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
        edge.storage.value.nullable = true;
        t::sub_type scalar{}; scalar.kind = t::composite_kind::struct_;
        scalar.fields.push_back(number); group.types.push_back(::std::move(scalar));
        t::sub_type refs{}; refs.kind = t::composite_kind::array;
        refs.fields.push_back(edge); group.types.push_back(::std::move(refs));
        t::sub_type node{}; node.kind = t::composite_kind::struct_;
        node.fields.push_back(edge); node.fields.push_back(number); group.types.push_back(::std::move(node));
        section.groups.push_back(::std::move(group)); return section;
    }
    void check_case(::std::size_t length, ::std::size_t position, unsigned pattern)
    {
        ++cases;
        auto schema{declarations()};
        ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array stores{::std::make_shared<gc::gc_object_store>(schema, leases[0uz]),
                           ::std::make_shared<gc::gc_object_store>(schema, leases[1uz])};
        CHECK(stores[0uz]->valid() && stores[1uz]->valid());
        reference a{}, b{}, array{}, dead{};
        CHECK(stores[0uz]->struct_new_default(2u, a) == status::ok);
        CHECK(stores[1uz]->struct_new_default(2u, b) == status::ok);
        CHECK(stores[0uz]->struct_set(a, 0uz, value::reference(b)) == status::ok);
        CHECK(stores[1uz]->struct_set(b, 0uz, value::reference(a)) == status::ok);
        CHECK(stores[0uz]->struct_set(a, 1uz, value::i32(111u)) == status::ok);
        CHECK(stores[1uz]->struct_set(b, 1uz, value::i32(222u)) == status::ok);
        CHECK(stores[0uz]->struct_new_default(0u, dead) == status::ok);
        CHECK(stores[0uz]->array_new_default(1u, length, array) == status::ok);
        reference unusual_null{}; unusual_null.kind = g::wasm_ref_kind::wasm_null;
        unusual_null.storage.ptr = ::std::addressof(checks); // A null kind does not depend on zero pointer bytes.
        for(::std::size_t i{}; i != length; ++i)
        {
            auto const immediate{pattern == 0u || (pattern == 2u && i % 2uz == 0uz) ? unusual_null :
                g::make_wasm_i31_reference(i % 2uz == 0uz ? -1 : 0x3fffffff)};
            CHECK(stores[0uz]->array_set(array, i, value::reference(immediate)) == status::ok);
        }
        bool const has_edge{position < length};
        if(has_edge) { CHECK(stores[0uz]->array_set(array, position, value::reference(a)) == status::ok); }
        {
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            CHECK(static_cast<bool>(exclusive));
            ::std::size_t reclaimed{999uz};
            CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                stores.data(), stores.size(), ::std::addressof(array), 1uz, reclaimed) == status::ok);
            CHECK(reclaimed == (has_edge ? 1uz : 3uz)); ++collections;
        }
        value observed{};
        CHECK(stores[0uz]->struct_get(dead, 0uz, false, observed) == status::invalid_reference);
        CHECK(stores[0uz]->struct_get(a, 1uz, false, observed) == (has_edge ? status::ok : status::invalid_reference));
        if(has_edge)
        {
            CHECK(observed.as<::std::uint32_t>() == 111u);
            CHECK(stores[0uz]->struct_get(b, 1uz, false, observed) == status::ok);
            CHECK(observed.as<::std::uint32_t>() == 222u);
        }
        for(::std::size_t i{}; i != length; ++i)
        {
            CHECK(stores[0uz]->array_get(array, i, false, observed) == status::ok);
            auto const actual{observed.as<reference>()};
            if(has_edge && i == position) { CHECK(actual.kind == a.kind && actual.storage.ptr == a.storage.ptr); }
            else if(pattern == 0u || (pattern == 2u && i % 2uz == 0uz))
            { CHECK(actual.kind == g::wasm_ref_kind::wasm_null && actual.storage.ptr == unusual_null.storage.ptr); }
            else { CHECK(actual.kind == g::wasm_ref_kind::wasm_i31 && actual.storage.wasm_i31.get_s() == (i % 2uz == 0uz ? -1 : 0x3fffffff)); }
        }
        {
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            CHECK(static_cast<bool>(exclusive));
            ::std::size_t reclaimed{};
            CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                stores.data(), stores.size(), nullptr, 0uz, reclaimed) == status::ok);
            CHECK(reclaimed == (has_edge ? 3uz : 1uz)); ++collections;
        }
    }
}
int main()
{
    for(::std::size_t length{}; length <= 40uz; ++length)
        for(::std::size_t position{}; position <= length; ++position)
            for(unsigned pattern{}; pattern != 3u; ++pattern) { check_case(length, position, pattern); }
    for(auto length : {63uz, 64uz, 65uz, 127uz, 128uz, 129uz, 255uz, 256uz, 257uz, 4095uz, 4096uz, 4097uz})
        for(auto position : {0uz, 1uz, 7uz, 8uz, 9uz, length / 2uz, length - 2uz, length - 1uz, length})
            for(unsigned pattern{}; pattern != 3u; ++pattern) { check_case(length, position, pattern); }
    ::fast_io::io::println("{\"checks\":", checks, ",\"cases\":", cases, ",\"collections\":", collections, "}");
}
