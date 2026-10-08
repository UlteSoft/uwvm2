// Pure HOST synchronous callback lifetime/latching component; no VM state.
#include <uwvm2/uwvm/debugger/management_wait_interrupt.h>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
struct flag { unsigned queries{}; bool value{}; };
static bool query(void* context) noexcept
{
    auto& state{*static_cast<flag*>(context)};
    ++state.queries;
    auto value{state.value}; state.value = false;
    return value; // a real synchronous transient HOST flag, no fake guest stop
}
static void check(bool value) noexcept { if(!value) { ::fast_io::fast_terminate(); } }
int main()
{
    flag state{};
    dbg::management_wait_interrupt_observation observed{{&state, &query}};
    auto borrowed{observed.borrow()};
    check(!borrowed.pending() && state.queries == 1u);
    state.value = true;
    check(borrowed.pending() && state.queries == 2u && !state.value);
    check(borrowed.pending() && borrowed.pending() && state.queries == 2u);
    // Recursive synchronous management commands must inherit the same observed
    // operation without taking ownership of its outer stack or original flag.
    dbg::management_wait_interrupt_observation nested{borrowed};
    auto inner{nested.borrow()}; check(inner.pending() && state.queries == 2u);
    check(inner.pending() && borrowed.pending() && state.queries == 2u);
    dbg::management_wait_interrupt_observation empty{{}};
    check(!empty.borrow().pending());
    // A NEW command with the consumed original flag must begin uncancelled.
    dbg::management_wait_interrupt_observation next{{&state, &query}};
    check(!next.borrow().pending() && state.queries == 3u);
    ::fast_io::io::println("debug_management_interrupt_observation: PASS");
}
