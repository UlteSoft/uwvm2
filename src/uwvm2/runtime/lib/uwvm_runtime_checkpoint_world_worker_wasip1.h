// Include inside the private world, after actual prepared frame definitions.
// This scope borrows only that world's final cache. It is never an execution
// credential, and does not call an import, write memory, or touch a guest FD.
#pragma once
class prepared_world_worker_wasip1_scope final
{
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
    using env_scope=::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t;
    using memory_scope=::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_memory_t;
    runtime_checkpoint_world_transaction& world_;
    prepared_world_thread const& thread_;
    ::std::optional<env_scope> environment_{};
    ::std::optional<memory_scope> memory_{};
    wasip1_environment const* inherited_environment_{};
    ::std::remove_reference_t<decltype(::uwvm2::uwvm::imported::wasi::wasip1::storage::current_wasip1_memory_binding_ref())> inherited_binding_{};
    wasip1_environment* selected_environment_{};
    native_memory_t* selected_memory_{};
    ::std::size_t visits_{};
    bool enabled_{}, removed_{}, restored_{};
    [[nodiscard]] auto const& context(prepared_world_frame const& frame) const noexcept
    { return world_.wasip1_dispatch_contexts_.index_unchecked(world_.modules_[frame.owner].new_actual_id); }
#endif
public:
    [[nodiscard]] static bool enabled(runtime_checkpoint_world_transaction const& world) noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        return !world.wasip1_dispatch_contexts_.empty();
#else
        static_cast<void>(world);return false;
#endif
    }
    // Called only after preflight has proved EVERY frame owner/dense/cache edge.
    [[nodiscard]] static bool expects_held(runtime_checkpoint_world_transaction const& world,
        prepared_world_thread const& thread) noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        return enabled(world) && world.wasip1_dispatch_contexts_.index_unchecked(
            world.modules_[thread.frames.back().owner].new_actual_id).import_visible;
#else
        static_cast<void>(world);static_cast<void>(thread);return false;
#endif
    }
    [[nodiscard]] static ::std::size_t expected_visits(runtime_checkpoint_world_transaction const& world,
        prepared_world_thread const& thread) noexcept
    {
        ::std::size_t count{};
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        if(enabled(world))
        { for(auto const& frame:thread.frames) { count+=world.wasip1_dispatch_contexts_.index_unchecked(world.modules_[frame.owner].new_actual_id).import_visible?1u:0u; } }
#else
        static_cast<void>(world);static_cast<void>(thread);
#endif
        return count;
    }
    [[nodiscard]] static bool preflight(runtime_checkpoint_world_transaction const& world,
        prepared_world_thread const& thread) noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        if(world.wasip1_dispatch_contexts_.empty()) { return world.wasip1_environments_.empty(); }
        if(thread.frames.empty() || world.wasip1_dispatch_contexts_.size()!=world.modules_.size() ||
           world.wasip1_modules_.size()!=world.modules_.size() ||
           world.actual_dense_module_owners_.size()!=world.modules_.size()) { return false; }
        for(auto const& frame:thread.frames)
        {
            if(frame.owner>=world.modules_.size()) { return false; }
            auto const dense{world.modules_[frame.owner].new_actual_id};
            if(dense>=world.wasip1_dispatch_contexts_.size() ||
               world.actual_dense_module_owners_[dense]!=frame.owner) { return false; }
            auto const& selected{world.wasip1_dispatch_contexts_.index_unchecked(dense)};
            auto const& binding{world.wasip1_modules_[dense]};
            if(!selected.import_visible)
            { if(selected.env!=nullptr || binding.environment!=SIZE_MAX) { return false; } continue; }
            // Exact world membership and exact NEW module memory precede
            // every pointee read or resolver invocation on the worker.
            if(binding.environment>=world.wasip1_environments_.size() ||
               selected.env!=::std::addressof(world.wasip1_environments_[binding.environment]->environment) ||
               selected.memory0!=binding.memory0 ||
               selected.memory0!=resolve_memory0_ptr(*world.modules_[frame.owner].actual) ||
               selected.env->wasip1_memory!=nullptr ||
               selected.env->wasip1_memory_resolver!=storage::resolve_current_wasip1_memory) { return false; }
        }
#else
        static_cast<void>(world);static_cast<void>(thread);
#endif
        return true;
    }
    explicit prepared_world_worker_wasip1_scope(runtime_checkpoint_world_transaction& world,
        prepared_world_thread const& thread) noexcept
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        :world_{world},thread_{thread},
         inherited_environment_{::std::addressof(::uwvm2::uwvm::imported::wasi::wasip1::storage::current_wasip1_env())},
         inherited_binding_{::uwvm2::uwvm::imported::wasi::wasip1::storage::current_wasip1_memory_binding_ref()},
         enabled_{!world.wasip1_dispatch_contexts_.empty()}
#endif
    {
#if defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) || !defined(UWVM_IMPORT_WASI_WASIP1)
        static_cast<void>(world);static_cast<void>(thread);
#endif
    }
    prepared_world_worker_wasip1_scope(prepared_world_worker_wasip1_scope const&)=delete;
    prepared_world_worker_wasip1_scope& operator=(prepared_world_worker_wasip1_scope const&)=delete;
    ~prepared_world_worker_wasip1_scope() noexcept { if(!remove()) { ::fast_io::fast_terminate(); } }
    [[nodiscard]] bool install() noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        if(!enabled_) { return true; }
        if(removed_ || environment_ || memory_ || inherited_binding_!=nullptr) { return false; }
        auto const& leaf{context(thread_.frames.back())};
        if(leaf.import_visible)
        {
            selected_environment_=leaf.env;selected_memory_=leaf.memory0;
            environment_.emplace(*leaf.env);memory_.emplace(*leaf.env,leaf.memory0);
        }
        // Keep the actual leaf context live while every caller's context is
        // nested and unwound. No old-world memory or mutable global selector.
        for(auto const& frame:thread_.frames)
        {
            auto const& selected{context(frame)};if(!selected.import_visible) { continue; }
            {
                env_scope nested_environment{*selected.env};
                memory_scope nested_memory{*selected.env,selected.memory0};
                if(::std::addressof(storage::current_wasip1_env())!=selected.env ||
                   selected.env->get_memory()!=selected.memory0) { return false; }
                {
                    memory_scope masked{*selected.env,nullptr};
                    if(selected.env->get_memory()!=nullptr) { return false; }
                }
                if(selected.env->get_memory()!=selected.memory0) { return false; }
            }
            if(!validate()) { return false; }
            ++visits_;
        }
#endif
        return validate();
    }
    [[nodiscard]] bool validate() const noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        if(removed_) { return false; }
        if(!selected_environment_)
        { return storage::current_wasip1_memory_binding_ref()==inherited_binding_ &&
                 ::std::addressof(storage::current_wasip1_env())==inherited_environment_; }
        return environment_ && memory_ &&
            storage::current_wasip1_memory_binding_ref()==::std::addressof(memory_->binding) &&
            ::std::addressof(storage::current_wasip1_env())==selected_environment_ &&
            selected_environment_->get_memory()==selected_memory_ && selected_environment_->wasip1_memory==nullptr;
#else
        return true;
#endif
    }
    [[nodiscard]] bool remove() noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
        if(removed_) { return restored_; }
        memory_.reset();environment_.reset();removed_=true;
        restored_=storage::current_wasip1_memory_binding_ref()==inherited_binding_ &&
            ::std::addressof(storage::current_wasip1_env())==inherited_environment_;
        return restored_;
#else
        return true;
#endif
    }
    [[nodiscard]] bool held() const noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        return selected_environment_!=nullptr;
#else
        return false;
#endif
    }
    [[nodiscard]] ::std::size_t visits() const noexcept
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        return visits_;
#else
        return 0u;
#endif
    }
};
