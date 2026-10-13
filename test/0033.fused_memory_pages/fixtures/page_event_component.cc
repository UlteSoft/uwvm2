#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <uwvm2/validation/standard/wasm3/memory_page_event.h>
#include <fast_io.h>

using namespace uwvm2::validation::standard::wasm3;
constexpr bool page_event_representation()
{
    for(unsigned opcode : {0x3fu, 0x40u})
    {
        for(auto address : {storage_address_type::i32, storage_address_type::i64})
        {
            validated_memory_page_event event{};
            if(!complete_memory_page_event(event, opcode, 1u, address, 7uz, 2uz, 3uz, true)) { return false; }
            auto const bytes{address == storage_address_type::i64 ? 8u : 4u};
            bool const grow{opcode == 0x40u};
            if(event.opcode != opcode || event.memory_index != 1u || event.address_type != address ||
               event.grow != grow || event.popped != (grow ? 1u : 0u) || event.pushed != 1u ||
               event.pop_bytes != (grow ? bytes : 0u) || event.push_bytes != bytes ||
               event.source_offset != 7uz || event.source_bytes != 2uz || event.control_depth != 3uz || !event.reachable) { return false; }
        }
    }
    validated_memory_page_event invalid{};
    if(complete_memory_page_event(invalid, 0x41u, 0u, storage_address_type::i64, 0uz, 2uz, 1uz, true)) { return false; }
    if(complete_memory_page_event(invalid, 0x40u, 0u, storage_address_type::i64, 0uz, 0uz, 1uz, true)) { return false; }
    if(complete_memory_page_event(invalid, 0x40u, 0u, static_cast<storage_address_type>(100u), 0uz, 2uz, 1uz, true)) { return false; }
    return true;
}
static_assert(page_event_representation());
int main()
{
    fast_io::println("{\"page_event_representation\":true,\"rows\":4,\"native_lowering_verified\":false}");
}
