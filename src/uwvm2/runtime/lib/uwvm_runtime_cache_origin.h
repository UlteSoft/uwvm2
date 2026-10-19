// Include INSIDE the actual uwvm2::runtime::lib TU, after the WASIp1 context
// resolver and ordinary execution-scope definitions, before full materialize.
// Required headers (memory/sysstat/sysmacros/unistd/runtime_origin) are TU-scope.
#if defined(__linux__) && defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_USE_THREAD_LOCAL) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE)
extern "C++"
{
    class runtime_cache_origin_bridge final
    {
    public:
        class batch;
    private:
        using source = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using ticket = ::uwvm2::runtime::llvm_jit_cache::runtime_origin::ticket;
        using use = ::uwvm2::runtime::llvm_jit_cache::runtime_origin::use;
        using reference = ::uwvm2::runtime::llvm_jit_cache::runtime_origin::reference;
        struct state final : ticket
        {
            // Declaration order releases Source BEFORE the genuine generation
            // lease. Reset cannot destroy providers/resources while an I/O use
            // remains. Metadata and engine captures never own this strong state.
            ::uwvm2::utils::thread::execution_domain::lease generation;
            source::owner original;
            ::std::uint_least64_t epoch;
            ::std::atomic_bool secret_phase{true};
            state(::uwvm2::utils::thread::execution_domain::lease actual, source::owner input,
                ::std::uint_least64_t observed) noexcept
                : generation{::std::move(actual)}, original{::std::move(input)}, epoch{observed} {}
        private:
            [[nodiscard]] bool actual_secret_phase() const noexcept override
            { return secret_phase.load(::std::memory_order_acquire); }
            [[nodiscard]] bool actual_current() const noexcept override
            {
                // No Root publication lock or mutable provider/FD re-read here:
                // parallel workers run while their parent holds that very lock.
                // Only this real ticket's live lease + immutable closed-capability
                // census survive the short cold constructor below.
                return generation && !generation.stop_requested() && epoch == current_runtime_generation() &&
                    ::uwvm2::runtime::llvm_jit_cache::kernel_origin::dumping_disabled() &&
                    source::has_canonical_owner(original) && original->initialized_from_actual_state();
            }
        };
        inline static thread_local batch* current_{};

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        [[nodiscard]] static bool safe_environment(wasip1_env_type& environment) noexcept
        {
            namespace storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
            namespace fd = ::uwvm2::imported::wasi::wasip1::fd_manager;
            if(!environment.mount_dir_roots.empty() || environment.trace_wasip1_call ||
               !environment.trace_wasip1_output_file_path_storage.empty() ||
               environment.debug_console_read || environment.debug_console_write || environment.debug_fd_created ||
               environment.wasip1_proc_exit_func_ptr != storage::uwvm_wasip1_proc_exit_func_ptr_overload ||
               environment.wasip1_proc_raise_func_ptr || environment.wasip1_sched_yield_func_ptr ||
               environment.wasip1_memory_resolver != storage::resolve_current_wasip1_memory) { return false; }
#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            if(!environment.preopen_sockets.empty()) { return false; }
#endif
            auto& map{environment.fd_storage};
            ::uwvm2::utils::mutex::rw_shared_guard_t maps{map.fds_rwlock};
            // First cold full publication has not executed Guest code. Reject
            // unknown/renumbered/extra FD graphs, not only CLI mount strings.
            if(map.opens.size() != 3u || !map.renumber_map.empty() || !map.closes.empty()) { return false; }
            for(::std::size_t index{}; index != 3u; ++index)
            {
                auto* cell{map.opens.index_unchecked(index).fd_p};
                if(cell == nullptr || cell->wasi_fd.ptr == nullptr || cell->close_pos != SIZE_MAX) { return false; }
                ::uwvm2::utils::mutex::mutex_guard_t locked{cell->fd_mutex};
                auto const& storage{cell->wasi_fd.ptr->wasi_fd_storage};
                if(storage.type != fd::wasi_fd_type_e::file) { return false; } // no borrowed/unknown/native directory
                int const handle{storage.storage.file_fd.native_handle()};
                struct ::stat metadata{};
                if(handle < 0 || ::fstat(handle, &metadata) != 0) { return false; }
                // Deny ALL regular files, including read-only /proc/self/mem,
                // as well as directories and unknown devices. Accepted stdio
                // has no filesystem namespace or fd-passing WASIp1 operation.
                bool const null_device{S_ISCHR(metadata.st_mode) && metadata.st_rdev == ::makedev(1u, 3u)};
                bool const terminal{S_ISCHR(metadata.st_mode) && ::isatty(handle) == 1};
                if(!S_ISFIFO(metadata.st_mode) && !S_ISSOCK(metadata.st_mode) && !null_device && !terminal) { return false; }
            }
            return true;
        }
        [[nodiscard]] static bool safe_wasi_environments() noexcept
        {
            namespace storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
            if(::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api || !safe_environment(storage::default_wasip1_env)) { return false; }
            for(auto& configured : storage::configured_wasip1_groups)
            {
                if(configured.expose_host_api || !configured.mount_dir_roots.empty() || configured.trace_wasip1_call ||
                   !configured.trace_wasip1_output_file_path_storage.empty()) { return false; }
#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
                if(!configured.preopen_sockets.empty()) { return false; }
#endif
                if(!safe_environment(configured.env)) { return false; }
            }
            // Check the actual per-module env binding too: it may differ from
            // the default/Single/Group configuration that initialized it.
            if(g_wasip1_runtime_module_context_cache.size() != g_runtime.modules.size()) { return false; }
            for(auto const& actual : g_wasip1_runtime_module_context_cache)
            { if(actual.env == nullptr || !safe_environment(*actual.env)) { return false; } }
            return true;
        }
#endif
        [[nodiscard]] static ::std::shared_ptr<state> mint(compiled_module_record const& record) noexcept
        {
            namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
            if(!::uwvm2::runtime::llvm_jit_cache::kernel_origin::dumping_disabled() ||
               get_runtime_state_publication_depth() != 1u || get_runtime_execution_entry_depth() != 1u ||
               get_runtime_execution_lease() == nullptr || get_runtime_execution_lease()->stop_requested() ||
               g_runtime.compiled_all.load(::std::memory_order_acquire) ||
               g_runtime.published_runtime_state.kind != details::runtime_state_kind::empty ||
               g_runtime.bridges_initialized.load(::std::memory_order_acquire) ||
               mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
               mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
               g_runtime.debug_pause_control || g_runtime.checkpoint_profile || g_runtime.modules.size() != 1u ||
               &g_runtime.modules.index_unchecked(0u) != &record || !record.llvm_jit_full_publication ||
               record.llvm_jit_full_publication->checkpoint_profile || record.llvm_jit_full_publication->plan)
            { return {}; }
#if defined(UWVM2_HAS_THREADED_PROFILE_HOST_API) && UWVM2_HAS_THREADED_PROFILE_HOST_API
            if(runtime_threaded_profile_builtin_requested_host_api()) { return {}; } // real atomic, never Root-lock recursion
#endif
            auto original{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            auto const* module{record.runtime_module};
            if(!source::has_canonical_owner(original) || !original->initialized_from_actual_state() ||
               module == nullptr || original->initialized_main_module() != module || original->preloaded_file_count() != 0u ||
               &original->registry() != &::uwvm2::uwvm::runtime::storage::active_runtime_registry() ||
               original->registry().size() != 1u ||
               record.llvm_jit_full_publication->source.get() != original.get() ||
               record.llvm_jit_full_publication->source.owner_before(original) ||
               original.owner_before(record.llvm_jit_full_publication->source)) { return {}; }
            auto const census{original->actual_cache_native_provider_census()};
            if(census == source::cache_native_provider_census::unsafe) { return {}; }
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            if(census == source::cache_native_provider_census::builtin_wasip1 && !safe_wasi_environments()) { return {}; }
#else
            if(census != source::cache_native_provider_census::no_native) { return {}; }
#endif
            // Admission is acquired while the actual outer entry is still
            // admitted. It may now fail during stop: that is a cache miss,
            // never a fabricated private generation or a compile fallback.
            auto generation{g_runtime.execution_domain.try_enter()};
            if(!generation || generation.stop_requested()) { return {}; }
            try { return ::std::shared_ptr<state>{new state{::std::move(generation), ::std::move(original), current_runtime_generation()}}; }
            catch(...) { return {}; }
        }
    public:
        class batch
        {
            friend class runtime_cache_origin_bridge;
            batch* previous_{};
            ::std::shared_ptr<state> actual_{};
        public:
            explicit batch(compiled_module_record const& actual) noexcept
                : previous_{current_}, actual_{previous_ == nullptr ? mint(actual) : ::std::shared_ptr<state>{}}
            { current_ = this; } // even a rejected nested scope masks any outer ticket
            batch(batch const&) = delete;
            batch& operator=(batch const&) = delete;
            ~batch()
            {
                if(actual_) { actual_->secret_phase.store(false, ::std::memory_order_release); }
                current_ = previous_;
            } // actual_ released after TLS/secret-read phase are cleared
        };
        [[nodiscard]] static reference capture() noexcept
        { return current_ && current_->actual_ && current_->actual_->authorized() ? reference{current_->actual_} : reference{}; }
    };
    [[gnu::visibility("hidden")]]
    ::uwvm2::runtime::llvm_jit_cache::runtime_origin::reference runtime_cache_current_origin() noexcept
    { return runtime_cache_origin_bridge::capture(); }
}
#endif
