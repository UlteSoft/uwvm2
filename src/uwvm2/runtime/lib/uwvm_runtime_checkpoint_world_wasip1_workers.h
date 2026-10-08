// Include ONLY inside the private candidate world. This validates actual
// per-thread native WASIp1 selection without invoking a guest or host import.
#pragma once
class private_wasip1_dispatch_workers final
{
    static constexpr ::std::size_t worker_count{2u};
    struct packet
    {
        ::std::unique_ptr<::fast_io::native_thread> native{};
        ::std::atomic<unsigned char> arrival{};
        ::std::size_t held_module{}, visits{};
        wasip1_environment const* selected_environment{};
        native_memory_t* selected_memory{};
        bool restored{};
    };
    runtime_checkpoint_world_transaction& world_;
    ::std::array<packet,worker_count> workers_{};
    ::std::atomic<bool> release_{};
    ::std::size_t started_{}, joined_{};
    void run(packet& owned) noexcept
    {
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        auto* const inherited_binding{storage::current_wasip1_memory_binding_ref()};
        auto const* inherited_environment{::std::addressof(storage::current_wasip1_env())};
        bool valid{inherited_binding==nullptr};
        for(auto const& context:world_.wasip1_dispatch_contexts_)
        {
            if(!context.import_visible) { continue; }
            auto& env{*context.env}; // All owned memberships proved before launch.
            storage::scoped_current_wasip1_env_t environment{env};
            storage::scoped_current_wasip1_memory_t memory{env,context.memory0};
            valid=valid && ::std::addressof(storage::current_wasip1_env())==context.env &&
                env.get_memory()==context.memory0 && env.wasip1_memory==nullptr;
            {
                storage::scoped_current_wasip1_memory_t hidden{env,nullptr};
                valid=valid && env.get_memory()==nullptr; // Null hides outer memory.
            }
            valid=valid && env.get_memory()==context.memory0;
            auto const& nested{world_.wasip1_dispatch_contexts_.index_unchecked(owned.held_module)};
            {
                {
                    storage::scoped_current_wasip1_env_t next_environment{*nested.env};
                    storage::scoped_current_wasip1_memory_t next_memory{*nested.env,nested.memory0};
                    valid=valid && ::std::addressof(storage::current_wasip1_env())==nested.env &&
                        nested.env->get_memory()==nested.memory0 &&
                        env.get_memory()==(nested.env==context.env?nested.memory0:context.memory0);
                }
                valid=valid && ::std::addressof(storage::current_wasip1_env())==context.env &&
                    env.get_memory()==context.memory0;
            }
            ++owned.visits;
        }
        auto const& held{world_.wasip1_dispatch_contexts_.index_unchecked(owned.held_module)};
        {
            storage::scoped_current_wasip1_env_t environment{*held.env};
            storage::scoped_current_wasip1_memory_t memory{*held.env,held.memory0};
            owned.selected_environment=::std::addressof(storage::current_wasip1_env());
            owned.selected_memory=held.env->get_memory();
            valid=valid && owned.selected_environment==held.env && owned.selected_memory==held.memory0;
            owned.arrival.store(valid?1u:2u,::std::memory_order_release);
            owned.arrival.notify_one();
            // Both genuine TLS scopes stay alive through coordinator inspection.
            while(!release_.load(::std::memory_order_acquire)) { release_.wait(false,::std::memory_order_acquire); }
            if(::std::addressof(storage::current_wasip1_env())!=held.env ||
               held.env->get_memory()!=held.memory0 || held.env->wasip1_memory!=nullptr)
            { ::fast_io::fast_terminate(); }
        }
        owned.restored=storage::current_wasip1_memory_binding_ref()==inherited_binding &&
            ::std::addressof(storage::current_wasip1_env())==inherited_environment;
    }
    void release_and_join() noexcept
    {
        release_.store(true,::std::memory_order_release);release_.notify_all();
        while(joined_<started_)
        {
            try { workers_[joined_].native->join(); } catch(...) { ::fast_io::fast_terminate(); }
            ++joined_;
        }
    }
public:
    explicit private_wasip1_dispatch_workers(runtime_checkpoint_world_transaction& world) noexcept :world_{world} {}
    private_wasip1_dispatch_workers(private_wasip1_dispatch_workers const&)=delete;
    private_wasip1_dispatch_workers& operator=(private_wasip1_dispatch_workers const&)=delete;
    ~private_wasip1_dispatch_workers() noexcept { release_and_join(); }
    [[nodiscard]] llvm_jit_wasip1_environment_capsule_status validate(::std::size_t maximum_workers)
    {
        using status=llvm_jit_wasip1_environment_capsule_status;
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        if(maximum_workers<worker_count ||
           !world_.charge_native(worker_count,sizeof(packet)+sizeof(::fast_io::native_thread)))
        { return status::resource_limit; }
        ::std::size_t first{SIZE_MAX},second{SIZE_MAX},visible{};
        for(::std::size_t n{};n<world_.wasip1_dispatch_contexts_.size();++n)
        {
            auto const& context{world_.wasip1_dispatch_contexts_.index_unchecked(n)};
            if(!context.import_visible) { continue; }
            bool owned{};
            for(auto const& env:world_.wasip1_environments_) { owned=owned || context.env==::std::addressof(env->environment); }
            // Exact member proof precedes every pointee read and resolver call.
            if(!owned || context.env->wasip1_memory!=nullptr ||
                context.env->wasip1_memory_resolver!=storage::resolve_current_wasip1_memory)
            { return status::stale_environment; }
            ++visible;
            if(first==SIZE_MAX) { first=n; }
            else if(second==SIZE_MAX) { second=n; }
        }
        if(first==SIZE_MAX || visible!=world_.wasip1_bound_modules_) { return status::stale_environment; }
        if(visible>SIZE_MAX/worker_count) { return status::resource_limit; }
        // Prefer a shared env with different actual module memories. Holding it
        // concurrently catches a regression back to one mutable env pointer.
        bool shared_pair{};
        for(auto const& owner:world_.wasip1_environments_)
        {
            ::std::size_t previous{SIZE_MAX};
            for(::std::size_t n{};n<world_.wasip1_dispatch_contexts_.size();++n)
            {
                auto const& context{world_.wasip1_dispatch_contexts_.index_unchecked(n)};
                if(!context.import_visible || context.env!=::std::addressof(owner->environment)) { continue; }
                if(previous==SIZE_MAX) { previous=n;continue; }
                if(context.memory0!=world_.wasip1_dispatch_contexts_.index_unchecked(previous).memory0)
                { first=previous;second=n;shared_pair=true;break; }
            }
            if(shared_pair) { break; }
        }
        workers_[0].held_module=first;workers_[1].held_module=second==SIZE_MAX?first:second;
        bool all{true};::std::size_t visits{};
        try
        {
            for(auto& worker:workers_)
            {
                worker.native.reset(new ::fast_io::native_thread{[this,actual=::std::addressof(worker)]() noexcept { run(*actual); }});
                ++started_;
            }
            for(auto const& worker:workers_)
            {
                auto observed{worker.arrival.load(::std::memory_order_acquire)};
                while(observed==0u) { worker.arrival.wait(0u,::std::memory_order_acquire);observed=worker.arrival.load(::std::memory_order_acquire); }
                auto const& expected{world_.wasip1_dispatch_contexts_.index_unchecked(worker.held_module)};
                all=all && observed==1u && worker.selected_environment==expected.env &&
                    worker.selected_memory==expected.memory0 && worker.visits==visible;
                visits+=worker.visits; // two bounded module counts, preflight above.
            }
        }
        catch(::fast_io::error const&) { return status::native_operation_failed; } // Destructor joins the started prefix.
        release_and_join();
        all=all && joined_==worker_count;
        for(auto const& worker:workers_) { all=all && worker.restored; }
        if(!all) { return status::stale_environment; }
        world_.wasip1_verified_workers_=joined_;
        world_.wasip1_verified_module_visits_=visits;
        world_.wasip1_dispatch_tls_restored_=true;
        return status::captured;
    }
};
