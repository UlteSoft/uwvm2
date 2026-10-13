// LLVM-free DATA predicates only. Passing this component does not qualify a
// physical debugger activation, native trap, NI, host-call return or finish.
#include <uwvm2/runtime/lib/uwvm_runtime_debug_activation.h>
#include <fast_io.h>
#include <vector>
namespace activation = ::uwvm2::runtime::lib::details::debug_activation;
static void check(bool value, char const* text) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_native_activation_prefix: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static void copy_expected(activation::ledger const& ledger, ::std::vector<activation::frame>& copy) noexcept
{
    auto const count{ledger.count()}; copy.resize(count);
    // [ledger-owned fixed source][cursor-test-owned fixed destination] end
    // [safe                                                        ] count <= max_frames
    //  ^^ before each corresponding array index; these are DATA, not stack addresses.
    for(::std::size_t i{}; i != count; ++i) { copy[i] = ledger.data()[i]; copy[i].island = 0u; }
}
int main()
{
    activation::ledger ledger{};
    auto const root{ledger.enter(0u, 1u, 7u, 11u)};
    auto const child{ledger.enter(3u, 9u, 13u, 11u)};
    ::std::vector<activation::frame> original;
    copy_expected(ledger, original);
    check(ledger.matches_native_chain(original.data(), 2u), "actual complete same-island chain");
    check(!ledger.matches_native_chain(nullptr, 2u) && !ledger.matches_native_chain(original.data(), 0u) &&
          !ledger.matches_native_chain(original.data(), 10000u + 1u), "bounded immutable array admission");
    check(!ledger.matches_native_descendant(original.data(),2u), "same depth is never a recursion witness");
    auto const recursive{ledger.enter(3u,9u,13u,11u)};
    check(ledger.matches_native_descendant(original.data(),2u), "same-body recursive incarnation extends original chain");
    auto const host_token{ledger.suspend_host()};
    check(!ledger.matches_native_descendant(original.data(),2u), "host suspension rejects descendant proof");
    auto const host_callback{ledger.enter(3u,9u,13u,11u)};
    check(!ledger.matches_native_descendant(original.data(),2u), "host reentry cannot be a native descendant");
    ledger.leave(host_callback,activation::exit_kind::returned);ledger.restore_host(host_token);
    check(ledger.matches_native_descendant(original.data(),2u), "exact host restoration recovers genuine descendant");
    original[1u].incarnation+=1u;
    check(!ledger.matches_native_descendant(original.data(),2u), "matching depth cannot hide changed origin incarnation");
    original[1u].incarnation-=1u;
    ledger.leave(recursive,activation::exit_kind::returned);
    check(!ledger.matches_native_descendant(original.data(),2u), "retired recursive activation supplies no descendant witness");
    auto const wrong_epoch{ledger.enter(3u,9u,13u,12u)};
    check(!ledger.matches_native_descendant(original.data(),2u), "descendant must retain original runtime epoch");
    ledger.leave(wrong_epoch,activation::exit_kind::returned);
    auto const correct{original[1u]};
    original[1u].incarnation = correct.incarnation + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "incarnation mismatch"); original[1u] = correct;
    original[1u].parent = correct.parent + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "parent mismatch"); original[1u] = correct;
    original[1u].continuation = correct.continuation + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "continuation mismatch"); original[1u] = correct;
    original[1u].module = correct.module + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "module mismatch"); original[1u] = correct;
    original[1u].function = correct.function + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "function mismatch"); original[1u] = correct;
    original[1u].function_generation = correct.function_generation + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "generation mismatch"); original[1u] = correct;
    original[1u].runtime_epoch = correct.runtime_epoch + 1u;
    check(!ledger.matches_native_chain(original.data(), 2u), "epoch mismatch"); original[1u] = correct;
    auto const token{ledger.suspend_host()};
    check(token != 0u && !ledger.matches_native_chain(original.data(), 2u), "host island never authenticates original native chain");
    auto const callback{ledger.enter(8u, 8u, 1u, 11u)};
    check(callback != 0u && !ledger.matches_native_chain(original.data(), 2u), "nested host callback chain rejected");
    ledger.leave(callback, activation::exit_kind::returned); ledger.restore_host(token);
    check(ledger.matches_native_chain(original.data(), 2u), "exact host restoration");
    ledger.leave(child, activation::exit_kind::returned);
    check(!ledger.matches_native_chain(original.data(), 2u) &&
          ledger.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::returned), "logical return prefix DATA");
    check(!ledger.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::exception), "EH never authorizes physical epilogue");
    original[0u].function_generation += 1u;
    check(!ledger.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::returned), "retired ancestor identity mismatch");
    original[0u].function_generation -= 1u;
    copy_expected(ledger, original);
    ledger.leave(root, activation::exit_kind::returned);
    check(ledger.matches_native_retired_prefix(original.data(), 1u, activation::exit_kind::returned), "empty original-root prefix DATA");
    activation::ledger tails{};
    auto const tail_root{tails.enter(0u, 1u, 1u, 1u)};
    auto const tail_child{tails.enter(0u, 2u, 1u, 1u)};
    copy_expected(tails, original);
    tails.leave(tail_child, activation::exit_kind::typed_tail);
    check(!tails.matches_native_chain(original.data(), 2u) &&
          tails.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::typed_tail), "exact pending typed-tail prefix DATA");
    check(!tails.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::returned) &&
          !tails.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::host_tail), "pending tail cannot be normal/host retirement");
    original[1u].continuation += 1u;
    check(!tails.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::typed_tail), "pending continuation mismatch");
    original[1u].continuation -= 1u;
    auto const tail_successor{tails.enter(0u, 3u, 1u, 1u)};
    check(tail_successor != 0u && tail_successor != tail_child &&
          !tails.matches_native_chain(original.data(), 2u) &&
          !tails.matches_native_retired_prefix(original.data(), 2u, activation::exit_kind::typed_tail), "typed successor never revives original incarnation");
    tails.leave(tail_successor, activation::exit_kind::host_tail);
    copy_expected(tails, original);
    tails.leave(tail_root, activation::exit_kind::host_tail);
    check(tails.matches_native_retired_prefix(original.data(), 1u, activation::exit_kind::host_tail), "host-tail empty prefix DATA");
    tails.poison();
    check(!tails.matches_native_chain(original.data(), 1u) &&
          !tails.matches_native_retired_prefix(original.data(), 1u, activation::exit_kind::host_tail), "poisoned ledger rejected");
    activation::ledger full{};
    for(::std::size_t i{}; i != 10000u; ++i) { check(full.enter(0u, i, 1u, 1u) != 0u, "bounded depth fill"); }
    copy_expected(full, original);
    check(full.matches_native_chain(original.data(), 10000u), "maximum depth exact chain");
    auto const last{original[10000u - 1u].incarnation};
    full.leave(last, activation::exit_kind::returned);
    check(full.matches_native_retired_prefix(original.data(), 10000u, activation::exit_kind::returned), "maximum depth exact prefix DATA");
    ::fast_io::io::println("debug_native_activation_prefix: PASS DATA only; physical-cursor/NI/finish-qualified=no");
}
