// SOURCE-only component fixture. Build/run belongs to the Linux keeper.
// Real layouts, canonical stores, lease roots and closed-cohort collection;
// no mock token, heap identity, native header, pause capability or LLVM IR.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>
#if !defined(UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_PROBE)
# error The exact native requested-extent probe must be explicitly enabled.
#endif
#if defined(UWVM_MODULE)
# error This component fixture is NONMODULE; MODULE qualification is separate.
#endif

namespace packed_test
{
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    using store_ptr = ::std::shared_ptr<gc::gc_object_store>;
    using value = gc::gc_object_value;
    using ref = gc::gc_reference;
    using status = gc::gc_object_status;
    inline ::std::size_t checks{}, collections{}, reclaimed_total{};
    inline ::std::vector<ref> issued{};
    inline ::std::atomic_size_t fail_extent{}, oom_injected{};
    inline void require(bool result, unsigned line) noexcept
    {
        ++checks;
        if(!result)
        {
            ::fast_io::io::perrln("FAIL packed numeric arrays line ", line);
            ::fast_io::fast_terminate();
        }
    }
#define ARRAY_CHECK(...) ::packed_test::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    [[nodiscard]] bool same(ref a, ref b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; } // Opaque keys, never dereferenced.
    void remember(status result, ref const& reference)
    {
        // Bind the output slot by reference: every argument has finished before
        // reading it, including the factory call that initializes that slot.
        ARRAY_CHECK(result == status::ok); issued.push_back(reference);
    }
    [[nodiscard]] t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 13u;
        t::recursive_group group{};
        group.first_type_index = 0u;
        auto array = [&](t::value_kind kind, t::packed_kind packed, bool mutable_, bool nullable = true)
        {
            t::sub_type type{};
            type.kind = t::composite_kind::array;
            t::field_type field{};
            field.storage.value.kind = kind;
            field.storage.packed = packed;
            field.mutable_ = mutable_;
            if(kind == t::value_kind::reference)
            {
                field.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
                field.storage.value.nullable = nullable;
            }
            type.fields.push_back(field);
            group.types.push_back(::std::move(type));
        };
        array(t::value_kind::i32, t::packed_kind::i8, true);   // 0
        array(t::value_kind::i32, t::packed_kind::i16, true);  // 1
        array(t::value_kind::i32, t::packed_kind::none, true);// 2
        array(t::value_kind::i64, t::packed_kind::none, true);// 3
        array(t::value_kind::f32, t::packed_kind::none, true);// 4
        array(t::value_kind::f64, t::packed_kind::none, true);// 5
        array(t::value_kind::v128,t::packed_kind::none, true);// 6
        array(t::value_kind::i32, t::packed_kind::none, false);// 7 immutable
        array(t::value_kind::reference, t::packed_kind::none, true);       // 8 nullable
        array(t::value_kind::reference, t::packed_kind::none, true, false);// 9 nonnullable
        auto structure = [&](t::value_kind kind, bool mutable_)
        {
            t::sub_type type{};
            type.kind = t::composite_kind::struct_;
            t::field_type field{};
            field.storage.value.kind = kind;
            field.mutable_ = mutable_;
            if(kind == t::value_kind::reference)
            {
                field.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
                field.storage.value.nullable = true;
            }
            type.fields.push_back(field);
            group.types.push_back(::std::move(type));
        };
        structure(t::value_kind::reference, true); // 10 actual recursive aggregate edge
        structure(t::value_kind::i32, true);       // 11 old carrier struct
        structure(t::value_kind::i32, false);      // 12 compact-eligible in a separately ON build
        section.groups.push_back(::std::move(group));
        return section;
    }
    [[nodiscard]] store_ptr make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto section{declarations()};
        auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
        ARRAY_CHECK(store->valid());
        // Constructor owns deep copies and canonical IDs before this section dies.
        return store;
    }
    [[nodiscard]] value observe(store_ptr const& store, ref reference, ::std::size_t index, bool sign = false)
    {
        value result{};
        ARRAY_CHECK(store->array_get(reference, index, sign, result) == status::ok);
        return result;
    }
    [[nodiscard]] value normalized(value input, ::std::uint_least32_t type)
    {
        if(type == 0u) { return value::i32(input.as<::std::uint32_t>() & 255u); }
        if(type == 1u) { return value::i32(input.as<::std::uint32_t>() & 65535u); }
        return input;
    }
    void equals(store_ptr const& store, ref reference, ::std::size_t index, value expected)
    { ARRAY_CHECK(observe(store, reference, index).bits == expected.bits); }
    [[nodiscard]] constexpr ::std::size_t width(::std::uint_least32_t type) noexcept
    {
        constexpr ::std::array<::std::size_t, 8uz> widths{1uz,2uz,4uz,8uz,4uz,8uz,16uz,4uz};
        return widths[type]; // Only the fixture's checked 0..7 numeric type IDs call this.
    }
    void extent(store_ptr const& store, ref reference, ::std::uint_least32_t type, ::std::size_t length,
                bool numeric = true)
    {
        ::std::size_t element{}, backing{}, empty_element{}, empty_backing{};
        bool raw{}, empty_raw{};
        ARRAY_CHECK(store->native_test_array_requested_extent(reference, element, backing, raw));
        ref empty{};
        // Same type and lifetime: zero length exposes the actual fixed requested prefix.
        if(type == 9u)
        { remember(store->array_new(type, observe(store, reference, 0uz), 0uz, empty), empty); }
        else { remember(store->array_new_default(type, 0uz, empty), empty); }
        ARRAY_CHECK(store->native_test_array_requested_extent(empty, empty_element, empty_backing, empty_raw));
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        auto const expected{numeric ? width(type) : sizeof(value)};
        ARRAY_CHECK(raw == numeric && empty_raw == numeric);
#else
        auto const expected{sizeof(value)};
        ARRAY_CHECK(!raw && !empty_raw);
#endif
        ARRAY_CHECK(element == expected && empty_element == expected);
        ARRAY_CHECK(backing >= empty_backing && backing - empty_backing == length * expected);
        // Requested new[] bytes ONLY. This is neither malloc charge nor RSS.
    }
    template<::std::size_t Count>
    void collect(::std::array<store_ptr, Count> const& cohort, ref const* roots, ::std::size_t count,
                 status expected, ::std::size_t expected_reclaimed)
    {
        // ALL real live stores are canonically pinned, including the empty one.
        // Ordinary shared leases/borrowed results have ended; no worker, VM,
        // import, debugger, extern/exn wrapper or unreported native root exists.
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        ARRAY_CHECK(static_cast<bool>(exclusive));
        ::std::size_t reclaimed{(~::std::size_t{})};
        ARRAY_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            cohort.data(), cohort.size(), roots, count, reclaimed) == expected);
        ARRAY_CHECK(reclaimed == expected_reclaimed);
        if(expected == status::ok) { ++collections; reclaimed_total += reclaimed; }
    }
    void numeric_factories(store_ptr const& store)
    {
        ::std::array<value, 7uz> seed{value::i32(0x87654380u), value::i32(0xabcd80f1u),
            value::i32(0x87654321u), value::i64(0x8765432101234567ULL), value::i32(0x7f800123u),
            value::i64(0x7ff0000000000123ULL), value::from(::std::array<::std::uint8_t,16uz>{
                0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255})};
        for(::std::uint_least32_t type{}; type != 7u; ++type)
        {
            auto const input_width{type < 2u ? 4uz : width(type)};
            auto const expected{normalized(seed[type], type)};
            auto const alternative{normalized(value{}, type)};
            ref filled{}, zero{}, fixed{}, stack{}, data{}, empty{};
            auto result{store->array_new(type, seed[type], 5uz, filled)}; remember(result, filled);
            extent(store, filled, type, 5uz);
            remember(store->array_new_default(type, 5uz, zero), zero);
            for(::std::size_t i{}; i != 5uz; ++i) { equals(store, filled, i, expected); equals(store, zero, i, value{}); }
            ::std::array<value,3uz> inputs{seed[type], value{}, seed[type]};
            remember(store->array_new_fixed(type, inputs.data(), inputs.size(), fixed), fixed);
            equals(store, fixed, 0uz, expected); equals(store, fixed, 1uz, alternative);
            ::std::array<::std::byte,32uz> stack_bytes{};
            // [stack_bytes:32B][two width<=16 live source carriers]
            // [safe ] destination+input_width is within this actual native byte array.
            ::fast_io::freestanding::my_memcpy(stack_bytes.data(), seed[type].bits.data(), input_width);
            ::fast_io::freestanding::my_memcpy(stack_bytes.data() + input_width, value{}.bits.data(), input_width);
            remember(store->array_new_fixed_from_stack(type, stack_bytes.data(), 2uz*input_width, 2uz, stack), stack);
            equals(store, stack, 0uz, expected); equals(store, stack, 1uz, alternative);
            ::std::array<::std::byte,33uz> bytes{};
            if(width(type) == 16uz)
            {
                // [bytes:33B][prefix:1B][complete 16B source]
                // [safe ] bytes+1 points inside this real byte array; copy ends before byte17.
                ::fast_io::freestanding::my_memcpy(bytes.data() + 1uz, seed[type].bits.data(), 16uz);
            }
            else
            {
                // Read the real carrier's declared numeric width. Taking an
                // eight-byte view of an i32 carrier would shift its value on BE.
                auto const bits{width(type) <= 4uz ? static_cast<::std::uint64_t>(seed[type].as<::std::uint32_t>()) :
                    seed[type].as<::std::uint64_t>()};
                // [bytes:33B][prefix:1B][complete scalar width<=8][zero tail]
                // [safe                                                    ]
                // ^^ width(type) is 1/2/4/8 here, before deriving either end.
                auto const* last{reinterpret_cast<char const*>(bytes.data()) + 1uz + width(type)};
                auto* first{reinterpret_cast<char*>(bytes.data()) + 1uz};
                auto const encode{[&](auto field) noexcept
                {
                    auto const* end{::fast_io::print_reserve_define(
                        ::fast_io::io_reserve_type<char, decltype(field)>, first, field)};
                    ARRAY_CHECK(end == last);
                }};
                switch(width(type))
                {
                    case 1uz: encode(::fast_io::mnp::le_put<8>(static_cast<::std::uint8_t>(bits))); break;
                    case 2uz: encode(::fast_io::mnp::le_put<16>(static_cast<::std::uint16_t>(bits))); break;
                    case 4uz: encode(::fast_io::mnp::le_put<32>(static_cast<::std::uint32_t>(bits))); break;
                    case 8uz: encode(::fast_io::mnp::le_put<64>(bits)); break;
                    default: ARRAY_CHECK(false);
                }
            }
            // Data bytes are Wasm little endian even on a BE host; source offset1 is unaligned.
            remember(store->array_new_data(type, bytes.data(), bytes.size(), 1uz, 2uz, data), data);
            equals(store, data, 0uz, expected); equals(store, data, 1uz, alternative);
            ARRAY_CHECK(store->array_init_data(type, filled, 1uz, bytes.data(), bytes.size(), 1uz, 2uz) == status::ok);
            equals(store, filled, 1uz, expected); equals(store, filled, 2uz, alternative);
            ARRAY_CHECK(store->array_set(filled, 4uz, value{}) == status::ok);
            ARRAY_CHECK(store->array_fill(filled, 2uz, seed[type], 2uz) == status::ok);
            equals(store, filled, 2uz, expected); equals(store, filled, 3uz, expected);
            auto const before{observe(store, filled, 0uz)};
            ARRAY_CHECK(store->array_set(filled, 5uz, value{}) == status::out_of_bounds);
            ARRAY_CHECK(store->array_fill(filled, 4uz, value{}, 2uz) == status::out_of_bounds);
            ARRAY_CHECK(store->array_copy(filled, 4uz, data, 0uz, 2uz) == status::out_of_bounds);
            ARRAY_CHECK(store->array_init_data(type, filled, 4uz, bytes.data(), bytes.size(), 1uz, 2uz) == status::out_of_bounds);
            for(::std::size_t i{}; i != 5uz; ++i)
            { equals(store, filled, i, i == 4uz ? alternative : expected); }
            ARRAY_CHECK(observe(store, filled, 0uz).bits == before.bits);
            auto unchanged{value::i64(0xabcdef1234567890ULL)};
            ARRAY_CHECK(store->array_get(filled, 5uz, false, unchanged) == status::out_of_bounds);
            ARRAY_CHECK(unchanged.bits == value::i64(0xabcdef1234567890ULL).bits);
            remember(store->array_new_default(type, 0uz, empty), empty);
            ARRAY_CHECK(store->array_fill(empty, 0uz, seed[type], 0uz) == status::ok);
            ARRAY_CHECK(store->array_copy(empty, 0uz, empty, 0uz, 0uz) == status::ok);
            ARRAY_CHECK(store->array_init_data(type, empty, 0uz, nullptr, 0uz, 0uz, 0uz) == status::ok);
            auto sentinel{filled};
            ARRAY_CHECK(store->array_new_fixed(type, nullptr, 1uz, sentinel) == status::invalid_value && same(sentinel, filled));
            ARRAY_CHECK(store->array_new_fixed_from_stack(type, stack_bytes.data(), input_width-1uz, 1uz, sentinel) == status::invalid_value);
            ARRAY_CHECK(store->array_new_data(type, bytes.data(), bytes.size(), bytes.size(), 1uz, sentinel) == status::out_of_bounds);
            ARRAY_CHECK(store->array_new(type, seed[type], (~::std::size_t{}), sentinel) == status::size_overflow);
            ARRAY_CHECK(same(sentinel, filled));
#if defined(UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_OOM) && UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_OOM == 1
            ::std::size_t element{}, backing{}; bool raw{};
            ARRAY_CHECK(store->native_test_array_requested_extent(filled, element, backing, raw));
            auto const injected_before{oom_injected.load(::std::memory_order_relaxed)};
            fail_extent.store(backing, ::std::memory_order_relaxed);
            ARRAY_CHECK(store->array_new(type, seed[type], 5uz, sentinel) == status::out_of_memory);
            ARRAY_CHECK(fail_extent.load(::std::memory_order_relaxed) == 0uz);
            ARRAY_CHECK(oom_injected.load(::std::memory_order_relaxed) == injected_before+1uz);
            ARRAY_CHECK(same(sentinel, filled)); equals(store, filled, 0uz, expected);
            ref retry{}; remember(store->array_new(type, seed[type], 5uz, retry), retry);
            equals(store, retry, 4uz, expected); // Real retry publishes only a newly issued token.
            ARRAY_CHECK(!same(retry, filled));
#endif
        }
        // Integer truncation before the native-byte store is BE significant.
        ref bytes{}, halves{};
        remember(store->array_new(0u, value::i32(0x12345680u), 1uz, bytes), bytes);
        remember(store->array_new(1u, value::i32(0x123480f1u), 1uz, halves), halves);
        ARRAY_CHECK(observe(store, bytes, 0uz, false).as<::std::uint32_t>() == 128u);
        ARRAY_CHECK(observe(store, bytes, 0uz, true).as<::std::uint32_t>() == 0xffffff80u);
        ARRAY_CHECK(observe(store, halves, 0uz, false).as<::std::uint32_t>() == 0x80f1u);
        ARRAY_CHECK(observe(store, halves, 0uz, true).as<::std::uint32_t>() == 0xffff80f1u);
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
# include "packed_numeric_arrays_scalar32.h"
#endif
    }
    void fill_boundaries(store_ptr const& store)
    {
        constexpr ::std::array<::std::size_t, 27uz> lengths{
            0,1,2,3,7,8,9,15,16,17,31,32,33,63,64,65,127,128,129,
            511,512,513,1023,1024,1025,2047,2048};
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
        for(::std::uint_least32_t type{}; type != 13u; ++type)
        {
            for(auto length : {0uz,255uz,256uz,1024uz,(~::std::size_t{})})
            {
                // Independent schema oracle: only actual i64/f64/v128 arrays
                // are scheduled earlier. References, structs and narrow arrays
                // retain count scheduling, including the SIZE_MAX boundary.
                auto const expected{(type == 3u || type == 5u || type == 6u) &&
                    length >= 256uz ? length : 0uz};
                ARRAY_CHECK(store->managed_wide_numeric_array_pressure_length(type,length) == expected);
            }
        }
        ARRAY_CHECK(store->managed_wide_numeric_array_pressure_length(0xffff'ffffu,1024uz) == 0uz);
#endif
        for(::std::uint_least32_t type{}; type != 7u; ++type)
        {
            value input{};
            if(type < 3u || type == 4u) { input = value::i32(0x7f800123u); }
            else if(type < 6u) { input = value::i64(0x7ff0000000000123ULL); }
            else { input = value::from(::std::array<::std::uint64_t,2uz>{
                0x7ff0000000000123ULL,0xfedcba9876543210ULL}); }
            auto const expected{normalized(input,type)};
            for(auto count : lengths)
            {
                ref result{};
                remember(store->array_new(type,input,count,result),result);
                for(::std::size_t i{}; i != count; ++i) { equals(store,result,i,expected); }
                auto const offset{count / 3uz};
                ARRAY_CHECK(store->array_fill(result,offset,value{},count-offset) == status::ok);
                for(::std::size_t i{}; i != count; ++i)
                { equals(store,result,i,i < offset ? expected : value{}); }
                ARRAY_CHECK(store->array_fill(result,0uz,input,count) == status::ok);
                for(::std::size_t i{}; i != count; ++i) { equals(store,result,i,expected); }
                ARRAY_CHECK(store->array_fill(result,count,input,0uz) == status::ok);
                ARRAY_CHECK(store->array_fill(result,count,input,1uz) == status::out_of_bounds);
                ARRAY_CHECK(store->array_fill(result,(~::std::size_t{}),input,1uz) == status::out_of_bounds);
            }
        }
    }
    void overlap_and_immutable(store_ptr const& store)
    {
        for(auto type : {0u,1u,2u,3u,4u,5u,6u})
        {
            ::std::array<value,5uz> original{};
            for(::std::size_t i{}; i != original.size(); ++i)
            {
                original[i] = width(type) <= 4uz ? value::i32(static_cast<::std::uint32_t>(0x80706050u + i)) :
                    value::i64(0x80706050ULL + i);
                if(type == 6u) { original[i].bits[15uz] = static_cast<::std::byte>(i+1uz); }
            }
            ref array{}; remember(store->array_new_fixed(type, original.data(), original.size(), array), array);
            ARRAY_CHECK(store->array_copy(array, 1uz, array, 0uz, 4uz) == status::ok);
            for(::std::size_t i{}; i != 5uz; ++i) { equals(store, array, i, normalized(original[i == 0uz ? 0uz : i-1uz], type)); }
            // Restore each checked slot before exercising overlap in the opposite direction.
            for(::std::size_t i{}; i != 5uz; ++i) { ARRAY_CHECK(store->array_set(array, i, original[i]) == status::ok); }
            ARRAY_CHECK(store->array_copy(array, 0uz, array, 1uz, 4uz) == status::ok);
            for(::std::size_t i{}; i != 5uz; ++i) { equals(store, array, i, normalized(original[i == 4uz ? 4uz : i+1uz], type)); }
        }
        ::std::array<value,2uz> values{value::i32(17u),value::i32(19u)};
        ::std::array<::std::byte,8uz> data{::std::byte{17},::std::byte{},::std::byte{},::std::byte{},
            ::std::byte{19},::std::byte{},::std::byte{},::std::byte{}};
        ref immutable{}, defaults{}, fixed{}, stack{}, segment{};
        remember(store->array_new(7u, values[0uz], 2uz, immutable), immutable);
        remember(store->array_new_default(7u, 2uz, defaults), defaults);
        remember(store->array_new_fixed(7u, values.data(), values.size(), fixed), fixed);
        ::std::array<::std::byte,8uz> native_stack{};
        // [real native byte array:8B][two complete i32 carriers:4B each]
        // [safe ] native_stack+4 stays in the same array, ending exactly at byte8.
        ::fast_io::freestanding::my_memcpy(native_stack.data(), values[0uz].bits.data(), 4uz);
        ::fast_io::freestanding::my_memcpy(native_stack.data()+4uz, values[1uz].bits.data(), 4uz);
        remember(store->array_new_fixed_from_stack(7u, native_stack.data(), native_stack.size(), 2uz, stack), stack);
        remember(store->array_new_data(7u, data.data(), data.size(), 0uz, 2uz, segment), segment);
        extent(store, immutable, 7u, 2uz); equals(store, defaults, 1uz, value{});
        for(auto reference : {fixed, stack, segment}) { equals(store, reference, 1uz, value::i32(19u)); }
        ARRAY_CHECK(store->array_set(immutable, 0uz, value{}) == status::immutable_field);
        ARRAY_CHECK(store->array_fill(immutable, 0uz, value{}, 1uz) == status::immutable_field);
        ARRAY_CHECK(store->array_copy(immutable, 0uz, fixed, 0uz, 1uz) == status::immutable_field);
        ARRAY_CHECK(store->array_init_data(7u, immutable, 0uz, data.data(), data.size(), 0uz, 1uz) == status::invalid_type);
        equals(store, immutable, 0uz, value::i32(17u));
    }
}

#if defined(UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_OOM) && UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_OOM == 1
# if !defined(__ELF__)
#  error The optional genuine nothrow array-allocation fault requires the ELF linker wrap ABI.
# endif
# if SIZE_MAX == UINT64_MAX
extern "C" void* packed_real_array(::std::size_t, ::std::nothrow_t const&) noexcept __asm__("__real__ZnamRKSt9nothrow_t");
extern "C" void* packed_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept __asm__("__wrap__ZnamRKSt9nothrow_t");
# elif SIZE_MAX == UINT32_MAX
extern "C" void* packed_real_array(::std::size_t, ::std::nothrow_t const&) noexcept __asm__("__real__ZnajRKSt9nothrow_t");
extern "C" void* packed_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept __asm__("__wrap__ZnajRKSt9nothrow_t");
# else
#  error The actual size_t array-new ABI must be qualified for this target.
# endif
extern "C" void* packed_wrap_array(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    auto expected{bytes};
    if(bytes != 0uz && packed_test::fail_extent.compare_exchange_strong(expected, 0uz, ::std::memory_order_relaxed))
    { packed_test::oom_injected.fetch_add(1uz, ::std::memory_order_relaxed); return nullptr; }
    // [real allocator result] forward untouched; delete[] still owns the exact returned array base.
    return packed_real_array(bytes, tag);
}
#endif

int main()
{
    using namespace packed_test;
    auto target_leases{::std::make_shared<gc::gc_lease_owner>()};
    auto source_leases{::std::make_shared<gc::gc_lease_owner>()};
    auto empty_leases{::std::make_shared<gc::gc_lease_owner>()};
    auto target{make_store(target_leases)}, source{make_store(source_leases)};
    t::recursive_type_section empty_section{};
    auto empty{::std::make_shared<gc::gc_object_store>(empty_section, empty_leases)};
    ARRAY_CHECK(empty->valid());
    ::std::weak_ptr<gc::gc_object_store> source_weak{source};
    ref foreign_numeric{}, foreign_node{}, graph_root{}, discarded{}, immutable_struct{};
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        ARRAY_CHECK(static_cast<bool>(admission));
        numeric_factories(target); fill_boundaries(target); overlap_and_immutable(target);
        remember(source->array_new(2u, value::i32(0xabcdef01u), 3uz, foreign_numeric), foreign_numeric);
        remember(target->array_new_default(2u, 3uz, discarded), discarded);
        ARRAY_CHECK(target->array_copy(discarded, 0uz, foreign_numeric, 0uz, 3uz) == status::ok);
        equals(target, discarded, 2uz, value::i32(0xabcdef01u));
        ARRAY_CHECK(target->reference_type_matches(foreign_numeric,
            {t::value_kind::reference,{2},false})); // Actual canonical equal declaration across two store owners.
        remember(source->struct_new_default(10u, foreign_node), foreign_node);
        ARRAY_CHECK(source->struct_set(foreign_node, 0uz, value::reference(foreign_numeric)) == status::ok);
        remember(target->array_new_default(8u, 2uz, graph_root), graph_root);
        ARRAY_CHECK(target->array_set(graph_root, 0uz, value::reference(graph_root)) == status::ok);
        ARRAY_CHECK(target->array_set(graph_root, 1uz, value::reference(foreign_node)) == status::ok);
        extent(target, graph_root, 8u, 2uz, false);
        ::std::array<ref,2uz> references{graph_root,foreign_node};
        ::std::array<value,2uz> carriers{value::reference(graph_root),value::reference(foreign_node)};
        ref broadcast{}, fixed{}, stack{}, elements{}, nonnull{};
        remember(target->array_new(8u, carriers[0uz], 2uz, broadcast), broadcast);
        remember(target->array_new_fixed(8u, carriers.data(), carriers.size(), fixed), fixed);
        ::std::array<::std::byte,2uz*sizeof(ref)> stack_bytes{};
        // [two real reference objects][complete native stack byte array]
        // [safe ] second destination+sizeof(ref) stays inside this actual array.
        ::fast_io::freestanding::my_memcpy(stack_bytes.data(), ::std::addressof(references[0uz]), sizeof(ref));
        ::fast_io::freestanding::my_memcpy(stack_bytes.data()+sizeof(ref), ::std::addressof(references[1uz]), sizeof(ref));
        remember(target->array_new_fixed_from_stack(8u, stack_bytes.data(), stack_bytes.size(), 2uz, stack), stack);
        remember(target->array_new_elements(8u, references.data(), references.size(), elements), elements);
        remember(target->array_new_elements(9u, references.data(), references.size(), nonnull), nonnull);
        for(auto reference : {broadcast,fixed,stack,elements}) { extent(target, reference, 8u, 2uz, false); }
        extent(target, nonnull, 9u, 2uz, false);
        ARRAY_CHECK(target->array_init_elements(8u, broadcast, 0uz, references.data(), references.size()) == status::ok);
        ARRAY_CHECK(same(observe(target, broadcast, 1uz).as<ref>(), foreign_node));
        ARRAY_CHECK(target->array_fill(fixed, 0uz, value::reference(graph_root), 2uz) == status::ok);
        ARRAY_CHECK(same(observe(target, fixed, 1uz).as<ref>(), graph_root));
        ARRAY_CHECK(target->array_copy(fixed, 0uz, elements, 0uz, 2uz) == status::ok);
        ARRAY_CHECK(same(observe(target, fixed, 1uz).as<ref>(), foreign_node));
        ARRAY_CHECK(same(observe(target, elements, 1uz).as<ref>(), foreign_node));
        auto saved{graph_root};
        ARRAY_CHECK(target->array_new_default(9u, 1uz, saved) == status::invalid_value && same(saved, graph_root));
        ARRAY_CHECK(target->array_set(nonnull, 0uz, value::reference(ref{})) == status::invalid_value);
        ARRAY_CHECK(same(observe(target, nonnull, 0uz).as<ref>(), graph_root));
        value field{value::i32(0x76543210u)};
        remember(target->struct_new(12u, &field, 1uz, immutable_struct), immutable_struct);
        ::std::size_t a{11uz}, b{13uz}; bool raw{true};
        ARRAY_CHECK(!target->native_test_array_requested_extent(immutable_struct, a, b, raw));
        ARRAY_CHECK(a == 11uz && b == 13uz && raw);
        {
            // Real valid receiver without lease roots is a negative, not an invalid-store mock.
            auto section{declarations()};
            auto unleased{::std::make_shared<gc::gc_object_store>(section)};
            ARRAY_CHECK(unleased->valid());
            ref local{};
            ARRAY_CHECK(unleased->array_new_default(2u, 1uz, local) == status::ok);
            ARRAY_CHECK(unleased->retain_gc_reference(local) == status::ok);
            ARRAY_CHECK(unleased->retain_gc_reference(foreign_numeric) == status::invalid_reference);
            // [unleased canonical store] last fixture owner drops only while shared admission protects teardown.
            unleased.reset();
        }
        ARRAY_CHECK(target->retain_gc_reference(foreign_numeric) == status::ok);
        // [source real control block] drop BOTH external source-module owners.
        // The receiving module's actual gc_lease_owner, not a cohort test pin,
        // must keep the source store and its immutable layout alive here.
        source.reset(); source_leases.reset();
        ARRAY_CHECK(!source_weak.expired());
        equals(target, foreign_numeric, 2uz, value::i32(0xabcdef01u));
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        ARRAY_CHECK(target->template array_get32<false>(foreign_numeric, 2uz) == 0xabcdef01u);
#endif
        ARRAY_CHECK(target->array_set(foreign_numeric, 1uz, value::i32(0x12345678u)) == status::ok);
        ARRAY_CHECK(target->array_copy(discarded, 0uz, foreign_numeric, 0uz, 3uz) == status::ok);
        equals(target, discarded, 1uz, value::i32(0x12345678u));
    }
    ::std::array cohort{target,source_weak.lock(),empty};
    ARRAY_CHECK(cohort[1uz] != nullptr);
    // Every issued reference is live in the first precise snapshot, including
    // every numeric allocation whose zero-edge tracing must not read carriers.
    collect(cohort, issued.data(), issued.size(), status::ok, 0uz);
    {
        ::std::array partial{cohort[0uz],cohort[1uz]};
        collect(partial, &graph_root, 1uz, status::invalid_reference, 0uz); // Omitted real EMPTY store stays forbidden.
    }
    auto const count{issued.size()}; ARRAY_CHECK(count > 3uz);
    collect(cohort, &graph_root, 1uz, status::ok, count-3uz);
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        equals(target, foreign_numeric, 1uz, value::i32(0x12345678u));
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        ARRAY_CHECK(target->template array_get32<false>(foreign_numeric, 1uz) == 0x12345678u);
#endif
        ARRAY_CHECK(same(observe(target, graph_root, 0uz).as<ref>(), graph_root));
        auto unchanged{value::i64(0xabcdef1234567890ULL)};
        ARRAY_CHECK(target->array_get(discarded, 0uz, false, unchanged) == status::invalid_reference);
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        ARRAY_CHECK(target->template array_get32<false>(discarded, 0uz) ==
            (static_cast<::std::uint64_t>(status::invalid_reference) << 32u));
#endif
        ARRAY_CHECK(unchanged.bits == value::i64(0xabcdef1234567890ULL).bits);
        ::std::size_t width_before{71uz}, bytes_before{73uz}; bool raw_before{true};
        ARRAY_CHECK(!target->native_test_array_requested_extent(discarded, width_before, bytes_before, raw_before));
        ARRAY_CHECK(width_before == 71uz && bytes_before == 73uz && raw_before);
        ARRAY_CHECK(target->retain_gc_reference(discarded) == status::invalid_reference);
    }
    // Saved tokens below are retired keys used only as negatives; they are not
    // live native roots. There is no raw carrier pointer across collection.
    collect(cohort, nullptr, 0uz, status::ok, 3uz);
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        value unchanged{};
        ARRAY_CHECK(target->array_get(graph_root, 0uz, false, unchanged) == status::invalid_reference);
        ARRAY_CHECK(target->array_get(foreign_numeric, 0uz, false, unchanged) == status::invalid_reference);
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        ARRAY_CHECK(target->template array_get32<false>(foreign_numeric, 0uz) ==
            (static_cast<::std::uint64_t>(status::invalid_reference) << 32u));
#endif
        ARRAY_CHECK(target->struct_get(foreign_node, 0uz, false, unchanged) == status::invalid_reference);
        // [canonical cohort pins] release them only after final readers leave.
    }
    {
        auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        // [last canonical fixture pins] native teardown is a genuine shared
        // administrative entrant; no collection or borrowed value is active.
        cohort[1uz].reset(); target.reset(); cohort[0uz].reset(); target_leases.reset();
        ARRAY_CHECK(source_weak.expired());
    }
    ::fast_io::io::println("PASS packed numeric arrays checks=", checks,
        " collections=", collections, " reclaimed=", reclaimed_total,
        " oom_injected=", oom_injected.load(::std::memory_order_relaxed),
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
        " packed_numeric=1 requested_extent_only=1 full_VM=0 RSS_measured=0");
#else
        " packed_numeric=0 requested_extent_only=1 full_VM=0 RSS_measured=0");
#endif
}
