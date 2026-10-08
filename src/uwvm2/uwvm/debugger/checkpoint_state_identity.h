// Bounded canonical checkpoint DATA integrity; never a capture/restore issuer.
#pragma once
#ifndef UWVM_MODULE
# include <algorithm>
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <vector>
# include <fast_io.h>
# include <fast_io_crypto.h>
# include "checkpoint_binding.h"
# include "checkpoint_codec.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint::state_identity
{
    enum class status : unsigned char
    {
        derived_data, invalid_graph, limit_exceeded, inconsistent_function,
        module_count_mismatch, function_closure_mismatch, event_prefix_mismatch,
        allocation_failed
    };
    struct result
    {
        status observation{status::invalid_graph};
        error graph_error{error::none};
        ::std::vector<binding::sha256> functions{};
        binding::sha256 events{};
        [[nodiscard]] constexpr bool grants_restore_authority() const noexcept { return false; }
    };
    namespace details
    {
        class canonical_hash
        {
            ::fast_io::sha256_context hash_{};
        public:
            // A synchronous native-owned extent, never an arbitrary request
            // pointer. derive() preflights the ENTIRE graph/aggregate quota.
            void bytes(::std::span<::std::byte const> value) noexcept
            {
                if(value.empty()) { return; }
                // [same actual host owner: data...bounded size] exclusive_end
                // [safe] graph/array extent<=PTRDIFF_MAX BEFORE first+size.
                auto const* first{value.data()}; hash_.update(first, first + value.size());
            }
            template<unsigned Bits, typename T> void put(T value) noexcept
            {
                static_assert(Bits == 8u || Bits == 16u || Bits == 32u || Bits == 64u);
                ::std::array<unsigned char, Bits / 8u> scratch{};
                // [complete fixed Bits/8 owned scratch] exclusive_end
                // [safe] fixed capacity BEFORE data()+size()/fast_io cursor.
                ::fast_io::basic_obuffer_view<unsigned char> out{scratch.data(), scratch.data() + scratch.size()};
                ::fast_io::print(out, ::fast_io::mnp::le_put<Bits>(value));
                bytes({reinterpret_cast<::std::byte const*>(scratch.data()), scratch.size()});
            }
            [[nodiscard]] binding::sha256 finish() noexcept
            { hash_.do_final(); binding::sha256 out{}; hash_.digest_to_byte_ptr(out.data()); return out; }
        };
        inline void hash_value(canonical_hash& out, value const& item) noexcept
        {
            // Exactly the unchanged format5 canonical 48-byte typed value; no object
            // representation, padding, pointer, native endian or raw register.
            out.put<8>(static_cast<::std::uint8_t>(item.type.kind));
            out.put<8>(static_cast<::std::uint8_t>(item.type.heap));
            out.put<8>(static_cast<::std::uint8_t>(item.type.nullable));
            out.put<8>(static_cast<::std::uint8_t>(item.reference));
            out.put<32>(::std::uint32_t{item.initialized ? 0u : 1u});
            out.put<64>(item.type.type_module); out.put<32>(item.type.type_index); out.put<32>(::std::uint32_t{});
            out.put<64>(item.target); out.put<64>(item.low_bits); out.put<64>(item.high_bits);
        }
        inline void hash_event(canonical_hash& out, object const& event) noexcept
        {
            // Complete canonical event object. All semantic counts, capability
            // IDs, typed arguments/results, error/effect and payload enter it.
            out.put<16>(static_cast<::std::uint16_t>(event.kind)); out.put<16>(event.flags); out.put<32>(::std::uint32_t{});
            for(auto word : event.words) { out.put<64>(word); }
            out.put<64>(static_cast<::std::uint64_t>(event.links.size()));
            out.put<64>(static_cast<::std::uint64_t>(event.values.size()));
            out.put<64>(static_cast<::std::uint64_t>(event.bytes.size()));
            for(auto link : event.links) { out.put<64>(link); }
            for(auto const& item : event.values) { hash_value(out, item); }
            out.bytes(event.bytes);
        }
        struct function_key { ::std::size_t module{}, instance{}, object_index{}; };
        [[nodiscard]] inline bool same_function(state const& saved, object const& a, object const& b) noexcept
        {
            if(a.flags != b.flags || a.words != b.words || a.bytes != b.bytes) { return false; }
            if(a.flags == 0u) { return true; }
            // Graph validation already proves links[1] is a dense resource ID.
            // Match host CODE adapter/version/flags, not mutable resource state
            // or file-local handle identity. The actual runtime issuer must
            // independently resolve that adapter; equal DATA is no capability.
            auto const& x{saved.objects[static_cast<::std::size_t>(a.links[1u] - 1u)]};
            auto const& y{saved.objects[static_cast<::std::size_t>(b.links[1u] - 1u)]};
            return x.flags == y.flags && x.words[0u] == y.words[0u] && x.words[1u] == y.words[1u] &&
                (x.flags != 3u || (x.words[2u] == y.words[2u] && x.bytes == y.bytes));
        }
    }
    // Hash ONLY an already caller-owned, synchronously stable native graph.
    // This validates file DATA, not actual executed capture, Wasm declarations,
    // source/provider ownership, stop admission or executable continuations.
    [[nodiscard]] inline result derive(state const& saved, limits const& cap = {}, binding::limits const& identity_cap = {}) noexcept
    {
        using hash = details::canonical_hash;
        auto const count{saved.objects.size()};
        if(count > cap.max_objects || count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
            sizeof(::std::size_t) || count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
            sizeof(details::function_key)) { return {status::limit_exceeded}; }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
        try
#endif
        {
            auto const valid{validate_graph(saved, cap)};
            if(valid != error::none) { return {status::invalid_graph, valid}; }
            ::std::uint64_t measured{};
            if(!codec_details::measure(saved, cap, measured) || measured >
                static_cast<::std::uint64_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) - header_bytes - footer_bytes)
            { return {status::limit_exceeded}; }
            // Every field/byte borrow below is now graph-valid and aggregate
            // bounded BEFORE allocation, indexing, reinterpretation or advance.
            auto constexpr absent{(::std::numeric_limits<::std::size_t>::max)()};
            ::std::vector<::std::size_t> module_ordinals(count, absent), module_objects{};
            ::std::vector<::std::size_t> instance_ordinals(count, absent), instances_per_module{};
            ::std::vector<details::function_key> functions{};
            ::std::size_t event_count{};
            for(::std::size_t i{}; i != count; ++i)
            {
                // [actual saved object vector0 ... i ... N] exclusive_end
                // [safe] i<N<=bounded count BEFORE this synchronous borrow.
                auto const& item{saved.objects[i]};
                if(item.kind == object_kind::module)
                {
                    if(module_objects.size() >= identity_cap.modules) { return {status::limit_exceeded}; }
                    module_ordinals[i] = module_objects.size(); module_objects.push_back(i);
                }
                else if(item.kind == object_kind::event) { ++event_count; } // event_count<=i+1<=N before advance
            }
            if(module_objects.empty()) { return {status::module_count_mismatch}; }
            instances_per_module.resize(module_objects.size());
            for(::std::size_t i{}; i != count; ++i)
            {
                auto const& item{saved.objects[i]}; // i<N BEFORE the actual instance borrow
                if(item.kind != object_kind::instance) { continue; }
                // [validated instance.links[0] -> module ID1..N] end
                // [safe] graph proved nonzero ID/kind/extent BEFORE -1/index.
                auto const ordinal{module_ordinals[static_cast<::std::size_t>(item.links[0u] - 1u)]};
                if(ordinal == absent || ordinal >= instances_per_module.size()) { return {status::module_count_mismatch}; }
                instance_ordinals[i] = instances_per_module[ordinal];
                ++instances_per_module[ordinal]; // total instances<=N bounds BEFORE increment
            }
            for(::std::size_t i{}; i != count; ++i)
            {
                auto const& item{saved.objects[i]}; // i<N checked BEFORE indexing
                if(item.kind != object_kind::function) { continue; }
                // [validated links0 -> instance -> validated links0 -> module]
                // [safe] validate_graph proved BOTH nonzero IDs<=N and kinds
                // BEFORE -1 and all four accesses. No native endpoint tokens.
                auto const& instance{saved.objects[static_cast<::std::size_t>(item.links[0u] - 1u)]};
                auto const module_index{static_cast<::std::size_t>(instance.links[0u] - 1u)};
                auto const ordinal{module_ordinals[module_index]};
                if(ordinal == absent || ordinal >= module_objects.size()) { return {status::module_count_mismatch}; }
                // Format3 original bodies use their exact module image; any
                // replacement records exact local-declaration/body bytes.
                if(item.flags == 0u && ((item.words[1u] == 1u) != item.bytes.empty()))
                { return {status::inconsistent_function}; }
                auto const instance_index{static_cast<::std::size_t>(item.links[0u] - 1u)};
                auto const instance_ordinal{instance_ordinals[instance_index]}; // validated instance index<N BEFORE access
                if(instance_ordinal == absent || instance_ordinal >= instances_per_module[ordinal]) { return {status::module_count_mismatch}; }
                functions.push_back({ordinal, instance_ordinal, i});
            }
            ::std::sort(functions.begin(), functions.end(), [&](auto const& a, auto const& b) noexcept
            {
                if(a.module != b.module) { return a.module < b.module; }
                if(a.instance != b.instance) { return a.instance < b.instance; }
                // [keys minted ONLY above from 0<=i<N] end
                // [safe] both original indices bounded BEFORE comparator reads.
                auto const x{saved.objects[a.object_index].words[0u]}, y{saved.objects[b.object_index].words[0u]};
                return x != y ? x < y : a.object_index < b.object_index;
            });
            result out{}; out.functions.reserve(module_objects.size());
            ::std::size_t cursor{};
            constexpr ::std::array<::std::byte, 8u> function_domain{::std::byte{'U'},::std::byte{'W'},::std::byte{'C'},::std::byte{'P'},
                ::std::byte{'F'},::std::byte{'G'},::std::byte{'0'},::std::byte{'2'}};
            for(::std::size_t ordinal{}; ordinal != module_objects.size(); ++ordinal)
            {
                auto const begin{cursor}; ::std::size_t unique{};
                object const* previous{}; ::std::size_t previous_instance{absent};
                while(cursor < functions.size() && functions[cursor].module == ordinal)
                {
                    // [same minted sorted keys, cursor<K] end
                    // [safe] key and original object-index bounds BEFORE read.
                    auto const& item{saved.objects[functions[cursor].object_index]};
                    if(previous && previous_instance == functions[cursor].instance && previous->words[0u] == item.words[0u])
                    { if(!details::same_function(saved, *previous, item)) { return {status::inconsistent_function}; } }
                    else { ++unique; previous = ::std::addressof(item); previous_instance = functions[cursor].instance; }
                    ++cursor; // cursor<K before advance; unique<=cursor<=K
                }
                hash code{}; code.bytes(function_domain); code.put<64>(static_cast<::std::uint64_t>(ordinal));
                // [module_objects ordinal<M, minted indices<N] end
                // [safe] BOTH bounds established BEFORE source-byte borrow.
                auto const& module{saved.objects[module_objects[ordinal]]};
                auto const original{binding::hash_bytes(module.bytes)};
                if(original.result != binding::status::identical_data) { return {status::limit_exceeded}; }
                code.bytes(original.digest);
                code.put<64>(static_cast<::std::uint64_t>(instances_per_module[ordinal]));
                code.put<64>(static_cast<::std::uint64_t>(unique)); previous = nullptr; previous_instance = absent;
                for(auto i{begin}; i != cursor; ++i)
                {
                    auto const& item{saved.objects[functions[i].object_index]}; // begin<=i<cursor<=K, all minted indices<N
                    if(previous && previous_instance == functions[i].instance && previous->words[0u] == item.words[0u]) { continue; }
                    code.put<64>(static_cast<::std::uint64_t>(functions[i].instance)); code.put<16>(item.flags); code.put<64>(item.words[0u]); code.put<64>(item.words[1u]); code.put<64>(item.words[2u]);
                    code.put<64>(static_cast<::std::uint64_t>(item.bytes.size())); code.bytes(item.bytes);
                    if(item.flags != 0u)
                    {
                        // [validated host-function links[1] -> resource ID<=N]
                        // [safe] graph proved kind/nonzero/extent BEFORE -1/index.
                        auto const& adapter{saved.objects[static_cast<::std::size_t>(item.links[1u] - 1u)]};
                        code.put<16>(adapter.flags); code.put<64>(adapter.words[0u]); code.put<64>(adapter.words[1u]);
                        if(adapter.flags == 3u)
                        {
                            // Exact binding-only code ABI/interface identity,
                            // not mutable external state or lifetime authority.
                            code.put<64>(adapter.words[2u]); code.put<64>(static_cast<::std::uint64_t>(adapter.bytes.size()));
                            code.bytes(adapter.bytes);
                        }
                    }
                    previous = ::std::addressof(item); previous_instance = functions[i].instance; // genuine synchronous object borrow, never emitted
                }
                out.functions.push_back(code.finish());
            }
            if(cursor != functions.size()) { return {status::module_count_mismatch}; }
            constexpr ::std::array<::std::byte, 8u> event_domain{::std::byte{'U'},::std::byte{'W'},::std::byte{'C'},::std::byte{'P'},
                ::std::byte{'E'},::std::byte{'V'},::std::byte{'0'},::std::byte{'2'}};
            hash events{}; events.bytes(event_domain); events.bytes(saved.recording_id);
            events.put<64>(saved.replay_event_cursor); events.put<64>(static_cast<::std::uint64_t>(event_count));
            // The full saved replay log PLUS its cursor defines this prefix.
            // Also include future resident events: repairing the outer SHA must
            // not permit changing the data consumed by later replay.
            for(auto const& item : saved.objects)
            { if(item.kind == object_kind::event) { details::hash_event(events, item); } }
            out.events = events.finish(); out.observation = status::derived_data; return out;
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
        catch(...) { return {status::allocation_failed}; }
#endif
    }
    [[nodiscard]] inline result compare(state const& saved, binding::manifest const& identity,
        limits const& cap = {}, binding::limits const& identity_cap = {}) noexcept
    {
        auto out{derive(saved, cap, identity_cap)};
        if(out.observation != status::derived_data) { return out; }
        if(out.functions.size() != identity.modules.size()) { out.observation = status::module_count_mismatch; return out; }
        for(::std::size_t i{}; i != out.functions.size(); ++i)
        {
            // [equal bounded owned vectors, i<N] end
            // [safe] equality and i<N BEFORE both DATA comparisons.
            if(out.functions[i] != identity.modules[i].effective_function_generation_closure)
            { out.observation = status::function_closure_mismatch; return out; }
        }
        if(out.events != identity.deterministic_event_prefix) { out.observation = status::event_prefix_mismatch; }
        return out; // DATA equality still grants no stop/native/restore capability
    }
}
