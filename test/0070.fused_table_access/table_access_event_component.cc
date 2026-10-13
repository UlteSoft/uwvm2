// Shared first-typing DATA component; no body/native/source authority is minted.
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <fast_io.h>
#include <array>
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace
{
    void require(bool condition, char const* reason)
    { if(!condition) { ::fast_io::print(::fast_io::err(), "table access kernel: ", ::fast_io::mnp::os_c_str(reason), "\n"); ::fast_io::fast_terminate(); } }
    struct stack
    {
        ::std::array<v::core3_operand, 4uz> cells{};
        ::std::size_t base{}, end{}, pops{}, pushes{};
        auto count() const noexcept { return end - base; }
        v::core3_operand pop() noexcept
        {
            require(end > base && end <= cells.size(), "actual frame suffix before indexed pop");
            // [preserved prefix][base ... checked live top] end
            // Kernel proves end>base BEFORE this indexed read/removal.
            ++pops; return cells[--end];
        }
        void push(t::core_value_type value) noexcept
        { require(end < cells.size(), "fixed owner capacity before indexed push"); cells[end++] = {value,false}; ++pushes; }
    };
    t::core_value_type ref(t::abstract_heap_type heap, bool nullable = true)
    { return {t::value_kind::reference, {static_cast<::std::int_least64_t>(heap)}, nullable}; }
    template<unsigned Opcode>
    auto apply(stack& actual, v::validated_table_access_event& event, bool poly, bool wide,
               t::core_value_type element, unsigned carrier, ::std::size_t bytes = 2uz)
    {
        auto const count{[&]() noexcept { return actual.count(); }};
        auto const consume{[&]() noexcept { return actual.pop(); }};
        auto const push{[&](auto value) noexcept { actual.push(value); }};
        auto const matches{[](auto a, auto e) noexcept
        { return v::core3_value_type_matches_with_context(a,e,
            v::core3_signature_view<t::owned_function_signature<
                ::uwvm2::parser::wasm::standard::wasm1::type::value_type>>{nullptr,0uz},nullptr); }};
        return v::transition_table_access_event<Opcode>(event,poly,count,consume,matches,push,
            0u,wide,element,carrier,9uz,bytes,1uz);
    }
    void check()
    {
        auto const func{ref(t::abstract_heap_type::func)};
        auto const eq{ref(t::abstract_heap_type::eq)};
        auto const exn{ref(t::abstract_heap_type::exn)};
        v::core3_operand const i32{{t::value_kind::i32},false}, i64{{t::value_kind::i64},false};
        stack narrow{}; narrow.cells[0] = i64; narrow.cells[1] = i32; narrow.base = 1uz; narrow.end = 2uz; v::validated_table_access_event event{};
        require(apply<0x25u>(narrow,event,false,false,func,0x70u).error == v::typed_stack_error::ok &&
            narrow.pops == 1uz && narrow.pushes == 1uz && narrow.end == 2uz && narrow.cells[0].type.kind == t::value_kind::i64 &&
            narrow.cells[1].type.kind == t::value_kind::reference && event.table_index == 0u &&
            event.source_bytes == 2uz && !event.address64, "table32 get preserves prefix and pushes exact reference");
        stack wide{}; wide.cells[0] = i32; wide.cells[1] = i64; wide.base = 1uz; wide.end = 2uz;
        require(apply<0x25u>(wide,event,false,true,eq,0x6fu,6uz).error == v::typed_stack_error::ok && event.address64 && event.source_bytes == 6uz,
            "table64 get and allowed five-byte tableidx encoded extent");
        stack missing{}; missing.cells[0] = {{t::value_kind::f64},false}; missing.end = 1uz;
        require(apply<0x26u>(missing,event,false,true,func,0x70u).error == v::typed_stack_error::stack_underflow && missing.pops == 0uz,
            "set entirearity outranks wrong top type without consuming prefix");
        stack badref{}; badref.cells[0] = i64; badref.cells[1] = {ref(t::abstract_heap_type::extern_),false}; badref.end = 2uz;
        auto const mismatch{apply<0x26u>(badref,event,false,true,eq,0x6fu)};
        require(mismatch.error == v::typed_stack_error::type_mismatch && mismatch.failed_pop_index == 0u && badref.pops == 1uz,
            "element family is checked top-first");
        stack badindex{}; badindex.cells[0] = i32; badindex.cells[1] = {ref(t::abstract_heap_type::i31,false),false}; badindex.end = 2uz;
        auto const badwidth{apply<0x26u>(badindex,event,false,true,eq,0x6fu)};
        require(badwidth.error == v::typed_stack_error::type_mismatch && badwidth.failed_pop_index == 1u && badindex.pops == 2uz,
            "non-null i31 subtype accepted before address64 mismatch");
        stack modern{}; modern.cells[0] = i64; modern.cells[1] = {ref(t::abstract_heap_type::noexn),false}; modern.end = 2uz;
        require(apply<0x26u>(modern,event,false,true,exn,0x69u).error == v::typed_stack_error::ok && modern.end == 0uz,
            "noexn subtype of exn works with empty retained signature context");
        stack empty{};
        require(apply<0x25u>(empty,event,true,true,func,0x70u).error == v::typed_stack_error::ok && empty.pops == 0uz && empty.pushes == 1uz,
            "empty unreachable get never consumes synthetic Bot");
        stack part{}; part.cells[0] = {ref(t::abstract_heap_type::nofunc),false}; part.end = 1uz;
        require(apply<0x26u>(part,event,true,true,func,0x70u).error == v::typed_stack_error::ok && part.pops == 1uz,
            "partly concrete unreachable set checks element then accepts synthetic index");
        stack knownbad{}; knownbad.cells[0] = i32; knownbad.end = 1uz;
        require(apply<0x25u>(knownbad,event,true,true,func,0x70u).error == v::typed_stack_error::type_mismatch,
            "concrete wrong width remains invalid in unreachable code");
        stack bot{}; bot.cells[0] = {{},true}; bot.end = 1uz;
        require(apply<0x25u>(bot,event,true,true,func,0x70u).error == v::typed_stack_error::ok && bot.pops == 1uz,
            "concrete value Bot accepts required address");
        stack heap{}; heap.cells[0] = {{t::value_kind::reference,{t::heap_type::bottom_code},false},false}; heap.end = 1uz;
        require(apply<0x25u>(heap,event,true,true,func,0x70u).error == v::typed_stack_error::type_mismatch,
            "known heap Bot is reference-only, never numeric index");
        stack invalid{}; invalid.cells[0] = i32; invalid.end = 1uz;
        require(apply<0x25u>(invalid,event,false,false,func,0x70u,7uz).error != v::typed_stack_error::ok && invalid.pops == 0uz,
            "invalid DATA extent is not a valid decoder certificate");
    }
}
int main() { check(); ::fast_io::print(::fast_io::out(), "TABLE_ACCESS_DATA kernel=firstarity+Bot+width+subtyping execution_authority=0\n"); }
