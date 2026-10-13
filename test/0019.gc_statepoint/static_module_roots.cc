// Native static-root candidate regression using the actual runtime module.
// This is not an activated product collector or a JIT/interpreter root test.
#include <uwvm2/uwvm/runtime/storage/gc_static_roots.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>
#include <fast_io.h>

namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace global = ::uwvm2::object::global;
namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
using reference = storage::gc_reference;
using root_status = storage::gc_static_root_status;

static ::std::size_t checks{};
static void check(bool condition, char const* label) noexcept
{
    ++checks;
    if(!condition)
    {
        ::fast_io::io::perrln("static module roots failed: ", ::fast_io::mnp::os_c_str(label));
        ::fast_io::fast_terminate();
    }
}

static types::recursive_type_section declarations()
{
    types::recursive_type_section section{};
    types::recursive_group group{};
    types::sub_type structure{};
    structure.kind = types::composite_kind::struct_;
    types::field_type field{};
    field.storage.value.kind = types::value_kind::i32;
    field.mutable_ = true;
    structure.fields.push_back(field);
    group.types.push_back(::std::move(structure));
    section.groups.push_back(::std::move(group));
    section.type_count = 1u;
    return section;
}

static reference allocate(storage::wasm_module_storage_t& module, ::std::uint32_t value)
{
    auto const input{storage::gc_object_value::i32(value)};
    reference result{};
    check(module.gc_store->struct_new(0u, ::std::addressof(input), 1uz, result) ==
          storage::gc_object_status::ok, "struct allocation");
    return result;
}

struct root_builder
{
    ::std::vector<reference> values{};
    bool operator()(reference value) noexcept
    {
        // The caller reserves before root publication. A stopped-world visitor
        // does not allocate, reenter Wasm or dereference the reference payload.
        check(values.size() != values.capacity(), "reserved root extent");
        values.push_back(value);
        return true;
    }
    void clear() noexcept { values.clear(); }
};

static bool same_reference(reference lhs, reference rhs) noexcept
{
    if(lhs.kind != rhs.kind) { return false; }
    if(lhs.kind == global::wasm_ref_kind::wasm_i31)
    { return lhs.storage.wasm_i31.get_u() == rhs.storage.wasm_i31.get_u(); }
    return lhs.storage.ptr == rhs.storage.ptr;
}

static bool contains(root_builder const& roots, reference wanted) noexcept
{
    for(auto const value : roots.values)
    { if(same_reference(value, wanted)) { return true; } }
    return false;
}

int main()
{
    ::std::array<storage::wasm_module_storage_t, 3uz> modules{};
    auto const layouts{declarations()};
    for(auto& module : modules)
    {
        module.gc_lease_roots = ::std::make_shared<storage::gc_lease_owner>();
        module.gc_store = ::std::make_shared<storage::gc_object_store>(layouts, module.gc_lease_roots);
        check(module.gc_store->valid(), "real module store");
    }
    ::std::array<storage::wasm_module_storage_t const*, 3uz> cohort{
        ::std::addressof(modules[0]), ::std::addressof(modules[1]), ::std::addressof(modules[2])};
    root_builder roots{};
    roots.values.reserve(128uz);
    auto const visit{[&]()
    {
        roots.clear();
        return storage::visit_quiescent_cohort_static_roots(cohort, roots);
    }};
    check(visit().status == root_status::ok && roots.values.empty(), "empty initialized cohort");

    auto const global_ref{allocate(modules[0], 101u)};
    auto const table_ref{allocate(modules[1], 202u)};
    auto const segment_ref{allocate(modules[2], 303u)};
    auto const dropped_ref{allocate(modules[2], 404u)};
    auto const numeric_ref{allocate(modules[0], 505u)};
    auto& globals{modules[0].local_defined_global_vec_storage};
    globals.resize(3uz);
    for(auto& record : globals) { record.init_state = storage::wasm_global_init_state::initialized; }
    globals.index_unchecked(0uz).global.kind = global::global_type::wasm_ref;
    globals.index_unchecked(0uz).global.storage.ref = global_ref;
    globals.index_unchecked(1uz).global.kind = global::global_type::wasm_v128;
    // Token-shaped numeric bytes must not keep the otherwise unreachable object.
    ::std::memcpy(::std::addressof(globals.index_unchecked(1uz).global.storage.v128),
                  ::std::addressof(numeric_ref), sizeof(numeric_ref));
    globals.index_unchecked(2uz).global.kind = global::global_type::wasm_i64;
    globals.index_unchecked(2uz).global.storage.i64 = 999;

    auto& tables{modules[1].local_defined_table_vec_storage};
    tables.resize(1uz);
    auto& slots{tables.front_unchecked().elems};
    slots.push_back(storage::runtime_table_slot_from_gc_reference(table_ref));
    slots.push_back(storage::runtime_table_slot_from_gc_reference(global_ref));
    auto const i31{global::make_wasm_i31_reference(-7)};
    slots.push_back(storage::runtime_table_slot_from_gc_reference(i31));
    slots.push_back({});

    auto& payload{modules[2].element_expr_gc_ref_vec_storage};
    payload.push_back(segment_ref);
    payload.push_back(dropped_ref);
    auto& segments{modules[2].local_defined_element_vec_storage};
    segments.resize(2uz);
    // [two owned references] end; both complete segment slices stay inside the
    // immutable backing vector. Every pointer is published after final resize.
    segments.index_unchecked(0uz).element.gc_ref_begin = payload.data();
    segments.index_unchecked(0uz).element.gc_ref_end = payload.data() + 1uz;
    segments.index_unchecked(0uz).element.kind = storage::wasm_element_segment_kind::passive;
    segments.index_unchecked(1uz).element.gc_ref_begin = payload.data() + 1uz;
    segments.index_unchecked(1uz).element.gc_ref_end = payload.data() + 2uz;
    storage::drop_wasm_element_segment_payload(segments.index_unchecked(1uz).element);
    auto result{visit()};
    check(result.status == root_status::ok && result.visited == 6uz, "exact six complete static carriers");
    check(contains(roots, global_ref) && contains(roots, table_ref) && contains(roots, segment_ref), "all live sources retained");
    check(!contains(roots, dropped_ref) && !contains(roots, numeric_ref), "dropped and numeric bytes excluded");
    check(contains(roots, i31) && contains(roots, reference{}), "i31 and null carriers preserved");

    // Resolve native aliases only through exact cohort record membership. The
    // referenced physical table/global is scanned once in its actual owner.
    auto& table_aliases{modules[0].imported_table_vec_storage};
    table_aliases.resize(2uz);
    using table_link = storage::imported_table_storage_t::imported_table_link_kind;
    table_aliases.index_unchecked(0uz).link_kind = table_link::imported;
    table_aliases.index_unchecked(0uz).target.imported_ptr = ::std::addressof(table_aliases.index_unchecked(1uz));
    table_aliases.index_unchecked(1uz).link_kind = table_link::defined;
    table_aliases.index_unchecked(1uz).target.defined_ptr = ::std::addressof(tables.front_unchecked());
    auto& global_aliases{modules[1].imported_global_vec_storage};
    global_aliases.resize(2uz);
    using global_link = storage::imported_global_storage_t::imported_global_link_kind;
    global_aliases.index_unchecked(0uz).link_kind = global_link::imported;
    global_aliases.index_unchecked(0uz).target.imported_ptr = ::std::addressof(global_aliases.index_unchecked(1uz));
    global_aliases.index_unchecked(1uz).link_kind = global_link::defined;
    global_aliases.index_unchecked(1uz).target.defined_ptr = ::std::addressof(globals.front_unchecked());
    check(visit().status == root_status::ok && roots.values.size() == 6uz, "import aliases do not duplicate owners");

    auto const saved_table_alias{table_aliases.index_unchecked(1uz)};
    table_aliases.index_unchecked(1uz).link_kind = table_link::imported;
    table_aliases.index_unchecked(1uz).target.imported_ptr = ::std::addressof(table_aliases.front_unchecked());
    check(visit().status == root_status::incomplete_imports && roots.values.empty(), "cyclic table aliases rejected before publication");
    table_aliases.index_unchecked(1uz) = saved_table_alias;
    auto const saved_global_alias{global_aliases.index_unchecked(1uz)};
    global_aliases.index_unchecked(1uz).target.defined_ptr = reinterpret_cast<storage::local_defined_global_storage_t*>(::std::uintptr_t{0x123u});
    check(visit().status == root_status::incomplete_imports && roots.values.empty(), "forged defined import never dereferenced");
    global_aliases.index_unchecked(1uz) = saved_global_alias;
    global_aliases.index_unchecked(1uz).link_kind = global_link::local_imported;
    global_aliases.index_unchecked(1uz).target.local_imported = {
        reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(::std::uintptr_t{0x123u}), 0uz};
    check(visit().status == root_status::host_roots_required && roots.values.empty(), "host provider cannot imply complete roots");
    global_aliases.index_unchecked(1uz) = saved_global_alias;
    ::std::array incomplete{cohort[0], cohort[2]};
    roots.clear();
    check(storage::visit_quiescent_cohort_static_roots(incomplete, roots).status == root_status::incomplete_imports && roots.values.empty(),
          "omitted import owner rejected");
    ::std::array duplicate{cohort[0], cohort[0]};
    check(storage::visit_quiescent_cohort_static_roots(duplicate, roots).status == root_status::invalid_cohort, "duplicate module rejected");
    ::std::array<storage::wasm_module_storage_t const*, 1uz> null_module{};
    check(storage::visit_quiescent_cohort_static_roots(null_module, roots).status == root_status::invalid_cohort, "null native module rejected");

    auto& element{segments.index_unchecked(0uz).element};
    auto const saved_element{element};
    element.gc_ref_begin = payload.data() + payload.size();
    element.gc_ref_end = element.gc_ref_begin;
    check(visit().status == root_status::ok && roots.values.size() == 5uz, "empty one-past segment slice");
    element = saved_element;
    element.gc_ref_end = nullptr;
    check(visit().status == root_status::invalid_storage, "half-null payload rejected");
    element = saved_element;
    element.gc_ref_begin = reinterpret_cast<reference const*>(reinterpret_cast<::std::uintptr_t>(payload.data()) + 1u);
    check(visit().status == root_status::invalid_storage, "misaligned interior payload rejected without access");
    element = saved_element;
    element.gc_ref_begin = ::std::addressof(global_ref);
    element.gc_ref_end = ::std::addressof(global_ref) + 1u;
    check(visit().status == root_status::invalid_storage, "unowned payload array rejected without access");
    element = saved_element;
    element.externref_begin = reinterpret_cast<void* const*>(::std::uintptr_t{0x123u});
    element.externref_end = element.externref_begin;
    check(visit().status == root_status::invalid_storage, "mixed payload representations rejected");
    element = saved_element;
    auto& dropped{segments.index_unchecked(1uz).element};
    dropped.gc_ref_begin = reinterpret_cast<reference const*>(::std::uintptr_t{0x123u});
    dropped.gc_ref_end = reinterpret_cast<reference const*>(::std::uintptr_t{0x321u});
    check(visit().status == root_status::ok && roots.values.size() == 6uz, "dropped payload cannot be dereferenced or rooted");

    globals.front_unchecked().init_state = storage::wasm_global_init_state::initializing;
    check(visit().status == root_status::uninitialized_global, "initializing global rejected");
    globals.front_unchecked().init_state = storage::wasm_global_init_state::initialized;
    globals.front_unchecked().global.kind = static_cast<global::global_type>(255u);
    check(visit().status == root_status::invalid_storage, "unknown native global union discriminant rejected");
    globals.front_unchecked().global.kind = global::global_type::wasm_ref;
    globals.front_unchecked().global.storage.ref.kind = global::wasm_ref_kind::wasm_func;
    check(visit().status == root_status::rejected_reference, "parser-only function index is not a runtime root");
    globals.front_unchecked().global.storage.ref.kind = static_cast<global::wasm_ref_kind>(255u);
    ::std::size_t unexpected_visits{};
    auto const must_not_visit{[&](reference) noexcept { ++unexpected_visits; return true; }};
    auto const unknown_shape{storage::visit_quiescent_cohort_static_roots(cohort, must_not_visit)};
    check(unknown_shape.status == root_status::rejected_reference && unknown_shape.visited == 0uz && unexpected_visits == 0uz,
          "unknown reference kind rejected before visitor");
    reference forged_struct{};
    forged_struct.kind = global::wasm_ref_kind::wasm_struct;
    forged_struct.storage.ptr = reinterpret_cast<void*>(::std::uintptr_t{0x123u});
    globals.front_unchecked().global.storage.ref = forged_struct;
    auto const checked_struct_visitor{[&](reference value) noexcept
    {
        // Shape alone does not prove that a token belongs to a live object.
        // The ACTUAL store checks its token registry before dereferencing a
        // purported struct. A rejected snapshot can never authorize a sweep.
        storage::gc_object_value observed{};
        return modules[0].gc_store->struct_get(value, 0u, false, observed) == storage::gc_object_status::ok;
    }};
    auto const forged_token{storage::visit_quiescent_cohort_static_roots(cohort, checked_struct_visitor)};
    check(forged_token.status == root_status::rejected_reference && forged_token.visited == 0uz,
          "legal reference shape with forged token rejected by real store");
    globals.front_unchecked().global.storage.ref = global_ref;
    auto const saved_slot{slots.front_unchecked()};
    slots.front_unchecked().type = static_cast<storage::local_defined_table_elem_storage_type_t>(255u);
    check(visit().status == root_status::invalid_storage, "unknown table payload discriminant rejected");
    slots.front_unchecked() = saved_slot;
    ::std::size_t rejected_visits{};
    auto const reject{[&](reference) noexcept { ++rejected_visits; return false; }};
    check(storage::visit_quiescent_cohort_static_roots(cohort, reject).status == root_status::rejected_reference && rejected_visits == 1uz,
          "visitor rejection stops all further accesses");

#if defined(UWVM_STATIC_ROOT_SWEEP_PROBE)
    // The explicit test-only sweep overlay is aggregate-only, exclusive and
    // requires ALL process stores. This native fixture is single-threaded and
    // owns exactly these three stores; no hidden exn/extern registry is used.
    // Actual reclamation counts, not mere root survival, qualify this experiment.
    auto const sweep{[&](::std::size_t expected)
    {
        auto const observed{visit()};
        check(observed.status == root_status::ok, "root snapshot complete before sweep");
        ::std::array stores{modules[0].gc_store, modules[1].gc_store, modules[2].gc_store};
        ::std::size_t reclaimed{};
        check(storage::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(),
            roots.values.data(), roots.values.size(), reclaimed) == storage::gc_object_status::ok, "exclusive aggregate sweep");
        check(reclaimed == expected, "exact actual reclamation count");
    }};
    sweep(2uz); // Token-shaped v128 and dropped segment object are unreachable.
    storage::gc_object_value observed{};
    for(auto const live : {global_ref, table_ref, segment_ref})
    { check(modules[0].gc_store->struct_get(live, 0u, false, observed) == storage::gc_object_status::ok, "live static root survives actual collection"); }
    for(auto const dead : {numeric_ref, dropped_ref})
    { check(modules[0].gc_store->struct_get(dead, 0u, false, observed) == storage::gc_object_status::invalid_reference, "unreachable static bytes reclaimed"); }
    globals.front_unchecked().global.storage.ref = numeric_ref;
    auto const stale_token{storage::visit_quiescent_cohort_static_roots(cohort, checked_struct_visitor)};
    check(stale_token.status == root_status::rejected_reference && stale_token.visited == 0uz,
          "previously valid reclaimed token rejected before collection commit");
    globals.front_unchecked().global.storage.ref = global_ref;
    storage::drop_wasm_element_segment_payload(element);
    globals.front_unchecked().global.storage.ref = {};
    slots.index_unchecked(1uz) = {};
    sweep(2uz); // Replacing the global and dropping the live segment retires both.
    check(modules[0].gc_store->struct_get(global_ref, 0u, false, observed) == storage::gc_object_status::invalid_reference,
          "replaced global root does not retain old object");
    check(modules[0].gc_store->struct_get(segment_ref, 0u, false, observed) == storage::gc_object_status::invalid_reference,
          "dropped passive segment does not retain backing-array object");
    slots.front_unchecked() = {};
    sweep(1uz); // Final table root removed.
    check(modules[0].gc_store->struct_get(table_ref, 0u, false, observed) == storage::gc_object_status::invalid_reference,
          "replaced table root reclaimed");
#endif

    // Exception and opaque host carriers must remain visible to the collector
    // codec. They are tested after the aggregate-only experimental sweeps:
    // that prototype correctly rejects a process containing an exn registry.
    auto const tag{::std::static_pointer_cast<void const>(::std::make_shared<unsigned char>())};
    auto const instance{::uwvm2::runtime::exception::value::make(tag, {})};
    reference exception_ref{};
    check(modules[0].gc_store->make_exn_reference(instance, exception_ref) == storage::gc_object_status::ok,
          "actual immutable exception token");
    globals.index_unchecked(2uz).global.kind = global::global_type::wasm_ref;
    globals.index_unchecked(2uz).global.storage.ref = exception_ref;
    unsigned char host_value{};
    auto& host_payload{modules[2].element_expr_externref_vec_storage};
    host_payload.push_back(::std::addressof(host_value));
    host_payload.push_back(nullptr);
    // [single-threaded unpublished test phase] Retire the old payload view
    // before publishing a distinct complete host-reference representation.
    element = {};
    element.kind = storage::wasm_element_segment_kind::passive;
    element.externref_begin = host_payload.data();
    element.externref_end = host_payload.data() + host_payload.size();
    reference host_ref{};
    host_ref.storage.ptr = ::std::addressof(host_value);
    host_ref.kind = global::wasm_ref_kind::wasm_extern;
    modules[0].local_defined_function_vec_storage.resize(1uz);
    reference function_ref{};
    function_ref.storage.ptr = ::std::addressof(modules[0].local_defined_function_vec_storage.front_unchecked());
    function_ref.kind = global::wasm_ref_kind::wasm_func_defined;
    auto& function_payload{modules[2].element_expr_funcref_vec_storage};
    function_payload.push_back(storage::runtime_table_slot_from_gc_reference(function_ref));
    segments.index_unchecked(1uz).element = {};
    segments.index_unchecked(1uz).element.kind = storage::wasm_element_segment_kind::passive;
    segments.index_unchecked(1uz).element.funcref_begin = function_payload.data();
    segments.index_unchecked(1uz).element.funcref_end = function_payload.data() + function_payload.size();
    check(visit().status == root_status::ok, "exception and two legacy payload representations");
    check(contains(roots, exception_ref) && contains(roots, host_ref) && contains(roots, function_ref),
          "exception/function/host identity preserved without pointer dereference");
    check(modules[0].gc_store->lookup_exn_reference(exception_ref).get() == instance.get(), "exception remains rooted by real token registry");
    storage::drop_wasm_element_segment_payload(element);
    storage::drop_wasm_element_segment_payload(segments.index_unchecked(1uz).element);
    check(visit().status == root_status::ok && !contains(roots, host_ref) && !contains(roots, function_ref),
          "both legacy dropped payloads logically empty");
#if defined(UWVM_STATIC_ROOT_SWEEP_PROBE)
    ::fast_io::io::println("static module root candidate PASS checks=", checks, " actual_collections=3 reclaimed=5");
#else
    ::fast_io::io::println("static module root candidate PASS checks=", checks, " collector_activation=absent");
#endif
}
