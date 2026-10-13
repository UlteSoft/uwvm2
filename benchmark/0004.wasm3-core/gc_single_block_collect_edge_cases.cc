// Native allocation-layout qualification, against exact 16fcee baseline and
// one-block candidate headers. This is not a VM root/automatic-GC benchmark.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using status = gc::gc_object_status;
using value = gc::gc_object_value;
using reference = gc::gc_reference;

#if defined(__linux__) && defined(UWVM2TEST_GC_BLOCK_FAULT_INJECT)
namespace allocation_probe
{
    struct record { void* pointer{}; ::std::size_t bytes{}; bool array{}; bool freed{}; };
    ::std::array<record, 64uz> records{};
    ::std::size_t record_count{}, scalar_calls{}, array_calls{}, allocation_calls{}, failed_calls{};
    ::std::size_t fail_call{}, fail_at_least{};
    bool recording{}, exhausted{};

    void arm(::std::size_t fail_nth = 0uz, ::std::size_t large_failure = 0uz) noexcept
    {
        records = {}; record_count = scalar_calls = array_calls = allocation_calls = failed_calls = 0uz;
        fail_call = fail_nth; fail_at_least = large_failure; exhausted = false; recording = true;
    }
    [[nodiscard]] bool should_fail(::std::size_t bytes, bool array) noexcept
    {
        if(!recording) { return false; }
        ++allocation_calls;
        if(array) { ++array_calls; } else { ++scalar_calls; }
        bool const reject{(fail_call != 0uz && allocation_calls == fail_call) ||
                          (fail_at_least != 0uz && bytes >= fail_at_least)};
        failed_calls += reject;
        return reject;
    }
    void allocated(void* pointer, ::std::size_t bytes, bool array) noexcept
    {
        if(!recording || pointer == nullptr) { return; }
        if(record_count == records.size()) { exhausted = true; return; }
        // [records, records + size) owns fixed, initialized diagnostic slots.
        // ^^ record_count is checked before selecting the next complete slot.
        records[record_count++] = {pointer, bytes, array, false};
    }
    void freed(void* pointer) noexcept
    {
        for(::std::size_t index{}; index != record_count; ++index)
        {
            if(records[index].pointer == pointer && !records[index].freed)
            { records[index].freed = true; return; }
        }
    }
    [[nodiscard]] bool all_freed() noexcept
    {
        if(exhausted) { return false; }
        for(::std::size_t index{}; index != record_count; ++index)
        { if(!records[index].freed) { return false; } }
        return true;
    }
}

// ELF wrapping is correctness-only. It preserves the real allocator/deallocator
// and causes deterministic nothrow failure without requesting huge RSS. Native
// performance binaries use a different fixture with no wrapper instrumentation.
extern "C" void* block_real_new(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnwmRKSt9nothrow_t");
extern "C" void* block_real_array_new(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnamRKSt9nothrow_t");
extern "C" void block_real_delete(void*) noexcept asm("__real__ZdlPv");
extern "C" void block_real_sized_delete(void*, ::std::size_t) noexcept asm("__real__ZdlPvm");
extern "C" void block_real_array_delete(void*) noexcept asm("__real__ZdaPv");
extern "C" void block_real_sized_array_delete(void*, ::std::size_t) noexcept asm("__real__ZdaPvm");
extern "C" void* block_wrap_new(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnwmRKSt9nothrow_t");
extern "C" void* block_wrap_array_new(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnamRKSt9nothrow_t");
extern "C" void block_wrap_delete(void*) noexcept asm("__wrap__ZdlPv");
extern "C" void block_wrap_sized_delete(void*, ::std::size_t) noexcept asm("__wrap__ZdlPvm");
extern "C" void block_wrap_array_delete(void*) noexcept asm("__wrap__ZdaPv");
extern "C" void block_wrap_sized_array_delete(void*, ::std::size_t) noexcept asm("__wrap__ZdaPvm");

extern "C" void* block_wrap_new(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(allocation_probe::should_fail(bytes, false)) { return nullptr; }
    auto* result{block_real_new(bytes, tag)};
    allocation_probe::allocated(result, bytes, false);
    return result;
}
extern "C" void* block_wrap_array_new(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(allocation_probe::should_fail(bytes, true)) { return nullptr; }
    auto* result{block_real_array_new(bytes, tag)};
    allocation_probe::allocated(result, bytes, true);
    return result;
}
extern "C" void block_wrap_delete(void* pointer) noexcept
{ allocation_probe::freed(pointer); block_real_delete(pointer); }
extern "C" void block_wrap_sized_delete(void* pointer, ::std::size_t bytes) noexcept
{ allocation_probe::freed(pointer); block_real_sized_delete(pointer, bytes); }
extern "C" void block_wrap_array_delete(void* pointer) noexcept
{ allocation_probe::freed(pointer); block_real_array_delete(pointer); }
extern "C" void block_wrap_sized_array_delete(void* pointer, ::std::size_t bytes) noexcept
{ allocation_probe::freed(pointer); block_real_sized_array_delete(pointer, bytes); }
#endif

namespace
{
    unsigned checks{}, collections{};
    ::std::size_t reclaimed_total{};
#if defined(UWVM2TEST_GC_SINGLE_BLOCK_CANDIDATE)
    constexpr bool single_block{true};
#else
    constexpr bool single_block{false};
#endif
    void require(bool result, char const* label) noexcept
    {
        ++checks;
        if(!result)
        {
            ::fast_io::io::perrln("FAIL one-block collector layout: ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }
    type::field_type field(type::value_kind kind, bool mutable_ = true,
                           type::packed_kind packed = type::packed_kind::none)
    {
        type::field_type result{};
        result.mutable_ = mutable_;
        result.storage.packed = packed;
        result.storage.value.kind = kind;
        if(kind == type::value_kind::reference)
        {
            result.storage.value.heap = {static_cast<::std::int_least64_t>(type::abstract_heap_type::any)};
            result.storage.value.nullable = true;
        }
        return result;
    }
    type::recursive_type_section declarations()
    {
        type::recursive_type_section result{};
        result.type_count = 8u;
        type::recursive_group group{};
        auto append{[&](type::composite_kind kind, ::std::initializer_list<type::field_type> fields)
        {
            type::sub_type type_{}; type_.kind = kind;
            for(auto const& next : fields) { type_.fields.push_back(next); }
            group.types.push_back(::std::move(type_));
        }};
        append(type::composite_kind::struct_, {field(type::value_kind::i32, false)}); // 0
        append(type::composite_kind::array, {field(type::value_kind::i64)}); // 1
        append(type::composite_kind::array, {field(type::value_kind::reference)}); // 2
        append(type::composite_kind::struct_, {}); // 3: zero fields
        append(type::composite_kind::struct_, {
            field(type::value_kind::i32, true, type::packed_kind::i8),
            field(type::value_kind::i32, true, type::packed_kind::i16),
            field(type::value_kind::i32), field(type::value_kind::i64),
            field(type::value_kind::f32), field(type::value_kind::f64),
            field(type::value_kind::v128), field(type::value_kind::reference)}); // 4
        append(type::composite_kind::struct_, {field(type::value_kind::reference), field(type::value_kind::reference)}); // 5
        append(type::composite_kind::struct_, {field(type::value_kind::reference)}); // 6
        append(type::composite_kind::array, {field(type::value_kind::v128)}); // 7
        result.groups.push_back(::std::move(group));
        return result;
    }
    template<::std::size_t Size>
    void collect(::std::array<::std::shared_ptr<gc::gc_object_store>, Size> const& cohort,
                 reference const* roots, ::std::size_t count, ::std::size_t expected)
    {
        ::std::size_t reclaimed{};
        require(gc::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), Size,
            roots, count, reclaimed) == status::ok, "closed aggregate cohort collection succeeds");
        require(reclaimed == expected, "collection releases exactly the unreachable object blocks");
        ++collections; reclaimed_total += reclaimed;
    }
    void numeric_and_carriers(type::recursive_type_section const& types)
    {
        auto store{::std::make_shared<gc::gc_object_store>(types)};
        require(store->valid(), "all numeric/packed/reference declarations valid");
        ::std::array cohort{store};
        reference empty_struct{}, empty_array{};
        require(store->struct_new(3u, nullptr, 0uz, empty_struct) == status::ok, "zero-field struct construction");
        require(store->array_new_default(1u, 0uz, empty_array) == status::ok, "zero-length array construction");
        ::std::size_t length{99uz}; value observed{};
        require(store->array_length(empty_array, length) == status::ok && length == 0uz, "empty array header length");
        require(store->array_get(empty_array, 0uz, false, observed) == status::out_of_bounds, "empty array does not access absent slots");
        require(store->array_copy(empty_array, 0uz, empty_array, 0uz, 0uz) == status::ok,
            "zero-length copy returns before forming an interior pointer from a null view");
        require(store->struct_get(empty_struct, 0uz, false, observed) == status::out_of_bounds, "zero-field struct does not access absent slots");
        ::std::array<::std::byte, 16uz> vector{};
        for(::std::size_t index{}; index != vector.size(); ++index)
        { vector[index] = static_cast<::std::byte>(index * 17uz + 3uz); }
        ::std::array fields{value::i32(0x1ffu), value::i32(0x1beefu), value::i32(0x1234'5678u),
            value::i64(0x1234'5678'9abc'def0ULL), value::f32(::std::bit_cast<float>(0x7fc1'2345u)),
            value::f64(::std::bit_cast<double>(0x8000'0000'0000'0000ULL)), value::from(vector), value::reference({})};
        reference aggregate{};
        require(store->struct_new(4u, fields.data(), fields.size(), aggregate) == status::ok, "all full-carrier struct fields constructed");
        for(::std::size_t index{}; index != fields.size(); ++index)
        {
            auto expected{fields[index]};
            if(index == 0uz) { expected = value::i32(0xffu); }
            if(index == 1uz) { expected = value::i32(0xbeefu); }
            require(store->struct_get(aggregate, index, false, observed) == status::ok,
                "all packed/numeric/v128/reference fields remain readable");
            if(index == 7uz)
            {
                // Host reference-struct padding is not a Wasm value. Compare
                // both semantic fields, never indeterminate host padding bytes.
                auto const reference_value{observed.as<reference>()};
                require(reference_value.kind == global::wasm_ref_kind::wasm_null &&
                    reference_value.storage.ptr == nullptr, "nullable reference payload preserved");
            }
            else { require(observed.bits == expected.bits, "all sixteen numeric/packed/v128 carrier bytes preserved"); }
        }
        require(store->struct_get(aggregate, 0uz, true, observed) == status::ok &&
            observed.as<::std::uint32_t>() == 0xffff'ffffu, "packed i8 signed extension preserved");
        require(store->struct_get(aggregate, 1uz, true, observed) == status::ok &&
            observed.as<::std::uint32_t>() == 0xffff'beefu, "packed i16 signed extension preserved");
        reference defaults{};
        require(store->struct_new_default(4u, defaults) == status::ok, "all default carriers constructed");
        for(::std::size_t index{}; index != fields.size(); ++index)
        {
            auto const expected{index == 7uz ? value::reference({}) : value{}};
            require(store->struct_get(defaults, index, false, observed) == status::ok, "default field readable");
            if(index == 7uz)
            {
                auto const reference_value{observed.as<reference>()};
                require(reference_value.kind == global::wasm_ref_kind::wasm_null &&
                    reference_value.storage.ptr == nullptr, "default reference is semantic null");
            }
            else { require(observed.bits == expected.bits, "default initialization writes every numeric/v128 carrier byte"); }
            auto const replacement{index == 7uz ? value::reference(empty_struct) : fields[index]};
            require(store->struct_set(defaults, index, replacement) == status::ok, "mutable typed field setter accepts complete carrier");
        }
        reference immutable{}; value const one[]{value::i32(7u)};
        require(store->struct_new(0u, one, 1uz, immutable) == status::ok, "immutable scalar constructed");
        require(store->struct_set(immutable, 0uz, value::i32(8u)) == status::immutable_field,
            "single block does not weaken immutable-field validation");
        reference large{};
        require(store->array_new(1u, value::i64(0x1020'3040'5060'7080ULL), 65536uz, large) == status::ok,
            "large aligned tail array constructed");
        require(store->array_get(large, 65535uz, false, observed) == status::ok &&
            observed.as<::std::uint64_t>() == 0x1020'3040'5060'7080ULL, "last large-array slot remains aligned and initialized");
        require(store->array_set(large, 32769uz, value::i64(0xffee'ddcc'bbaa'9988ULL)) == status::ok,
            "middle large-array i64 slot writable");
        require(store->array_copy(large, 32770uz, large, 32769uz, 1uz) == status::ok &&
            store->array_get(large, 32770uz, false, observed) == status::ok &&
            observed.as<::std::uint64_t>() == 0xffee'ddcc'bbaa'9988ULL, "value view preserves bounded memmove copy");
        reference vectors{};
        require(store->array_new(7u, value::from(vector), 17uz, vectors) == status::ok &&
            store->array_get(vectors, 16uz, false, observed) == status::ok && observed.bits == vector,
            "v128 array keeps full sixteen-byte elements");
        auto const too_long{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(value) + 1uz};
        require(store->array_new(1u, value::i64(0u), too_long, vectors) == status::size_overflow, "PTRDIFF_MAX payload overflow rejected");
        require(store->array_new(1u, value::i64(0u), (::std::numeric_limits<::std::size_t>::max)(), vectors) == status::size_overflow,
            "SIZE_MAX arithmetic overflow rejected");
        collect(cohort, nullptr, 0uz, 7uz);
        require(store->struct_get(aggregate, 0uz, false, observed) == status::invalid_reference,
            "swept token never resolves through freed interior storage");
    }
#if defined(__linux__) && defined(UWVM2TEST_GC_BLOCK_FAULT_INJECT)
    void allocation_counts(type::recursive_type_section const& types)
    {
        {
            auto store{::std::make_shared<gc::gc_object_store>(types)};
            ::std::array cohort{store}; reference object{};
            allocation_probe::arm();
            auto const result{store->struct_new(3u, nullptr, 0uz, object)};
            allocation_probe::recording = false;
            require(result == status::ok && allocation_probe::scalar_calls == 1uz &&
                allocation_probe::array_calls == 0uz, "zero-field struct creates one complete header block and no tail allocation");
            collect(cohort, nullptr, 0uz, 1uz);
            require(allocation_probe::all_freed(), "zero-field struct sweep returns the original allocation base");
        }
        for(auto const length : {0uz, 1uz, 1024uz})
        {
            auto store{::std::make_shared<gc::gc_object_store>(types)};
            ::std::array cohort{store}; reference object{};
            allocation_probe::arm();
            auto const result{store->array_new(1u, value::i64(77u), length, object)};
            allocation_probe::recording = false;
            require(result == status::ok && !allocation_probe::exhausted, "counted numeric allocation succeeds");
            require(allocation_probe::scalar_calls == 1uz &&
                allocation_probe::array_calls == ((!single_block && length != 0uz) ? 1uz : 0uz),
                "nonempty candidate needs one allocator call; empty allocation keeps one header call");
            for(::std::size_t index{}; index != allocation_probe::record_count; ++index)
            {
                require(reinterpret_cast<::std::uintptr_t>(allocation_probe::records[index].pointer) % alignof(value) == 0uz,
                    "real native allocator pointer supplies eight-byte slot alignment");
            }
            collect(cohort, nullptr, 0uz, 1uz);
            require(allocation_probe::all_freed(), "sweep returns each counted native block once through the matching delete path");
        }
        for(auto const failure : {1uz, 2uz})
        {
            auto store{::std::make_shared<gc::gc_object_store>(types)};
            ::std::array cohort{store}; reference object{};
            allocation_probe::arm(failure);
            auto const result{store->array_new(1u, value::i64(1u), 1uz, object)};
            allocation_probe::recording = false;
            bool const has_second{!single_block && failure == 2uz};
            bool const fails{failure == 1uz || has_second};
            require(result == (fails ? status::out_of_memory : status::ok), "deterministic first/second allocation fault follows actual allocation count");
            require(allocation_probe::failed_calls == (fails ? 1uz : 0uz), "fault fixture observes the exact rejected allocation");
            collect(cohort, nullptr, 0uz, fails ? 0uz : 1uz);
            require(allocation_probe::all_freed(), "failed tail allocation frees its header; successful candidate has no second allocation");
        }
        {
            auto store{::std::make_shared<gc::gc_object_store>(types)};
            ::std::array cohort{store}; reference object{};
            allocation_probe::arm(0uz, 4096uz);
            auto const result{store->array_new(1u, value::i64(1u), 1024uz, object)};
            allocation_probe::recording = false;
            require(result == status::out_of_memory && allocation_probe::failed_calls == 1uz,
                "large block OOM is injected without allocating large RSS");
            require(allocation_probe::scalar_calls == 1uz && allocation_probe::array_calls == (single_block ? 0uz : 1uz),
                "large candidate fails its one combined scalar allocation");
            collect(cohort, nullptr, 0uz, 0uz);
            require(allocation_probe::all_freed(), "large allocation failure returns every already-allocated header");
        }
        {
            auto store{::std::make_shared<gc::gc_object_store>(types)};
            ::std::array cohort{store}; reference object{};
            auto const boundary{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(value)};
            allocation_probe::arm(0uz, 4096uz);
            auto const result{store->array_new(1u, value::i64(1u), boundary, object)};
            allocation_probe::recording = false;
            require(result == (single_block ? status::size_overflow : status::out_of_memory),
                "combined header plus payload rejects total PTRDIFF_MAX overflow before allocation");
            require(single_block ? allocation_probe::allocation_calls == 0uz : allocation_probe::all_freed(),
                "new total-length overflow makes no allocator call; old payload-only failure cleans header");
            collect(cohort, nullptr, 0uz, 0uz);
        }
    }
#endif
    void failure_and_foreign_cycle(type::recursive_type_section const& types)
    {
        auto leases_a{::std::make_shared<gc::gc_lease_owner>()};
        auto leases_b{::std::make_shared<gc::gc_lease_owner>()};
        auto a{::std::make_shared<gc::gc_object_store>(types, leases_a)};
        auto b{::std::make_shared<gc::gc_object_store>(types, leases_b)};
        ::std::array cohort{a, b};
        reference source{}; value const one[]{value::i32(42u)};
        require(b->struct_new(0u, one, 1uz, source) == status::ok, "foreign source scalar constructed");
        value readback{};
        require(a->struct_get(source, 0uz, false, readback) == status::ok, "foreign module lease acquired before failed object construction");
        reference forged{}; forged.kind = global::wasm_ref_kind::wasm_struct;
        forged.storage.ptr = reinterpret_cast<void*>(::std::uintptr_t{1u});
        ::std::array<::std::byte, 2uz * sizeof(reference)> stack{};
        // [stack, stack + sizeof(stack)) owns two complete reference carriers.
        ::std::memcpy(stack.data(), ::std::addressof(source), sizeof(source));
        // [first complete carrier][second complete carrier] end
        //                         ^^ sizeof(reference) selects the checked second slot.
        ::std::memcpy(stack.data() + sizeof(reference), ::std::addressof(forged), sizeof(forged));
        for(unsigned attempt{}; attempt != 256u; ++attempt)
        {
            reference rejected{};
            require(a->struct_new_from_stack(5u, stack.data(), stack.size(), rejected) == status::invalid_value,
                "partial construction releases a block after retaining a real foreign first field");
            require(a->array_new_fixed_from_stack(2u, stack.data(), stack.size(), 2uz, rejected) == status::invalid_value,
                "partial array construction releases a block and its retained foreign first element");
        }
#if defined(__linux__) && defined(UWVM2TEST_GC_BLOCK_FAULT_INJECT)
        {
            // The prior foreign read populated the module lease list. The only
            // new lease allocation belongs to this unpublished object, after
            // one combined block or the baseline header plus separate values.
            auto const lease_call{single_block ? 2uz : 3uz};
            reference rejected{};
            allocation_probe::arm(lease_call);
            auto const result{a->struct_new_from_stack(5u, stack.data(), stack.size(), rejected)};
            allocation_probe::recording = false;
            require(result == status::out_of_memory && allocation_probe::failed_calls == 1uz &&
                allocation_probe::allocation_calls == lease_call, "foreign object-owned lease OOM occurs after initialized object storage");
            require(allocation_probe::all_freed(), "foreign lease OOM retires every unpublished object/value block");
        }
#endif
        reference node_a{}, node_b{};
        require(a->struct_new_default(6u, node_a) == status::ok && b->struct_new_default(6u, node_b) == status::ok,
            "cross-store cycle nodes constructed");
        require(a->struct_set(node_a, 0uz, value::reference(node_b)) == status::ok &&
            b->struct_set(node_b, 0uz, value::reference(node_a)) == status::ok, "two-way foreign aggregate leases installed");
        collect(cohort, &node_a, 1uz, 1uz); // source is unrooted, cycle is rooted transitively
        require(a->struct_get(node_a, 0uz, false, readback) == status::ok &&
            readback.as<reference>().storage.ptr == node_b.storage.ptr, "one precise root keeps both foreign cycle blocks live");
        collect(cohort, nullptr, 0uz, 2uz);
        require(a->struct_get(node_a, 0uz, false, readback) == status::invalid_reference &&
            b->struct_get(node_b, 0uz, false, readback) == status::invalid_reference, "unrooted cyclic blocks are freed from both indexes");
        reference later{};
        require(a->struct_new_default(6u, later) == status::ok && later.storage.ptr != node_a.storage.ptr &&
            later.storage.ptr != node_b.storage.ptr, "opaque token identity stays nonrecycling after combined-block free");
        collect(cohort, nullptr, 0uz, 1uz);
        ::std::weak_ptr<gc::gc_object_store> weak_a{a}, weak_b{b};
        cohort = {}; a.reset(); b.reset(); leases_a.reset(); leases_b.reset();
        require(weak_a.expired() && weak_b.expired(), "cycle reclamation and teardown release canonical and foreign leases");
    }
}

int main()
{
    auto const types{declarations()};
    numeric_and_carriers(types);
#if defined(__linux__) && defined(UWVM2TEST_GC_BLOCK_FAULT_INJECT)
    allocation_counts(types);
#endif
    failure_and_foreign_cycle(types);
    ::fast_io::io::println("GC_SINGLE_BLOCK_EDGE {\"checks\":", checks,
        ",\"collections\":", collections, ",\"reclaimed\":", reclaimed_total,
        ",\"single_block\":", ::fast_io::mnp::os_c_str(single_block ? "true" : "false"),
        ",\"native_component_only\":true,\"automatic_vm_gc\":false}");
}
