// The generated header contains the exact native aggregate bridge from the
// supplied candidate. It includes neither a mock module nor the LLVM emitter.
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <limits>
#include <utility>
#include <fast_io.h>
namespace bridge_fixture
{
#include "native_gc_aggregate_bridge.h"
}

namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using value = storage::gc_object_value;
using reference = storage::gc_reference;
using status = storage::gc_object_status;

static void check(bool condition, char const* label)
{
    if(!condition)
    {
        ::fast_io::io::perrln("gc fixed bridge: ", ::fast_io::mnp::os_c_str(label));
        ::fast_io::fast_terminate();
    }
}
static type::field_type field(type::value_kind kind, bool mutable_,
    type::packed_kind packed = type::packed_kind::none)
{
    type::field_type result{};
    result.storage.value.kind = kind;
    result.storage.packed = packed;
    result.mutable_ = mutable_;
    if(kind == type::value_kind::reference)
    {
        result.storage.value.heap.code = static_cast<::std::int_least64_t>(type::abstract_heap_type::func);
        result.storage.value.nullable = true;
    }
    return result;
}
static type::recursive_type_section types()
{
    type::recursive_type_section section{};
    type::recursive_group group{};
    auto const structure{[&](::std::initializer_list<type::field_type> fields)
    {
        type::sub_type entry{};
        entry.kind = type::composite_kind::struct_;
        for(auto const item : fields) { entry.fields.push_back(item); }
        group.types.push_back(::std::move(entry));
    }};
    auto const array{[&](type::field_type item)
    {
        type::sub_type entry{};
        entry.kind = type::composite_kind::array;
        entry.fields.push_back(item);
        group.types.push_back(::std::move(entry));
    }};
    structure({field(type::value_kind::i32, true)}); // 0
    structure({}); // 1
    structure({field(type::value_kind::i32, true, type::packed_kind::i8),
        field(type::value_kind::i32, true, type::packed_kind::i16), field(type::value_kind::i64, false)}); // 2
    array(field(type::value_kind::i32, true, type::packed_kind::i16)); // 3
    array(field(type::value_kind::reference, true)); // 4
    structure({field(type::value_kind::v128, true)}); // 5
    array(field(type::value_kind::i32, true)); // 6
    for(::std::size_t count{}; count != 10uz; ++count)
    {
        type::sub_type entry{};
        entry.kind = type::composite_kind::struct_;
        for(::std::size_t i{}; i != count; ++i) { entry.fields.push_back(field(type::value_kind::i32, true)); }
        group.types.push_back(::std::move(entry)); // 7 + count
    }
    section.type_count = static_cast<::std::uint_least32_t>(group.types.size());
    section.groups.push_back(::std::move(group));
    return section;
}

template<bool Fixed, ::std::uint_least32_t Opcode, ::std::size_t Count>
static value invoke(storage::wasm_module_storage_t& module, ::std::uint_least32_t first,
    ::std::uint_least32_t second, ::std::array<value, Count> const& inputs,
    status expected = status::ok)
{
    // [canary][one complete output value][canary] end
    // [safe                                   ] output starts at byte 1 and spans exactly 16 bytes.
    ::std::array<::std::byte, sizeof(value) + 2uz> output{};
    output.fill(::std::byte{0xa5u});
    auto const address{reinterpret_cast<::std::uintptr_t>(output.data() + 1uz)};
    ::std::uint_least32_t actual{};
    if constexpr(Fixed && Count <= 8uz)
    {
#if defined(UWVM_FIXED_GC_FIVE_ARGUMENT_ABI)
        actual = bridge_fixture::llvm_jit_gc_aggregate_fixed_bridge<Opcode, Count>(
            reinterpret_cast<::std::uintptr_t>(::std::addressof(module)), first, second,
            reinterpret_cast<::std::uintptr_t>(inputs.data()), address);
#else
        actual = bridge_fixture::llvm_jit_gc_aggregate_bridge<Opcode, Count>(
            reinterpret_cast<::std::uintptr_t>(::std::addressof(module)), Opcode, first, second,
            reinterpret_cast<::std::uintptr_t>(inputs.data()), Count, address);
#endif
    }
    else
    {
        actual = bridge_fixture::llvm_jit_gc_aggregate_bridge<>(
            reinterpret_cast<::std::uintptr_t>(::std::addressof(module)), Opcode, first, second,
            reinterpret_cast<::std::uintptr_t>(inputs.data()), Count, address);
    }
    check(actual == static_cast<::std::uint_least32_t>(expected), "operation status");
    check(output.front() == ::std::byte{0xa5u} && output.back() == ::std::byte{0xa5u}, "output bounds");
    value result{};
    if(expected == status::ok)
    { ::std::memcpy(::std::addressof(result), output.data() + 1uz, sizeof(result)); }
    else
    {
        for(auto const byte : output) { check(byte == ::std::byte{0xa5u}, "failed operation leaves output untouched"); }
    }
    return result;
}

template<bool Fixed, ::std::size_t Count>
static void count_case(storage::wasm_module_storage_t& module)
{
    ::std::array<value, Count> inputs{};
    for(::std::size_t i{}; i != Count; ++i) { inputs[i] = value::i32(static_cast<::std::uint32_t>(i + 100uz)); }
    auto const structure{invoke<Fixed, 0u, Count>(module, static_cast<::std::uint_least32_t>(7uz + Count), 0u, inputs).template as<reference>()};
    auto const array{invoke<Fixed, 8u, Count>(module, 6u, static_cast<::std::uint_least32_t>(Count), inputs).template as<reference>()};
    value observed{};
    ::std::size_t length{};
    check(module.gc_store->array_length(array, length) == status::ok && length == Count, "fixed array count");
    for(::std::size_t i{}; i != Count; ++i)
    {
        check(module.gc_store->struct_get(structure, i, false, observed) == status::ok && observed.bits == inputs[i].bits, "struct complete input copy");
        check(module.gc_store->array_get(array, i, false, observed) == status::ok && observed.bits == inputs[i].bits, "array complete input copy");
    }
}

template<bool Fixed>
static void run()
{
    storage::wasm_module_storage_t module{};
    auto declarations{types()};
    module.gc_lease_roots = ::std::make_shared<storage::gc_lease_owner>();
    module.gc_store = ::std::make_shared<storage::gc_object_store>(declarations, module.gc_lease_roots);
    check(module.gc_store->valid(), "real runtime module type layouts");
    ::std::array<::std::byte, 4uz> data{::std::byte{1u}, ::std::byte{2u}, ::std::byte{3u}, ::std::byte{4u}};
    storage::local_defined_data_storage_t segment{};
    // [four immutable data bytes] end
    // [safe                     ] segment borrows this array for the entire synchronous fixture.
    segment.data.byte_begin = data.data();
    segment.data.byte_end = data.data() + data.size();
    segment.data.kind = storage::wasm_data_segment_kind::passive;
    module.local_defined_data_vec_storage.push_back(segment);
    module.local_defined_function_vec_storage.push_back({});
    ::std::array<storage::wasm_element_storage_t::func_idx_t, 1uz> functions{0u};
    storage::local_defined_element_storage_t element{};
    // [one validated function index] end
    // [safe                        ] the complete borrowed range remains live through all segment operations.
    element.element.funcidx_begin = functions.data();
    element.element.funcidx_end = functions.data() + functions.size();
    element.element.kind = storage::wasm_element_segment_kind::passive;
    module.local_defined_element_vec_storage.push_back(element);
    auto const ref_value{[](reference ref) { return value::reference(ref); }};
    auto const structure{invoke<Fixed, 0u, 3uz>(module, 2u, 0u,
        {value::i32(0xffu), value::i32(0x8001u), value::i64(0xfedcba9876543210ull)}).template as<reference>()};
    auto const default_structure{invoke<Fixed, 1u, 0uz>(module, 2u, 0u, {}).template as<reference>()};
    check(invoke<Fixed, 2u, 1uz>(module, 2u, 2u, {ref_value(structure)}).template as<::std::uint64_t>() == 0xfedcba9876543210ull, "struct i64 bits");
    check(invoke<Fixed, 3u, 1uz>(module, 2u, 0u, {ref_value(structure)}).template as<::std::uint32_t>() == 0xffffffffu, "struct signed i8");
    check(invoke<Fixed, 4u, 1uz>(module, 2u, 1u, {ref_value(structure)}).template as<::std::uint32_t>() == 0x8001u, "struct unsigned i16");
    check(invoke<Fixed, 2u, 1uz>(module, 2u, 2u, {ref_value(default_structure)}).template as<::std::uint64_t>() == 0u, "default struct");
    (void)invoke<Fixed, 5u, 2uz>(module, 2u, 0u, {ref_value(structure), value::i32(0x81u)});
    check(invoke<Fixed, 3u, 1uz>(module, 2u, 0u, {ref_value(structure)}).template as<::std::uint32_t>() == 0xffffff81u, "struct set packed");
    (void)invoke<Fixed, 5u, 2uz>(module, 2u, 2u, {ref_value(structure), value::i64(0u)}, status::immutable_field);
    ::std::array<::std::byte, 16uz> vector_bits{};
    for(::std::size_t i{}; i != vector_bits.size(); ++i) { vector_bits[i] = static_cast<::std::byte>(0x80uz + i); }
    auto const vector_structure{invoke<Fixed, 0u, 1uz>(module, 5u, 0u, {value::from(vector_bits)}).template as<reference>()};
    check(invoke<Fixed, 2u, 1uz>(module, 5u, 0u, {ref_value(vector_structure)}).bits == vector_bits, "full v128 input and result");
    auto const numeric_array{invoke<Fixed, 6u, 2uz>(module, 6u, 0u, {value::i32(0x12345678u), value::i32(4u)}).template as<reference>()};
    check(invoke<Fixed, 11u, 2uz>(module, 6u, 0u, {ref_value(numeric_array), value::i32(3u)}).template as<::std::uint32_t>() == 0x12345678u, "array i32 bits");
    auto const packed_array{invoke<Fixed, 8u, 3uz>(module, 3u, 3u, {value::i32(0x8001u), value::i32(2u), value::i32(3u)}).template as<reference>()};
    auto const default_array{invoke<Fixed, 7u, 1uz>(module, 3u, 0u, {value::i32(3u)}).template as<reference>()};
    check(invoke<Fixed, 12u, 2uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(0u)}).template as<::std::uint32_t>() == 0xffff8001u, "array signed i16");
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(0u)}).template as<::std::uint32_t>() == 0x8001u, "array unsigned i16");
    (void)invoke<Fixed, 14u, 3uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(1u), value::i32(0x9001u)});
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(1u)}).template as<::std::uint32_t>() == 0x9001u, "array set");
    check(invoke<Fixed, 15u, 1uz>(module, 0u, 0u, {ref_value(packed_array)}).template as<::std::uint32_t>() == 3u, "array length");
    (void)invoke<Fixed, 16u, 4uz>(module, 3u, 0u, {ref_value(default_array), value::i32(0u), value::i32(0x1234u), value::i32(2u)});
    (void)invoke<Fixed, 16u, 4uz>(module, 3u, 0u, {ref_value(default_array), value::i32(2u), value::i32(0u), value::i32(2u)}, status::out_of_bounds);
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(default_array), value::i32(0u)}).template as<::std::uint32_t>() == 0x1234u, "fill bounds preserve earlier elements");
    (void)invoke<Fixed, 17u, 5uz>(module, 3u, 3u, {ref_value(packed_array), value::i32(1u), ref_value(packed_array), value::i32(0u), value::i32(2u)});
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(2u)}).template as<::std::uint32_t>() == 0x9001u, "overlapping array copy");
    auto const data_array{invoke<Fixed, 9u, 2uz>(module, 3u, 0u, {value::i32(0u), value::i32(2u)}).template as<reference>()};
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(data_array), value::i32(0u)}).template as<::std::uint32_t>() == 0x0201u, "data segment endian");
    (void)invoke<Fixed, 18u, 4uz>(module, 3u, 0u, {ref_value(data_array), value::i32(1u), value::i32(2u), value::i32(1u)});
    check(invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(data_array), value::i32(1u)}).template as<::std::uint32_t>() == 0x0403u, "data segment init");
    (void)invoke<Fixed, 18u, 4uz>(module, 3u, 0u, {ref_value(data_array), value::i32(0u), value::i32(3u), value::i32(2u)}, status::out_of_bounds);
    auto const element_array{invoke<Fixed, 10u, 2uz>(module, 4u, 0u, {value::i32(0u), value::i32(1u)}).template as<reference>()};
    auto const function_ref{invoke<Fixed, 11u, 2uz>(module, 4u, 0u, {ref_value(element_array), value::i32(0u)}).template as<reference>()};
    check(function_ref.kind == global::wasm_ref_kind::wasm_func_defined && function_ref.storage.ptr == ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(0uz)), "element canonical function identity");
    auto const target{invoke<Fixed, 7u, 1uz>(module, 4u, 0u, {value::i32(2u)}).template as<reference>()};
    (void)invoke<Fixed, 19u, 4uz>(module, 4u, 0u, {ref_value(target), value::i32(1u), value::i32(0u), value::i32(1u)});
    check(invoke<Fixed, 11u, 2uz>(module, 4u, 0u, {ref_value(target), value::i32(1u)}).template as<reference>().storage.ptr == function_ref.storage.ptr, "element init");
    (void)invoke<Fixed, 19u, 4uz>(module, 4u, 0u, {ref_value(target), value::i32(0u), value::i32(1u), value::i32(1u)}, status::out_of_bounds);
    reference null{};
    null.kind = global::wasm_ref_kind::wasm_null;
    (void)invoke<Fixed, 2u, 1uz>(module, 2u, 0u, {ref_value(null)}, status::null_reference);
    (void)invoke<Fixed, 13u, 2uz>(module, 3u, 0u, {ref_value(packed_array), value::i32(3u)}, status::out_of_bounds);
    storage::drop_wasm_data_segment_payload(module.local_defined_data_vec_storage.index_unchecked(0uz).data);
    (void)invoke<Fixed, 9u, 2uz>(module, 3u, 0u, {value::i32(0u), value::i32(1u)}, status::out_of_bounds);
    storage::drop_wasm_element_segment_payload(module.local_defined_element_vec_storage.index_unchecked(0uz).element);
    (void)invoke<Fixed, 10u, 2uz>(module, 4u, 0u, {value::i32(0u), value::i32(1u)}, status::out_of_bounds);
    count_case<Fixed, 0uz>(module); count_case<Fixed, 1uz>(module); count_case<Fixed, 2uz>(module);
    count_case<Fixed, 3uz>(module); count_case<Fixed, 4uz>(module); count_case<Fixed, 5uz>(module);
    count_case<Fixed, 6uz>(module); count_case<Fixed, 7uz>(module); count_case<Fixed, 8uz>(module);
    count_case<Fixed, 9uz>(module); // larger input retains the generic allocation path
    value output{};
    auto const out{reinterpret_cast<::std::uintptr_t>(::std::addressof(output))};
    auto const module_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(module))};
    auto const invalid{static_cast<::std::uint_least32_t>(status::invalid_value)};
    ::std::array<value, 2uz> valid_inputs{ref_value(structure), value::i32(0u)};
    auto const valid_address{reinterpret_cast<::std::uintptr_t>(valid_inputs.data())};
    // Both inputs and the underlying object are valid. A null-input rejection
    // must not mask a missing fixed-opcode/count guard in these two tests.
    output.bits.fill(::std::byte{0xa5u});
    auto const sentinel{output.bits};
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<2u, 1uz>(module_address, 3u, 2u, 0u, valid_address, 1uz, out) == invalid &&
        output.bits == sentinel, "fixed opcode mismatch with otherwise valid input");
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<2u, 1uz>(module_address, 2u, 2u, 0u, valid_address, 2uz, out) == invalid &&
        output.bits == sentinel, "fixed count mismatch with otherwise valid complete input");
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<>(0u, 1u, 0u, 0u, 0u, 0uz, out) == invalid, "null module");
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<>(module_address, 1u, 0u, 0u, 0u, 0uz, 0u) == invalid, "null output");
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<>(module_address, 0u, 0u, 0u, 0u, 1uz, out) == invalid, "null nonempty input");
    check(bridge_fixture::llvm_jit_gc_aggregate_bridge<>(module_address, 0u, 0u, 0u, 1u,
        (::std::numeric_limits<::std::size_t>::max)() / sizeof(value) + 1uz, out) == invalid, "input extent overflow before read");
}

// Dynamic ABI arguments deliberately prevent propagation of a known opcode
// into the generic bridge in the assembly comparison.
#define BRIDGE_ARGUMENTS ::std::uintptr_t module, ::std::uint_least32_t opcode, ::std::uint_least32_t first, ::std::uint_least32_t second, ::std::uintptr_t input, ::std::size_t count, ::std::uintptr_t output
#define BRIDGE_VALUES module, opcode, first, second, input, count, output
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_generic_aggregate(BRIDGE_ARGUMENTS) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_bridge<>(BRIDGE_VALUES); }
#if defined(UWVM_FIXED_GC_FIVE_ARGUMENT_ABI)
#define FIXED_ARGUMENTS ::std::uintptr_t module, ::std::uint_least32_t first, ::std::uint_least32_t second, ::std::uintptr_t input, ::std::uintptr_t output
#define FIXED_VALUES module, first, second, input, output
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_fixed_struct_new_one(FIXED_ARGUMENTS) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_fixed_bridge<0u, 1uz>(FIXED_VALUES); }
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_fixed_struct_get_one(FIXED_ARGUMENTS) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_fixed_bridge<2u, 1uz>(FIXED_VALUES); }
#undef FIXED_VALUES
#undef FIXED_ARGUMENTS
#else
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_fixed_struct_new_one(BRIDGE_ARGUMENTS) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_bridge<0u, 1uz>(BRIDGE_VALUES); }
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_fixed_struct_get_one(BRIDGE_ARGUMENTS) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_bridge<2u, 1uz>(BRIDGE_VALUES); }
#endif
#undef BRIDGE_VALUES
#undef BRIDGE_ARGUMENTS
int main()
{
    run<false>();
    run<true>();
    ::fast_io::io::println("PASS real GC aggregate bridge: all 20 opcodes; fixed counts 0..8; dynamic 9; full v128; segments; traps; exact output extent");
}
