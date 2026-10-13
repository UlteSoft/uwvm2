// Bounded typed native-packet DATA component, not a guest-state capture token.
#include <uwvm2/runtime/checkpoint/dynamic_native_packet.h>
#include <fast_io.h>
#include <bit>
namespace cp = ::uwvm2::runtime::checkpoint;
namespace global = ::uwvm2::object::global;
static void require(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("checkpoint dynamic packet: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static cp::types::core_value_type reference(cp::types::abstract_heap_type heap, bool nullable = false)
{ return {cp::types::value_kind::reference, {static_cast<::std::int_least64_t>(heap)}, nullable}; }
static cp::sealed_function_plan::owner plan()
{
    cp::function_plan source{}; source.profile = cp::compilation_profile::create_for_trusted_manager();
    source.expression_bytes = 48u; source.function_generation = 7u; source.module = 3u; source.function = 11u;
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = 6u;
    entry.slots = {{reference(cp::types::abstract_heap_type::i31), true},
                   {reference(cp::types::abstract_heap_type::i31), false},
                   {{cp::types::value_kind::f32}, true}, {{cp::types::value_kind::f64}, true},
                   {{cp::types::value_kind::v128}, true}, {reference(cp::types::abstract_heap_type::array, true), true}};
    cp::control_layout function{}; function.end_offset = 47u; entry.controls.push_back(function);
    source.sites.push_back(entry);
    // The static validator no longer proves slot1 after a merge; actual dynamic
    // assignment can still be present. The operand is an exact live i31 ref.
    auto after_merge{entry}; after_merge.identifier = 2u; after_merge.opcode_offset = 31u;
    after_merge.operand_count = 1u; after_merge.slots.push_back({reference(cp::types::abstract_heap_type::i31), true});
    source.sites.push_back(::std::move(after_merge));
    return cp::sealed_function_plan::seal_compiler_metadata(::std::move(source));
}
using bytes = ::std::array<::std::byte, 7u * cp::native_slot_bytes>;
template<typename T> static void put(bytes& native, ::std::size_t index, T const& value)
{
    static_assert(sizeof(T) <= cp::native_slot_bytes);
    require(index < 7u, "fixture slot is bounded before subspan");
    // [owned fixture slots0 ... index ...6] bytes_end
    // [safe                               ] index<7 and sizeof(T)<=16 before
    // changing the byte range; native ABI fixtures are not canonical wire bytes.
    auto slot{::std::span<::std::byte>{native}.subspan(index * cp::native_slot_bytes, cp::native_slot_bytes)};
    ::std::memcpy(slot.data(), ::std::addressof(value), sizeof(T));
}
int main()
{
    auto const source{plan()}; require(bool(source), "sealed exact typed site metadata");
    bytes payload{};
    auto const parameter{global::make_wasm_i31_reference(-5)};
    auto const assigned{global::make_wasm_i31_reference(-17)};
    put(payload, 0u, parameter); put(payload, 1u, assigned);
    ::std::uint32_t const nan32{0x7fa12345u}; ::std::uint64_t const nan64{0xfff0123456789abcu};
    put(payload, 2u, nan32); put(payload, 3u, nan64);
    ::std::array<::std::uint64_t, 2u> const vector_bits{0x0102030405060708u, 0x8899aabbccddeeffu};
    put(payload, 4u, vector_bits);
    global::wasm_global_ref_t nullable{}; nullable.kind = global::wasm_ref_kind::wasm_null;
    put(payload, 5u, nullable); put(payload, 6u, assigned);
    ::std::array<::std::uint8_t, 6u> flags{1u, 1u, 1u, 1u, 1u, 1u};
    cp::dynamic_native_packet::owner accepted{};
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, flags, accepted) == cp::status::ok && accepted,
        "actual assigned nondefaultable local survives conservative proof merge");
    require(accepted->site() == 2u && accepted->plan() == source && accepted->values().size() == 7u &&
        accepted->values()[1u].declaration.initialized && !source->get().sites[1u].slots[1u].initialized,
        "immutable source proof and actual initialization remain distinct at original slot index");
    ::std::uint32_t observed32{}; ::std::uint64_t observed64{};
    ::std::memcpy(::std::addressof(observed32), accepted->values()[2u].bits.data(), sizeof(observed32));
    ::std::memcpy(::std::addressof(observed64), accepted->values()[3u].bits.data(), sizeof(observed64));
    require(observed32 == nan32 && observed64 == nan64 && accepted->values()[4u].bits ==
        ::std::bit_cast<::std::array<::std::byte, cp::native_slot_bytes>>(vector_bits), "NaN and v128 exact native bit preservation");
    cp::native_reference observed_ref{};
    ::std::memcpy(::std::addressof(observed_ref), accepted->values()[6u].bits.data(), sizeof(observed_ref));
    require(observed_ref.kind == global::wasm_ref_kind::wasm_i31 && observed_ref.storage.wasm_i31.get_s() == -17,
        "exact i31 operand carrier; never dereference opaque reference bits");
    auto const prior{accepted};
    auto malformed{flags}; malformed[5u] = 2u;
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, malformed, accepted) == cp::status::invalid_layout && accepted == prior,
        "late noncanonical flag rejected before payload publication");
    malformed = flags; malformed[0u] = 0u;
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, malformed, accepted) == cp::status::invalid_layout && accepted == prior,
        "incoming parameter cannot become unset");
    malformed = flags; malformed[5u] = 0u;
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, malformed, accepted) == cp::status::invalid_layout && accepted == prior,
        "defaultable nullable reference cannot become unset");
    malformed = flags; malformed[1u] = 0u;
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, malformed, accepted) == cp::status::invalid_layout && accepted == prior,
        "unset local rejects native nonzero payload; no fabricated null");
    auto unset_payload{payload};
    for(auto& byte : ::std::span<::std::byte>{unset_payload}.subspan(cp::native_slot_bytes, cp::native_slot_bytes)) { byte = ::std::byte{}; }
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, unset_payload, malformed, accepted) == cp::status::ok &&
        !accepted->values()[1u].declaration.initialized, "canonical zero unset packet retains exact nonnull type");
    auto const unset_owner{accepted};
    auto bad_ref{payload}; global::wasm_global_ref_t wrong{}; wrong.kind = global::wasm_ref_kind::wasm_array;
    put(bad_ref, 1u, wrong);
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, bad_ref, flags, accepted) == cp::status::invalid_reference && accepted == unset_owner,
        "array tag cannot inhabit known abstract i31 heap envelope");
    wrong.kind = global::wasm_ref_kind::wasm_func; put(bad_ref, 1u, wrong);
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, bad_ref, flags, accepted) == cp::status::invalid_reference && accepted == unset_owner,
        "parser ref.func index is not a live runtime reference");
    wrong.kind = global::wasm_ref_kind::wasm_null; put(bad_ref, 1u, wrong);
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, bad_ref, flags, accepted) == cp::status::invalid_reference && accepted == unset_owner,
        "initialized nonnull local rejects null carrier");
    wrong.kind = static_cast<global::wasm_ref_kind>(0xffu); put(bad_ref, 1u, wrong);
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, bad_ref, flags, accepted) == cp::status::invalid_reference && accepted == unset_owner,
        "unknown runtime carrier kind rejected");
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, ::std::span<::std::byte const>{payload}.first(payload.size() - 1u), flags, accepted)
        == cp::status::invalid_layout && accepted == unset_owner, "truncated payload rejects before any slot-range changes");
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, ::std::span<::std::uint8_t const>{flags}.first(5u), accepted)
        == cp::status::invalid_layout && accepted == unset_owner, "truncated flags cannot publish a partially described frame");
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 3u, payload, flags, accepted) == cp::status::invalid_plan && accepted == unset_owner,
        "site outside actual immutable plan rejected");
    require(accepted->executable_restore_capability() == cp::status::unavailable_resume,
        "bounded typed packet is data and has no runtime/GC/worldstop/resume permission");
    cp::activation_identity const identity{13u, 0u, 17u, 23u};
    cp::shadow_ledger ledger{source->get().profile};
    require(ledger.enter(identity, source) == cp::status::ok, "component ledger retains exactly the owned metadata DATA");
    require(ledger.materialize_validated_native_data(identity, source, accepted->site(), accepted->values()) == cp::status::ok,
        "bounded unset nondefaultable DATA commits with exact original declaration");
    cp::dynamic_native_packet::owner actual_assigned{};
    require(cp::dynamic_native_packet::copy_compiler_packet(source, 2u, payload, flags, actual_assigned) == cp::status::ok,
        "canonical stronger actual executed-local flags copy before DATA commit");
    require(ledger.materialize_validated_native_data(identity, source, 2u, actual_assigned->values()) == cp::status::ok &&
        ledger.frames_while_actually_stopped().size() == 1u &&
        ledger.frames_while_actually_stopped()[0u].values[1u].declaration.initialized &&
        !source->get().sites[1u].slots[1u].initialized,
        "actual assignment stays distinct from conservative validator initialization proof");
    auto const previous_bits{ledger.frames_while_actually_stopped()[0u].values[1u].bits};
    ::std::vector<cp::native_value> forged{actual_assigned->values().begin(), actual_assigned->values().end()};
    forged[1u].declaration.type.kind = cp::types::value_kind::i64;
    require(ledger.materialize_validated_native_data(identity, source, 2u, forged) == cp::status::invalid_layout &&
        ledger.frames_while_actually_stopped()[0u].site == 2u &&
        ledger.frames_while_actually_stopped()[0u].values[1u].bits == previous_bits,
        "typed declaration mismatch rejects before changing existing frame DATA");
    require(ledger.materialize_validated_native_data(identity, source, 2u, actual_assigned->values()) == cp::status::invalid_layout,
        "recording failure is sticky and cannot manufacture a new valid snapshot");
    cp::shadow_ledger aliased{source->get().profile}; require(aliased.enter(identity, source) == cp::status::ok, "new independent component ledger");
    cp::sealed_function_plan::owner same_address_other_owner{source.get(), [](cp::sealed_function_plan const*) noexcept {}};
    require(aliased.materialize_validated_native_data(identity, same_address_other_owner, 2u, actual_assigned->values()) == cp::status::invalid_plan &&
        !aliased.frames_while_actually_stopped()[0u].materialized,
        "same raw address with different control block rejects before metadata/payload use");
    cp::shadow_ledger wrong_heap{source->get().profile}; require(wrong_heap.enter(identity, source) == cp::status::ok, "fresh heap-envelope component");
    forged.assign(actual_assigned->values().begin(), actual_assigned->values().end());
    global::wasm_global_ref_t array_tag{}; array_tag.kind = global::wasm_ref_kind::wasm_array;
    ::std::memcpy(forged[1u].bits.data(), ::std::addressof(array_tag), sizeof(array_tag));
    require(wrong_heap.materialize_validated_native_data(identity, source, 2u, forged) == cp::status::invalid_reference &&
        !wrong_heap.frames_while_actually_stopped()[0u].materialized,
        "array tag cannot commit as a known abstract i31 typed local");
    require(ledger.executable_restore_capability() == cp::status::unavailable_resume &&
        wrong_heap.executable_restore_capability() == cp::status::unavailable_resume,
        "DATA ledger validation has no runtime/root/coherent-manager/executable restore authority");
    ::fast_io::io::println("checkpoint straightline ledger DATA component PASS; actual VM producer/whole restore acceptance=false");
}
