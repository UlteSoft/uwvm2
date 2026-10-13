// Regression for a host that allocates before transferring a GC store from
// unique to shared ownership. Older global-object registration allows a
// foreign lookup after that transfer. A block registry must either preserve
// this behavior or explicitly change the public store construction contract.
// This is a native lifecycle probe, not a collector benchmark.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;

namespace
{
    void check(bool condition, char const* label)
    {
        if(!condition)
        {
            ::fast_io::io::perr("FAIL GC ownership transition: ",
                                ::fast_io::mnp::os_c_str(label), "\n");
            ::fast_io::fast_terminate();
        }
    }

    gc_type::recursive_type_section make_types()
    {
        gc_type::recursive_type_section section{};
        section.type_count = 1u;
        gc_type::recursive_group group{};
        group.first_type_index = 0u;
        gc_type::sub_type structure{};
        structure.kind = gc_type::composite_kind::struct_;
        gc_type::field_type field{};
        field.storage.value.kind = gc_type::value_kind::i32;
        structure.fields.push_back(field);
        group.types.push_back(::std::move(structure));
        section.groups.push_back(::std::move(group));
        return section;
    }
}

int main()
{
    auto const types{make_types()};
    auto source_roots{::std::make_shared<gc::gc_lease_owner>()};
    auto raw{::std::make_unique<gc::gc_object_store>(types, source_roots)};
    check(raw->valid(), "source validates under unique ownership");
    gc::gc_reference reference{};
    check(raw->struct_new_default(0u, reference) == gc::gc_object_status::ok,
          "source publishes one object before shared ownership");
    ::std::shared_ptr<gc::gc_object_store> source{::std::move(raw)};
    ::std::weak_ptr<gc::gc_object_store> weak_source{source};
    auto recipient_roots{::std::make_shared<gc::gc_lease_owner>()};
    auto recipient{::std::make_shared<gc::gc_object_store>(types, recipient_roots)};
    check(recipient->valid(), "recipient validates");
    gc::gc_object_value observed{};
    check(recipient->struct_get(reference, 0u, false, observed) == gc::gc_object_status::ok &&
          observed.as<::std::uint32_t>() == 0u,
          "foreign lookup works after promotion to shared ownership");
    source.reset();
    source_roots.reset();
    check(!weak_source.expired(), "recipient lease retains promoted source");
    recipient.reset();
    recipient_roots.reset();
    check(weak_source.expired(), "release of recipient lease frees source");
    ::fast_io::io::println("PASS GC ownership transition");
}
