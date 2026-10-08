#include <fast_io.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace r = ::uwvm2::uwvm::runtime::storage;
namespace g = ::uwvm2::object::global;

static void check(bool condition, char const* label)
{
    if(!condition) { ::fast_io::io::perrln("gc_object_store: ", ::fast_io::mnp::os_c_str(label)); ::fast_io::fast_terminate(); }
}
static t::field_type field(t::value_kind kind, bool mutable_, t::packed_kind packed = t::packed_kind::none,
                           bool nullable = false, t::heap_type heap = {})
{
    t::field_type result{};
    result.storage.value.kind = kind;
    result.storage.value.heap = heap;
    result.storage.value.nullable = nullable;
    result.storage.packed = packed;
    result.mutable_ = mutable_;
    return result;
}
static t::recursive_type_section make_types()
{
    t::recursive_type_section section{};
    section.type_count = 4;
    t::recursive_group group{};
    group.first_type_index = 0;
    t::sub_type structure{};
    structure.kind = t::composite_kind::struct_;
    structure.fields.push_back(field(t::value_kind::i32, true, t::packed_kind::i8));
    structure.fields.push_back(field(t::value_kind::i64, false));
    structure.fields.push_back(field(t::value_kind::reference, true, t::packed_kind::none, true,
        t::heap_type{static_cast<::std::int_least64_t>(t::abstract_heap_type::struct_)}));
    group.types.push_back(::std::move(structure));
    t::sub_type packed_array{};
    packed_array.kind = t::composite_kind::array;
    packed_array.fields.push_back(field(t::value_kind::i32, true, t::packed_kind::i16));
    group.types.push_back(::std::move(packed_array));
    t::sub_type ref_array{};
    ref_array.kind = t::composite_kind::array;
    ref_array.fields.push_back(field(t::value_kind::reference, true, t::packed_kind::none, true,
        t::heap_type{static_cast<::std::int_least64_t>(t::abstract_heap_type::any)}));
    group.types.push_back(::std::move(ref_array));
    t::sub_type func_array{};
    func_array.kind = t::composite_kind::array;
    func_array.fields.push_back(field(t::value_kind::reference, true, t::packed_kind::none, true,
        t::heap_type{static_cast<::std::int_least64_t>(t::abstract_heap_type::func)}));
    group.types.push_back(::std::move(func_array));
    section.groups.push_back(::std::move(group));
    return section;
}

static t::recursive_type_section make_recursive_subtypes(::std::uint_least32_t first = 0u)
{
    t::recursive_type_section section{};
    section.type_count = first + 2u;
    if(first != 0u)
    {
        t::recursive_group prefix{};
        prefix.first_type_index = 0u;
        t::sub_type function{};
        function.kind = t::composite_kind::function;
        prefix.types.push_back(::std::move(function));
        section.groups.push_back(::std::move(prefix));
    }
    t::recursive_group group{};
    group.first_type_index = first;
    t::sub_type base{};
    base.kind = t::composite_kind::struct_;
    base.final_ = false;
    base.fields.push_back(field(t::value_kind::i32, true));
    base.fields.push_back(field(t::value_kind::reference, true, t::packed_kind::none,
                                true, t::heap_type{static_cast<::std::int_least64_t>(first)}));
    t::sub_type derived{base};
    derived.final_ = true;
    derived.supertypes.push_back(first);
    derived.fields.push_back(field(t::value_kind::i64, false));
    group.types.push_back(::std::move(base));
    group.types.push_back(::std::move(derived));
    section.groups.push_back(::std::move(group));
    return section;
}

int main()
{
    // An abstract-only module has no aggregate type layout and must still support
    // ref.test/ref.cast without allocating the 64K object-membership index.
    t::recursive_type_section abstract_types{};
    r::gc_object_store abstract_store{abstract_types};
    check(abstract_store.valid(), "empty abstract GC store");
    auto const i31{g::make_wasm_i31_reference(7)};
    t::core_value_type nonnull_i31{t::value_kind::reference,
        {static_cast<::std::int_least64_t>(t::abstract_heap_type::i31)}, false};
    check(abstract_store.reference_type_matches(i31, nonnull_i31), "abstract i31 cast");
    check(r::uwvm2_gc_reference_type_matches(&abstract_store, &i31, nonnull_i31.heap.code, false),
          "abstract i31 C bridge");
    g::wasm_global_ref_t abstract_null{};
    abstract_null.kind = g::wasm_ref_kind::wasm_null;
    check(!abstract_store.reference_type_matches(abstract_null, nonnull_i31), "abstract non-null reject");
    nonnull_i31.nullable = true;
    check(abstract_store.reference_type_matches(abstract_null, nonnull_i31), "abstract nullable accept");

    auto section{make_types()};
    ::std::weak_ptr<r::gc_object_store> weak;
    g::wasm_global_ref_t retired_bridge{};
    {
        r::wasm_module_storage_t original{};
        original.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        original.gc_store = ::std::make_shared<r::gc_object_store>(section, original.gc_lease_roots);
        check(original.gc_store->valid(), "validated type layouts");
        weak = original.gc_store;
        r::wasm_module_storage_t module{::std::move(original)};
        auto& store{*module.gc_store};

        g::wasm_global_ref_t null_ref{};
        null_ref.kind = g::wasm_ref_kind::wasm_null;
        r::gc_object_value const initial[]{r::gc_object_value::i32(0xffu),
            r::gc_object_value::i64(77u), r::gc_object_value::reference(null_ref)};
        g::wasm_global_ref_t structure{};
        check(store.struct_new(0u, initial, 3u, structure) == r::gc_object_status::ok, "struct.new");
        t::core_value_type struct_ref{t::value_kind::reference,
            {static_cast<::std::int_least64_t>(t::abstract_heap_type::struct_)}, false};
        check(store.reference_type_matches(structure, struct_ref), "checked aggregate cast");
        check(r::uwvm2_gc_reference_type_matches(&store, &structure, struct_ref.heap.code, false),
              "stable C cast bridge");
        check(!r::uwvm2_gc_reference_type_matches(&store, &structure, 0x1'0000'0000ll, false),
              "oversized host heap index never wraps into a local type");
        g::wasm_global_ref_t extern_structure{};
        g::wasm_global_ref_t recovered_structure{};
        check(r::uwvm2_gc_extern_convert_any(&store, &structure, &extern_structure) == r::gc_object_status::ok &&
              extern_structure.kind == g::wasm_ref_kind::wasm_extern,
              "aggregate to extern wrapper");
        check(r::uwvm2_gc_any_convert_extern(&store, &extern_structure, &recovered_structure) ==
              r::gc_object_status::ok && recovered_structure.kind == structure.kind &&
              recovered_structure.storage.ptr == structure.storage.ptr,
              "extern aggregate round-trip identity");
        retired_bridge = extern_structure;
        g::wasm_global_ref_t extern_i31{};
        g::wasm_global_ref_t recovered_i31{};
        check(store.extern_convert_any(i31, extern_i31) == r::gc_object_status::ok &&
              store.any_convert_extern(extern_i31, recovered_i31) == r::gc_object_status::ok &&
              recovered_i31.kind == g::wasm_ref_kind::wasm_i31 &&
              recovered_i31.storage.wasm_i31.get_u() == i31.storage.wasm_i31.get_u(),
              "i31 extern round-trip");
        g::wasm_global_ref_t host_external{};
        host_external.kind = g::wasm_ref_kind::wasm_extern;
        host_external.storage.ptr = reinterpret_cast<void*>(0x1234u);
        g::wasm_global_ref_t host_any{};
        g::wasm_global_ref_t host_recovered{};
        check(store.any_convert_extern(host_external, host_any) == r::gc_object_status::ok &&
              store.extern_convert_any(host_any, host_recovered) == r::gc_object_status::ok &&
              host_recovered.kind == host_external.kind && host_recovered.storage.ptr == host_external.storage.ptr,
              "opaque host extern round-trip without dereference");
        t::core_value_type any_ref{t::value_kind::reference,
            {static_cast<::std::int_least64_t>(t::abstract_heap_type::any)}, false};
        check(store.reference_type_matches(host_any, any_ref), "opaque host extern is anyref after conversion");
        g::wasm_global_ref_t null_extern{};
        g::wasm_global_ref_t null_any{};
        check(store.extern_convert_any(null_ref, null_extern) == r::gc_object_status::ok &&
              store.any_convert_extern(null_extern, null_any) == r::gc_object_status::ok &&
              null_any.kind == g::wasm_ref_kind::wasm_null, "null conversion round-trip");
        ::std::array<::std::byte, 4u + 8u + sizeof(g::wasm_global_ref_t)> struct_stack{};
        ::std::memcpy(struct_stack.data(), initial[0].bits.data(), 4u);
        ::std::memcpy(struct_stack.data() + 4u, initial[1].bits.data(), 8u);
        ::std::memcpy(struct_stack.data() + 12u, initial[2].bits.data(), sizeof(g::wasm_global_ref_t));
        g::wasm_global_ref_t stack_structure{};
        check(store.struct_new_from_stack(0u, struct_stack.data(), struct_stack.size(), stack_structure) ==
              r::gc_object_status::ok, "struct.new from interpreter stack");
        r::gc_object_value result{};
        check(store.struct_get(structure, 0u, true, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0xffffffffu, "struct.get_s packed i8");
        check(store.struct_get(structure, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0xffu, "struct.get_u packed i8");
        check(store.struct_set(structure, 0u, r::gc_object_value::i32(0x102u)) == r::gc_object_status::ok,
              "struct.set packed i8");
        check(store.struct_get(structure, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 2u, "struct truncation");
        check(store.struct_set(structure, 1u, r::gc_object_value::i64(2u)) ==
              r::gc_object_status::immutable_field, "immutable struct field");
        check(store.struct_get(structure, 3u, false, result) == r::gc_object_status::out_of_bounds,
              "struct bounds");
        check(store.struct_get(null_ref, 0u, false, result) == r::gc_object_status::null_reference,
              "null struct trap");
        g::wasm_global_ref_t forged{};
        forged.kind = g::wasm_ref_kind::wasm_struct;
        forged.storage.ptr = reinterpret_cast<void*>(1u);
        check(!store.reference_type_matches(forged, struct_ref),
              "forged cast payload rejected without dereference");
        check(store.struct_get(forged, 0u, false, result) == r::gc_object_status::invalid_reference,
              "forged object payload rejected without dereference");
        check(store.struct_set(structure, 2u, r::gc_object_value::reference(forged)) ==
              r::gc_object_status::invalid_value, "forged reference field rejected");
        r::wasm_module_storage_t recipient{};
        recipient.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        recipient.gc_store = ::std::make_shared<r::gc_object_store>(section, recipient.gc_lease_roots);
        auto& foreign_store{*recipient.gc_store};
        g::wasm_global_ref_t cross_module_unwrapped{};
        g::wasm_global_ref_t cross_module_rewrapped{};
        check(foreign_store.any_convert_extern(extern_structure, cross_module_unwrapped) ==
              r::gc_object_status::ok && cross_module_unwrapped.storage.ptr == structure.storage.ptr,
              "cross-module checked unwrap");
        check(foreign_store.extern_convert_any(cross_module_unwrapped, cross_module_rewrapped) ==
              r::gc_object_status::ok && cross_module_rewrapped.storage.ptr == extern_structure.storage.ptr,
              "cross-module wrapper identity");
        t::core_value_type canonical_struct_ref{t::value_kind::reference, {0}, false};
        check(foreign_store.valid(), "second canonical store constructed");
        check(foreign_store.reference_type_matches(structure, canonical_struct_ref),
              "cross-store canonical struct identity");
        check(foreign_store.struct_get(structure, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 2u, "cross-store checked struct read");
        {
            auto incompatible{make_types()};
            incompatible.groups.index_unchecked(0u).types.index_unchecked(0u).
                fields.index_unchecked(2u).storage.value.heap =
                {static_cast<::std::int_least64_t>(t::abstract_heap_type::array)};
            r::wasm_module_storage_t mismatch_module{};
            mismatch_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
            mismatch_module.gc_store = ::std::make_shared<r::gc_object_store>(
                incompatible, mismatch_module.gc_lease_roots);
            check(mismatch_module.gc_store->valid() &&
                  !mismatch_module.gc_store->reference_type_matches(structure, canonical_struct_ref),
                  "same local type index with different structure never aliases");

            t::recursive_type_section shifted{};
            shifted.type_count = 5u;
            t::recursive_group prefix{};
            prefix.first_type_index = 0u;
            t::sub_type function{};
            function.kind = t::composite_kind::function;
            prefix.types.push_back(::std::move(function));
            shifted.groups.push_back(::std::move(prefix));
            auto base{make_types()};
            auto body{::std::move(base.groups.index_unchecked(0u))};
            body.first_type_index = 1u;
            shifted.groups.push_back(::std::move(body));
            r::wasm_module_storage_t shifted_module{};
            shifted_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
            shifted_module.gc_store = ::std::make_shared<r::gc_object_store>(
                shifted, shifted_module.gc_lease_roots);
            t::core_value_type shifted_struct_ref{t::value_kind::reference, {1}, false};
            check(shifted_module.gc_store->valid() &&
                  shifted_module.gc_store->reference_type_matches(structure, shifted_struct_ref),
                  "same canonical struct at different local type indexes matches");
        }
        g::wasm_global_ref_t default_structure{};
        check(store.struct_new_default(0u, default_structure) == r::gc_object_status::ok,
              "struct.new_default");
        check(store.struct_get(default_structure, 2u, false, result) == r::gc_object_status::ok &&
              result.as<g::wasm_global_ref_t>().kind == g::wasm_ref_kind::wasm_null,
              "default nullable reference");

        r::gc_object_value const fixed[]{r::gc_object_value::i32(0x8001u),
            r::gc_object_value::i32(2u), r::gc_object_value::i32(3u)};
        g::wasm_global_ref_t array{};
        check(store.array_new_fixed(1u, fixed, 3u, array) == r::gc_object_status::ok,
              "array.new_fixed");
        check(foreign_store.array_get(array, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0x8001u,
              "foreign raw array payload resolved through published owner");
        ::std::size_t foreign_length{};
        check(foreign_store.array_length(array, foreign_length) == r::gc_object_status::ok &&
              foreign_length == 3u, "foreign raw array length retains owner");
        ::std::array<::std::byte, 12u> array_stack{};
        for(::std::size_t i{}; i != 3u; ++i)
        { ::std::memcpy(array_stack.data() + i * 4u, fixed[i].bits.data(), 4u); }
        g::wasm_global_ref_t stack_array{};
        check(store.array_new_fixed_from_stack(1u, array_stack.data(), array_stack.size(), 3u, stack_array) ==
              r::gc_object_status::ok, "array.new_fixed from interpreter stack");
        ::std::size_t length{};
        check(store.array_length(array, length) == r::gc_object_status::ok && length == 3u,
              "array.len");
        check(store.array_get(array, 0u, true, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0xffff8001u, "array.get_s packed i16");
        check(store.array_get(array, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0x8001u, "array.get_u packed i16");
        check(store.array_copy(array, 1u, array, 0u, 2u) == r::gc_object_status::ok,
              "overlapping array.copy");
        check(store.array_get(array, 1u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0x8001u, "overlap copied element 1");
        check(store.array_get(array, 2u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 2u, "overlap copied element 2");
        check(store.array_fill(array, 1u, r::gc_object_value::i32(9u), 2u) == r::gc_object_status::ok,
              "array.fill");
        check(store.array_fill(array, 2u, r::gc_object_value::i32(9u), 2u) ==
              r::gc_object_status::out_of_bounds, "array fill bounds");
        check(store.array_get(array, 3u, false, result) == r::gc_object_status::out_of_bounds,
              "array get bounds");
        check(store.array_get(null_ref, 0u, false, result) == r::gc_object_status::null_reference,
              "null array trap");
        g::wasm_global_ref_t overflow_array{};
        check(store.array_new(1u, r::gc_object_value::i32(0u),
              (::std::numeric_limits<::std::size_t>::max)(), overflow_array) ==
              r::gc_object_status::size_overflow, "array length overflow");

        g::wasm_global_ref_t references{};
        check(store.array_new_default(2u, 2u, references) == r::gc_object_status::ok,
              "reference array.new_default");
        check(store.array_set(references, 0u, r::gc_object_value::reference(structure)) ==
              r::gc_object_status::ok, "reference array.set");
        check(store.array_get(references, 0u, false, result) == r::gc_object_status::ok &&
              result.as<g::wasm_global_ref_t>().storage.ptr == structure.storage.ptr,
              "reference identity");

        // Numeric array segment offsets are bytes on the source and elements on
        // the destination. Keep the segment payload live until after these calls.
        ::std::array<::std::byte, 4u> segment_bytes{::std::byte{1u}, ::std::byte{2u},
                                                     ::std::byte{3u}, ::std::byte{4u}};
        r::local_defined_data_storage_t data_segment{};
        data_segment.data.byte_begin = segment_bytes.data();
        data_segment.data.byte_end = segment_bytes.data() + segment_bytes.size();
        data_segment.data.kind = r::wasm_data_segment_kind::passive;
        module.local_defined_data_vec_storage.push_back(data_segment);
        g::wasm_global_ref_t data_array{};
        check(r::uwvm2_gc_array_new_data(&store, &module, 1u, 0u, 0u, 2u, &data_array) ==
              r::gc_object_status::ok, "array.new_data checked segment");
        check(store.array_get(data_array, 0u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0x0201u, "array.new_data little-endian packed i16");
        check(r::uwvm2_gc_array_init_data(&store, &module, 1u, 0u, &data_array, 1u, 0u, 1u) ==
              r::gc_object_status::ok, "array.init_data checked destination");
        check(store.array_get(data_array, 1u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 0x0201u, "array.init_data value");
        check(r::uwvm2_gc_array_new_data(&store, &module, 1u, 0u, 3u, 1u, &data_array) ==
              r::gc_object_status::out_of_bounds, "array.new_data byte bounds");
        check(r::uwvm2_gc_array_init_data(&store, &module, 1u, 0u, &data_array, 2u, 0u, 1u) ==
              r::gc_object_status::out_of_bounds, "array.init_data destination bounds");
        r::drop_wasm_data_segment_payload(module.local_defined_data_vec_storage.index_unchecked(0u).data);
        check(r::uwvm2_gc_array_new_data(&store, &module, 1u, 0u, 0u, 1u, &data_array) ==
              r::gc_object_status::out_of_bounds, "array.new_data dropped segment");
        check(r::uwvm2_gc_array_new_data(&store, &module, 1u, 0u, 0u, 0u, &data_array) ==
              r::gc_object_status::ok, "array.new_data zero length after drop");

        module.local_defined_function_vec_storage.push_back({});
        ::std::array<r::wasm_element_storage_t::func_idx_t, 1u> function_indices{0u};
        r::local_defined_element_storage_t element_segment{};
        element_segment.element.funcidx_begin = function_indices.data();
        element_segment.element.funcidx_end = function_indices.data() + function_indices.size();
        element_segment.element.kind = r::wasm_element_segment_kind::passive;
        module.local_defined_element_vec_storage.push_back(element_segment);
        g::wasm_global_ref_t element_array{};
        check(r::uwvm2_gc_array_new_elem(&store, &module, 3u, 0u, 0u, 1u, &element_array) ==
              r::gc_object_status::ok, "array.new_elem function identity");
        check(store.array_get(element_array, 0u, false, result) == r::gc_object_status::ok &&
              result.as<g::wasm_global_ref_t>().kind == g::wasm_ref_kind::wasm_func_defined &&
              result.as<g::wasm_global_ref_t>().storage.ptr ==
                  ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(0u)),
              "array.new_elem canonical function pointer");
        g::wasm_global_ref_t init_array{};
        check(store.array_new_default(3u, 1u, init_array) == r::gc_object_status::ok,
              "array.init_elem target");
        check(r::uwvm2_gc_array_init_elem(&store, &module, 3u, 0u, &init_array, 0u, 0u, 1u) ==
              r::gc_object_status::ok, "array.init_elem checked copy");
        check(store.array_get(init_array, 0u, false, result) == r::gc_object_status::ok &&
              result.as<g::wasm_global_ref_t>().storage.ptr ==
                  ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(0u)),
              "array.init_elem identity");
        r::drop_wasm_element_segment_payload(module.local_defined_element_vec_storage.index_unchecked(0u).element);
        check(r::uwvm2_gc_array_new_elem(&store, &module, 3u, 0u, 0u, 1u, &element_array) ==
              r::gc_object_status::out_of_bounds, "array.new_elem dropped segment");

        g::wasm_global_ref_t parallel{};
        check(store.array_new_default(1u, 8u, parallel) == r::gc_object_status::ok,
              "parallel array");
        ::std::atomic<bool> race_ok{true};
        ::std::vector<::std::thread> workers;
        for(::std::size_t worker{}; worker != 8u; ++worker)
        {
            workers.emplace_back([&, worker]
            {
                for(::std::uint32_t iteration{}; iteration != 2000u; ++iteration)
                {
                    if(store.array_set(parallel, worker, r::gc_object_value::i32(iteration)) != r::gc_object_status::ok)
                    { race_ok.store(false); }
                    r::gc_object_value observed{};
                    if(store.array_get(parallel, worker, false, observed) != r::gc_object_status::ok)
                    { race_ok.store(false); }
                }
            });
        }
        for(auto& worker : workers) { worker.join(); }
        check(race_ok.load(), "thread-safe array slots");
        check(store.array_get(parallel, 7u, false, result) == r::gc_object_status::ok &&
              result.as<::std::uint32_t>() == 1999u, "final parallel value");

        check(r::uwvm2_gc_array_length(module.gc_store.get(), parallel, &length) ==
              r::gc_object_status::ok && length == 8u, "stable C bridge");

        // A smoke benchmark on the same cgroup records the cost of membership and
        // mutable-slot locking. It is not a replacement for end-to-end JIT timing.
        constexpr ::std::uint32_t iterations{1000000u};
        auto const measure = [](auto&& body)
        {
            auto const begin{::std::chrono::steady_clock::now()};
            body();
            return ::std::chrono::duration_cast<::std::chrono::nanoseconds>(
                ::std::chrono::steady_clock::now() - begin).count();
        };
        ::std::uint64_t native_sum{};
        volatile ::std::uint32_t native_slot{};
        auto const native_ns{measure([&]
        {
            for(::std::uint32_t i{}; i != iterations; ++i)
            {
                native_slot = i;
                native_sum += native_slot;
            }
        })};
        ::std::uint64_t store_sum{};
        bool bridge_ok{true};
        auto const store_ns{measure([&]
        {
            for(::std::uint32_t i{}; i != iterations; ++i)
            {
                bridge_ok &= store.struct_set(structure, 0u, r::gc_object_value::i32(i)) == r::gc_object_status::ok;
                bridge_ok &= store.struct_get(structure, 0u, false, result) == r::gc_object_status::ok;
                store_sum += result.as<::std::uint32_t>();
            }
        })};
        check(bridge_ok && native_sum != 0u && store_sum != 0u, "benchmark completed");
        ::std::uint64_t immutable_sum{};
        auto const immutable_get_ns{measure([&]
        {
            for(::std::uint32_t i{}; i != iterations; ++i)
            {
                bridge_ok &= store.struct_get(structure, 1u, false, result) == r::gc_object_status::ok;
                immutable_sum += result.as<::std::uint64_t>();
            }
        })};
        ::std::uint64_t mutable_sum{};
        auto const mutable_get_ns{measure([&]
        {
            for(::std::uint32_t i{}; i != iterations; ++i)
            {
                bridge_ok &= store.struct_get(structure, 0u, false, result) == r::gc_object_status::ok;
                mutable_sum += result.as<::std::uint32_t>();
            }
        })};
        check(bridge_ok && immutable_sum != 0u && mutable_sum != 0u, "read benchmarks completed");
        ::fast_io::io::println("gc_object_store: native slot ",
            ::fast_io::mnp::fixed(static_cast<double>(native_ns) / iterations, 2u),
            " ns/op; checked immutable get ",
            ::fast_io::mnp::fixed(static_cast<double>(immutable_get_ns) / iterations, 2u),
            " ns/op; checked mutable get ",
            ::fast_io::mnp::fixed(static_cast<double>(mutable_get_ns) / iterations, 2u),
            " ns/op; checked mutable get/set ",
            ::fast_io::mnp::fixed(static_cast<double>(store_ns) / iterations, 2u), " ns/pair");
        // Releasing the originating module must not invalidate a token still
        // held by a live receiving module. Its independent lease roots keep the
        // arena alive without forming a store-to-store ownership cycle.
        module.gc_store.reset();
        module.gc_lease_roots.reset();
        check(!weak.expired(), "foreign module lease keeps origin arena alive");
        g::wasm_global_ref_t after_origin_reset{};
        check(foreign_store.any_convert_extern(extern_structure, after_origin_reset) ==
              r::gc_object_status::ok && after_origin_reset.storage.ptr == structure.storage.ptr,
              "cross-module wrapper survives origin reset");
        recipient.gc_lease_roots.reset();
        check(weak.expired(), "recipient reset releases origin arena");
        // Keep the raw guest-visible handles after their origin arena has been
        // reclaimed. The receiving store must compare them only as keys: no
        // stale payload may be dereferenced while reporting the type failure.
        check(foreign_store.struct_get(structure, 0u, false, result) ==
              r::gc_object_status::invalid_reference,
              "retired foreign struct handle rejected without dereference");
        check(foreign_store.array_get(array, 0u, false, result) ==
              r::gc_object_status::invalid_reference,
              "retired foreign array handle rejected without dereference");
    }
    check(weak.expired(), "module reset releases cyclic-capable object arena");
    g::wasm_global_ref_t stale_result{};
    check(abstract_store.any_convert_extern(retired_bridge, stale_result) ==
          r::gc_object_status::invalid_reference, "retired bridge token rejected after module reset");
    ::std::weak_ptr<r::gc_object_store> weak_a;
    ::std::weak_ptr<r::gc_object_store> weak_b;
    {
        r::wasm_module_storage_t a{};
        r::wasm_module_storage_t b{};
        a.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        b.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        a.gc_store = ::std::make_shared<r::gc_object_store>(section, a.gc_lease_roots);
        b.gc_store = ::std::make_shared<r::gc_object_store>(section, b.gc_lease_roots);
        weak_a = a.gc_store;
        weak_b = b.gc_store;
        g::wasm_global_ref_t a_object{}, b_object{}, a_token{}, b_token{}, unwrapped{};
        check(a.gc_store->struct_new_default(0u, a_object) == r::gc_object_status::ok &&
              b.gc_store->struct_new_default(0u, b_object) == r::gc_object_status::ok,
              "two module arenas constructed");
        check(a.gc_store->extern_convert_any(a_object, a_token) == r::gc_object_status::ok &&
              b.gc_store->extern_convert_any(b_object, b_token) == r::gc_object_status::ok,
              "two module wrapper tokens constructed");
        check(a.gc_store->any_convert_extern(b_token, unwrapped) == r::gc_object_status::ok &&
              b.gc_store->any_convert_extern(a_token, unwrapped) == r::gc_object_status::ok,
              "two-way module leases constructed");
        a.gc_store.reset();
        b.gc_store.reset();
        check(!weak_a.expired() && !weak_b.expired(), "two-way foreign leases preserve live refs");
        a.gc_lease_roots.reset();
        b.gc_lease_roots.reset();
    }
    check(weak_a.expired() && weak_b.expired(), "two-way leases release without ownership cycle");
    {
        auto transfer_types{make_types()};
        r::wasm_module_storage_t source_module{}, receiving_module{};
        source_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        receiving_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        source_module.gc_store = ::std::make_shared<r::gc_object_store>(
            transfer_types, source_module.gc_lease_roots);
        receiving_module.gc_store = ::std::make_shared<r::gc_object_store>(
            transfer_types, receiving_module.gc_lease_roots);
        g::wasm_global_ref_t transferred{};
        check(source_module.gc_store->struct_new_default(0u, transferred) == r::gc_object_status::ok,
              "raw cross-module transfer source constructed");
        g::wasm_global_ref_t wrapped{};
        check(source_module.gc_store->extern_convert_any(transferred, wrapped) == r::gc_object_status::ok,
              "table externref bridge constructed");
        r::local_defined_table_storage_t receiving_table{};
        receiving_table.owner_module_rt_ptr = ::std::addressof(receiving_module);
        check(r::retain_runtime_table_extern_payload(::std::addressof(receiving_table), wrapped.storage.ptr) ==
              r::gc_object_status::ok, "externref table publication acquires source arena lease");
        ::std::weak_ptr<r::gc_object_store> source_weak{source_module.gc_store};
        source_module.gc_store.reset();
        source_module.gc_lease_roots.reset();
        r::gc_object_value retained_value{};
        g::wasm_global_ref_t recovered{};
        check(!source_weak.expired() &&
              receiving_module.gc_store->any_convert_extern(wrapped, recovered) == r::gc_object_status::ok &&
              recovered.storage.ptr == transferred.storage.ptr &&
              receiving_module.gc_store->struct_get(recovered, 0u, false, retained_value) ==
                  r::gc_object_status::ok,
              "externref table bridge remains usable after source module unload");
        receiving_module.gc_store.reset();
        receiving_module.gc_lease_roots.reset();
        check(source_weak.expired(), "raw transfer lease released with receiving module");
    }
    {
        auto source_types{make_recursive_subtypes()};
        auto receiving_types{make_recursive_subtypes(1u)};
        r::wasm_module_storage_t source_module{}, receiving_module{};
        source_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        receiving_module.gc_lease_roots = ::std::make_shared<r::gc_lease_owner>();
        source_module.gc_store = ::std::make_shared<r::gc_object_store>(
            source_types, source_module.gc_lease_roots);
        receiving_module.gc_store = ::std::make_shared<r::gc_object_store>(
            receiving_types, receiving_module.gc_lease_roots);
        check(source_module.gc_store->valid() && receiving_module.gc_store->valid(),
              "recursive subtype groups canonicalized across shifted modules");
        g::wasm_global_ref_t base{}, derived{}, receiver{};
        check(source_module.gc_store->struct_new_default(0u, base) == r::gc_object_status::ok &&
              source_module.gc_store->struct_new_default(1u, derived) == r::gc_object_status::ok &&
              receiving_module.gc_store->struct_new_default(2u, receiver) == r::gc_object_status::ok,
              "recursive base and derived objects constructed");
        t::core_value_type receiving_base{t::value_kind::reference, {1}, false};
        t::core_value_type receiving_derived{t::value_kind::reference, {2}, false};
        check(receiving_module.gc_store->reference_type_matches(derived, receiving_base) &&
              receiving_module.gc_store->reference_type_matches(derived, receiving_derived) &&
              !receiving_module.gc_store->reference_type_matches(base, receiving_derived),
              "cross-module recursive subtype and reverse rejection");
        check(receiving_module.gc_store->struct_set(receiver, 1u,
              r::gc_object_value::reference(derived)) == r::gc_object_status::ok,
              "foreign aggregate stored with object-owned arena lease");
        ::std::weak_ptr<r::gc_object_store> source_weak{source_module.gc_store};
        source_module.gc_store.reset();
        source_module.gc_lease_roots.reset();
        receiving_module.gc_lease_roots.reset();
        check(!source_weak.expired(), "foreign field keeps originating arena after module unload");
        receiving_module.gc_store.reset();
        check(source_weak.expired(), "foreign field lease releases with receiving arena");
    }
    for(::std::uint32_t exn_repeat{}; exn_repeat != 32u; ++exn_repeat)
    {
        auto source_roots = ::std::make_shared<r::gc_lease_owner>();
        auto recipient_roots = ::std::make_shared<r::gc_lease_owner>();
        auto source = ::std::make_shared<r::gc_object_store>(section, source_roots);
        auto recipient = ::std::make_shared<r::gc_object_store>(section, recipient_roots);
        check(source->valid() && recipient->valid(), "stores can own exception references");
        ::std::weak_ptr<r::gc_object_store> source_weak{source};
        g::wasm_global_ref_t payload_object{};
        check(source->struct_new_default(0u, payload_object) == r::gc_object_status::ok,
              "exception payload local aggregate constructed");
        check(!source->root_reference(payload_object),
              "local exception payload cannot own its issuing store cyclically");
        auto const payload_bytes{::std::as_bytes(::std::span{::std::addressof(payload_object), 1uz})};
        auto payload_field{::uwvm2::runtime::exception::payload_field::wasm_reference(
            payload_bytes, source->root_reference(payload_object))};
        check(static_cast<bool>(payload_field), "complete 16-byte exception payload created");
        auto tag = ::std::make_shared<int>(41);
        auto exception = ::uwvm2::runtime::exception::value::make(tag,
            ::std::span{::std::addressof(*payload_field), 1uz});
        check(static_cast<bool>(exception), "exception value constructed");
        g::wasm_global_ref_t exn{};
        check(source->make_exn_reference(exception, exn) == r::gc_object_status::ok &&
              exn.kind == g::wasm_ref_kind::wasm_exn, "catch_ref publishes VM-managed token");
        check(r::gc_object_store::lookup_exn_reference(exn) == exception,
              "token lookup returns the same immutable exception value");
        check(!source->root_reference(exn) && static_cast<bool>(recipient->root_reference(exn)),
              "own exn token avoids self-cycle while a foreign payload takes a root");
        check(r::uwvm2_gc_retain_reference(recipient.get(), &exn) == r::gc_object_status::ok,
              "receiving module roots imported exnref");
        exception.reset();
        source.reset();
        source_roots.reset();
        check(!source_weak.expired(), "foreign exn token leases issuing object arena");
        auto retained_exception{r::gc_object_store::lookup_exn_reference(exn)};
        check(retained_exception && retained_exception->fields().size() == 1uz &&
              retained_exception->fields()[0].kind() ==
                  ::uwvm2::runtime::exception::payload_kind::wasm_reference,
              "exnref remains live after source module reset");
        retained_exception.reset();
        recipient.reset();
        recipient_roots.reset();
        check(source_weak.expired(), "exception payload releases store after both modules reset");
        check(!r::gc_object_store::lookup_exn_reference(exn),
              "last module root retires token without keeping value forever");
        g::wasm_global_ref_t forged{exn};
        forged.storage.ptr = reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(exn.storage.ptr) + 1u);
        check(!r::gc_object_store::lookup_exn_reference(forged),
              "forged token rejected before reading any payload");
    }
    {
        // Exercise the receiving store's token-membership index beyond one
        // bucket cycle. Re-reading an already rooted token must not create a
        // second root or a fresh allocation on the table.get hot path.
        auto source_roots = ::std::make_shared<r::gc_lease_owner>();
        auto recipient_roots = ::std::make_shared<r::gc_lease_owner>();
        auto source = ::std::make_shared<r::gc_object_store>(section, source_roots);
        auto recipient = ::std::make_shared<r::gc_object_store>(section, recipient_roots);
        check(source->valid() && recipient->valid(), "large exn root stores valid");
        ::std::weak_ptr<r::gc_object_store> source_weak{source};
        auto tag = ::std::make_shared<int>(57);
        auto exception = ::uwvm2::runtime::exception::value::make(
            tag, ::std::span<::uwvm2::runtime::exception::payload_field const>{});
        check(static_cast<bool>(exception), "empty exception payload constructed");
        ::std::vector<g::wasm_global_ref_t> tokens(4096uz);
        for(auto& token : tokens)
        {
            check(source->make_exn_reference(exception, token) == r::gc_object_status::ok,
                  "distinct exception token constructed");
            check(r::uwvm2_gc_retain_reference(recipient.get(), &token) == r::gc_object_status::ok,
                  "distinct exception token transferred");
        }
        auto const reread_begin{::std::chrono::steady_clock::now()};
        for(unsigned repetition{}; repetition != 4u; ++repetition)
        {
            for(auto const& token : tokens)
            {
                check(r::uwvm2_gc_retain_reference(recipient.get(), &token) == r::gc_object_status::ok,
                      "already rooted token retained without duplicate membership");
            }
        }
        auto const reread_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
            ::std::chrono::steady_clock::now() - reread_begin).count()};
        ::fast_io::io::println("exn_root_reread_ns=", reread_ns,
            " tokens=", tokens.size(), " repetitions=4");
        source.reset();
        source_roots.reset();
        check(!source_weak.expired(), "many foreign tokens keep source store alive");
        for(auto const& token : tokens)
        {
            check(r::gc_object_store::lookup_exn_reference(token) == exception,
                  "many foreign tokens remain registered after source module reset");
        }
        recipient.reset();
        recipient_roots.reset();
        check(source_weak.expired(), "last receiving store releases all foreign token roots");
        for(auto const& token : tokens)
        {
            check(!r::gc_object_store::lookup_exn_reference(token),
                  "teardown removes every foreign token from the registry");
        }
    }
    ::std::puts("gc_object_store: PASS");
}
