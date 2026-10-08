// Debug-full event identity, independent of diagnostic instruction/unwind frames.
// No location, CFA, symbol or source metadata can mint an activation. Only exact
// generated entry/exit hooks feed this native ledger. Including this
// LLVM-free component does not enable instrumentation or grant runtime authority.
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <new>
#endif
namespace uwvm2::runtime::lib::details::debug_activation
{
    inline constexpr ::std::size_t frames_per_block{64u};
    inline constexpr ::std::size_t default_storage_budget{256u * 1024u * 1024u};
    enum class failure { none, invalid_identity, resource_exhausted, allocation_failed, identity_exhausted };
    enum class exit_kind : unsigned { returned, typed_tail, host_tail, exception };
    struct frame
    {
        ::std::uint64_t incarnation{}, parent{}, continuation{};
        ::std::uint64_t module{}, function{}, function_generation{}, runtime_epoch{}, island{};
    };
    // Blocks never move. Only a genuine on-worker entry/host bridge may grow
    // the directory; a Wasm-owned kernel witness cannot interrupt that bridge.
    // Readers use raw pointers only: no allocation/container/lock in SIGTRAP.
    template<typename T>
    class segmented_storage
    {
        T first_[frames_per_block]{};
        T** blocks_{};
        ::std::size_t size_{}, capacity_{};
    public:
        struct view
        {
            T const* first{};
            T* const* blocks{};
            ::std::size_t count{};
            [[nodiscard]] T const& operator[](::std::size_t index) const noexcept
            {
                // [ledger-owned blocks, index < count] end
                // [safe] every caller proves the actual recorded extent first.
                return index < frames_per_block ? first[index] :
                    blocks[index / frames_per_block - 1u][index % frames_per_block];
            }
        };
        segmented_storage() = default;
        segmented_storage(segmented_storage const&) = delete;
        segmented_storage& operator=(segmented_storage const&) = delete;
        ~segmented_storage()
        {
            for(::std::size_t i{}; i != size_; ++i) { delete[] blocks_[i]; }
            delete[] blocks_;
        }
        [[nodiscard]] failure ensure(::std::size_t index, ::std::size_t& used, ::std::size_t budget) noexcept
        {
            if(index < frames_per_block || index / frames_per_block <= size_) { return failure::none; }
            if(index / frames_per_block != size_ + 1u) { return failure::invalid_identity; }
            auto const max_directory{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(T*)};
            auto next_capacity{capacity_};
            if(size_ == capacity_)
            {
                if(capacity_ > max_directory / 2u) { return failure::resource_exhausted; }
                next_capacity = capacity_ == 0u ? 1u : capacity_ * 2u;
            }
            auto const directory_bytes{(next_capacity - capacity_) * sizeof(T*)};
            constexpr auto block_bytes{frames_per_block * sizeof(T)};
            if(used > budget || block_bytes > budget - used || directory_bytes > budget - used - block_bytes)
            { return failure::resource_exhausted; }
            auto* block{new(::std::nothrow) T[frames_per_block]{}};
            if(block == nullptr) { return failure::allocation_failed; }
            if(next_capacity != capacity_)
            {
                auto* directory{new(::std::nothrow) T*[next_capacity]{}};
                if(directory == nullptr) { delete[] block; return failure::allocation_failed; }
                for(::std::size_t i{}; i != size_; ++i) { directory[i] = blocks_[i]; }
                delete[] blocks_; blocks_ = directory; capacity_ = next_capacity;
            }
            blocks_[size_++] = block; used += block_bytes + directory_bytes;
            return failure::none;
        }
        [[nodiscard]] T& operator[](::std::size_t index) noexcept
        { return index < frames_per_block ? first_[index] : blocks_[index / frames_per_block - 1u][index % frames_per_block]; }
        [[nodiscard]] T const& operator[](::std::size_t index) const noexcept
        { return index < frames_per_block ? first_[index] : blocks_[index / frames_per_block - 1u][index % frames_per_block]; }
        [[nodiscard]] view data(::std::size_t count) const noexcept { return {first_, blocks_, count}; }
    };
    class ledger
    {
        segmented_storage<frame> frames_{};
        struct host_checkpoint { ::std::uint64_t token{}, island{}; ::std::size_t count{}; };
        segmented_storage<host_checkpoint> hosts_{};
        ::std::size_t storage_bytes_{frames_per_block * (sizeof(frame) + sizeof(host_checkpoint))};
        ::std::size_t storage_budget_{};
        ::std::size_t count_{}, host_count_{};
        ::std::uint64_t next_{1u}, island_{1u}, next_island_{2u};
        struct tail_continuation { ::std::uint64_t continuation{}, parent{}, island{}; ::std::size_t count{}; } pending_{};
        bool poisoned_{};
        failure failure_{failure::none};
        [[nodiscard]] ::std::uint64_t mint() noexcept
        {
            if(next_ == 0u) { poison(failure::identity_exhausted); return 0u; }
            auto const result{next_}; ++next_; return result;
        }
        // Compare the canonical seven event fields. The public activation
        // snapshot omits island; its expected frame copies therefore leave that
        // field zero. Actual same-island ownership is checked independently.
        [[nodiscard]] static bool native_identity_equal(frame const& actual, frame const& expected) noexcept
        {
            return actual.incarnation == expected.incarnation && actual.parent == expected.parent &&
                actual.continuation == expected.continuation && actual.module == expected.module &&
                actual.function == expected.function && actual.function_generation == expected.function_generation &&
                actual.runtime_epoch == expected.runtime_epoch;
        }
        [[nodiscard]] static bool native_expected_chain(frame const* expected, ::std::size_t expected_count) noexcept
        {
            if(expected == nullptr || expected_count == 0u) { return false; }
            // [runtime-cursor-owned immutable frame array ... expected_count] end
            // [safe                                                        ] the sealed
            //  ^^ caller owns this fixed array; checked count bounds every i below.
            // No guest/native stack address is accepted by this DATA predicate.
            for(::std::size_t i{}; i != expected_count; ++i)
            {
                auto const& current{expected[i]};
                if(current.incarnation == 0u || current.continuation == 0u || current.function_generation == 0u ||
                   current.runtime_epoch == 0u || current.runtime_epoch != expected[0u].runtime_epoch ||
                   current.parent != (i == 0u ? 0u : expected[i - 1u].incarnation) ||
                   (i != 0u && current.incarnation <= current.parent)) { return false; }
            }
            return true;
        }
    public:
        inline static constexpr ::std::size_t inline_storage_bytes{frames_per_block * (sizeof(frame) + sizeof(host_checkpoint))};
        explicit ledger(::std::size_t budget = default_storage_budget) noexcept : storage_budget_{budget}
        { if(storage_bytes_ > budget) { poison(failure::resource_exhausted); } }
        ledger(ledger const&) = delete;
        ledger& operator=(ledger const&) = delete;
        void poison(failure reason = failure::invalid_identity) noexcept
        { if(!poisoned_) { failure_ = reason; } poisoned_ = true; count_ = host_count_ = 0u; pending_ = {}; }
        [[nodiscard]] failure failure_reason() const noexcept { return failure_; }
        [[nodiscard]] ::std::size_t storage_bytes() const noexcept { return storage_bytes_; }
        [[nodiscard]] bool valid() const noexcept { return !poisoned_; }
        [[nodiscard]] ::std::size_t count() const noexcept { return count_; }
        // [ledger-owned segmented frame storage] end
        // [safe                            ] immutable borrow, count() bounds it;
        //  ^^ no frame/native-stack/guest address is represented or advanced.
        [[nodiscard]] segmented_storage<frame>::view data() const noexcept { return frames_.data(count_); }
        [[nodiscard]] bool complete() const noexcept
        {
            if(poisoned_ || count_ == 0u || host_count_ != 0u || pending_.continuation != 0u) { return false; }
            for(::std::size_t i{}; i != count_; ++i)
            { if(frames_[i].island != island_) { return false; } }
            return true;
        }
        // Before the first generated continuation activation exists, only a
        // clean empty ledger is admissible. This is native entry bookkeeping;
        // it cannot report a complete Wasm chain or grant a capture/PC cursor.
        [[nodiscard]] bool ready_for_root_entry() const noexcept
        {
            return !poisoned_ && count_ == 0u && host_count_ == 0u && island_ != 0u &&
                pending_.continuation == 0u && pending_.parent == 0u &&
                pending_.island == 0u && pending_.count == 0u;
        }
        // Signal-path DATA comparison only. It neither mints a physical cursor
        // nor proves native stack liveness. Call only on the actual worker after
        // its kernel PC has been authenticated inside the unchanged Wasm owner;
        // no ledger-writing host bridge may be interrupted by this comparison.
        // The sealed cursor retains the original immutable chain and its outer
        // execution lease through the backend's actual worker release ACK.
        [[nodiscard]] bool matches_native_chain(frame const* expected, ::std::size_t expected_count) const noexcept
        {
            if(poisoned_ || host_count_ != 0u ||
               count_ != expected_count || island_ == 0u || pending_.continuation != 0u ||
               pending_.parent != 0u || pending_.island != 0u || pending_.count != 0u ||
               !native_expected_chain(expected, expected_count)) { return false; }
            // [ledger-owned frame blocks ... count_] frame_end
            // [safe                                             ] expected_count was
            //  ^^ bounded and equals count_; every frame read uses i < count_.
            for(::std::size_t i{}; i != count_; ++i)
            {
                if(frames_[i].island != island_ || !native_identity_equal(frames_[i], expected[i])) { return false; }
            }
            return true;
        }
        // Signal-only comparison after an owned Wasm PC is authenticated. A
        // deeper recursion must retain the SAME original chain and island;
        // neither a reused SP nor a new host island is a descendant witness.
        [[nodiscard]] bool matches_native_descendant(frame const* expected, ::std::size_t expected_count) const noexcept
        {
            if(poisoned_ || host_count_!=0u || island_==0u || count_<=expected_count || count_>262144u ||
               pending_.continuation!=0u || pending_.parent!=0u || pending_.island!=0u || pending_.count!=0u ||
               !native_expected_chain(expected,expected_count)) { return false; }
            for(::std::size_t i{};i!=count_;++i)
            {
                auto const& actual{frames_[i]};
                if(actual.island!=island_ || actual.runtime_epoch!=expected[0u].runtime_epoch ||
                   actual.incarnation==0u || actual.continuation==0u || actual.function_generation==0u ||
                   actual.parent!=(i==0u ? 0u : frames_[i-1u].incarnation) ||
                   (i!=0u && actual.incarnation<=actual.parent) ||
                   (i<expected_count && !native_identity_equal(actual,expected[i]))) { return false; }
            }
            return true;
        }
        // A generated logical leave precedes the physical Wasm epilogue. Permit
        // that exact original-chain prefix only after the sealed provider has
        // independently recorded the genuine matching-incarnation leave event.
        // The kernel trajectory must still be in the original physical owner;
        // returns/out-of-owner jumps and EH unwind cannot use this predicate to
        // keep a retired physical activation alive.
        [[nodiscard]] bool matches_native_retired_prefix(frame const* expected, ::std::size_t expected_count,
            exit_kind retired_kind) const noexcept
        {
            if(expected_count == 0u || poisoned_ || host_count_ != 0u ||
               count_ != expected_count - 1u || island_ == 0u ||
               (retired_kind != exit_kind::returned && retired_kind != exit_kind::host_tail &&
                retired_kind != exit_kind::typed_tail) || !native_expected_chain(expected, expected_count)) { return false; }
            // [ledger-owned actual prefix ... expected_count - 1] prefix_end
            // [safe                                              ] both counts were
            //  ^^ checked before indexing; an empty genuine root prefix is valid.
            for(::std::size_t i{}; i != count_; ++i)
            {
                if(frames_[i].island != island_ || !native_identity_equal(frames_[i], expected[i])) { return false; }
            }
            if(retired_kind == exit_kind::typed_tail)
            {
                // [cursor-owned original array ... expected_count] original_end
                // [safe                                           ] expected_count is
                //  ^^ nonzero and actual-count matched; the original frame is live DATA.
                auto const& retired{expected[expected_count - 1u]};
                return pending_.continuation == retired.continuation && pending_.parent == retired.parent &&
                    pending_.island == island_ && pending_.count == count_;
            }
            return pending_.continuation == 0u && pending_.parent == 0u && pending_.island == 0u && pending_.count == 0u;
        }
        [[nodiscard]] ::std::uint64_t enter(::std::uint64_t module, ::std::uint64_t function,
            ::std::uint64_t generation, ::std::uint64_t epoch) noexcept
        {
            if(poisoned_) { return 0u; }
            if(generation == 0u || epoch == 0u) { poison(); return 0u; }
            auto const room{frames_.ensure(count_, storage_bytes_, storage_budget_)};
            if(room != failure::none) { poison(room); return 0u; }
            auto const identity{mint()}; if(identity == 0u) { return 0u; }
            auto const parent{count_ == 0u ? 0u : frames_[count_ - 1u].incarnation};
            auto continuation{identity};
            if(pending_.continuation != 0u)
            {
                if(pending_.count != count_ || pending_.parent != parent || pending_.island != island_)
                { poison(); return 0u; }
                continuation = pending_.continuation; pending_ = {};
            }
            frames_[count_++] = {identity, parent, continuation, module, function, generation, epoch, island_};
            return identity;
        }
        void leave(::std::uint64_t identity, exit_kind kind) noexcept
        {
            if(poisoned_) { return; }
            if(identity == 0u || count_ == 0u || pending_.continuation != 0u ||
               frames_[count_ - 1u].incarnation != identity || frames_[count_ - 1u].island != island_ ||
               (kind != exit_kind::returned && kind != exit_kind::typed_tail &&
                kind != exit_kind::host_tail && kind != exit_kind::exception)) { poison(); return; }
            auto const retired{frames_[--count_]};
            if(kind == exit_kind::typed_tail)
            { pending_ = {retired.continuation, retired.parent, island_, count_}; }
            // A host tail has NO typed successor. Keeping a pending identity here
            // would mislabel the next ordinary call after its adapter returned.
        }
        [[nodiscard]] ::std::uint64_t suspend_host() noexcept
        {
            if(poisoned_) { return 0u; }
            if(pending_.continuation != 0u || next_island_ == 0u)
            { poison(); return 0u; }
            auto const room{hosts_.ensure(host_count_, storage_bytes_, storage_budget_)};
            if(room != failure::none) { poison(room); return 0u; }
            auto const token{mint()}; if(token == 0u) { return 0u; }
            hosts_[host_count_++] = {token, island_, count_};
            island_ = next_island_++; return token;
        }
        void restore_host(::std::uint64_t token) noexcept
        {
            if(poisoned_) { return; }
            if(token == 0u || host_count_ == 0u || hosts_[host_count_ - 1u].token != token ||
               hosts_[host_count_ - 1u].count != count_ || pending_.continuation != 0u)
            { poison(); return; }
            island_ = hosts_[--host_count_].island;
        }
    };
}
