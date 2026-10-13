#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <uwvm2/validation/standard/wasm3/bulk_memory_semantics.h>
#include <uwvm2/validation/standard/wasm3/reference_validation.h>
#include <fast_io.h>

namespace v = uwvm2::validation::standard::wasm3;
namespace t = uwvm2::parser::wasm::standard::wasm3::type;
constexpr auto i32=v::storage_address_type::i32;
constexpr auto i64=v::storage_address_type::i64;

constexpr bool events()
{
    std::size_t rows{};
    for(auto destination : {i32,i64})
    {
        for(auto kind : {v::bulk_memory_instruction_kind::init,v::bulk_memory_instruction_kind::fill})
        {
            v::decoded_bulk_memory_instruction const decoded{.kind=kind,.data_index=3u,.destination_memory_index=1u,
                .destination_address=destination};
            v::validated_bulk_memory_event event{};
            if(!v::complete_bulk_memory_event(event,decoded,7uz,4uz,2uz,true)) { return false; }
            auto const expected{destination==i64 ? (kind==v::bulk_memory_instruction_kind::fill ? 20u:16u) : 12u};
            if(event.popped!=3u || event.pop_bytes!=expected || event.source_offset!=7uz ||
               event.source_bytes!=4uz || event.control_depth!=2uz || !event.reachable) { return false; }
            ++rows;
        }
        for(auto source : {i32,i64})
        {
            v::decoded_bulk_memory_instruction const decoded{.kind=v::bulk_memory_instruction_kind::copy,
                .destination_memory_index=1u,.source_memory_index=0u,.destination_address=destination,.source_address=source};
            v::validated_bulk_memory_event event{};
            if(!v::complete_bulk_memory_event(event,decoded,7uz,4uz,2uz,true)) { return false; }
            auto const length{destination==i64&&source==i64?i64:i32};
            auto const expected{(destination==i64?8u:4u)+(source==i64?8u:4u)+(length==i64?8u:4u)};
            if(event.pop_bytes!=expected || v::bulk_memory_expected_operand_address(decoded,0u)!=length ||
               v::bulk_memory_expected_operand_address(decoded,1u)!=source ||
               v::bulk_memory_expected_operand_address(decoded,2u)!=destination) { return false; }
            ++rows;
        }
    }
    v::decoded_bulk_memory_instruction const drop{.kind=v::bulk_memory_instruction_kind::data_drop,.data_index=3u};
    v::validated_bulk_memory_event event{};
    if(!v::complete_bulk_memory_event(event,drop,7uz,3uz,1uz,true) || event.popped || event.pop_bytes) { return false; }
    if(v::complete_bulk_memory_event(event,drop,7uz,0uz,1uz,true)) { return false; }
    ++rows;
    return rows==9uz;
}
constexpr bool sequences()
{
    std::array<v::core3_operand,3> values{};
    std::size_t size{},consumed{};
    auto const count{[&]() constexpr noexcept { return size; }};
    auto const consume{[&]() constexpr noexcept
    {
        // Common arity/single-pop preflight proves size > 0 and size was <=3.
        // The decrement therefore addresses one live owned array cell.
        ++consumed;return values[--size];
    }};
    v::decoded_bulk_memory_instruction const copy{.kind=v::bulk_memory_instruction_kind::copy,
        .destination_address=i64,.source_address=i32};
    values={v::core3_operand{{t::value_kind::i64},false},v::core3_operand{{t::value_kind::i32},false},v::core3_operand{{t::value_kind::i32},false}};
    size=3uz;
    auto result=v::validate_bulk_memory_operand_sequence(copy,false,count,consume);
    if(result.error!=v::typed_stack_error::ok || size || consumed!=3uz) { return false; }
    values[0]={{t::value_kind::f32},false};size=1uz;consumed=0uz;
    result=v::validate_bulk_memory_operand_sequence(copy,false,count,consume);
    if(result.error!=v::typed_stack_error::stack_underflow || size!=1uz || consumed) { return false; }
    values={v::core3_operand{{t::value_kind::i64},false},v::core3_operand{{t::value_kind::i32},false},v::core3_operand{{t::value_kind::i64},false}};
    size=3uz;
    result=v::validate_bulk_memory_operand_sequence(copy,false,count,consume);
    if(result.error!=v::typed_stack_error::type_mismatch || result.failed_pop_index!=0u) { return false; }
    values[0]={{t::value_kind::reference,{t::heap_type::bottom_code},false},false};size=1uz;
    result=v::validate_bulk_memory_operand_sequence(copy,true,count,consume);
    if(result.error!=v::typed_stack_error::type_mismatch) { return false; }
    values[0]={{},true};size=1uz;
    result=v::validate_bulk_memory_operand_sequence(copy,true,count,consume);
    if(result.error!=v::typed_stack_error::ok || size) { return false; }
    consumed=0uz;
    result=v::validate_bulk_memory_operand_sequence(copy,true,count,consume);
    if(result.error!=v::typed_stack_error::ok || consumed) { return false; }
    v::decoded_bulk_memory_instruction const drop{.kind=v::bulk_memory_instruction_kind::data_drop};
    size=1uz;
    result=v::validate_bulk_memory_operand_sequence(drop,false,count,consume);
    return result.error==v::typed_stack_error::ok && size==1uz && consumed==0uz;
}
static_assert(events());
static_assert(sequences());
int main()
{
    fast_io::println("{\"bulk_event_rows\":9,\"common_sequence_controls\":true,\"VM_oracle_verified\":false}");
}
