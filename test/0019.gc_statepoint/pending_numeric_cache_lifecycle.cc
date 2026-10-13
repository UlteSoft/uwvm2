// Header-only native ABI component. No LLVM/VM/GC authority is fabricated.
// Uses the real prepared registry, tag identity type, native chain/context and
// numeric_header. Whole runtime/codegen/performance remain separate NULL gates.
#if !defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) || UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH != 1
# error Enable the source-only fused-catch candidate for this fixture.
#endif
#include <uwvm2/runtime/exception/pending_numeric_bridge.h>
#include <uwvm2/uwvm/runtime/storage/tag_instance_identity.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <thread>
#include <type_traits>

namespace e = ::uwvm2::runtime::exception;
namespace p = e::pending_experiment;
namespace b = p::numeric_guest_bridge;
namespace s = ::uwvm2::uwvm::runtime::storage;
namespace
{
    using word = b::word;
    constexpr ::std::size_t tuple_bytes{40uz};
    unsigned checks{};
    void check(bool success, unsigned line) noexcept
    {
        ++checks;
        if(!success)
        {
            ::fast_io::io::perrln("pending_numeric_fused_catch FAIL line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)
    static_assert(::std::is_same_v<decltype(&b::uwvm2_pending_numeric_take_r3), b::quaternary_abi>);
    static_assert(noexcept(b::uwvm2_pending_numeric_take_r3(0u, 0u, 0u, 0u)));
    static_assert(::std::u8string_view{b::take_semantic} == u8"uwvm2_pending_numeric_take_r3");

    word address(void const* pointer) noexcept
    {
        // [live caller-owned native object][pointer-to-word only]
        // [safe ] retained by the current lexical scope; no authority is made.
        return reinterpret_cast<word>(pointer);
    }
    auto tuple()
    {
        ::std::array<::std::byte, tuple_bytes> result{};
        ::std::size_t used{};
        auto append{[&](auto const& value) noexcept
        {
            auto const bits{::std::as_bytes(::std::span{::std::addressof(value), 1uz})};
            CHECK(bits.size() <= result.size() - used);
            // [result, result+40)[used,used+sizeof(value))][complete source]
            // [safe ] the whole native prefix is checked before this offset.
            ::fast_io::freestanding::my_memcpy(result.data() + used, bits.data(), bits.size());
            used += bits.size();
        }};
        append(::std::uint32_t{0xfedcba98u});
        append(::std::uint64_t{0xfedcba9876543210ULL});
        append(::std::uint32_t{0x7fa12345u}); // signaling f32 NaN raw bits
        append(::std::uint64_t{0x7ff123456789abcdULL}); // signaling f64 NaN
        append(::std::array<::std::uint64_t, 2uz>{0x0123456789abcdefULL, 0xfedcba9876543210ULL});
        CHECK(used == result.size());
        return result;
    }
    struct fixture_registry
    {
        ::std::array<e::payload_kind, 5uz> kinds{
            e::payload_kind::i32, e::payload_kind::i64, e::payload_kind::f32,
            e::payload_kind::f64, e::payload_kind::v128};
        ::std::array<e::payload_kind, 1uz> reference_kind{e::payload_kind::wasm_reference};
        ::std::array<::std::shared_ptr<s::tag_instance_identity const>, 10uz> identities{};
        p::prepared_registry::owner registry{};
        fixture_registry()
        {
            for(auto& identity : identities) { identity = ::std::make_shared<s::tag_instance_identity>(); }
            identities[1] = identities[0]; // actual same-instance alias, not same shape
            ::std::array<p::admitted_tag, 10uz> tags{};
            tags[0] = {identities[0], kinds};
            tags[1] = {e::instance_root{identities[0], identities[0].get()}, kinds};
            tags[2] = {identities[2], kinds};
            tags[3] = {identities[3], {}};
            for(::std::size_t index{}; index != kinds.size(); ++index)
            {
                // [five live signature entries][index<5]
                // [safe ] the one-element borrowed schema is copied by prepare.
                tags[4uz + index] = {identities[4uz + index], {kinds.data() + index, 1uz}};
            }
            tags[9] = {identities[9], reference_kind};
            registry = p::prepared_registry::prepare(tags, {});
            CHECK(p::prepared_registry::canonical(registry));
        }
    };
    struct snapshot
    {
        word projected{};
        p::phase phase{};
        ::std::size_t count{}, bytes{}, frames{};
        bool truncated{};
        ::std::array<::std::byte, tuple_bytes> bits{};
        ::std::array<e::payload_kind, 5uz> kinds{};
        ::std::array<p::retired_frame, p::max_retired_frames> trace{};
    };
    snapshot capture(p::pending_context const& context, b::numeric_header const& header) noexcept
    {
        p::numeric_leaf_state state{};
        CHECK(context.borrow_numeric_state(state) == p::status::ok);
        snapshot result{};
        result.projected = b::native_details::header_access::phase_on_owner(header);
        result.phase = context.state(); result.count = state.fields.size();
        result.frames = state.retired_frames.size(); result.truncated = state.truncated;
        CHECK(result.count <= result.kinds.size() && result.frames <= result.trace.size());
        e::instance_root const empty{};
        for(::std::size_t index{}; index != result.count; ++index)
        {
            auto const& field{state.fields[index]};
            CHECK(!field.root().owner_before(empty) && !empty.owner_before(field.root()));
            result.kinds[index] = field.kind();
            auto const bits{field.bits()};
            CHECK(bits.size() <= result.bits.size() - result.bytes);
            // [snapshot owned bytes][checked prefix][live private field]
            // [safe ] copy only its initialized exact width; no slow borrow escapes.
            ::fast_io::freestanding::my_memcpy(result.bits.data() + result.bytes, bits.data(), bits.size());
            result.bytes += bits.size();
        }
        for(::std::size_t index{}; index != result.frames; ++index)
        {
            // [both 64-frame arrays][index < actual retired prefix]
            // [safe ] logical IDs copy by value; no native stack pointer survives.
            result.trace[index] = state.retired_frames[index];
        }
        return result;
    }
    bool same(snapshot const& left, snapshot const& right) noexcept
    {
        if(left.projected != right.projected || left.phase != right.phase ||
           left.count != right.count || left.bytes != right.bytes || left.frames != right.frames ||
           left.truncated != right.truncated || left.bits != right.bits || left.kinds != right.kinds)
        { return false; }
        for(::std::size_t index{}; index != left.frames; ++index)
        {
            if(left.trace[index].module_id != right.trace[index].module_id ||
               left.trace[index].function_index != right.trace[index].function_index)
            { return false; }
        }
        return true;
    }
    void numeric_and_errors(fixture_registry const& f, ::std::array<::std::byte, tuple_bytes> const& input)
    {
        p::native_island_chain chain{};
        p::pending_context context{f.registry};
        p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
        b::numeric_header header{context}; CHECK(header.activated_on_owner());
        auto const h{address(::std::addressof(header))};
        CHECK(b::uwvm2_pending_numeric_publish_r2(h, 0u, address(input.data()), input.size()) == b::result(p::status::ok));
        for(::std::size_t index{}; index != 70uz; ++index)
        { CHECK(b::uwvm2_pending_numeric_append_exit_r2(h, 21u, index) == b::result(p::status::ok)); }
        auto const before{capture(context, header)};
        CHECK(before.frames == p::max_retired_frames && before.truncated);
        ::std::array<::std::byte, tuple_bytes> output{};
        output.fill(::std::byte{0xd3});
        auto const untouched{output};
        auto reject{[&](word handle, word tag, word destination, word bytes, p::status expected) noexcept
        {
            CHECK(b::uwvm2_pending_numeric_take_r3(handle, tag, destination, bytes) == b::result(expected));
            CHECK(output == untouched && same(before, capture(context, header)));
            CHECK(context.matches(0uz) == p::status::ok && context.matches(1uz) == p::status::ok &&
                  context.matches(2uz) == p::status::no_match);
            CHECK(p::prepared_registry::canonical(f.registry));
        }};
        auto const out{address(output.data())};
        reject(h, 2u, out, output.size(), p::status::no_match);
        reject(h, static_cast<word>(f.identities.size()), out, output.size(), p::status::invalid_tag);
        reject(h, 0u, out, output.size() - 1uz, p::status::invalid_signature);
        reject(h, 0u, out, output.size() + 1uz, p::status::invalid_signature);
        reject(h, 0u, 0u, output.size(), p::status::invalid_reference);
        reject(h, 0u, h, output.size(), p::status::invalid_reference);
        reject(h, 0u, address(::std::addressof(context)), output.size(), p::status::invalid_reference);
        // Negative arithmetic-only destination: extent rejects before forming
        // or dereferencing any such pointer. It is never used as authority.
        reject(h, 0u, (~word{}) - output.size() + 2u, output.size(), p::status::size_overflow);
        reject(0u, 0u, out, output.size(), p::status::invalid_registry);
        reject(h + 1u, 0u, out, output.size(), p::status::invalid_registry);

        word peer_result{};
        ::std::thread peer{[&]() noexcept
        {
            // Only this actual read-only rejected leaf runs on the wrong thread.
            // Main retains all native objects and waits for join before observing.
            peer_result = b::uwvm2_pending_numeric_take_r3(h, 0u, out, output.size());
        }};
        peer.join();
        CHECK(peer_result == b::result(p::status::wrong_thread));
        CHECK(output == untouched && same(before, capture(context, header)));
        {
            p::pending_context child_context{f.registry};
            p::execution_island child{chain, child_context};
            CHECK(child.admission() == p::status::ok);
            b::numeric_header child_header{child_context}; CHECK(child_header.activated_on_owner());
            // A real nested scope suspends the parent; it is not fake busy state.
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 0u, out, output.size()) == b::result(p::status::busy));
            CHECK(output == untouched);
        }
        CHECK(same(before, capture(context, header)));
        // Same-signature distinct tag was rejected; actual alias of tag0 succeeds.
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 1u, out, output.size()) == b::result(p::status::ok));
        CHECK(output == input && !context.has_pending() && !header.pending_on_owner());
        p::numeric_leaf_state empty{};
        CHECK(context.borrow_numeric_state(empty) == p::status::no_pending);
        output.fill(::std::byte{0xd3});
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 0u, out, output.size()) == b::result(p::status::no_pending));
        CHECK(output == untouched);

        constexpr ::std::array<::std::size_t, 5uz> offsets{0uz, 4uz, 12uz, 16uz, 24uz};
        constexpr ::std::array<::std::size_t, 5uz> widths{4uz, 8uz, 4uz, 8uz, 16uz};
        for(::std::size_t index{}; index != widths.size(); ++index)
        {
            CHECK(offsets[index] + widths[index] <= input.size());
            // [live forty-byte input][one full numeric raw field][end]
            // [safe ] checked native slice, not FP arithmetic or an unaligned typed load.
            auto const* source{input.data() + offsets[index]};
            CHECK(b::uwvm2_pending_numeric_publish_r2(h, 4uz + index, address(source), widths[index]) == b::result(p::status::ok));
            ::std::array<::std::byte, 48uz> guarded{}; guarded.fill(::std::byte{0xa6});
            // [guarded prefix 4][owned exact field][guarded suffix]
            // [safe ] width<=16, so this full output extent lies in the live array.
            auto* destination{guarded.data() + 4uz};
            CHECK(b::uwvm2_pending_numeric_take_r3(h, 4uz + index, address(destination), widths[index]) == b::result(p::status::ok));
            for(::std::size_t byte{}; byte != guarded.size(); ++byte)
            {
                auto const expected{byte >= 4uz && byte < 4uz + widths[index] ?
                    source[byte - 4uz] : ::std::byte{0xa6}};
                CHECK(guarded[byte] == expected);
            }
            CHECK(!context.has_pending() && !header.pending_on_owner());
        }
        CHECK(b::uwvm2_pending_numeric_publish_r2(h, 3u, 0u, 0u) == b::result(p::status::ok));
        CHECK(context.has_pending());
        auto const empty_before{capture(context, header)};
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 3u, out, 1u) == b::result(p::status::invalid_signature));
        CHECK(output == untouched && same(empty_before, capture(context, header)));
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 3u, 0u, 0u) == b::result(p::status::ok));
        CHECK(!context.has_pending() && !header.pending_on_owner());
    }
    void declined_native_states(fixture_registry const& f)
    {
        p::pending_context inactive{f.registry};
        b::numeric_header inactive_header{inactive};
        ::std::array<::std::byte, 4uz> output{}; output.fill(::std::byte{0xb4});
        auto const untouched{output};
        CHECK(b::uwvm2_pending_numeric_take_r3(address(::std::addressof(inactive_header)), 4u,
            address(output.data()), output.size()) == b::result(p::status::inactive));
        CHECK(output == untouched);
        p::native_island_chain chain{};
        p::pending_context context{f.registry};
        p::execution_island island{chain, context}; CHECK(island.admission() == p::status::ok);
        b::numeric_header header{context}; CHECK(header.activated_on_owner());
        auto const h{address(::std::addressof(header))};
        ::std::uint32_t raw{0x7fa12345u};
        auto field{e::payload_field::numeric(e::payload_kind::f32, ::std::as_bytes(::std::span{::std::addressof(raw), 1uz}))};
        CHECK(field);
        // This observable value is actual immutable value::make ownership.
        auto instance{e::value::make(f.identities[6], {::std::addressof(*field), 1uz})};
        CHECK(instance);
        // [owned immutable value][same owner retained by pending then catch]
        // [safe ] compare identity only while those real owners remain alive.
        auto const* identity{instance.get()};
        CHECK(context.publish_existing(::std::move(instance)) == p::status::ok);
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 6u, address(output.data()), output.size()) == b::result(p::status::host_codec_required));
        CHECK(output == untouched && context.state() == p::phase::existing && context.has_pending());
        {
            p::caught_payload target{f.registry};
            p::caught_root_scope scope{island, target}; CHECK(scope.admission() == p::status::ok);
            bool called{};
            CHECK(context.materialize_in_registered(target, [&](auto, bool)
                { called = true; return e::diagnostic_trace_ref{}; }) == p::status::ok);
            CHECK(!called && target.observable().get() == identity);
        }
        auto const reference{::uwvm2::object::global::make_wasm_i31_reference(-1)};
        auto ref_field{e::payload_field::wasm_reference(::std::as_bytes(::std::span{::std::addressof(reference), 1uz}))};
        CHECK(ref_field);
        auto validate_i31{[](::std::size_t, ::std::size_t, p::reference actual) noexcept
            { return actual.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31; }};
        CHECK(context.publish_fresh(9uz, {::std::addressof(*ref_field), 1uz}, validate_i31) == p::status::ok);
        CHECK(b::uwvm2_pending_numeric_take_r3(h, 9u, address(output.data()), output.size()) == b::result(p::status::host_codec_required));
        CHECK(output == untouched && context.matches(9uz) == p::status::ok && context.state() == p::phase::fresh);
        {
            p::caught_payload target{f.registry};
            p::caught_root_scope scope{island, target}; CHECK(scope.admission() == p::status::ok);
            CHECK(context.transfer_matching(9uz, target) == p::status::ok);
            CHECK(target.fields().size() == 1uz && target.fields()[0].kind() == e::payload_kind::wasm_reference);
            p::reference copied{};
            // [complete actual immutable carrier][complete aligned local]
            // [safe ] exact real carrier width; no aggregate pointer is followed.
            ::fast_io::freestanding::my_memcpy(::std::addressof(copied),
                target.fields()[0].bits().data(), sizeof(copied));
            CHECK(copied.kind == reference.kind && copied.storage.wasm_i31.get_s() == -1);
        }
        CHECK(!context.has_pending() && !header.pending_on_owner());
        CHECK(p::prepared_registry::canonical(f.registry));
    }
    #include "pending_numeric_cache_lifecycle.h"
}
int main()
{
    fixture_registry f{};
    auto const input{tuple()};
    numeric_and_errors(f, input);
    declined_native_states(f);
    numeric_cache_lifecycle(f, input);
    numeric_cache_transfer_and_unwind(f, input);
    ::fast_io::io::println("pending_numeric_fused_catch checks=", checks,
        " native_component=true actual_VM=false actual_GC=false performance=false");
}
