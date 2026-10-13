#include <uwvm2/validation/standard/wasm3/memory_page_semantics.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    struct operand { t::core_value_type type{}; bool unknown{}; };
    ::std::size_t checks{};
    void require(bool condition)
    {
        if(!condition) [[unlikely]]
        {
            ::fast_io::print(::fast_io::err(), "page semantics component failed at ", ::fast_io::mnp::dec(checks), "\n");
            ::fast_io::fast_terminate();
        }
        ++checks;
    }
}
int main()
{
    for(auto address : {v::storage_address_type::i32, v::storage_address_type::i64})
    {
        auto const numeric{address == v::storage_address_type::i64 ? t::value_kind::i64 : t::value_kind::i32};
        ::std::size_t used{};
        ::std::array<operand, 1uz> cells{operand{t::core_value_type{numeric}, false}};
        auto count = [&] { return cells.size() - used; };
        auto consume = [&]
        {
            // [owned one-cell suffix] end; common complete-arity preflight proves
            // [safe                 ] used < cells.size() before this access.
            require(used < cells.size());
            return cells[used++];
        };
        auto result{v::validate_memory_page_operand_sequence(address, false, false, count, consume)};
        require(result.error == v::typed_stack_error::ok && used == 0uz);
        result = v::validate_memory_page_operand_sequence(address, true, false, count, consume);
        require(result.error == v::typed_stack_error::ok && used == 1uz);
        used = 1uz;
        result = v::validate_memory_page_operand_sequence(address, true, false, count, consume);
        require(result.error == v::typed_stack_error::stack_underflow && used == 1uz);
        result = v::validate_memory_page_operand_sequence(address, true, true, count, consume);
        require(result.error == v::typed_stack_error::ok && used == 1uz);
        used = 0uz;
        cells[0] = {t::core_value_type{address == v::storage_address_type::i64 ? t::value_kind::i32 : t::value_kind::i64}, false};
        result = v::validate_memory_page_operand_sequence(address, true, false, count, consume);
        require(result.error == v::typed_stack_error::type_mismatch && used == 1uz);
        used = 0uz;
        cells[0] = {t::core_value_type{t::value_kind::f32}, true};
        result = v::validate_memory_page_operand_sequence(address, true, true, count, consume);
        require(result.error == v::typed_stack_error::ok && used == 1uz);
        used = 0uz;
        cells[0] = {t::core_value_type{t::value_kind::reference, {t::heap_type::bottom_code}, false}, false};
        result = v::validate_memory_page_operand_sequence(address, true, true, count, consume);
        require(result.error == v::typed_stack_error::type_mismatch && used == 1uz);
    }
    ::fast_io::print(::fast_io::out(), "page semantics component checks=", ::fast_io::mnp::dec(checks), "\n");
}
