#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <uwvm2/validation/standard/wasm3/scalar_memory_semantics.h>
#include <uwvm2/validation/standard/wasm3/reference_validation.h>
#include <fast_io.h>
namespace v=uwvm2::validation::standard::wasm3;
namespace t=uwvm2::parser::wasm::standard::wasm3::type;
constexpr bool controls()
{
    std::array<v::core3_operand,3> values{};
    std::size_t size{},consumed{};
    constexpr std::size_t base=1uz;
    // A real current-frame suffix excludes the preserved older prefix cell.
    auto const count{[&]() constexpr noexcept { return size-base; }};
    auto const consume{[&]() constexpr noexcept
    {
        // Shared preflight proves size > base and size <= values.size().
        // The decrement copies one live owned top before retirement.
        ++consumed;return values[--size];
    }};
    auto const bottom{v::core3_operand{{t::value_kind::reference,{t::heap_type::bottom_code},false},false}};
    std::size_t valid_rows{};
    for(auto width : {v::storage_address_type::i32,v::storage_address_type::i64})
    {
        v::typed_memory_argument const decoded{.address_type=width};
        t::core_value_type const address{width==v::storage_address_type::i64?t::value_kind::i64:t::value_kind::i32};
        for(auto kind : {t::value_kind::i32,t::value_kind::i64,t::value_kind::f32,t::value_kind::f64})
        {
            t::core_value_type const scalar{kind};
            values[0]=bottom;values[1]={address,false};size=2uz;consumed=0uz;
            auto result=v::validate_scalar_memory_operand_sequence(decoded,false,scalar,false,count,consume);
            if(result.error!=v::typed_stack_error::ok || size!=base || consumed!=1uz) { return false; }
            ++valid_rows;
            values[1]={address,false};values[2]={scalar,false};size=3uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,false,count,consume);
            if(result.error!=v::typed_stack_error::ok || size!=base || consumed!=2uz) { return false; }
            ++valid_rows;
            values[1]={{t::value_kind::f32},false};size=2uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,false,count,consume);
            if(result.error!=v::typed_stack_error::stack_underflow || size!=2uz || consumed) { return false; }
            // A known reference-only Bot is never a numeric address/value.
            values[1]=bottom;size=2uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,false,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::type_mismatch || result.failed_pop_index!=0u) { return false; }
            values[1]={address,false};values[2]=bottom;size=3uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::type_mismatch || result.failed_pop_index!=0u || consumed!=1uz) { return false; }
            values[1]=bottom;values[2]={scalar,false};size=3uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::type_mismatch || result.failed_pop_index!=1u || consumed!=2uz) { return false; }
            values[1]=bottom;values[2]=bottom;size=3uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,false,count,consume);
            if(result.error!=v::typed_stack_error::type_mismatch || result.failed_pop_index!=0u || consumed!=1uz) { return false; }
            // Value Bot and the absent synthetic polymorphic remainder retain
            // their separate meanings; neither reads the older prefix cell.
            values[1]={{},true};values[2]={{},true};size=3uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::ok || size!=base || consumed!=2uz) { return false; }
            size=base;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::ok || size!=base || consumed) { return false; }
            values[1]={scalar,false};size=2uz;consumed=0uz;
            result=v::validate_scalar_memory_operand_sequence(decoded,true,scalar,true,count,consume);
            if(result.error!=v::typed_stack_error::ok || size!=base || consumed!=1uz) { return false; }
            if(values[0].type.kind!=t::value_kind::reference || values[0].unknown) { return false; }
        }
    }
    return valid_rows==16uz;
}
static_assert(controls());
int main()
{ fast_io::println("{\"valid_scalar_sequence_rows\":16,\"controls\":true,\"VM_oracle_verified\":false}"); }
