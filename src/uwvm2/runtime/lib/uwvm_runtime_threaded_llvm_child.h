// Include INSIDE uwvm2::runtime::lib after the existing cold LLVM materializer
// helpers. The separate *_headers.h belongs at translation-unit scope.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && \
    defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && defined(__linux__) && !defined(UWVM_MODULE)
extern "C++"
{
    class ThreadedLlvmChild final
    {
    public:
        enum class run_status:unsigned{completed,group_stopped,rejected};
        enum class preparation_status:unsigned{ok,invalid_world,unsupported_shape,budget,invalid_body,unsupported_bridge,native_failure};
        struct preparation_result
        {::std::unique_ptr<ThreadedLlvmChild> child;preparation_status status{preparation_status::invalid_world};};
    private:
        using world_type=::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld;
        using module_type=::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using memory_type=::uwvm2::object::memory::linear::native_memory_t;
        using lease_type=::uwvm2::runtime::wasm_threads::ThreadedGroup::ChildLease;
        using local_type=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t;
        using target_type=::uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t;
        using native_bridge=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::threaded_native_bridge;
        struct entries { ::std::string typed_name,raw_name;::std::uintptr_t typed{},raw{}; };
        struct external { ::std::string name;::std::uintptr_t address{}; };
        // Strong world/source precedes every borrowed object and executable.
        world_type::owner world_;
        module_type const* module_{};
        ::std::size_t imports_{},start_{},call_limit_{},code_limit_{};
        ::std::vector<local_type> locals_{};
        ::std::vector<entries> entries_{};
        ::std::vector<::std::uint_least32_t> local_type_ids_{};
        ::std::vector<memory_type*> memories_{};
        ::std::vector<::std::vector<target_type>> table_targets_{};
        ::std::vector<::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_view_t> table_views_{};
        ::std::size_t native_bytes_{},storage_bytes_{},storage_limit_{};
        ::uwvm2::object::memory::linear::threaded_grow_result last_grow_{};
        ::std::vector<native_bridge> emitted_bridges_{};
        ::std::vector<external> externals_{};
        class native_arena;
        ::std::unique_ptr<native_arena> native_arena_{};
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> context_{};
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine_{};
        // Listener dies before engine; engine/context die before source/world.
        ::uwvm2::utils::container::delete_owned_ptr<details::pending_llvm_jit_code_ranges> ranges_{};
        ::std::atomic_flag running_=ATOMIC_FLAG_INIT;
        bool bound_tables_{},ever_entered_{};
        struct execution final
        {
            ThreadedLlvmChild* child{};lease_type* lease{};
            ::std::size_t function{SIZE_MAX},depth{};
            bool rejected{},fp_active{};
        };
        inline static thread_local execution* active_{};
        explicit ThreadedLlvmChild(world_type::owner world):world_{::std::move(world)}{}
        ThreadedLlvmChild(ThreadedLlvmChild const&)=delete;
        ThreadedLlvmChild& operator=(ThreadedLlvmChild const&)=delete;
        ThreadedLlvmChild(ThreadedLlvmChild&&)=delete;
        static void fail(::std::uint32_t kind) noexcept
        {
            auto* const e=active_;
            if(e==nullptr)return; // Missing scope is rejected by raw entry, never guest success.
            e->rejected=true;
            e->lease->request_trap(kind,0u,e->function,UINT64_MAX);
        }
        [[nodiscard]] static bool stopped() noexcept
        {return active_==nullptr||active_->rejected||active_->lease->cancellation().stop_requested();}
        [[nodiscard]] static ::std::uintptr_t poll(::std::size_t function) noexcept
        {if(active_!=nullptr)active_->function=function;return stopped()?1u:0u;}
        [[nodiscard]] static ::std::uintptr_t enter(::std::size_t function) noexcept
        {
            if(active_==nullptr)return 1u;
            active_->function=function;++active_->depth;
            if(active_->depth>active_->child->call_limit_)
                fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));
            return stopped()?1u:0u;
        }
        static void leave() noexcept
        {if(active_==nullptr)return;if(active_->depth==0u)fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));else --active_->depth;}
        static void trap(::std::uint32_t kind,::std::uintptr_t,::std::uintptr_t) noexcept{fail(kind);}
        static void memory_trap(::std::size_t,::std::uint64_t,::std::uint64_t,::std::uint32_t,
            ::std::uint64_t,::std::size_t,::std::uintptr_t,::std::uintptr_t) noexcept
        {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::memory_out_of_bounds));}
        [[nodiscard]] static memory_type* actual_memory(::std::uintptr_t address) noexcept
        {
            if(active_==nullptr)return nullptr;
            for(auto* memory:active_->child->memories_)
                if(reinterpret_cast<::std::uintptr_t>(memory)==address)return memory;
            fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return nullptr;
        }
        // Exact original raw import ABI, but resolution is the real lease/world
        // dispatcher. No g_runtime, g_import_call_cache or parent native entry.
        static void import_call(::std::uintptr_t module,::std::size_t function,
            ::std::uintptr_t result,::std::size_t result_bytes,::std::uintptr_t params,::std::size_t param_bytes) noexcept
        {
            auto* e=active_;if(e==nullptr||stopped())return;
            if(module!=reinterpret_cast<::std::uintptr_t>(e->child->module_)||function>=e->child->imports_||
               (result_bytes!=0u&&result==0u)||(param_bytes!=0u&&params==0u))
            {fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return;}
            details::scoped_llvm_wasm_host_fp_environment_restore host_fp{e->fp_active};
            if(!host_fp.ready()){fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return;}
            auto const status=::uwvm2::runtime::wasm_threads::dispatch_import(*e->lease,*e->child->world_,0u,e->function,function,
                {reinterpret_cast<::std::byte*>(result),result_bytes},{reinterpret_cast<::std::byte const*>(params),param_bytes});
            using status_type=::uwvm2::runtime::wasm_threads::host_dispatch_status;
            if(status==status_type::rejected)fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));
            else if(status==status_type::group_stopped&&!e->lease->cancellation().stop_requested())
                fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));
        }
        template<unsigned Operation>
        [[nodiscard]] static ::std::uint32_t wait_notify(::std::uintptr_t memory_address,::std::uint32_t offset,
            ::std::uint32_t address,::std::uint64_t expected,::std::int64_t timeout) noexcept
        {
            static_assert(Operation<=2u);auto* memory=actual_memory(memory_address);if(memory==nullptr||stopped())return 0u;
            namespace wait=::uwvm2::runtime::wasm_threads;
            auto const effective=static_cast<::std::uint64_t>(offset)+address;
            try
            {
                auto const result=[&]{if constexpr(Operation==0u)return wait::memory_notify(*memory,effective,static_cast<::std::uint32_t>(expected));
                    else return wait::memory_wait<Operation==1u?4u:8u>(*memory,wait::is_shared(*memory),effective,expected,timeout);}();
                switch(result.status)
                {
                    case wait::wait_status::notified:case wait::wait_status::not_equal:case wait::wait_status::timed_out:
                        return Operation==0u?result.notified:static_cast<::std::uint32_t>(result.status);
                    case wait::wait_status::cancelled:
                        if(!active_->lease->cancellation().stop_requested())fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::atomic_wait_cancelled));
                        return 0u; // NEVER published as a fourth Wasm wait value: immediate IR poll returns.
                    case wait::wait_status::out_of_bounds:fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::memory_out_of_bounds));break;
                    case wait::wait_status::unaligned:fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::unaligned_atomic));break;
                    case wait::wait_status::not_shared:fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::atomic_wait_non_shared));break;
                    default:fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::atomic_wait_unavailable));break;
                }
            }
            catch(...){fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));}
            return 0u;
        }
        template<typename Memory>
        [[nodiscard]] static ::std::int32_t grow_actual(Memory* memory,::std::size_t original_maximum,::std::int32_t delta) noexcept
        {
            namespace linear=::uwvm2::object::memory::linear;using phase=linear::threaded_grow_status;
            if(active_==nullptr||memory==nullptr||stopped())return -1;
            auto* e=active_;
            auto record_failure=[&](phase why,bool tainted) noexcept
            {
                e->rejected=true;e->lease->report_memory_failure(static_cast<::std::uint32_t>(why),tainted,0u,e->function,UINT64_MAX);
            };
            // An actual launched main/child permanently closes old-engine
            // reuse. A bare token/prepare flag is insufficient for this effect.
            if(!e->lease->full_source_reset_required()){record_failure(phase::invalid,false);return -1;}
            if constexpr(requires(Memory& m){m.grow_threaded_profile(::std::size_t{},::std::size_t{},::std::stop_token{});})
            {
                auto const cap=(::std::min)(original_maximum,e->child->world_->admission().limits().max_shared_memory_bytes);
                auto const result=memory->grow_threaded_profile(static_cast<::std::uint32_t>(delta),cap,e->lease->cancellation());
                e->child->last_grow_=result;
                switch(result.status)
                {
                    case phase::success:return ::std::bit_cast<::std::int32_t>(static_cast<::std::uint32_t>(result.old_pages));
                    case phase::limit:case phase::commit_failed:return -1; // confirmed no visible length/protection change
                    case phase::cancelled:
                        if(!e->lease->cancellation().stop_requested())record_failure(phase::invalid,false);
                        return -1; // post-call stop probe dominates Guest result use
                    case phase::rollback_failed:record_failure(result.status,true);return -1;
                    case phase::invalid:record_failure(result.status,result.tainted);return -1;
                }
            }
            else record_failure(phase::invalid,false);
            return -1;
        }
        [[nodiscard]] static ::std::int32_t memory_grow(::std::uintptr_t address,::std::size_t original_maximum,::std::int32_t delta) noexcept
        {return grow_actual(actual_memory(address),original_maximum,delta);}
        [[nodiscard]] static ::std::int32_t memory_size(::std::uintptr_t address) noexcept
        {
            auto* memory=actual_memory(address);if(memory==nullptr||stopped())return 0;
            return static_cast<::std::int32_t>(memory->get_page_size());
        }
#include "uwvm_runtime_threaded_llvm_exception.h"
#include "uwvm_runtime_threaded_llvm_control.h"
#include "uwvm_runtime_threaded_llvm_memory.h"
#include "uwvm_runtime_threaded_llvm_stack.h"
#include "uwvm_runtime_threaded_llvm_factory.h"
    public:
        // Only the actual Group-owned dedicated compiler pthread may compile.
        // No synchronous Guest-stack fallback and no externally set ready bit.
        [[nodiscard]] static preparation_result prepare(world_type::owner world,
            ::uwvm2::runtime::wasm_threads::ThreadedGroup::CompilerLease const& compiler,
            ::std::size_t backend_storage_budget) noexcept
        {return build(::std::move(world),compiler,backend_storage_budget);}
        // Cold actual-active-segment rebind. No compiler/type-walk/allocation,
        // no ordinary compact-view update and no second engine. Root owns the
        // genuine unlaunched reservation, marks shared effects BEFORE active
        // writes and calls this before handing this backend to native launch.
        [[nodiscard]] bool refresh_table_views_before_launch() noexcept
        {
            if(running_.test_and_set())return false;
            struct release_flag{::std::atomic_flag& flag;~release_flag(){flag.clear();}} release{running_};
            if(ever_entered_||!world_||!world_->admission().owns_actual_generation())return false;
            auto code=world_->validated_code();
            if(!code||!code->covers(*world_))return false;
            // A failed fixed-shape validation never leaves callable half-views.
            // Root must full-reset after actual shared active effects, even
            // when native launch has not happened yet.
            if(!validate_table_targets_noalloc())
            {for(auto& view:table_views_)view={};bound_tables_=false;return false;}
            return refresh_table_targets_noalloc();
        }
        [[nodiscard]] auto const& retained_world() const noexcept{return world_;}
        [[nodiscard]] ::std::size_t owned_code_bytes() const noexcept{return native_bytes_;}
        [[nodiscard]] ::std::size_t owned_storage_bytes() const noexcept{return storage_bytes_;}
        // Fixed actual-code frame records only. This is not a Native Host/
        // compiler stack certificate and cannot authorize profile publication.
        [[nodiscard]] auto actual_fixed_frame_records() const noexcept
        {return ::std::span<native_frame_record const>{native_frames_};}
        [[nodiscard]] ::std::size_t actual_fixed_guest_stack_charge() const noexcept{return guest_fixed_stack_charge_;}

        [[nodiscard]] ::std::size_t compiled_guest_stack_usable_bytes() const noexcept{return guest_native_stack_usable_bytes_;}
        [[nodiscard]] ::std::size_t compiler_worker_stack_usable_bytes() const noexcept{return compiler_native_stack_usable_bytes_;}

        // Host reads only after actual worker retirement, never concurrently.
        [[nodiscard]] auto const& last_grow_observation() const noexcept{return last_grow_;}
        [[nodiscard]] run_status run_entry(lease_type& lease,::std::size_t function,
            ::std::span<::std::byte> results,::std::span<::std::byte const> params) noexcept
        {
            if(!world_||::std::addressof(lease.world())!=world_.get()||!lease.owns_current_native_thread()||
               ::std::addressof(lease.admission())!=::std::addressof(world_->admission())||active_!=nullptr||
               lease.native_stack_usable_bytes()!=guest_native_stack_usable_bytes_||
               function<imports_||function-imports_>=entries_.size()||entries_[function-imports_].raw==0u)
                return run_status::rejected;
            auto const& declaration=module_->local_defined_function_vec_storage.index_unchecked(function-imports_);
            if(declaration.function_type_ptr==nullptr)return run_status::rejected;
            auto const layout=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_runtime_wasm_call_abi_layout(*declaration.function_type_ptr);
            if(!layout.valid||layout.parameter_bytes!=params.size()||layout.result_bytes!=results.size()||running_.test_and_set())
                return run_status::rejected;
            struct reset_flag{::std::atomic_flag& value;~reset_flag(){value.clear();}} reset{running_};
            if(!bound_tables_)return run_status::rejected;
            ever_entered_=true;
            if(lease.cancellation().stop_requested())return run_status::group_stopped;
            execution e{this,::std::addressof(lease)};active_=::std::addressof(e);
            struct reset_active{~reset_active(){active_=nullptr;}} reset_scope;
            ::uwvm2::runtime::wasm_threads::execution_scope waits{lease.waits(),lease.cancellation()};
            details::scoped_llvm_wasm_fp_environment guest_fp{e.fp_active};
            if(!guest_fp.ready()){fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return run_status::rejected;}
            using entry=void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t,::std::uintptr_t,::std::size_t,::std::uintptr_t,::std::size_t);
            auto actual=reinterpret_cast<entry>(entries_[function-imports_].raw);
            actual(0u,reinterpret_cast<::std::uintptr_t>(results.data()),results.size(),
                reinterpret_cast<::std::uintptr_t>(params.data()),params.size());
            if(e.depth!=0u){fail(static_cast<::std::uint32_t>(llvm_jit_trap_kind::runtime_invariant_failure));return run_status::rejected;}
            if(!e.rejected&&!lease.cancellation().stop_requested()&&eh_pending_!=0u)
            {exception_failure(eh_failure::uncaught);return run_status::rejected;}
            if(e.rejected)return run_status::rejected;
            return lease.cancellation().stop_requested()?run_status::group_stopped:run_status::completed;
        }
        [[nodiscard]] run_status run(lease_type& lease,::std::uint32_t tid,::std::uint32_t argument) noexcept
        {
            if(tid==0u||tid>=(1u<<29u))return run_status::rejected;
            // Native-endian raw ABI, not guest offsets; libc owns stack/TLS setup.
            ::std::array<::std::uint32_t,2u> params{tid,argument};
            return run_entry(lease,start_,{},::std::as_bytes(::std::span{params}));
        }
        ~ThreadedLlvmChild()
        {
            // Root group owns this object until real pthread join. A live entry
            // cannot be detached, handed off, reset or destroyed as "retired".
            if(running_.test())::fast_io::fast_terminate();
            // Views are this engine's private allocation, never parent module
            // arrays. Detach before target vectors / engine / world destruction.
            for(auto& view:table_views_)view={};
        }
    };
    inline ::std::uintptr_t threaded_exception_bridge_api::pending() noexcept{return ThreadedLlvmChild::exception_pending();}
    inline void threaded_exception_bridge_api::publish(::std::uintptr_t module,::std::uintptr_t tag,::std::uintptr_t tuple,::std::size_t bytes) noexcept
    {ThreadedLlvmChild::exception_publish(module,tag,tuple,bytes);}
    inline ::std::uintptr_t threaded_exception_bridge_api::matches(::std::uintptr_t token,::std::uintptr_t tag) noexcept
    {return ThreadedLlvmChild::exception_matches(token,tag);}
    inline void threaded_exception_bridge_api::copy(::std::uintptr_t token,::std::uintptr_t tag,::std::uintptr_t output,::std::size_t bytes) noexcept
    {ThreadedLlvmChild::exception_copy(token,tag,output,bytes);}
    inline void threaded_exception_bridge_api::make_reference(::std::uintptr_t token,::std::uintptr_t output) noexcept
    {ThreadedLlvmChild::exception_make_reference(token,output);}
    inline void threaded_exception_bridge_api::consume(::std::uintptr_t token) noexcept{ThreadedLlvmChild::exception_consume(token);}
    inline void threaded_exception_bridge_api::throw_reference(::std::uintptr_t module,::std::uintptr_t input) noexcept
    {ThreadedLlvmChild::exception_throw_reference(module,input);}
}
#endif
