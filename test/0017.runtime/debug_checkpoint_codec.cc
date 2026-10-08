/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#include <uwvm2/uwvm/debugger/checkpoint_database.h>
#include <uwvm2/uwvm/debugger/checkpoint_replay.h>
#include <fast_io_dsal/string_view.h>
#include <bit>
#include <limits>
#include <span>
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
static void require(bool condition, char const* description)
{
    if(!condition)
    { ::fast_io::io::perr("checkpoint codec failure: ", ::fast_io::mnp::os_c_str(description), "\n"); ::fast_io::fast_terminate(); }
}
static cp::value number(cp::value_kind kind, ::std::uint64_t low, ::std::uint64_t high = 0u)
{ cp::value result{}; result.type.kind = kind; result.low_bits = low; result.high_bits = high; return result; }
static cp::value reference(cp::heap_kind heap, cp::reference_kind kind, cp::object_id target = 0u)
{
    cp::value result{}; result.type.kind = cp::value_kind::reference; result.type.heap = heap;
    result.type.nullable = true; result.reference = kind; result.target = target;
    if(heap == cp::heap_kind::defined) { result.type.type_module = 1u; result.type.type_index = 0u; }
    return result;
}
static cp::state sample()
{
    cp::state result{}; result.recording_id[0] = ::std::byte{0x80u}; result.recording_id[15] = ::std::byte{1u};
    result.checkpoint_id = 2u; result.parent_checkpoint_id = 1u; result.logical_instruction = 200u;
    result.required_features = cp::known_feature_mask; result.next_logical_thread = 2u;
    result.objects.resize(27u);
    auto item = [&](::std::size_t id, cp::object_kind kind) -> cp::object&
    { require(id != 0u && id <= result.objects.size(), "sample ID bounded"); auto& object{result.objects[id - 1u]}; object.kind = kind; return object; };
    auto& module{item(1u, cp::object_kind::module)};
    module.bytes = {::std::byte{}, ::std::byte{'a'}, ::std::byte{'s'}, ::std::byte{'m'}, ::std::byte{1u}, ::std::byte{}, ::std::byte{}, ::std::byte{}};
    auto& instance{item(2u, cp::object_kind::instance)};
    instance.words = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u}; instance.links = {1u, 3u, 6u, 4u, 8u, 9u, 10u, 11u};
    auto& function{item(3u, cp::object_kind::function)}; function.words[1] = 7u; function.words[2] = 3u; function.links = {2u};
    auto& memory{item(4u, cp::object_kind::memory)};
    memory.flags = 1u; memory.words = {64u, ::std::uint64_t{1u} << 48u, 0u, ::std::uint64_t{1u} << 48u};
    auto& memory_chunk{item(5u, cp::object_kind::memory_chunk)};
    memory_chunk.links = {4u}; memory_chunk.words[0] = (::std::numeric_limits<::std::uint64_t>::max)() - 3u;
    memory_chunk.bytes = {::std::byte{1u}, ::std::byte{2u}, ::std::byte{3u}, ::std::byte{4u}};
    auto const node{reference(cp::heap_kind::defined, cp::reference_kind::structure, 12u)};
    auto& table{item(6u, cp::object_kind::table)};
    table.words = {64u, 0x100000001u, 0u, (::std::numeric_limits<::std::uint64_t>::max)()};
    table.values = {reference(cp::heap_kind::defined, cp::reference_kind::null)};
    auto& table_chunk{item(7u, cp::object_kind::table_chunk)}; table_chunk.links = {6u}; table_chunk.words[0] = 0x100000000u; table_chunk.values = {node};
    auto& global{item(8u, cp::object_kind::global)}; global.flags = 1u; global.values = {node};
    auto& tag{item(9u, cp::object_kind::tag)}; tag.links = {1u}; tag.words[0] = 2u;
    item(10u, cp::object_kind::data).bytes = {::std::byte{}, ::std::byte{0xffu}, ::std::byte{1u}};
    item(11u, cp::object_kind::element).values = {reference(cp::heap_kind::func, cp::reference_kind::function, 3u)};
    auto& structure{item(12u, cp::object_kind::structure)}; structure.links = {1u}; structure.values = {node, number(cp::value_kind::i8, 255u)};
    auto& array{item(13u, cp::object_kind::array)}; array.links = {1u}; array.words[0] = 1u; array.values = {node, node};
    auto& exception{item(14u, cp::object_kind::exception)};
    exception.links = {9u, 23u}; exception.values = {node, number(cp::value_kind::i64, (::std::numeric_limits<::std::uint64_t>::max)())};
    auto& external{item(15u, cp::object_kind::external)}; external.values = {reference(cp::heap_kind::any, cp::reference_kind::host, 24u)};
    auto& thread{item(16u, cp::object_kind::thread)}; thread.flags = 2u; thread.words[0] = 1u; thread.words[1] = 1u; thread.links = {17u, 20u};
    auto& frame{item(17u, cp::object_kind::frame)}; frame.links = {3u, 18u, 19u}; frame.words = {123u, 6u, 3u, 1u, 1u, 7u, 0u, 0u};
    auto i31{reference(cp::heap_kind::i31, cp::reference_kind::i31)}; i31.low_bits = 0x7fffffffu;
    auto unset{reference(cp::heap_kind::defined, cp::reference_kind::null)}; unset.type.nullable = false; unset.initialized = false;
    frame.values = {node, i31, number(cp::value_kind::v128, 0x0807060504030201u, 0x100f0e0d0c0b0a09u),
        reference(cp::heap_kind::external, cp::reference_kind::external, 15u), reference(cp::heap_kind::exception, cp::reference_kind::exception, 14u), unset,
        number(cp::value_kind::i32, 0xffffffffu), number(cp::value_kind::f32, 0x7fc01234u), number(cp::value_kind::f64, 0x8000000000000000u)};
    auto& control{item(18u, cp::object_kind::control)}; control.words = {100u, 200u, 0u, 1u};
    auto& handler{item(19u, cp::object_kind::handler)}; handler.flags = 1u; handler.links = {9u}; handler.words = {0u, 333u, 14u};
    auto& wait{item(20u, cp::object_kind::atomic_wait)};
    wait.links = {4u}; wait.words = {(::std::numeric_limits<::std::uint64_t>::max)() - 3u, 4u, 0u, 999u};
    auto& host{item(21u, cp::object_kind::host_resource)}; host.flags = 2u; host.words = {42u, 1u, 1u}; host.bytes = {::std::byte{0x7fu}};
    auto& event{item(22u, cp::object_kind::event)}; event.flags = 2u; event.links = {21u};
    event.words = {1u, 100u, 1u, 7u, 1u, 1u, 0u, 5u}; event.values = {number(cp::value_kind::i32, 4u), number(cp::value_kind::i32, 111u)};
    event.bytes = {::std::byte{0xdeu}, ::std::byte{0xadu}};
    auto& trace{item(23u, cp::object_kind::exception_trace)}; trace.links = {3u}; trace.values = {number(cp::value_kind::i64, 123u)};
    auto& host_reference{item(24u, cp::object_kind::host_reference)};
    host_reference.links = {21u}; host_reference.words[0] = 1u; host_reference.bytes = {::std::byte{0x5au}};
    item(25u, cp::object_kind::external).values = {node};
    item(26u, cp::object_kind::external).values = {i31};
    item(27u, cp::object_kind::external).values = {reference(cp::heap_kind::array, cp::reference_kind::array, 13u)};
    result.root_instances = {2u}; result.retained_roots = {node, reference(cp::heap_kind::exception, cp::reference_kind::exception, 14u)};
    return result;
}
static cp::state empty_nonnullable_tables()
{
    cp::state result{}; result.recording_id[0] = ::std::byte{1u}; result.checkpoint_id = 1u;
    result.required_features = cp::known_feature_mask; result.objects.resize(6u); result.root_instances = {2u};
    auto& module{result.objects[0u]}; module.kind = cp::object_kind::module;
    // Exact bytes for fixtures/checkpoint_empty_nonnullable_tables.wat; Core3
    // explicit table initializers, typed function references and table64.
    // Native module validation is a separate required test, not graph authority.
    constexpr unsigned char wasm[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x15u, 0x02u, 0x40u, 0x00u, 0x64u, 0x00u, 0x01u, 0x00u, 0x00u, 0xd2u, 0x00u, 0x0bu, 0x40u, 0x00u, 0x64u, 0x00u, 0x05u, 0x00u, 0x00u, 0xd2u, 0x00u, 0x0bu, 0x09u, 0x05u, 0x01u, 0x03u, 0x00u, 0x01u, 0x00u, 0x0au, 0x04u, 0x01u, 0x02u, 0x00u, 0x0bu};
    for(auto byte : wasm) { module.bytes.push_back(static_cast<::std::byte>(byte)); }
    auto& instance{result.objects[1u]}; instance.kind = cp::object_kind::instance;
    instance.words = {1u, 2u, 0u, 0u, 0u, 0u, 1u, 0u}; instance.links = {1u, 3u, 4u, 5u, 6u};
    auto& function{result.objects[2u]}; function.kind = cp::object_kind::function;
    function.words[1u] = 1u; function.links = {2u};
    auto descriptor{reference(cp::heap_kind::defined, cp::reference_kind::null)};
    descriptor.type.nullable = false; descriptor.initialized = false;
    for(::std::size_t index{3u}; index != 5u; ++index)
    {
        auto& table{result.objects[index]}; table.kind = cp::object_kind::table;
        table.words[0u] = index == 3u ? 32u : 64u; table.values = {descriptor};
    }
    auto& declaration{result.objects[5u]}; declaration.kind = cp::object_kind::element;
    declaration.flags = 1u; // A declarative segment is already dropped.
    return result;
}
static ::std::vector<::std::byte> encode(cp::state const& snapshot)
{
    ::std::vector<unsigned char> buffer(65536u);
    // [65536 bytes owned by buffer] end
    // [safe                      ] unsafe (one-past)
    //  ^^ output starts; both endpoints come from this live exact-size owner.
    ::fast_io::basic_obuffer_view<unsigned char> output{buffer.data(), buffer.data() + buffer.size()};
    require(cp::encode_database(output, snapshot) == cp::error::none, "canonical encode");
    // [complete encoded prefix][unused owner bytes] end
    // [safe                                       ] unsafe (one-past)
    //                           ^^ output.curr_ptr remains inside this owner.
    auto const size{static_cast<::std::size_t>(output.curr_ptr - buffer.data())};
    ::std::vector<::std::byte> bytes(size);
    for(::std::size_t i{}; i != size; ++i) { bytes[i] = static_cast<::std::byte>(buffer[i]); }
    return bytes;
}
template<unsigned Bits, typename T> static void overwrite(::std::vector<::std::byte>& bytes, ::std::size_t offset, T value)
{
    require(offset <= bytes.size() && Bits / 8u <= bytes.size() - offset, "mutation range bounded before deriving pointers");
    // [owned bytes ... offset][exact Bits/8 mutation bytes] ... end
    // [safe                                                     ] unsafe (one-past)
    //                         ^^ first derived only after range proof.
    auto* first{reinterpret_cast<unsigned char*>(bytes.data()) + offset};
    ::fast_io::basic_obuffer_view<unsigned char> output{first, first + Bits / 8u};
    ::fast_io::io::print(output, ::fast_io::mnp::le_put<Bits>(value));
    // [exact Bits/8 encoded mutation bytes] end
    // [safe                              ] unsafe (one-past)
    //                                      ^^ output cursor is now one-past.
}
static void resign(::std::vector<::std::byte>& bytes)
{
    require(bytes.size() >= cp::header_bytes + cp::footer_bytes, "complete file before re-signing malicious fixture");
    // [owned header+body][48-byte owned footer] end
    // [safe                                     ] unsafe (one-past)
    //                    ^^ checked hashed end; digest destination is exact32.
    auto const* body_end{bytes.data() + bytes.size() - cp::footer_bytes};
    ::fast_io::sha256_context hash{}; hash.update(bytes.data(), body_end); hash.do_final();
    hash.digest_to_byte_ptr(bytes.data() + bytes.size() - 32u);
}
int main(int argc, char** argv)
{
    require(argc == 2 || argc == 3, "fresh output path and optional --require-big");
    if(argc == 3)
    {
        require(::fast_io::string_view{::fast_io::mnp::os_c_str(argv[2])} == "--require-big", "exact endian selector");
        require(::std::endian::native == ::std::endian::big, "real native big-endian target, not a swapped little-endian simulation");
    }
    static_assert(cp::database_format_version == 6u);
    auto const empty_tables{empty_nonnullable_tables()};
    require(cp::validate_graph(empty_tables) == cp::error::none, "empty nonnullable table32/table64 retain only exact declared type");
    auto const empty_bytes{encode(empty_tables)}; cp::state empty_decoded{};
    require(cp::decode_database(empty_bytes, empty_decoded) == cp::error::none && empty_decoded == empty_tables &&
        encode(empty_decoded) == empty_bytes, "canonical state4 empty-table type-only roundtrip");
    auto empty_bad{empty_tables}; empty_bad.objects[3u].words[1u] = 1u; empty_bad.objects[3u].words[3u] = 1u;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only descriptor cannot represent nonempty table default");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].initialized = true;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "nonnull empty-table descriptor cannot fabricate null VALUE");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].initialized = true;
    empty_bad.objects[3u].values[0u].reference = cp::reference_kind::function; empty_bad.objects[3u].values[0u].target = 3u;
    require(cp::validate_graph(empty_bad) == cp::error::invalid_shape, "empty table cannot retain an unused actual function reference");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].type.nullable = true;
    require(cp::validate_graph(empty_bad) == cp::error::none, "nullable empty table also has canonical type-only descriptor");
    auto nullable_empty{empty_bad}; nullable_empty.objects[3u].values[0u].initialized = true;
    require(cp::validate_graph(nullable_empty) == cp::error::invalid_shape, "empty table cannot retain an unused initialized default value");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].target = 3u;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only descriptor cannot retain object identity");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].low_bits = 1u;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only descriptor has canonical zero low carrier");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].high_bits = 1u;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only descriptor has canonical zero high carrier");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].reference = cp::reference_kind::function;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only descriptor cannot encode a runtime reference kind");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u].type.type_module = 0u;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only defined type still requires actual module object ID");
    empty_bad = empty_tables; empty_bad.objects[3u].words[4u] = 0xfeed0000u;
    require(cp::validate_graph(empty_bad) == cp::error::invalid_shape, "unused descriptor word cannot carry native pointer authority");
    empty_bad = empty_tables; empty_bad.retained_roots = {empty_tables.objects[3u].values[0u]};
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only table descriptor is not a retained root VALUE");
    empty_bad = empty_tables; empty_bad.objects[3u].values[0u] = number(cp::value_kind::i32, 0u); empty_bad.objects[3u].values[0u].initialized = false;
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "type-only table descriptor cannot have numeric type");
    empty_bad = empty_tables; cp::object chunk{}; chunk.kind = cp::object_kind::table_chunk;
    chunk.links = {4u}; chunk.values = {empty_tables.objects[3u].values[0u]}; empty_bad.objects.push_back(chunk);
    require(cp::validate_graph(empty_bad) == cp::error::incompatible_type, "actual table element cannot be type-only or uninitialized");
    empty_bad = empty_tables; chunk.values[0u].initialized = true; chunk.values[0u].reference = cp::reference_kind::function; chunk.values[0u].target = 3u;
    empty_bad.objects.push_back(chunk);
    require(cp::validate_graph(empty_bad) == cp::error::invalid_shape, "zero-length table cannot own any actual element chunk");
    auto malformed_empty_wire{empty_bytes};
    ::std::size_t descriptor_offset{cp::header_bytes + 8u};
    for(::std::size_t i{}; i != 3u; ++i)
    {
        auto const& item{empty_tables.objects[i]};
        descriptor_offset += cp::object_header_bytes + item.links.size() * 8u + item.values.size() * cp::value_bytes + item.bytes.size();
    }
    descriptor_offset += cp::object_header_bytes; overwrite<32>(malformed_empty_wire, descriptor_offset + 4u, ::std::uint32_t{});
    resign(malformed_empty_wire); auto const empty_before{empty_decoded};
    require(cp::decode_database(malformed_empty_wire, empty_decoded) == cp::error::incompatible_type && empty_decoded == empty_before,
        "checksum-valid fabricated initialized descriptor is rejected before detached output publication");
    auto old_state3{empty_bytes}; overwrite<64>(old_state3, 0u, cp::legacy_database_magic3);
    overwrite<16>(old_state3, 8u, ::std::uint16_t{3u}); resign(old_state3);
    require(cp::decode_database(old_state3, empty_decoded) == cp::error::unsupported_version && empty_decoded == empty_before,
        "old state3 cannot be guessed into new empty-table representation");
    auto const original{sample()}; require(cp::validate_graph(original) == cp::error::none, "full schema graph including GC cycles/aliases/exception/thread and 64-bit final memory byte");
    auto const bytes{encode(original)}; cp::state decoded{};
    require(cp::decode_database(bytes, decoded) == cp::error::none && decoded == original, "exact canonical roundtrip including NaN/v128/i31/typed refs");
    require(encode(decoded) == bytes, "single canonical endian representation");
    require(decoded.objects[14u].values[0].reference == cp::reference_kind::host && decoded.objects[14u].values[0].target == 24u &&
        decoded.objects[24u].values[0] == decoded.objects[7u].values[0] && decoded.objects[25u].values[0] == decoded.objects[16u].values[1u] &&
        decoded.objects[26u].values[0].target == 13u && decoded.objects[12u].values[0].target == 12u,
        "host/struct/i31/array extern conversion roundtrip retains inner identity and GC alias/cycle");
    for(::std::size_t i{}; i != bytes.size(); ++i)
    {
        auto sentinel{original}; sentinel.checkpoint_id = 99u; auto const before{sentinel};
        require(cp::decode_database(::std::span<::std::byte const>{bytes}.first(i), sentinel) != cp::error::none && sentinel == before,
            "every truncation leaves previous state unchanged");
    }
    auto corrupted{bytes}; corrupted[cp::header_bytes + 9u] ^= ::std::byte{1u};
    require(cp::decode_database(corrupted, decoded) == cp::error::digest_mismatch, "body corruption rejected before graph allocation");
    corrupted = bytes; overwrite<16>(corrupted, 8u, ::std::uint16_t{1u}); resign(corrupted);
    require(cp::decode_database(corrupted, decoded) == cp::error::unsupported_version, "development format 1 is not migrated by guessing host/wrapper identity");
    corrupted = bytes; overwrite<64>(corrupted, 88u, (::std::numeric_limits<::std::uint64_t>::max)()); resign(corrupted);
    require(cp::decode_database(corrupted, decoded) == cp::error::limit_exceeded, "attacker-computed checksum cannot authorize oversized object count");
    corrupted = bytes; overwrite<16>(corrupted, cp::header_bytes + 8u + 2u * cp::value_bytes, ::std::uint16_t{0xffffu}); resign(corrupted);
    require(cp::decode_database(corrupted, decoded) == cp::error::malformed, "checksum-valid unknown object kind rejected");
    corrupted = bytes; overwrite<64>(corrupted, cp::header_bytes + 8u + 24u, ::std::uint64_t{999u}); resign(corrupted);
    require(cp::decode_database(corrupted, decoded) == cp::error::incompatible_type, "checksum-valid dangling typed reference rejected");
    require(cp::decode_database(::std::span<::std::byte const>{}, decoded) == cp::error::truncated, "null empty input never subtracts null pointers");
    require(!decoded.objects[16u].values[5u].initialized && !decoded.objects[16u].values[5u].type.nullable,
        "uninitialized nondefaultable frame local roundtrip never fabricates a null value");
    auto malformed{original}; malformed.objects[7u].values[0] = original.objects[16u].values[5u];
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "uninitialized global is invalid");
    malformed = original; malformed.objects[16u].values[6u] = original.objects[16u].values[5u];
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "uninitialized operand is invalid");
    malformed = original; malformed.objects[11u].values[0] = original.objects[16u].values[5u];
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "uninitialized GC field is invalid");
    malformed = original; malformed.objects[16u].values[5u].type.nullable = true;
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "defaultable locals cannot use uninitialized marker");
    malformed = original; malformed.objects[16u].values[5u].target = 12u;
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "uninitialized marker cannot retain fabricated reference identity");
    malformed = original; malformed.objects[11u].values[0].target = 999u;
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "dangling GC reference rejected");
    malformed = original; malformed.objects[23u].kind = cp::object_kind::external;
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "bare host reference cannot target an external wrapper");
    malformed = original; malformed.objects[14u].values[0].type.heap = cp::heap_kind::external;
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "bare host reference requires any heap, not extern or eq");
    malformed = original; malformed.objects[24u].values[0] = reference(cp::heap_kind::external, cp::reference_kind::external, 25u);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "self-referential wrapper is not an internal any reference");
    malformed = original; malformed.objects[24u].values[0] = reference(cp::heap_kind::external, cp::reference_kind::external, 26u);
    malformed.objects[25u].values[0] = reference(cp::heap_kind::external, cp::reference_kind::external, 25u);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "mutual wrapper cycle fails without recursive stack traversal");
    malformed = original; malformed.objects[24u].values[0] = reference(cp::heap_kind::any, cp::reference_kind::null);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "null conversion is plain null and cannot manufacture wrapper");
    malformed = original; malformed.objects[24u].values[0] = number(cp::value_kind::i32, 42u);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "numeric values cannot be extern wrapped");
    malformed = original; malformed.objects[24u].values[0] = reference(cp::heap_kind::func, cp::reference_kind::function, 3u);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "function references are outside any hierarchy");
    malformed = original; malformed.objects[24u].values[0] = reference(cp::heap_kind::exception, cp::reference_kind::exception, 14u);
    require(cp::validate_graph(malformed) == cp::error::incompatible_type, "exception references are outside any hierarchy");
    malformed = original; malformed.objects[5u].values.clear();
    require(cp::validate_graph(malformed) == cp::error::invalid_shape, "malformed table parent cannot index an empty default-value vector");
    malformed = original; malformed.objects[4u].words[0] = (::std::numeric_limits<::std::uint64_t>::max)() - 2u;
    require(cp::validate_graph(malformed) == cp::error::invalid_shape, "64-bit chunk endpoint overflow rejected");
    malformed = original; malformed.objects[16u].words[5] = 8u;
    require(cp::validate_graph(malformed) == cp::error::invalid_shape, "stale function generation rejected");
    malformed = original; malformed.objects[15u].links.push_back(17u);
    require(cp::validate_graph(malformed) == cp::error::invalid_shape, "duplicate logical frame ownership rejected");
    malformed = original; malformed.objects.push_back(malformed.objects[4u]);
    require(cp::validate_graph(malformed) == cp::error::overlapping_chunks, "overlapping sparse memory chunk rejected");
    cp::limits small{}; small.max_values = 1u;
    require(cp::decode_database(bytes, decoded, small) == cp::error::limit_exceeded, "allocation budget checked");
    cp::replay_log log{}; require(log.bind(original) == cp::error::none, "owned replay log binding");
    ::std::array<cp::object_id, 1u> resources{21u}; ::std::array<cp::value, 1u> arguments{number(cp::value_kind::i32, 4u)};
    cp::replay_request request{1u, 100u, 1u, 7u, 2u, resources, arguments}; request.logical_thread = 2u;
    require(log.consume(request).status == cp::error::replay_diverged && log.cursor() == 0u, "wrong logical thread cannot consume event");
    request.logical_thread = 1u; auto const replay{log.consume(request)};
    require(replay.status == cp::error::none && replay.results.size() == 1u && replay.results[0].low_bits == 111u && replay.effect_ordinal == 5u && log.exhausted(),
        "exact event match returns recorded typed result without host execution");
    require(log.consume(request).status == cp::error::replay_diverged, "event cannot replay twice");
    ::std::array<cp::checkpoint_index, 4u> catalog{{{1u, 0u, 10u, 0u}, {2u, 1u, 30u, 1u}, {3u, 1u, 25u, 1u}, {4u, 2u, 50u, 2u}}};
    auto const reverse{cp::plan_reverse(catalog, 4u, 70u, 28u)};
    require(reverse.status == cp::error::none && reverse.origin.identifier == 1u && reverse.target_instruction == 28u, "reverse plan follows exact ancestry, not another branch's nearer checkpoint");
    {
        ::fast_io::obuf_file file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::excl,
            static_cast<::fast_io::perms>(0600u)};
        auto const result{cp::write_unpublished_database(file, original)};
        require(result.status == cp::error::none && result.synchronized == cp::sync_guarantee::os_file_sync, "buffered output and real OS file sync");
        file.close();
    }
    // No guest/runtime is running in this standalone codec test; this private
    // fresh test file has no concurrent writer and remains stable while mapped.
    // Production additionally requires the real sealed management asset owner.
    ::fast_io::native_file_loader mapping{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    require(cp::decode_database(mapping, decoded) == cp::error::none && decoded == original, "RAII mapped-file decode under immutable test owner");
    ::fast_io::io::print("checkpoint schema/codec/replay components PASS; native endian=", ::fast_io::mnp::os_c_str(::std::endian::native == ::std::endian::big ? "big" : "little"),
        "; bytes=", ::fast_io::mnp::dec(bytes.size()), "; full VM continuation/restore acceptance=false\n");
}
