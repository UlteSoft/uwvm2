#pragma once
#ifndef UWVM_MODULE
# include <algorithm>
# include <array>
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include <utility>
# include <vector>
# include "wasm_state.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasm_path
{
    // Session-local original-index DATA compression ONLY. This ledger has no
    // runtime/GC/native owner, pointer, ticket, read permission or restore API.
    // Controller performs the full genuine state query before every commit and
    // every page; matching labels NEVER substitute for that native proof.
    inline constexpr ::std::uint32_t protocol_version{1u};
    inline constexpr ::std::uint32_t bound_view_version{wasm_state::view_version};
    inline constexpr ::std::size_t maximum_entries{128u}, maximum_cohort{256u};
    inline constexpr ::std::size_t maximum_indices{32768u}; // 256KiB path DATA/session.
    enum class operation : unsigned char { create, extend, members, clear };
    struct request
    {
        operation action{operation::clear};
        wasm_state::request root{};
        ::std::uint64_t session{}, handle{}, first{}, count{16u};
        ::std::array<::std::uint64_t,wasm_state::maximum_path> suffix{};
        unsigned char suffix_size{};
    };
    [[nodiscard]] constexpr bool valid(request const& query) noexcept
    {
        if(query.action>operation::clear || query.suffix_size>wasm_state::maximum_path) { return false; }
        for(::std::size_t i{query.suffix_size};i!=query.suffix.size();++i)
        { if(query.suffix[i]!=0u) { return false; } }
        if(query.action==operation::clear)
        { return query.session==0u && query.handle==0u && query.suffix_size==0u && query.first==0u && query.count==16u; }
        if(query.action==operation::create)
        { return query.session==0u && query.handle==0u && query.first==0u && query.count==16u &&
            wasm_state::valid(query.root) && query.root.member_page() && query.root.member_first==0u &&
            query.root.member_count==1u && query.root.path_size==0u && !query.root.compressed_path(); }
        return query.session!=0u && query.handle!=0u &&
            (query.action==operation::extend ? (query.suffix_size!=0u && query.first==0u && query.count==16u) :
                (query.suffix_size==0u && query.count!=0u && query.count<=wasm_state::maximum_rows));
    }
    struct cohort_member
    {
        ::std::uint64_t participant{}, module{}, function{}, offset{}, code_epoch{}, function_generation{};
        friend constexpr bool operator==(cohort_member const&,cohort_member const&) noexcept = default;
    };
    struct stop_key
    {
        ::std::uint64_t stop{}, runtime_epoch{};
        ::std::vector<cohort_member> cohort{};
        friend bool operator==(stop_key const&,stop_key const&) noexcept = default;
    };
    [[nodiscard]] inline bool valid(stop_key const& key) noexcept
    {
        if(key.stop==0u || key.runtime_epoch==0u || key.cohort.empty() || key.cohort.size()>maximum_cohort) { return false; }
        ::std::uint64_t previous{};
        for(auto const& member:key.cohort)
        { if(member.participant<=previous || member.code_epoch!=key.runtime_epoch || member.function_generation==0u) { return false; }
          previous=member.participant; }
        return true;
    }
    struct prepared_path
    {
        wasm_state::request query{};
        ::std::uint64_t predecessor{}, proposed_handle{};
        bool data_prepared{}; // DATA construction only; never native query admission.
        wasm_state::status result{wasm_state::status::invalid_selection}; // Failure reason, not read authority.
    };
    struct reply
    {
        wasm_state::status result{wasm_state::status::invalid_selection};
        ::std::uint64_t session{}, handle{}, depth{};
        bool cleared{};
    };
    class ledger
    {
        struct entry { ::std::uint64_t handle{}; wasm_state::request root{}; ::std::vector<::std::uint64_t> path{}; };
        inline static ::std::atomic<::std::uint64_t> next_session_{1u};
        ::std::uint64_t session_{}, next_handle_{1u};
        stop_key key_{};
        ::std::vector<entry> entries_{};
        ::std::size_t indices_{};
        [[nodiscard]] static ::std::uint64_t issue_session() noexcept
        {
            auto current{next_session_.load(::std::memory_order_relaxed)};
            while(current!=0u)
            {
                auto const successor{current==(::std::numeric_limits<::std::uint64_t>::max)() ? 0u : current+1u};
                if(next_session_.compare_exchange_weak(current,successor,::std::memory_order_relaxed)) { return current; }
            }
            return 0u; // Exhaustion permanently disables labels; never wrap.
        }
        [[nodiscard]] entry const* find(::std::uint64_t handle) const noexcept
        { for(auto const& item:entries_) { if(item.handle==handle) { return __builtin_addressof(item); } }return nullptr; }
    public:
        ledger() noexcept : session_{issue_session()} {}
        ledger(ledger const&)=delete; ledger& operator=(ledger const&)=delete;
        ledger(ledger&&)=delete; ledger& operator=(ledger&&)=delete;
        [[nodiscard]] ::std::uint64_t session() const noexcept { return session_; }
        [[nodiscard]] stop_key const& key() const noexcept { return key_; }
        void clear() noexcept { entries_.clear();key_={};indices_=0u; } // counters NEVER reset.
        [[nodiscard]] prepared_path prepare(request const& requested, stop_key const& current) const
        {
            prepared_path out{};
            if(!valid(requested) || requested.action==operation::clear) { return out; }
            if(!valid(current)) { out.result=wasm_state::status::stale_stop_or_generation;return out; }
            if(session_==0u) { out.result=wasm_state::status::resource_limit;return out; }
            if(requested.action!=operation::create &&
               (requested.session!=session_ || current!=key_))
            { out.result=wasm_state::status::stale_stop_or_generation;return out; }
            auto const* previous{requested.action==operation::create ? nullptr : find(requested.handle)};
            if(requested.action!=operation::create && previous==nullptr)
            { out.result=wasm_state::status::stale_stop_or_generation;return out; }
            out.query=previous==nullptr ? requested.root : previous->root;
            out.query.long_path=previous==nullptr ? ::std::vector<::std::uint64_t>{} : previous->path;
            if(requested.suffix_size>wasm_state::maximum_long_path-out.query.long_path.size())
            { out.result=wasm_state::status::resource_limit;return out; }
            // [actual bounded suffix0..<=16] [owned original-index path<=4096]
            // [safe] full sum is bounded BEFORE resize/push/index selection.
            out.query.long_path.reserve(out.query.long_path.size()+requested.suffix_size);
            for(::std::size_t i{};i!=requested.suffix_size;++i) { out.query.long_path.push_back(requested.suffix[i]); }
            if(requested.action==operation::members)
            {
                out.query.path_session=session_;out.query.path_handle=requested.handle;
                out.query.member_first=requested.first;out.query.member_count=requested.count;
            }
            else
            {
                if(next_handle_==0u) { out.result=wasm_state::status::resource_limit;return out; }
                auto const retired{previous==nullptr ? 0u : previous->path.size()};
                auto const retained{current==key_ ? indices_ : 0u};
                auto const entries{current==key_ ? entries_.size() : 0u};
                if(retired>retained || retained-retired>maximum_indices ||
                   out.query.long_path.size()>maximum_indices-(retained-retired) ||
                   (previous==nullptr && entries==maximum_entries))
                { out.result=wasm_state::status::resource_limit;return out; }
                out.proposed_handle=next_handle_;out.predecessor=previous==nullptr ? 0u : previous->handle;
                out.query.path_session=session_;out.query.path_handle=out.proposed_handle;
                out.query.member_first=0u;out.query.member_count=1u;
            }
            out.data_prepared=wasm_state::valid(out.query);
            if(out.data_prepared) { out.result=wasm_state::status::available; } return out;
        }
        // Original-index DATA extraction only; this returns NO owner or write
        // authority. Caller must repeat the actual canonical cohort/host/N/source
        // proof in the mutation API, and clear this ledger after a real commit.
        [[nodiscard]] prepared_path prepare_value(::std::uint64_t session,::std::uint64_t handle,
            ::std::span<::std::uint64_t const> suffix,stop_key const& current) const
        {
            prepared_path out{};
            if(suffix.size()>wasm_state::maximum_path) { return out; }
            if(session==0u || session!=session_ || handle==0u || !valid(current) || current!=key_)
            { out.result=wasm_state::status::stale_stop_or_generation;return out; }
            auto const* previous{find(handle)};
            if(previous==nullptr) { out.result=wasm_state::status::stale_stop_or_generation;return out; }
            if(previous->path.size()>wasm_state::maximum_long_path ||
               suffix.size()>wasm_state::maximum_long_path-previous->path.size())
            { out.result=wasm_state::status::resource_limit;return out; }
            // Full subtraction bounds BEFORE any vector copy/growth or index.
            // previous is a current ledger-owned record, never supplied native.
            out.query=previous->root;out.query.long_path=previous->path;
            out.query.long_path.reserve(previous->path.size()+suffix.size());
            for(auto edge:suffix) { out.query.long_path.push_back(edge); }
            out.query.path_session=session_;out.query.path_handle=handle;
            out.query.member_first=0u;out.query.member_count=1u;out.query.count=1u;
            out.data_prepared=wasm_state::valid(out.query);
            if(out.data_prepared) { out.result=wasm_state::status::available; }return out;
        }
        // Called by the controller only AFTER its fresh real query returned
        // available and exactly this current key/epoch. Still DATA-only: the
        // public helper itself has no way to authenticate or borrow any owner.
        [[nodiscard]] reply commit(prepared_path const& prepared,stop_key const& current)
        {
            reply out{};
            if(!prepared.data_prepared || prepared.proposed_handle==0u || prepared.proposed_handle!=next_handle_ ||
               prepared.query.path_session!=session_ || prepared.query.path_handle!=prepared.proposed_handle ||
               !wasm_state::valid(prepared.query) || !valid(current)) { return out; }
            auto const* old{prepared.predecessor==0u ? nullptr : find(prepared.predecessor)};
            if(prepared.predecessor!=0u && (old==nullptr || current!=key_)) { return out; }
            auto const retired{old==nullptr ? 0u : old->path.size()};
            auto const retained{current==key_ ? indices_ : 0u};auto const entries{current==key_ ? entries_.size() : 0u};
            if(retired>retained || retained-retired>maximum_indices ||
               prepared.query.long_path.size()>maximum_indices-(retained-retired) ||
               (old==nullptr && entries==maximum_entries)) { out.result=wasm_state::status::resource_limit;return out; }
            entry next{};next.handle=next_handle_;next.root=prepared.query;next.path=prepared.query.long_path;
            next.root.path_session=next.root.path_handle=0u;next.root.long_path={}; // release duplicate storage; quota counts the SOLE owned path.
            next.root.member_first=0u;next.root.member_count=1u;
            // Allocate/copy complete candidate key before ANY visible mutation.
            auto copied_key{current};
            if(old==nullptr) { entries_.reserve(entries+1u); }
            if(current!=key_) { clear(); }
            if(prepared.predecessor==0u) { entries_.push_back(::std::move(next)); }
            else
            {
                bool replaced{};
                for(auto& item:entries_) { if(item.handle==prepared.predecessor) { item=::std::move(next);replaced=true;break; } }
                if(!replaced) { return out; }
            }
            key_=::std::move(copied_key);indices_=retained-retired+prepared.query.long_path.size();
            auto const published{next_handle_};
            next_handle_=next_handle_==(::std::numeric_limits<::std::uint64_t>::max)() ? 0u : next_handle_+1u;
            out.result=wasm_state::status::available;out.session=session_;out.handle=published;out.depth=prepared.query.long_path.size();return out;
        }
    };
}
