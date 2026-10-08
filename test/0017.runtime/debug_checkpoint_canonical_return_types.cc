// Cold declared-type relation only. No checkpoint issuer or native-value authority.
#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;
static void require(bool condition, char const* label)
{
    if(!condition) { ::fast_io::io::perrln("checkpoint canonical return types: ", ::fast_io::mnp::os_c_str(label)); ::fast_io::fast_terminate(); }
}
static t::core_value_type ref(::std::int_least64_t heap, bool nullable = false)
{ return {t::value_kind::reference, {heap}, nullable}; }
static t::recursive_type_section declarations(bool prefix, bool incompatible = false)
{
    t::recursive_type_section section{}; section.type_count = prefix ? 3u : 2u;
    if(prefix)
    {
        t::recursive_group leading{}; t::sub_type function{};
        function.kind = t::composite_kind::function; leading.types.push_back(::std::move(function));
        section.groups.push_back(::std::move(leading));
    }
    t::recursive_group group{}; group.first_type_index = prefix ? 1u : 0u;
    t::sub_type base{}; base.kind = t::composite_kind::struct_; base.final_ = false;
    t::field_type field{}; field.storage.value.kind = incompatible ? t::value_kind::i64 : t::value_kind::i32;
    base.fields.push_back(field); group.types.push_back(::std::move(base));
    t::sub_type child{}; child.kind = t::composite_kind::struct_; child.supertypes.push_back(prefix ? 1u : 0u);
    child.fields.push_back(field); child.fields.push_back(field); group.types.push_back(::std::move(child));
    section.groups.push_back(::std::move(group)); return section;
}
int main()
{
    gc::gc_object_store left{declarations(false)}, shifted{declarations(true)}, unrelated{declarations(false, true)};
    require(left.valid() && shifted.valid() && unrelated.valid(), "real immutable independently initialized stores");
    auto const matches{gc::gc_object_store::canonical_value_type_matches};
    using h = t::abstract_heap_type;
    auto const heap{[](h value) { return static_cast<::std::int_least64_t>(value); }};
    require(matches(&left,ref(1),&left,ref(0)), "actual declared child is covariant with its real parent");
    require(!matches(&left,ref(0),&left,ref(1)), "supertype cannot be assigned to its child");
    require(matches(&shifted,ref(2),&left,ref(0)), "cross-module shifted indices use true canonical subtype");
    require(matches(&shifted,ref(1),&left,ref(0)), "cross-module canonical equality ignores unrelated flat index numbering");
    require(!matches(&unrelated,ref(0),&left,ref(0)), "equal flat indices in different stores are not type identity");
    require(!matches(&shifted,ref(0),&left,ref(0)), "function cannot match unrelated struct despite flat index equality");
    require(matches(&left,ref(1),nullptr,ref(heap(h::eq),true)), "defined struct subtype widens to nullable eq");
    require(!matches(&left,ref(1,true),nullptr,ref(heap(h::eq))), "nullable does not widen to nonnullable");
    require(matches(nullptr,ref(heap(h::none),true),&left,ref(0,true)), "none bottom belongs to actual aggregate family");
    require(!matches(nullptr,ref(heap(h::nofunc),true),&left,ref(0,true)), "function bottom cannot match aggregate family");
    require(matches(nullptr,ref(heap(h::nofunc),true),&shifted,ref(0,true)), "function bottom matches actual defined function family");
    require(!matches(nullptr,ref(heap(h::none),true),&shifted,ref(0,true)), "aggregate bottom cannot match defined function");
    require(matches(nullptr,ref(heap(h::noexn),true),nullptr,ref(heap(h::exn),true)), "shared Core 3 exception abstract subtype");
    require(!matches(nullptr,ref(heap(h::noextern),true),nullptr,ref(heap(h::exn),true)), "extern and exception families stay disjoint");
    require(matches(nullptr,{t::value_kind::v128},nullptr,{t::value_kind::v128}) &&
        !matches(nullptr,{t::value_kind::i64},nullptr,{t::value_kind::f64}), "same physical width never replaces scalar semantic identity");
    require(!matches(&left,ref(0x1'0000'0000ll),&left,ref(0)) &&
        !matches(nullptr,ref(-99),nullptr,ref(heap(h::any))) &&
        !matches(nullptr,ref(t::heap_type::bottom_code),nullptr,ref(heap(h::any))), "bounded defined indices and no live validation bottom");
    ::fast_io::io::println("checkpoint canonical return declaration component PASS; native authority=false");
}
