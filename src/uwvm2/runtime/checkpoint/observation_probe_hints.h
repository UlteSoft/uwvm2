#pragma once
// Process-lifetime dispatch hints only. No native address, register, capture,
// root or continuation authority is created by a byte in this table.
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <mutex>
# include <span>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    namespace observation_probe_details
    {
        struct dispatch_storage
        {
            // Each generated gate reads ONE real atomic object. Do not alias
            // packed words with byte atomics: mixed-size atomic accesses do
            // not provide the C++ publication contract used by the producer.
            ::std::array<::std::atomic<::std::uint8_t>,4096u> bytes{};
            ::std::atomic<::std::uint64_t> full_token{1u};
            dispatch_storage() noexcept
            { for(auto& byte : bytes) { byte.store(1u,::std::memory_order_relaxed); } }
        };
    }
    class observation_probe_hints
    {
        inline static observation_probe_details::dispatch_storage storage_{};
        inline static ::std::mutex administration_{};
        inline static void const* configured_context_{}; // administration_ only
        inline static ::std::uint64_t publication_{}; // administration_ only
        inline static bool publication_exhausted_{};
        inline static bool closing_{}; // administration_ only; sticky until real configuration
        [[nodiscard]] static ::std::uint64_t begin_publication() noexcept
        {
            // Even tokens invalidate reuse BEFORE changing any dispatch byte.
            // Never recycle a token, including the theoretical wraparound.
            if(publication_ > UINT64_MAX-2u) { publication_exhausted_=true; }
            if(publication_exhausted_) { storage_.full_token.store(0u,::std::memory_order_release);return 0u; }
            publication_+=2u;storage_.full_token.store(publication_,::std::memory_order_release);return publication_;
        }
        static void publish_all() noexcept
        {
            auto const token{begin_publication()};
            for(auto& byte:storage_.bytes) { byte.store(1u,::std::memory_order_release); }
            if(token!=0u) { storage_.full_token.store(token|1u,::std::memory_order_release); }
        }
    public:
        static constexpr ::std::size_t word_count{64u}, bucket_count{word_count*64u};
        using bitmap = ::std::array<::std::uint64_t,word_count>;
        [[nodiscard]] static constexpr ::std::size_t bucket(::std::uint64_t module,
            ::std::uint64_t function, ::std::uint64_t offset) noexcept
        {
            auto mix{[](::std::uint64_t n) constexpr noexcept
            { n^=n>>30u;n*=0xbf58476d1ce4e5b9ull;n^=n>>27u;n*=0x94d049bb133111ebull;return n^(n>>31u); }};
            return (mix(module)^mix(function+0x9e3779b97f4a7c15ull)^mix(offset))&(bucket_count-1u);
        }
        // Only serialized runtime configuration selects an actual observer.
        // Null, unsupported and retired observers always take the full path.
        static void configure(void const* actual_context) noexcept
        {
            ::std::lock_guard lock{administration_};configured_context_=actual_context;
            closing_=false;publish_all();
        }
        // The authenticated managed-shutdown owner calls this BEFORE publishing
        // its actual request or waking guests. Later controller publications
        // cannot clear it. Only real quiescent runtime configuration (or a
        // null configuration that leaves every byte enabled) resets the latch.
        // This is a collection hint, never shutdown/unwind authority itself.
        static void force_collection_until_configured() noexcept
        {
            ::std::lock_guard lock{administration_};closing_=true;publish_all();
        }
        [[nodiscard]] static bool publish(void const* actual_context, bitmap const& wanted) noexcept
        {
            ::std::lock_guard lock{administration_};
            if(actual_context==nullptr || actual_context!=configured_context_) { return false; }
            if(closing_) { publish_all();return true; }
            auto const token{begin_publication()};bool all{true};
            for(::std::size_t i{};i!=word_count;++i)
            {
                auto const word{wanted[i]};all=all && word==UINT64_MAX;
                for(::std::size_t j{};j!=64u;++j)
                { storage_.bytes[i*64u+j].store(static_cast<::std::uint8_t>((word>>j)&1u),::std::memory_order_release); }
            }
            if(all && token!=0u) { storage_.full_token.store(token|1u,::std::memory_order_release); }
            return true;
        }
        static void require_all(void const* actual_context) noexcept
        { bitmap all{};all.fill(UINT64_MAX);static_cast<void>(publish(actual_context,all)); }
        [[nodiscard]] static void const* byte_address(::std::size_t index) noexcept
        {
            if constexpr(::std::atomic<::std::uint8_t>::is_always_lock_free && sizeof(::std::atomic<::std::uint8_t>)==1u)
            { if(index<bucket_count) { return ::std::addressof(storage_.bytes[index]); } }
            return nullptr;
        }
        // An odd unchanged token proves only continuous FULL collection. No
        // token grants packet/native access or executable resume authority.
        [[nodiscard]] static ::std::uint64_t full_observation_token() noexcept
        { return storage_.full_token.load(::std::memory_order_acquire); }
        [[nodiscard]] static void const* full_token_address() noexcept
        {
            if constexpr(::std::atomic<::std::uint64_t>::is_always_lock_free && sizeof(::std::atomic<::std::uint64_t>)==8u)
            { return ::std::addressof(storage_.full_token); }
            return nullptr;
        }
        [[nodiscard]] static bool may_observe(::std::uint64_t module,::std::uint64_t function,::std::uint64_t offset) noexcept
        {
            auto const b{bucket(module,function,offset)};
            return storage_.bytes[b].load(::std::memory_order_acquire)!=0u;
        }
        // Whitelist: calls, memory/table access, throw, divide and
        // trapping conversion retain their complete before-opcode packet.
        [[nodiscard]] static constexpr bool plain_numeric_opcode(::std::uint8_t op) noexcept
        {
            if(op==0x01u || (op>=0x02u && op<=0x05u) || (op>=0x0bu && op<=0x0fu) ||
               (op>=0x1au && op<=0x1cu) || (op>=0x20u && op<=0x24u) || (op>=0x41u && op<=0x6cu)) { return true; }
            if(op>=0x71u && op<=0x7eu) { return true; }
            if(op>=0x83u && op<=0xa7u) { return true; }
            if(op==0xacu || op==0xadu || (op>=0xb2u && op<=0xc4u)) { return true; }
            return false;
        }
        // Exact non-memory Core 3 SIMD arithmetic. All accepted operators
        // are nonallocating, cannot wait/call, and do not trap. Reserved gaps
        // and every vector memory access retain the full observation path.
        // This classification is DATA; the actual typed site still determines
        // whether a packet may be skipped or must precede a cold callback.
        [[nodiscard]] static constexpr bool demand_simd_opcode(::std::uint32_t op) noexcept
        {
            return (op>=0x0cu && op<=0x53u) || (op>=0x5eu && op<=0x99u) ||
                (op>=0x9bu && op<=0xa1u) || (op>=0xa3u && op<=0xa4u) ||
                (op>=0xa7u && op<=0xaeu) || op==0xb1u || (op>=0xb5u && op<=0xbau) ||
                (op>=0xbcu && op<=0xc1u) || (op>=0xc3u && op<=0xc4u) ||
                (op>=0xc7u && op<=0xceu) || op==0xd1u || (op>=0xd5u && op<=0xe1u) ||
                (op>=0xe3u && op<=0xedu) || (op>=0xefu && op<=0x113u);
        }
        // Borrow only bounded bytes from the actual validated expression.
        // Core 3 SIMD subopcodes fit two uLEB bytes. Longer valid encodings
        // conservatively stay full; truncated/continuing bytes are never read.
        [[nodiscard]] static constexpr bool demand_simd_encoding(::std::span<::std::byte const> bytes) noexcept
        {
            if(bytes.empty()) { return false; }
            auto const first{static_cast<::std::uint8_t>(bytes.front())};
            if(first<0x80u) { return demand_simd_opcode(first); }
            if(bytes.size()<2u) { return false; }
            auto const second{static_cast<::std::uint8_t>(bytes[1u])};
            return second<0x80u && demand_simd_opcode((first&0x7fu)|(static_cast<::std::uint32_t>(second)<<7u));
        }
        // The opting-in controller requests ALL sites before tracing, stepping,
        // pausing or enabling a terminal catchpoint. These numeric operations
        // cannot enter a host wait, call another activation or allocate a GC
        // object. A requested breakpoint/trap still captures the exact complete
        // before-opcode state; an unrequested terminal trap cannot create a stop.
        // Memory grow/size, table, atomic wait and prefixed operations stay full.
        [[nodiscard]] static constexpr bool demand_numeric_opcode(::std::uint8_t op) noexcept
        {
            return plain_numeric_opcode(op) || (op>=0x28u && op<=0x3eu) ||
                (op>=0x6du && op<=0x70u) || (op>=0x7fu && op<=0x82u) ||
                (op>=0xa8u && op<=0xabu) || (op>=0xaeu && op<=0xb1u);
        }
    };
}
