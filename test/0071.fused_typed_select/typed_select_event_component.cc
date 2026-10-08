// Shared first typing DATA only: no module/source/native permission.
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <fast_io.h>
#include <array>
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace
{
    void require(bool valid, char const* text)
    { if(!valid) { ::fast_io::print(::fast_io::err(),"typed select DATA: ",::fast_io::mnp::os_c_str(text),"\n"); ::fast_io::fast_terminate(); } }
    struct stack
    {
        ::std::array<v::core3_operand,4uz> cells{};
        ::std::size_t base{},end{},pops{},pushes{};
        auto count() const noexcept { return end - base; }
        auto pop() noexcept
        { require(end>base && end<=cells.size(),"actual suffix before indexed pop"); ++pops; return cells[--end]; }
        void push(t::core_value_type type) noexcept
        { require(end<cells.size(),"actual capacity before indexed push"); cells[end++]={type,false}; ++pushes; }
    };
    auto ref(t::abstract_heap_type heap, bool nullable=true) noexcept
    { return t::core_value_type{t::value_kind::reference,{static_cast<::std::int_least64_t>(heap)},nullable}; }
    auto apply(stack& owner,v::validated_typed_select_event& event,bool poly,t::core_value_type type,unsigned carrier,
               ::std::size_t bytes=3uz)
    {
        auto const count{[&]() noexcept { return owner.count(); }};
        auto const pop{[&]() noexcept { return owner.pop(); }};
        auto const push{[&](auto value) noexcept { owner.push(value); }};
        auto const matches{[](auto a,auto e) noexcept
        { return v::core3_value_type_matches_with_context(a,e,
            v::core3_signature_view<t::owned_function_signature<::uwvm2::parser::wasm::standard::wasm1::type::value_type>>{nullptr,0uz},nullptr); }};
        return v::transition_typed_select_event(event,poly,count,pop,matches,push,type,carrier,7uz,bytes,1uz);
    }
    void check()
    {
        v::core3_operand const i32{{t::value_kind::i32},false},i64{{t::value_kind::i64},false};
        v::validated_typed_select_event event{};
        stack first{};first.cells[0]=i64;first.cells[1]=i64;first.cells[2]=i64;first.cells[3]=i32;first.base=1uz;first.end=4uz;
        require(apply(first,event,false,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::ok &&
            first.pops==3uz && first.pushes==1uz && first.end==2uz && first.cells[0].type.kind==t::value_kind::i64 &&
            event.left_concrete && !event.left_unknown,"whole3 operands preserve prefix and actual left metadata");
        stack arity{};arity.cells[0]=i64;arity.cells[1]={{t::value_kind::f64},false};arity.end=2uz;
        require(apply(arity,event,false,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::stack_underflow && arity.pops==0uz,
            "entire3 arity outranks wrong condition before any pop");
        stack cond{};cond.cells[0]=i64;cond.cells[1]=i64;cond.cells[2]=i64;cond.end=3uz;
        auto const condition{apply(cond,event,false,{t::value_kind::i64},0x7eu)};
        require(condition.error==v::typed_stack_error::type_mismatch && condition.failed_pop_index==0u && cond.pops==1uz,"condition first once");
        stack family{};family.cells[0]={ref(t::abstract_heap_type::eq),false};family.cells[1]={ref(t::abstract_heap_type::extern_),false};family.cells[2]=i32;family.end=3uz;
        auto const badfamily{apply(family,event,false,ref(t::abstract_heap_type::eq),0x6fu)};
        require(badfamily.error==v::typed_stack_error::type_mismatch && badfamily.failed_pop_index==1u && family.pops==2uz,"v2 family before v1");
        stack modern{};modern.cells[0]={ref(t::abstract_heap_type::noexn),false};modern.cells[1]={ref(t::abstract_heap_type::noexn),false};modern.cells[2]=i32;modern.end=3uz;
        require(apply(modern,event,false,ref(t::abstract_heap_type::exn),0x69u,12uz).error==v::typed_stack_error::ok,
            "no rich context noexn subtype with maximal possible owned immediate extent");
        stack null{};null.cells[0]={ref(t::abstract_heap_type::i31),false};null.cells[1]={ref(t::abstract_heap_type::i31,false),false};null.cells[2]=i32;null.end=3uz;
        auto const nonnull{apply(null,event,false,ref(t::abstract_heap_type::i31,false),0x6fu)};
        require(nonnull.error==v::typed_stack_error::type_mismatch && nonnull.failed_pop_index==2u,"nonnull left still validated");
        stack empty{};require(apply(empty,event,true,ref(t::abstract_heap_type::exn),0x69u).error==v::typed_stack_error::ok &&
            empty.pops==0uz && empty.pushes==1uz && !event.left_concrete,"synthetic3 Bot is never consumed");
        stack part{};part.cells[0]=i32;part.end=1uz;
        require(apply(part,event,true,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::ok && part.pops==1uz && !event.left_concrete,
            "actual sole condition then synthetic values");
        stack baddead{};baddead.cells[0]={{t::value_kind::f64},false};baddead.cells[1]=i32;baddead.end=2uz;
        require(apply(baddead,event,true,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::type_mismatch,"unreachable concrete value not Bot");
        stack heap{};heap.cells[0]={{t::value_kind::reference,{t::heap_type::bottom_code},false},false};heap.end=1uz;
        require(apply(heap,event,true,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::type_mismatch,"heap Bot not numeric condition");
        stack unknown{};unknown.cells[0]={{},true};unknown.cells[1]=i64;unknown.cells[2]=i32;unknown.end=3uz;
        require(apply(unknown,event,true,{t::value_kind::i64},0x7eu).error==v::typed_stack_error::ok && event.left_concrete && event.left_unknown,
            "actual unknown left preserves original ring emission decision");
        stack extent{};extent.cells[0]=i32;extent.end=1uz;
        require(apply(extent,event,true,{t::value_kind::i32},0x7fu,13uz).error!=v::typed_stack_error::ok && extent.pops==0uz,"invalid metadata never authorizes decoder");
    }
}
int main() { check(); ::fast_io::print(::fast_io::out(),"TYPED_SELECT_DATA firstarity+Bot+context+left_metadata=1 authority=0\n"); }
