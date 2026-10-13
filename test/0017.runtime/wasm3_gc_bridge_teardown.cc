// One-direction foreign bridge lease teardown regression; no collector or performance claim.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <cstdlib>
#include <memory>
#include <utility>
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace r = ::uwvm2::uwvm::runtime::storage;
static void check(bool condition, char const* text) {
 if(!condition) { ::fast_io::io::perr("FAIL ", ::fast_io::mnp::os_c_str(text), "\n"); ::fast_io::fast_terminate(); }
}
static t::recursive_type_section types() {
 t::recursive_type_section section{}; section.type_count=1u;
 t::recursive_group group{}; group.first_type_index=0u;
 t::sub_type structure{}; structure.kind=t::composite_kind::struct_;
 t::field_type field{}; field.storage.value.kind=t::value_kind::i32;
 structure.fields.push_back(field); group.types.push_back(::std::move(structure));
 section.groups.push_back(::std::move(group)); return section;
}
int main() {
 auto type_section=types();
 auto source_roots=::std::make_shared<r::gc_lease_owner>();
 auto source=::std::make_shared<r::gc_object_store>(type_section,source_roots);
 auto receiver_roots=::std::make_shared<r::gc_lease_owner>();
 auto receiver=::std::make_shared<r::gc_object_store>(type_section,receiver_roots);
 check(source->valid() && receiver->valid(),"stores validate");
 ::std::weak_ptr<r::gc_object_store> source_weak=source, receiver_weak=receiver;
 r::gc_reference object{}, wrapped{};
 check(source->struct_new_default(0u,object)==r::gc_object_status::ok,"source object");
 check(receiver->extern_convert_any(object,wrapped)==r::gc_object_status::ok,"foreign wrapper");
 source.reset(); source_roots.reset();
 // Runtime module member order destroys gc_lease_roots before gc_store.
 // This removes the independent lease, leaving wrapper.inner_owner as the final source pin.
 receiver_roots.reset();
 check(!source_weak.expired(),"foreign bridge retains source");
 ::fast_io::io::perr("TEARDOWN_BEGIN last foreign inner_owner pin\n");
 receiver.reset();
 check(source_weak.expired() && receiver_weak.expired(),"both stores are released");
 ::fast_io::io::perr("PASS one-direction foreign bridge teardown\n");
}
