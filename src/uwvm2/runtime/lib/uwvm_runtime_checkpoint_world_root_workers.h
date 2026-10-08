// Include ONLY inside the genuine private candidate world transaction.
// Actual FastIO native workers own their own TLS roots; the caller never
// supplies a body, native pointer, thread ID, root record or startup permission.
#pragma once
#include "uwvm_runtime_checkpoint_world_worker_wasip1.h"
class prepared_world_root_workers final
{
    using pause_domain=::uwvm2::utils::thread::cooperative_pause_domain;
    using activation_ledger=details::debug_activation::ledger;
    using shadow_ledger=::uwvm2::runtime::checkpoint::shadow_ledger;
    struct packet final
    {
        prepared_world_thread* thread{};
        ::std::unique_ptr<::fast_io::native_thread> native{};
        ::std::atomic<unsigned char> arrival{};
        pause_domain::participant participant{};
        ::std::shared_ptr<activation_ledger> activation{};
        ::std::unique_ptr<shadow_ledger> checkpoint{};
        details::debug_operand_workspace_stack workspaces{};
        ::std::uint_least64_t participant_id{}; // Private comparison, never returned.
        bool debug_installed{}, debug_restored{};
        ::std::size_t wasip1_visits{};
        bool wasip1_held{}, wasip1_restored{};
        ::uwvm2::runtime::gc::root_frame const* installed_head{}; // private comparison only
        ::std::size_t carriers{};
        bool restored{};
    };
    runtime_checkpoint_world_transaction& world_;
    // Destruction order: joined packet/TLS owners retire BEFORE this private
    // controller. The real currently paused runtime controller is untouched.
    ::std::unique_ptr<pause_domain> control_{};
    ::std::vector<::std::unique_ptr<packet>> packets_{};
    ::std::atomic<bool> release_{};
    ::std::size_t started_{},joined_{};
    ::std::size_t expected_wasip1_workers_{},expected_wasip1_visits_{};
    [[nodiscard]] static bool fresh_debug_tls() noexcept
    {
        return g_debug_pause_participant==nullptr && g_debug_activation_ledger==nullptr &&
            g_debug_activation_ledger_owner==nullptr && g_checkpoint_shadow_ledger==nullptr &&
            g_debug_operand_workspaces==nullptr && g_debug_native_stack_scope==nullptr &&
            !g_debug_observer_active && !g_debug_activation_park_site.before_park;
    }
    [[nodiscard]] bool validate_debug_tls(packet const& owned)
    {
        return owned.debug_installed && owned.participant && owned.participant_id!=0u &&
            owned.participant.identifier()==owned.participant_id && control_->owns_current_participant(owned.participant) &&
            g_debug_pause_participant==::std::addressof(owned.participant) &&
            g_debug_activation_ledger==owned.activation.get() &&
            g_debug_activation_ledger_owner==::std::addressof(owned.activation) &&
            g_checkpoint_shadow_ledger==owned.checkpoint.get() &&
            g_debug_operand_workspaces==::std::addressof(owned.workspaces) &&
            owned.activation->ready_for_root_entry() &&
            owned.checkpoint->failure()==::uwvm2::runtime::checkpoint::status::ok &&
            owned.checkpoint->size()==0u && owned.workspaces.size()==0u &&
            g_debug_native_stack_scope==nullptr && !g_debug_observer_active &&
            !g_debug_activation_park_site.before_park;
    }
    [[nodiscard]] bool install_debug_tls(packet& owned) noexcept
    {
        if(!fresh_debug_tls() || !owned.activation || !owned.activation->ready_for_root_entry() ||
           !owned.checkpoint || owned.checkpoint->failure()!=::uwvm2::runtime::checkpoint::status::ok ||
           owned.checkpoint->size()!=0u || owned.workspaces.size()!=0u) { return false; }
        try { owned.participant=control_->enter(); }
        catch(...) { return false; }
        if(!owned.participant) { return false; }
        owned.participant_id=owned.participant.identifier();
        g_debug_pause_participant=::std::addressof(owned.participant);
        g_debug_activation_ledger=owned.activation.get();
        g_debug_activation_ledger_owner=::std::addressof(owned.activation);
        g_checkpoint_shadow_ledger=owned.checkpoint.get();
        g_debug_operand_workspaces=::std::addressof(owned.workspaces);
        owned.debug_installed=true;
        return true;
    }
    void remove_debug_tls(packet& owned) noexcept
    {
        if(owned.debug_installed)
        {
            if(!validate_debug_tls(owned)) { ::fast_io::fast_terminate(); }
            // Empty real ledgers, never a forged restored frame/activation.
            // Clear SAME-thread native borrows before participant unregister
            // and before the coordinator can destroy any owned context.
            owned.activation->poison();
            g_debug_operand_workspaces=nullptr;
            g_checkpoint_shadow_ledger=nullptr;
            g_debug_activation_ledger_owner=nullptr;
            g_debug_activation_ledger=nullptr;
            g_debug_pause_participant=nullptr;
            owned.debug_installed=false;
        }
        owned.participant.reset();
        owned.debug_restored=fresh_debug_tls();
    }
    void run(packet& owned) noexcept
    {
        namespace gc=::uwvm2::runtime::gc;
        auto const* inherited{gc::current_root_frames()};
        bool const debug_ready{install_debug_tls(owned)};
        {
            prepared_world_root_scope actual{*owned.thread,world_.retained_roots_};
            prepared_world_worker_wasip1_scope wasip1{world_,*owned.thread};
            bool const valid{debug_ready && inherited==nullptr && actual.install() && actual.validate_actual_prefix(owned.carriers) &&
                validate_debug_tls(owned) && wasip1.install()};
            owned.wasip1_held=wasip1.held();owned.wasip1_visits=wasip1.visits();
            if(valid) { owned.installed_head=gc::current_root_frames(); }
            owned.arrival.store(valid?1u:2u,::std::memory_order_release);
            owned.arrival.notify_one();
            // All actual scopes remain live together until the sole coordinator
            // has inspected EVERY arrival. No execution entry/domain/GC lease
            // or publication is synthesized for this private candidate rehearsal.
            while(!release_.load(::std::memory_order_acquire)) { release_.wait(false,::std::memory_order_acquire); }
            if(valid)
            {
                ::std::size_t current{};
                if(!validate_debug_tls(owned) || !wasip1.validate() || !actual.validate_actual_prefix(current) || current!=owned.carriers ||
                   gc::current_root_frames()!=owned.installed_head) { ::fast_io::fast_terminate(); }
            }
            owned.wasip1_restored=wasip1.remove();
            if(!owned.wasip1_restored) { ::fast_io::fast_terminate(); }
        } // Genuine same-worker LIFO removal before native/TLS destruction.
        if(gc::current_root_frames()!=inherited) { ::fast_io::fast_terminate(); }
        bool cleared{inherited==nullptr};
        for(auto const& record:owned.thread->root_records)
        { cleared=cleared && record.previous==nullptr && record.slots==nullptr && record.capacity_and_active==0u && record.live_count==0u; }
        owned.restored=cleared;
        remove_debug_tls(owned); // Root RAII cleanup precedes debug TLS teardown.
    }
    void release_and_join() noexcept
    {
        release_.store(true,::std::memory_order_release);release_.notify_all();
        // These finite private bodies have no guest or host callback. Genuine
        // blocking FastIO join includes callable/TLS teardown and OS retirement.
        // This is NOT a bounded restored-world retirement/admission credential.
        while(joined_<started_)
        {
            auto& actual{packets_[joined_]->native};
            try { actual->join(); } catch(...) { ::fast_io::fast_terminate(); }
            ++joined_;
        }
    }
public:
    explicit prepared_world_root_workers(runtime_checkpoint_world_transaction& world) noexcept :world_{world} {}
    prepared_world_root_workers(prepared_world_root_workers const&)=delete;
    prepared_world_root_workers& operator=(prepared_world_root_workers const&)=delete;
    ~prepared_world_root_workers() noexcept { release_and_join(); }
    [[nodiscard]] bool prepare(::std::size_t maximum_workers)
    {
        auto const count{world_.prepared_threads_.size()};
        auto const control_bytes{pause_domain::fixed_native_payload_bytes(count)};
        if(count==0u || count>maximum_workers || count>256u || count>packets_.max_size() ||
           control_bytes==SIZE_MAX || !world_.actual_prepared_profile_ ||
           !world_.charge_native(1u,control_bytes) ||
           !world_.charge_native(count,sizeof(::std::unique_ptr<packet>)+sizeof(packet)+sizeof(::fast_io::native_thread)+
                sizeof(activation_ledger)+sizeof(shadow_ledger)+sizeof(prepared_world_worker_wasip1_scope))) { return false; }
        // Allocate ALL stable records before any OS launch. The candidate world
        // keeps carriers, staged stores, source and engines alive through join.
        packets_.reserve(count);
        control_=::std::make_unique<pause_domain>(count);
        for(auto& thread:world_.prepared_threads_)
        {
            if(!prepared_world_worker_wasip1_scope::preflight(world_,thread)) { return false; }
            auto const visits{prepared_world_worker_wasip1_scope::expected_visits(world_,thread)};
            if(visits>SIZE_MAX-expected_wasip1_visits_) { return false; }
            expected_wasip1_visits_+=visits;
            expected_wasip1_workers_+=prepared_world_worker_wasip1_scope::expects_held(world_,thread)?1u:0u;
            auto owned{::std::make_unique<packet>()};owned->thread=::std::addressof(thread);
            owned->activation=::std::make_shared<activation_ledger>(g_runtime.debug_activation_storage_budget);
            owned->checkpoint=::std::make_unique<shadow_ledger>(world_.actual_prepared_profile_);
            if(!owned->activation->ready_for_root_entry() ||
               owned->checkpoint->failure()!=::uwvm2::runtime::checkpoint::status::ok) { return false; }
            packets_.push_back(::std::move(owned));
        }
        return true;
    }
    [[nodiscard]] bool validate_and_join(llvm_jit_checkpoint_prepare_result& out) noexcept
    {
        bool all{};
        out.prepared_private_debug_workers=packets_.size();
        out.prepared_private_wasip1_workers=expected_wasip1_workers_;
        try
        {
            for(auto& owned:packets_)
            {
                // No fallible statement follows successful OS ownership before
                // recording started_. A later create failure releases and joins
                // the actual prefix; it cannot destroy a live borrowed packet.
                owned->native.reset(new ::fast_io::native_thread{[this,actual=owned.get()]() noexcept { run(*actual); }});
                ++started_;
            }
            all=started_==packets_.size();
            ::std::size_t carriers{},wasip1_workers{},wasip1_visits{};
            for(auto const& owned:packets_)
            {
                auto observed{owned->arrival.load(::std::memory_order_acquire)};
                while(observed==0u) { owned->arrival.wait(0u,::std::memory_order_acquire);observed=owned->arrival.load(::std::memory_order_acquire); }
                // Arrival's release/acquire proves the real worker published its
                // prefix, with a scope still alive behind the unreleased gate.
                if(observed!=1u || owned->thread->root_records.empty() ||
                   owned->installed_head!=::std::addressof(owned->thread->root_records.back()) ||
                   owned->carriers>SIZE_MAX-carriers || !owned->debug_installed ||
                   owned->participant_id==0u || !control_->owns_current_participant(owned->participant)) { all=false; }
                else { carriers+=owned->carriers; }
                if(owned->wasip1_held!=prepared_world_worker_wasip1_scope::expects_held(world_,*owned->thread) ||
                   owned->wasip1_visits!=prepared_world_worker_wasip1_scope::expected_visits(world_,*owned->thread) ||
                   owned->wasip1_visits>SIZE_MAX-wasip1_visits) { all=false; }
                else { wasip1_visits+=owned->wasip1_visits;wasip1_workers+=owned->wasip1_held?1u:0u; }
            }
            if(carriers!=out.installed_private_root_carriers) { all=false; }
            if(wasip1_workers!=expected_wasip1_workers_ || wasip1_visits!=expected_wasip1_visits_) { all=false; }
            for(::std::size_t i{};i<packets_.size();++i)
            {
                for(::std::size_t j{};j<i;++j)
                {
                    if(packets_[i]->participant_id==packets_[j]->participant_id ||
                       packets_[i]->activation.get()==packets_[j]->activation.get() ||
                       packets_[i]->checkpoint.get()==packets_[j]->checkpoint.get()) { all=false; }
                }
            }
            if(all)
            {
                out.enrolled_private_root_workers=started_;
                out.private_worker_cohort_held=true;
                out.enrolled_private_debug_workers=started_;
                out.private_debug_cohort_held=true;
                out.enrolled_private_wasip1_workers=wasip1_workers;
                out.private_worker_wasip1_frame_visits=wasip1_visits;
                out.private_wasip1_cohort_held=wasip1_workers!=0u;
            }
        }
        catch(...) { all=false; } // Includes genuine OS create/allocation failure.
        release_and_join();
        out.started_private_root_workers=started_;out.joined_private_root_workers=joined_;
        bool restored{joined_==started_};
        for(::std::size_t n{};n<started_;++n) { restored=restored && packets_[n]->restored; }
        out.private_worker_roots_restored=restored;
        bool debug_restored{joined_==started_};
        for(::std::size_t n{};n<started_;++n)
        { debug_restored=debug_restored && packets_[n]->debug_restored && !packets_[n]->participant && !packets_[n]->debug_installed; }
        out.private_debug_tls_restored=debug_restored;
        restored=restored && debug_restored;
        bool wasip1_restored{joined_==started_};
        for(::std::size_t n{};n<started_;++n) { wasip1_restored=wasip1_restored && packets_[n]->wasip1_restored; }
        out.private_wasip1_tls_restored=prepared_world_worker_wasip1_scope::enabled(world_) && wasip1_restored;
        restored=restored && wasip1_restored;
        return all && restored;
    }
};

[[nodiscard]] bool validate_private_world_root_workers(::std::size_t maximum_workers,
    llvm_jit_checkpoint_prepare_result& out)
{
    if(phase_!=preparation_status::resources_prepared || !out.private_root_chain_restored) { return false; }
    prepared_world_root_workers actual{*this};
    if(!actual.prepare(maximum_workers)) { out.native_payload_bytes=native_bytes_;return false; }
    out.native_payload_bytes=native_bytes_;
    return actual.validate_and_join(out);
}
