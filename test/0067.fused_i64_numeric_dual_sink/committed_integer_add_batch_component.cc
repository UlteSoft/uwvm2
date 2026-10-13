// Compiler DATA component; actual ring/native qualification is the separate
// original-walker runtime fixture. This does not mint validation/publication rights.
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <fast_io.h>
#include <type_traits>
namespace t = ::uwvm2::validation::standard::wasm3;
namespace
{
    struct sink
    {
        static constexpr bool receives_fused_i32_operations{true};
        ::std::size_t providers{}, adds{}, opcodes{}, aborted{};
        bool wide{}, invalid{};
        constexpr void opcode(unsigned op, ::std::size_t offset) noexcept
        { ++opcodes; if(offset > 22u || (op != 0x20u && op != (wide ? 0x7cu : 0x6au))) { invalid = true; } }
        constexpr void provider(t::validated_i32_provider_event const& e) noexcept
        { ++providers; invalid |= wide || e.opcode != 0x20u || e.value >= 8u || e.source_bytes != 2u || e.control_depth != 1u; }
        constexpr void provider64(t::validated_i64_provider_event const& e) noexcept
        { ++providers; invalid |= !wide || e.opcode != 0x20u || e.value >= 8u || e.source_bytes != 2u || e.control_depth != 1u; }
        constexpr void numeric(t::validated_i32_numeric_event const& e) noexcept
        { ++adds; invalid |= wide || e.popped != 2u || e.pushed != 1u || e.pop_bytes != 8u || e.push_bytes != 4u; }
        constexpr void numeric64(t::validated_i64_numeric_event const& e) noexcept
        { ++adds; invalid |= !wide || e.popped != 2u || e.pushed != 1u || e.pop_bytes != 16u || e.push_bytes != 8u; }
        constexpr void abort_unsealed() noexcept { ++aborted; }
        constexpr void complete(::std::size_t) noexcept {}
    };
    constexpr bool valid(bool wide)
    {
        sink result{}; result.wide = wide;
        t::fused_i32_function_transaction<sink> transaction{&result};
        t::committed_integer_add_batch<true> batch{};
        batch.begin(wide,0u,0u,2u,1u);
        for(::std::size_t i{1u}; i != 8u; ++i) { batch.record_provider(static_cast<::std::uint_least32_t>(i),i*2u,2u); }
        for(::std::size_t i{}; i != 7u; ++i)
        { batch.record_add(16u+i); if(result.providers != 0u || result.adds != 0u) { return false; } }
        if(!batch.complete()) { return false; }
        batch.publish_after_ring_commit(transaction); transaction.complete(23u);
        return !result.invalid && result.providers == 8u && result.adds == 7u && result.opcodes == 14u;
    }
    constexpr bool invalid()
    {
        sink result{}; t::fused_i32_function_transaction<sink> transaction{&result};
        t::committed_integer_add_batch<true> batch{};
        batch.begin(false,0u,0u,2u,1u); batch.record_provider(1u,3u,2u); // gap: no publish
        batch.publish_after_ring_commit(transaction); transaction.complete(5u);
        t::committed_integer_add_batch<true> incomplete{};
        incomplete.begin(false,0u,0u,2u,1u); incomplete.record_add(2u); // missing seven gets
        incomplete.publish_after_ring_commit(transaction);
        return !batch.complete() && !incomplete.complete() && result.providers == 0u && result.adds == 0u;
    }
    constexpr bool preload(bool wide)
    {
        sink result{}; result.wide = wide;
        t::fused_i32_function_transaction<sink> transaction{&result};
        t::committed_integer_add_batch<true> batch{};
        batch.begin(wide,0u,0u,2u,1u); batch.record_provider(1u,2u,2u); batch.record_provider(2u,4u,2u);
        if(!batch.complete_providers(3u) || batch.complete_providers(2u) || result.providers != 0u) { return false; }
        batch.publish_providers_after_ring_commit(transaction,3u); transaction.complete(6u);
        return !result.invalid && result.providers == 3u && result.adds == 0u && result.opcodes == 2u;
    }
    static_assert(::std::is_empty_v<t::committed_integer_add_batch<false>>);
    static_assert(valid(false) && valid(true) && invalid() && preload(false) && preload(true));
}
int main()
{
    if(!valid(false) || !valid(true) || !invalid() || !preload(false) || !preload(true)) { ::fast_io::fast_terminate(); }
    ::fast_io::print(::fast_io::out(), "COMMITTED_INTEGER_ADD_BATCH DATA=checked native=unqualified\n");
}
