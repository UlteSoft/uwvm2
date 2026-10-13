// Private cold root installation on the actual calling native thread. The
// transaction owns all carriers/staged objects; no worker/activation is forged.
#pragma once
class prepared_world_root_scope final
{
    prepared_world_thread& thread_;
    ::std::span<reference const> retained_;
    ::uwvm2::runtime::gc::root_frame const* previous_{};
    ::std::size_t entered_{};
    [[nodiscard]] bool enter(::std::span<reference const> slots, ::std::size_t live) noexcept
    {
        namespace gc=::uwvm2::runtime::gc;
        if(entered_>=thread_.root_records.size() || live>slots.size()) { return false; }
        auto& actual{thread_.root_records[entered_]};
        if(!gc::enter_root_frame(actual,reinterpret_cast<::std::byte const*>(slots.data()),slots.size())) { return false; }
        ++entered_; // Own even a rejected publish, for genuine LIFO rollback.
        return gc::publish_root_frame(actual,live);
    }
public:
    prepared_world_root_scope(prepared_world_thread& thread, ::std::span<reference const> retained) noexcept
        :thread_{thread},retained_{retained},previous_{::uwvm2::runtime::gc::current_root_frames()} {}
    prepared_world_root_scope(prepared_world_root_scope const&)=delete;
    prepared_world_root_scope& operator=(prepared_world_root_scope const&)=delete;
    prepared_world_root_scope(prepared_world_root_scope&&)=delete;
    prepared_world_root_scope& operator=(prepared_world_root_scope&&)=delete;
    ~prepared_world_root_scope() noexcept { restore(); }
    void restore() noexcept
    {
        while(entered_!=0u)
        {
            --entered_;
            if(!::uwvm2::runtime::gc::leave_root_frame(thread_.root_records[entered_])) { ::fast_io::fast_terminate(); }
        }
        if(::uwvm2::runtime::gc::current_root_frames()!=previous_) { ::fast_io::fast_terminate(); }
    }
    [[nodiscard]] bool install() noexcept
    {
        if(entered_!=0u || thread_.frames.size()>(SIZE_MAX-1u)/2u ||
           thread_.root_records.size()!=1u+2u*thread_.frames.size()) { return false; }
        // Allocate and authenticate every carrier/strong pending-store owner
        // BEFORE publishing even one record into actual native TLS.
        for(auto const& frame:thread_.root_records)
        { if(frame.previous!=nullptr || frame.slots!=nullptr || frame.capacity_and_active!=0u || frame.live_count!=0u) { return false; } }
        if(!enter(retained_,retained_.size())) { return false; }
        for(auto const& frame:thread_.frames)
        {
            if(frame.input_roots.size()!=frame.input_root_owners.size() ||
               !enter(frame.input_roots,frame.input_roots.size()) || !enter(frame.result_roots,0u)) { return false; }
        }
        return true;
    }
    [[nodiscard]] bool validate_actual_prefix(::std::size_t& roots) const noexcept
    {
        namespace gc=::uwvm2::runtime::gc;
        if(entered_!=thread_.root_records.size()) { return false; }
        auto const* actual{gc::current_root_frames()};::std::size_t count{};
        for(::std::size_t n{entered_};n!=0u;)
        {
            --n;auto const& expected{thread_.root_records[n]};
            // Compare against THIS scope's actual live records before reading
            // any link. Never inspect the inherited caller's root chain.
            if(actual!=::std::addressof(expected)) { return false; }
            ::std::span<reference const> slots{};::std::size_t live{};
            if(n==0u) { slots=retained_;live=slots.size(); }
            else
            {
                auto const index{(n-1u)/2u};if(index>=thread_.frames.size()) { return false; }
                auto const& frame{thread_.frames[index]};
                if((n&1u)!=0u) { slots=frame.input_roots;live=slots.size(); }
                else { slots=frame.result_roots; } // Unused result capacity is not a live root.
            }
            if(expected.slots!=reinterpret_cast<::std::byte const*>(slots.data()) ||
               expected.capacity_and_active!=(slots.size()|gc::frame_root_details::active_bit) ||
               expected.live_count!=live || live>SIZE_MAX-count) { return false; }
            for(::std::size_t k{};k<live;++k)
            {
                reference value{};
                // [fixed complete owned carriers] k<live<=actual capacity.
                ::fast_io::freestanding::my_memcpy(::std::addressof(value),expected.slots+k*sizeof(reference),sizeof(reference));
                if(!gc::frame_root_details::runtime_kind(value)) { return false; }
            }
            count+=live;actual=expected.previous;
        }
        if(actual!=previous_) { return false; }
        roots=count;return true;
    }
};

[[nodiscard]] bool validate_private_world_root_installation(::std::size_t maximum_frames,
    llvm_jit_checkpoint_prepare_result& out) noexcept
{
    namespace gc=::uwvm2::runtime::gc;
    auto const* inherited{gc::current_root_frames()};::std::size_t total_frames{};
    if(phase_!=preparation_status::resources_prepared || prepared_threads_.empty() ||
       retained_roots_.size()!=retained_root_owners_.size()) { return false; }
    for(auto const& thread:prepared_threads_)
    {
        if(thread.root_records.size()>maximum_frames-total_frames) { return false; }
        total_frames+=thread.root_records.size();
    }
    ::std::size_t scopes{},carriers{};
    for(auto& thread:prepared_threads_)
    {
        ::std::size_t actual_roots{};
        {
            prepared_world_root_scope actual{thread,retained_roots_};
            if(!actual.install() || !actual.validate_actual_prefix(actual_roots) || actual_roots>SIZE_MAX-carriers) { return false; }
        } // Genuine calling-thread LIFO removal BEFORE candidate destruction.
        if(gc::current_root_frames()!=inherited) { ::fast_io::fast_terminate(); }
        for(auto const& record:thread.root_records)
        { if(record.previous!=nullptr || record.slots!=nullptr || record.capacity_and_active!=0u || record.live_count!=0u) { return false; } }
        ++scopes;carriers+=actual_roots;
    }
    out.validated_private_root_scopes=scopes;
    out.installed_private_root_frames=total_frames;
    out.installed_private_root_carriers=carriers;
    out.private_root_chain_restored=true;
    return true;
}
