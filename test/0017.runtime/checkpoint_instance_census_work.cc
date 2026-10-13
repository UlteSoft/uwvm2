// Finite cold DATA-builder tests only. This wrapper deliberately has NO actual
// runtime source/engine, execution lease, participant, capture or restore issuer.
// It cannot invoke copy_complete_instance_resources or turn a native key into
// authority. Actual census integration requires the genuine manager fixture.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include <fast_io.h>
#include <uwvm2/object/global/ref.h>
#include <uwvm2/runtime/exception/value.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/checkpoint_state.h>
struct cold_test_context
{
    using reference = ::uwvm2::object::global::wasm_global_ref_t;
    using reference_kind = ::uwvm2::object::global::wasm_ref_kind;
    struct work {}; // Deliberately empty: no genuine runtime modules/proofs.
#include <uwvm2/runtime/lib/uwvm_runtime_checkpoint_instance_census.h>
};
using context = cold_test_context;
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
static void require(bool good, unsigned line)
{
    if(good) { return; }
    ::fast_io::print(::fast_io::err(), "checkpoint_instance_census_work FAIL line=", ::fast_io::mnp::dec(line), "\n");
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static context::reference key(void* token, context::reference_kind kind)
{ context::reference result{}; result.kind = kind; result.storage.ptr = token; return result; }
int main()
{
    // Dense IDs and exact accounting survive vector relocation.
    context::complete_census_work dense{}; dense.cap.max_objects = 128u;
    for(::std::uint64_t i{1u}; i != 129u; ++i)
    { REQUIRE(dense.allocate(cp::object_kind::structure) == i); REQUIRE(dense.object(i)->kind == cp::object_kind::structure); }
    REQUIRE(dense.wire_bytes == 176u + 128u * 96u);
    REQUIRE(dense.allocate(cp::object_kind::array) == 0u && dense.status == cp::error::limit_exceeded);
    // File/object/link/value limits reject BEFORE publishing another item.
    context::complete_census_work exact{}; exact.cap.max_file_bytes = 176u + 96u;
    REQUIRE(exact.allocate(cp::object_kind::module) == 1u);
    REQUIRE(exact.allocate(cp::object_kind::instance) == 0u && exact.snapshot.objects.size() == 1u);
    context::complete_census_work links{}; links.cap.max_links = 1u;
    auto const root{links.allocate(cp::object_kind::structure)}; REQUIRE(root == 1u);
    REQUIRE(links.append_link(root, root)); REQUIRE(!links.append_link(root, root));
    REQUIRE(links.object(root)->links.size() == 1u && links.links == 1u);
    context::complete_census_work values{}; values.cap.max_values = 1u;
    auto const global{values.allocate(cp::object_kind::global)}; cp::value number{}; number.low_bits = 0xffffffffu;
    REQUIRE(values.append_value(global, number)); REQUIRE(!values.append_value(global, number));
    REQUIRE(values.object(global)->values.size() == 1u && values.values == 1u);
    context::complete_census_work payload{}; payload.cap.max_payload_bytes = 4u;
    REQUIRE(payload.charge_payload(4u)); REQUIRE(!payload.charge_payload(1u) && payload.payload == 4u);
    // Kind+native key indexes are comparison DATA only. Grow/reprobe preserves
    // aliases, while a different kind remains a distinct comparison key.
    context::complete_census_work indexed{}; indexed.cap.max_objects = 64u;
    ::std::array<::std::byte, 64u> tokens{};
    for(::std::size_t i{}; i != tokens.size(); ++i)
    {
        auto const id{indexed.allocate(cp::object_kind::function)}; REQUIRE(id == i + 1u);
        auto const ref{key(::std::addressof(tokens[i]), context::reference_kind::wasm_func_defined)};
        REQUIRE(indexed.remember_reference(ref, id)); REQUIRE(indexed.remember_reference(ref, id));
    }
    REQUIRE(indexed.indexed_references == tokens.size() && indexed.reference_index.size() <= indexed.cap.max_objects * 2u);
    for(::std::size_t i{}; i != tokens.size(); ++i)
    { REQUIRE(indexed.find_reference(key(::std::addressof(tokens[i]), context::reference_kind::wasm_func_defined)) == i + 1u); }
    REQUIRE(indexed.find_reference(key(::std::addressof(tokens[0u]), context::reference_kind::wasm_func_imported)) == 0u);
    auto const first{key(::std::addressof(tokens[0u]), context::reference_kind::wasm_func_defined)};
    REQUIRE(!indexed.remember_reference(first, 2u) && indexed.status == cp::error::invalid_reference);
    context::complete_census_work aliases{}; REQUIRE(aliases.allocate(cp::object_kind::function) == 1u);
    REQUIRE(aliases.remember_reference(key(::std::addressof(tokens[0u]), context::reference_kind::wasm_func_defined), 1u));
    REQUIRE(aliases.remember_reference(key(::std::addressof(tokens[1u]), context::reference_kind::wasm_func_imported), 1u));
    REQUIRE(aliases.snapshot.objects.size() == 1u && aliases.indexed_references == 2u);
    context::reference scalar{}; scalar.kind = context::reference_kind::wasm_i31;
    ::std::uintptr_t ignored{123u}; REQUIRE(!context::complete_census_work::pointer_key(scalar, ignored) && ignored == 0u);
    REQUIRE(aliases.find_reference(scalar) == 0u);
    // Exception-token aliases are not instance identity. This is a COLD DATA
    // index test using actual immutable factory-owned records, with no registry,
    // module/cohort/tag producer, lease or permission to publish runtime state.
    namespace exceptions = ::uwvm2::runtime::exception;
    context::complete_census_work exception_records{}; exception_records.cap.max_objects = 64u;
    auto const tag_owner{::std::make_shared<int>(7)};
    exceptions::instance_root tag{tag_owner};
    ::std::vector<exceptions::value_ref> originals{};
    for(::std::size_t ordinal{}; ordinal != 64u; ++ordinal)
    {
        auto record{exceptions::value::make(tag, ::std::span<exceptions::payload_field const>{})}; REQUIRE(record);
        auto const id{exception_records.allocate(cp::object_kind::exception)}; REQUIRE(id == ordinal + 1u);
        auto const pending{exception_records.pending.size()};
        exception_records.pending.push_back({{}, 0u, id, record});
        REQUIRE(exception_records.remember_exception(pending));
        REQUIRE(exception_records.find_exception(record) == id);
        originals.push_back(::std::move(record));
    }
    REQUIRE(exception_records.indexed_references == 64u);
    for(::std::size_t ordinal{}; ordinal != originals.size(); ++ordinal)
    {
        auto const copy{originals[ordinal]};
        REQUIRE(exception_records.find_exception(copy) == ordinal + 1u);
    }
    auto const token{key(::std::addressof(tokens[0u]), context::reference_kind::wasm_exn)};
    REQUIRE(!context::complete_census_work::pointer_key(token, ignored) && ignored == 0u);
    REQUIRE(exception_records.find_reference(token) == 0u); // Tokens never form record keys.
    // The same pointee with an unrelated shared control block must not merge.
    auto const unrelated{::std::make_shared<int>(9)};
    exceptions::value_ref forged{unrelated, originals[0u].get()};
    REQUIRE(exception_records.find_exception(forged) == 0u && exception_records.status == cp::error::invalid_reference);
    context::complete_census_work missing_exception{};
    REQUIRE(!missing_exception.remember_exception(0u) && missing_exception.status == cp::error::invalid_reference);
    // Bad targets fail; large limit arithmetic cannot wrap accepted wire cost.
    context::complete_census_work invalid{}; REQUIRE(!invalid.append_link(0u, 1u));
    context::complete_census_work overflow{}; overflow.cap.max_file_bytes = (std::numeric_limits<::std::uint64_t>::max)();
    REQUIRE(!overflow.charge_wire((std::numeric_limits<::std::uint64_t>::max)(), 48u));
    REQUIRE(overflow.wire_bytes == 176u);
    context::complete_census_work bad_count{}; bad_count.values = 2u; bad_count.cap.max_values = 1u;
    REQUIRE(!bad_count.charge_values(1u) && bad_count.values == 2u);
    context::complete_census_work bad_capacity{}; bad_capacity.cap.max_objects = (std::numeric_limits<::std::uint64_t>::max)();
    REQUIRE(!bad_capacity.grow_reference_index() && bad_capacity.reference_index.empty());
    ::fast_io::print("checkpoint_instance_census_work PASS (cold DATA only)\n");
}
