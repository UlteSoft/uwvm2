// Source fixture: the real shared first-decode Core 3 transition.
// Actual compilation/execution belongs to the admitted Linux cgroup keeper.
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <array>
#include <utility>
#include <fast_io.h>
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
struct stack_fixture
{
    ::std::array<v::core3_operand, 4> values{};
    ::std::size_t size{}, base{}, consumed{}, pushed{};
    constexpr auto count() const noexcept { return size - base; }
    constexpr v::core3_operand consume() noexcept
    {
        // [outer entries below base][current-frame suffix] end
        // [preserved outer frame   ][safe               ] one-past
        // The common sequence kernel proves size>base before this subtraction.
        ++consumed; auto const owned{values[size - 1uz]}; --size; return owned;
    }
    constexpr void push(t::core_value_type type) noexcept
    {
        // Tests bound size<4 before each transition; a successful numeric
        // transition consumes at least its concrete suffix before one push.
        values[size] = {type, false}; ++size; ++pushed;
    }
};
template<unsigned Opcode>
constexpr bool one_family()
{
    constexpr auto arity{1uz};
    constexpr v::core3_operand i64{{Opcode == 0xa7u ? t::value_kind::i64 : t::value_kind::i32}, false};
    constexpr v::core3_operand i32{{Opcode == 0xa7u ? t::value_kind::i32 : t::value_kind::i64}, false};
    constexpr v::core3_operand heap_bottom{
        {t::value_kind::reference, {t::heap_type::bottom_code}, false}, false};
    auto run{[](stack_fixture& stack, bool poly, v::validated_integer_width_event& output)
    {
        return v::transition_integer_width_event<Opcode>(output, poly,
            [&]() constexpr noexcept { return stack.count(); },
            [&]() constexpr noexcept { return stack.consume(); },
            [&](t::core_value_type value) constexpr noexcept { stack.push(value); }, 19uz, 3uz);
    }};
    // Preserve a wrong-typed outer frame sentinel: only the current suffix counts.
    stack_fixture normal{}; normal.values[0] = i32; normal.base = normal.size = 1uz;
    for(::std::size_t i{}; i != arity; ++i) { normal.values[normal.size++] = i64; }
    v::validated_integer_width_event event{};
    auto result{run(normal, false, event)};
    if(result.error != v::typed_stack_error::ok || normal.size != 2uz ||
       normal.consumed != arity || normal.pushed != 1uz ||
       normal.values[0].type.kind != i32.type.kind ||
       normal.values[1].type.kind != i32.type.kind ||
       event.opcode != Opcode || event.popped != arity || event.pushed != 1u ||
       event.pop_bytes != (Opcode == 0xa7u ? 8u : 4u) || event.push_bytes != (Opcode == 0xa7u ? 4u : 8u) ||
       event.source_offset != 19uz || event.source_bytes != 1uz ||
       event.control_depth != 3uz || event.stack_polymorphic) { return false; }
    // Complete arity preflight must happen before a concrete mismatch/pop.
    stack_fixture short_frame{}; short_frame.values[0] = i32;
    short_frame.size = arity - 1uz;
    v::validated_integer_width_event untouched{.opcode = 0xabcu};
    result = run(short_frame, false, untouched);
    if(result.error != v::typed_stack_error::stack_underflow ||
       short_frame.consumed != 0uz || short_frame.pushed != 0uz ||
       untouched.opcode != 0xabcu) { return false; }
    // An empty unreachable frame synthesizes all required operands without
    // touching the outer frame or consuming fictitious entries.
    stack_fixture empty{}; empty.values[0] = i32; empty.base = empty.size = 1uz;
    result = run(empty, true, event);
    if(result.error != v::typed_stack_error::ok || empty.consumed != 0uz ||
       empty.size != 2uz || !event.stack_polymorphic) { return false; }
    // One known concrete wrong value remains invalid after unreachable.
    stack_fixture wrong{}; wrong.values[0] = i32; wrong.size = 1uz;
    result = run(wrong, true, untouched);
    if(result.error != v::typed_stack_error::type_mismatch ||
       result.failed_pop_index != 0u || wrong.consumed != 1uz ||
       wrong.pushed != 0uz || untouched.opcode != 0xabcu) { return false; }
    // Reference-only Bot is not the unconstrained VALUE Bot.
    stack_fixture reference{}; reference.values[0] = heap_bottom; reference.size = 1uz;
    result = run(reference, true, untouched);
    if(result.error != v::typed_stack_error::type_mismatch || reference.pushed != 0uz) { return false; }
    // Genuine VALUE Bot may match i64 while concrete rich reference types may not.
    stack_fixture unknown{}; unknown.values[0] = {heap_bottom.type, true}; unknown.size = 1uz;
    result = run(unknown, true, event);
    if(result.error != v::typed_stack_error::ok || unknown.consumed != 1uz ||
       unknown.pushed != 1uz || !event.stack_polymorphic) { return false; }
    return true;
}
constexpr bool all_family() noexcept
{ return one_family<0xa7u>() && one_family<0xacu>() && one_family<0xadu>(); }
static_assert(all_family());
int main()
{
    if(!all_family()) { ::fast_io::fast_terminate(); }
    ::fast_io::io::println("all3 Core3 integer-width first-typing DATA transitions PASS; native/wholeVM qualification=false");
}
