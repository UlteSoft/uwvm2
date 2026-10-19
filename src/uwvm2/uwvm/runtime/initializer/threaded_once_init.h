// Audited finite non-PIC wasm32 passive-memory initializer grammar.
// This validates an actual body; it NEVER executes or replaces that body.
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace uwvm2::uwvm::runtime::initializer::threaded_once_init_details
{
    struct contract
    {
        ::std::size_t function_index{};
        ::std::uint32_t flag_offset{};
    };
    class reader final
    {
        ::std::byte const* current_;
        ::std::byte const* end_;
    public:
        reader(::std::byte const* begin, ::std::byte const* end) noexcept : current_{begin}, end_{end} {}
        [[nodiscard]] unsigned peek() const noexcept
        { return current_ == end_ ? 256u : ::std::to_integer<unsigned>(*current_); }
        [[nodiscard]] bool byte(unsigned expected) noexcept
        { if(peek() != expected) { return false; } ++current_; return true; }
        [[nodiscard]] bool finished() const noexcept { return current_ == end_; }
        [[nodiscard]] bool u32(::std::uint32_t& value) noexcept
        {
            value = 0u;
            for(unsigned shift{}; shift <= 28u; shift += 7u)
            {
                auto const next{peek()}; if(next == 256u) { return false; } ++current_;
                if(shift == 28u && (next & 0xf0u) != 0u) { return false; }
                value |= (next & 127u) << shift;
                if((next & 128u) == 0u) { return true; }
            }
            return false;
        }
        [[nodiscard]] bool signed32_bits(::std::uint32_t& value) noexcept
        {
            value = 0u;
            for(unsigned shift{}; shift <= 28u; shift += 7u)
            {
                auto const next{peek()}; if(next == 256u) { return false; } ++current_;
                if(shift == 28u && (next & 0x78u) != 0u && (next & 0x78u) != 0x78u) { return false; }
                value |= (next & 127u) << shift;
                if((next & 128u) == 0u)
                {
                    if(shift < 28u && (next & 64u) != 0u)
                    { value |= ~::std::uint32_t{} << (shift + 7u); }
                    return true;
                }
            }
            return false;
        }
        [[nodiscard]] bool constant(::std::uint32_t& value) noexcept
        { return byte(0x41u) && signed32_bits(value); }
        [[nodiscard]] bool constant_is(::std::uint32_t expected) noexcept
        { ::std::uint32_t actual{}; return constant(actual) && actual == expected; }
        [[nodiscard]] bool atomic(unsigned opcode) noexcept
        { return byte(0xfeu) && byte(opcode) && byte(2u) && byte(0u); }
    };

    template<typename Module, typename Memory>
    [[nodiscard]] ::std::optional<contract> validate(Module const& module, Memory const& memory,
        ::std::size_t function_index, bool require_already_initialized,
        ::std::byte const* source_begin, ::std::byte const* source_end, ::std::size_t maximum_body_bytes) noexcept
    {
        // Fixed stack metadata, not an unchecked reserve or recursive parser.
        constexpr ::std::size_t maximum_segments{64uz};
        std::array<bool, maximum_segments> copied{}, tls{}, dropped{};
        auto const imports{module.imported_function_vec_storage.size()};
        if(function_index < imports || function_index - imports >= module.local_defined_function_vec_storage.size() ||
            module.imported_memory_vec_storage.size() != 1uz || !module.local_defined_memory_vec_storage.empty() ||
            module.local_defined_data_vec_storage.size() > maximum_segments || memory.memory_length_p == nullptr)
        { return {}; }
        auto const& function{module.local_defined_function_vec_storage.index_unchecked(function_index - imports)};
        auto const* type{function.function_type_ptr};
        if(type == nullptr || type->parameter.begin != type->parameter.end || type->result.begin != type->result.end ||
            function.wasm_code_ptr == nullptr) { return {}; }
        auto const& body{function.wasm_code_ptr->body};
        auto const* begin{reinterpret_cast<::std::byte const*>(body.code_begin)};
        auto const* end{reinterpret_cast<::std::byte const*>(body.code_end)};
        auto const low{reinterpret_cast<::std::uintptr_t>(source_begin)};
        auto const high{reinterpret_cast<::std::uintptr_t>(source_end)};
        auto const first{reinterpret_cast<::std::uintptr_t>(begin)};
        auto const last{reinterpret_cast<::std::uintptr_t>(end)};
        if(low == 0u || low >= high || first < low || first >= last || last > high ||
            last - first > maximum_body_bytes) { return {}; }
        reader r{begin, end};
        // No locals, three empty blocks, flag compare/exchange and exact branch
        // destinations. An arbitrary syntactically valid body cannot qualify.
        if(!r.byte(0u) || !r.byte(2u) || !r.byte(0x40u) || !r.byte(2u) || !r.byte(0x40u) ||
            !r.byte(2u) || !r.byte(0x40u)) { return {}; }
        ::std::uint32_t flag{};
        if(!r.constant(flag) || !r.constant_is(0u) || !r.constant_is(1u) || !r.atomic(0x48u) ||
            !r.byte(0x0eu) || !r.byte(2u) || !r.byte(0u) || !r.byte(1u) || !r.byte(2u) || !r.byte(0x0bu)) { return {}; }
        auto const length{memory.memory_length_p->load(::std::memory_order_seq_cst)};
        if((flag & 3u) != 0u || flag > length || 4uz > length - flag) { return {}; }
        auto const* flag_address{reinterpret_cast<::std::uint32_t const*>(memory.memory_begin + flag)};
#if defined(__GNUC__) || defined(__clang__)
        auto const state{__atomic_load_n(flag_address, __ATOMIC_SEQ_CST)};
        if((require_already_initialized && state != 2u) || (!require_already_initialized && state != 0u && state != 2u)) { return {}; }
#else
        return {};
#endif
        // Guarded memory initializers. No calls, loops, arbitrary stores or
        // foreign globals can be hidden inside this finite grammar.
        constexpr ::std::size_t maximum_initializers{128uz};
        ::std::size_t initializer_count{};
        for(;;)
        {
            auto final_store{r};
            if(final_store.constant_is(flag) && final_store.constant_is(2u) && final_store.atomic(0x17u))
            { r = final_store; break; }
            if(initializer_count++ == maximum_initializers) { return {}; }
            ::std::uint32_t destination{}, size{};
            if(!r.constant(destination)) { return {}; }
            bool tls_copy{};
            auto possible_tls{r}; ::std::uint32_t duplicate{}, global{};
            if(possible_tls.constant(duplicate) && duplicate == destination && possible_tls.byte(0x24u))
            {
                if(!possible_tls.u32(global)) { return {}; }
                auto const imported_globals{module.imported_global_vec_storage.size()};
                if(global < imported_globals || global - imported_globals >= module.local_defined_global_vec_storage.size()) { return {}; }
                auto const& slot{module.local_defined_global_vec_storage.index_unchecked(global - imported_globals)};
                if(!slot.global.is_mutable || slot.global.kind != ::uwvm2::object::global::global_type::wasm_i32) { return {}; }
                r = possible_tls; tls_copy = true;
            }
            if(!r.constant_is(0u) || !r.constant(size) || !r.byte(0xfcu)) { return {}; }
            if(destination > length || size > length - destination ||
                (size != 0u && destination < static_cast<::std::uint_least64_t>(flag) + 4u && flag < static_cast<::std::uint_least64_t>(destination) + size))
            { return {}; }
            if(r.byte(8u))
            {
                ::std::uint32_t index{}, memory_index{};
                if(!r.u32(index) || !r.u32(memory_index) || memory_index != 0u ||
                    index >= module.local_defined_data_vec_storage.size() || copied[index]) { return {}; }
                auto const* declaration{module.local_defined_data_vec_storage.index_unchecked(index).data_type_ptr};
                if(declaration == nullptr || declaration->storage.segment.active) { return {}; }
                auto const& bytes{declaration->storage.segment.byte};
                auto const extent{bytes.begin == bytes.end ? 0uz : static_cast<::std::size_t>(bytes.end - bytes.begin)};
                if(extent != size) { return {}; }
                copied[index] = true; tls[index] = tls_copy;
            }
            else if(r.byte(11u))
            {
                if(tls_copy || !r.byte(0u)) { return {}; }
            }
            else { return {}; }
        }
        if(!r.constant_is(flag) || !r.constant_is(0xffffffffu) || !r.atomic(0u) || !r.byte(0x1au) ||
            !r.byte(0x0cu) || !r.byte(1u) || !r.byte(0x0bu) ||
            !r.constant_is(flag) || !r.constant_is(1u) || !r.byte(0x42u) || !r.byte(0x7fu) ||
            !r.atomic(1u) || !r.byte(0x1au) || !r.byte(0x0bu)) { return {}; }
        while(r.peek() == 0xfcu)
        {
            ::std::uint32_t index{};
            if(!r.byte(0xfcu) || !r.byte(9u) || !r.u32(index) || index >= module.local_defined_data_vec_storage.size() ||
                !copied[index] || tls[index] || dropped[index]) { return {}; }
            dropped[index] = true;
        }
        if(!r.byte(0x0bu) || !r.finished()) { return {}; }
        for(::std::size_t i{}; i != module.local_defined_data_vec_storage.size(); ++i)
        { if(copied[i] && !tls[i] && !dropped[i]) { return {}; } }
        return contract{function_index, flag};
    }
}
