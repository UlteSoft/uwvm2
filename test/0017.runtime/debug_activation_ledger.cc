// LLVM-free event semantics. No runtime/source authority is fabricated here.
#include <uwvm2/runtime/lib/uwvm_runtime_debug_activation.h>
#include <fast_io.h>
#include <cstdint>
#include <vector>
namespace activation = uwvm2::runtime::lib::details::debug_activation;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_activation_ledger: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main()
{
    activation::ledger ledger{};
    check(ledger.ready_for_root_entry() && !ledger.complete(), "clean pre-entry ledger grants no live chain");
    auto root{ledger.enter(0u, 7u, 1u, 9u)};
    check(root != 0u && ledger.complete() && ledger.count() == 1u, "real entry identity");
    // Observing offset-zero/backedges never calls enter: no PC/CFA API exists.
    for(unsigned i{}; i != 100u; ++i) { check(ledger.data()[0u].incarnation == root, "loop preserves instance"); }
    check(!ledger.ready_for_root_entry(), "live Wasm activation cannot rebind root entry");
    auto child{ledger.enter(0u, 7u, 1u, 9u)};
    check(child != root && ledger.data()[1u].parent == root, "same function recursion distinct");
    auto continuation{ledger.data()[1u].continuation};
    ledger.leave(child, activation::exit_kind::typed_tail);
    check(!ledger.complete(), "tail pending unavailable");
    for(unsigned i{}; i != 10000u; ++i)
    {
        auto next{ledger.enter(0u, 7u, 1u, 9u)};
        check(next != child && ledger.count() == 2u && ledger.complete() &&
              ledger.data()[1u].continuation == continuation && ledger.data()[1u].parent == root, "same-CFA tail has fresh id and same continuation");
        child = next; ledger.leave(child, activation::exit_kind::typed_tail);
    }
    child = ledger.enter(1u, 4u, 2u, 9u);
    check(ledger.data()[1u].function_generation == 2u, "per-function compiled generation retained");
    ledger.leave(child, activation::exit_kind::exception);
    check(ledger.complete() && ledger.count() == 1u, "cross-frame exceptional exit retires child");
    child = ledger.enter(0u, 3u, 1u, 9u);
    ledger.leave(child, activation::exit_kind::host_tail);
    auto ordinary{ledger.enter(0u, 4u, 1u, 9u)};
    check(ledger.data()[1u].continuation == ordinary && ordinary != continuation, "host tail cannot leak typed continuation");
    ledger.leave(ordinary, activation::exit_kind::returned);
    auto host{ledger.suspend_host()}; check(host != 0u && !ledger.complete(), "host transition unknown");
    auto reentry{ledger.enter(0u, 7u, 1u, 9u)}; check(!ledger.complete(), "host reentry cannot prove physical chain");
    auto nested_host{ledger.suspend_host()};
    auto nested{ledger.enter(0u, 7u, 1u, 9u)}; check(!ledger.complete(), "nested host reentry unknown");
    ledger.leave(nested, activation::exit_kind::returned); ledger.restore_host(nested_host);
    ledger.leave(reentry, activation::exit_kind::exception); ledger.restore_host(host);
    check(ledger.complete() && ledger.data()[0u].incarnation == root, "exact returns restore original known island");
    ledger.leave(root, activation::exit_kind::returned); check(!ledger.complete() && ledger.valid(), "outer returned no current activation");
    host = ledger.suspend_host(); check(!ledger.ready_for_root_entry(), "empty foreign host island cannot bind a root"); reentry = ledger.enter(0u, 7u, 1u, 9u);
    check(!ledger.complete(), "empty prior chain still declines host reentry");
    ledger.leave(reentry, activation::exit_kind::returned); ledger.restore_host(host);
    check(ledger.valid() && ledger.ready_for_root_entry(), "empty host island returns cleanly");
    activation::ledger full{};
    ::std::vector<::std::uint64_t> identities;
    activation::frame const* stable{};
    for(::std::size_t i{}; i != 10000u; ++i)
    {
        auto const id{full.enter(0u, i, 1u, 9u)}; check(id != 0u, "deep segmented recursive entry"); identities.push_back(id);
        if(i == 64u) { stable = ::std::addressof(full.data()[i]); }
        if(i == 62u || i == 63u || i == 64u || i == 127u || i == 1023u || i == 4096u || i == 9999u)
        { check(full.complete() && full.count() == i + 1u && full.data()[i].parent == (i == 0u ? 0u : identities[i - 1u]), "boundary identity chain complete"); }
    }
    check(stable == ::std::addressof(full.data()[64u]) && stable->incarnation == identities[64u], "block addresses survive directory growth");
    auto const high_water{full.storage_bytes()};
    while(!identities.empty()) { full.leave(identities.back(), activation::exit_kind::exception); identities.pop_back(); }
    check(full.valid() && full.ready_for_root_entry(), "deep exact EH retirement leaves clean ledger");
    for(::std::size_t i{}; i != 10000u; ++i) { identities.push_back(full.enter(0u, i, 1u, 9u)); }
    check(full.complete() && full.storage_bytes() == high_water, "recording reuses existing blocks");
    activation::ledger host_depth{}; auto const host_root{host_depth.enter(0u, 0u, 1u, 9u)};
    ::std::vector<::std::uint64_t> tokens, callbacks;
    for(::std::size_t i{}; i != 129u; ++i) { tokens.push_back(host_depth.suspend_host()); callbacks.push_back(host_depth.enter(0u, i, 1u, 9u)); }
    check(host_depth.valid() && !host_depth.complete(), "deep host islands cannot grant physical authority");
    while(!tokens.empty()) { host_depth.leave(callbacks.back(), activation::exit_kind::returned); callbacks.pop_back(); host_depth.restore_host(tokens.back()); tokens.pop_back(); }
    check(host_depth.complete() && host_depth.count() == 1u && host_depth.data()[0u].incarnation == host_root, "nested islands restore exact original root");
    activation::ledger quota{activation::ledger::inline_storage_bytes};
    for(::std::size_t i{}; i != activation::frames_per_block; ++i) { check(quota.enter(0u, i, 1u, 9u) != 0u, "configured inline budget"); }
    check(quota.enter(0u, 0u, 1u, 9u) == 0u && !quota.valid() && quota.count() == 0u &&
          quota.failure_reason() == activation::failure::resource_exhausted, "resource exhaustion explicit and no partial chain");
    check(!quota.ready_for_root_entry() && quota.enter(0u, 0u, 1u, 9u) == 0u, "failed ledger cannot resurrect");
    activation::ledger tiny{activation::ledger::inline_storage_bytes - 1u};
    check(!tiny.valid() && tiny.failure_reason() == activation::failure::resource_exhausted, "insufficient initial byte budget explicit");
    activation::ledger bad{}; root = bad.enter(0u, 0u, 1u, 9u); child = bad.enter(0u, 0u, 1u, 9u);
    bad.leave(root, activation::exit_kind::returned); check(!bad.valid() && bad.count() == 0u, "mismatched unwind zero partial chain");
    activation::ledger invalid{}; root = invalid.enter(0u, 0u, 1u, 9u);
    invalid.leave(root, static_cast<activation::exit_kind>(4u)); check(!invalid.valid(), "unknown exit kind rejected");
    activation::ledger pending{}; root = pending.enter(0u, 0u, 1u, 9u); pending.leave(root, activation::exit_kind::typed_tail);
    check(!pending.ready_for_root_entry(), "pending typed-tail successor cannot bind a root");
    check(pending.suspend_host() == 0u && !pending.valid(), "typed successor cannot cross unknown host transfer");
    activation::ledger incomplete{}; root = incomplete.enter(0u, 0u, 1u, 9u); host = incomplete.suspend_host();
    child = incomplete.enter(0u, 0u, 1u, 9u); incomplete.restore_host(host);
    check(!incomplete.valid(), "unretired callback child cannot resurrect parent");
    activation::ledger generation{}; check(generation.enter(0u, 0u, 0u, 9u) == 0u && !generation.valid(), "zero generation rejected");
    ::fast_io::io::println("debug_activation_ledger: PASS exact entry, recursion, bounded tails, return, EH retirement, host islands, fail closed");
}
