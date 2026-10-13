/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <thread>
# include <cstdint>
# include <memory>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    class llvm_jit_debug_native_activation_cursor;
    using llvm_jit_debug_native_activation_cursor_owner = ::std::shared_ptr<llvm_jit_debug_native_activation_cursor const>;
    // A runtime-minted immutable borrower descriptor. Only the private physical
    // cursor constructs one. Numeric PCs, copied snapshots and console requests
    // cannot supply its callbacks/context. The controller retains its strong
    // cursor until the actual backend worker has acknowledged session release.
    class llvm_jit_debug_native_activation_provider final
    {
        // Copying a trusted host descriptor retains the real private owner;
        // stale provider copies cannot leave a freed raw callback context.
        // This strong owner is never read or modified in the kernel handler.
        llvm_jit_debug_native_activation_cursor_owner owner_{};
        void* context_{};
        bool (*witness_)(void*, void const*, ::std::uint_least64_t, ::std::uintptr_t, ::std::uintptr_t) noexcept{};
        void (*leave_)(void*, void const*, ::std::uint64_t, unsigned) noexcept{};
        bool (*descendant_)(void*, void const*, ::std::uint_least64_t, ::std::uintptr_t, ::std::uintptr_t) noexcept{};
        void const* session_{};
        ::std::uint_least64_t thread_{};
        ::std::uintptr_t begin_{}, end_{}, first_{};
        friend class llvm_jit_debug_native_activation_cursor;
        llvm_jit_debug_native_activation_provider(llvm_jit_debug_native_activation_cursor_owner owner, void* context,
            bool (*witness)(void*, void const*, ::std::uint_least64_t, ::std::uintptr_t, ::std::uintptr_t) noexcept,
            void (*leave)(void*, void const*, ::std::uint64_t, unsigned) noexcept,
            void const* session, ::std::uint_least64_t thread, ::std::uintptr_t begin,
            ::std::uintptr_t end, ::std::uintptr_t first,
            bool (*descendant)(void*, void const*, ::std::uint_least64_t, ::std::uintptr_t, ::std::uintptr_t) noexcept = nullptr) noexcept
            : owner_{owner}, context_{context}, witness_{witness}, leave_{leave}, descendant_{descendant},
              session_{session}, thread_{thread}, begin_{begin}, end_{end}, first_{first} {}
    public:
        llvm_jit_debug_native_activation_provider() = default;
        // Copying rvalues also retains ownership. Suppress implicit moves whose
        // moved-from raw callbacks would otherwise outlive their strong owner.
        llvm_jit_debug_native_activation_provider(llvm_jit_debug_native_activation_provider const&) = default;
        llvm_jit_debug_native_activation_provider& operator=(llvm_jit_debug_native_activation_provider const&) = default;
        [[nodiscard]] bool valid() const noexcept { return context_ != nullptr && witness_ != nullptr && leave_ != nullptr; }
        [[nodiscard]] bool bound_to(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t begin, ::std::uintptr_t end, ::std::uintptr_t first) const noexcept
        { return valid() && session == session_ && thread == thread_ && begin == begin_ && end == end_ && first == first_; }
        [[nodiscard]] bool witness(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t pc, ::std::uintptr_t sp) const noexcept
        { return valid() && session == session_ && thread == thread_ && witness_(context_, session, thread, pc, sp); }
        void report_leave(void const* session, ::std::uint64_t incarnation, unsigned kind) const noexcept
        { if(valid() && session == session_) { leave_(context_, session, incarnation, kind); } }
        [[nodiscard]] bool descendant(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t pc, ::std::uintptr_t sp) const noexcept
        { return valid() && descendant_ && session==session_ && thread==thread_ && descendant_(context_,session,thread,pc,sp); }
    };
    class llvm_jit_debug_native_return_continuation;
    class llvm_jit_debug_native_return_event;
    struct llvm_jit_debug_native_caller_view;
    // Opaque backend identity preserves the native session type's own module
    // attachment. No second forward declaration in uwvm2.runtime is permitted.
    extern "C++" bool llvm_jit_debug_continue_native_return_event_host_api(
        void*, llvm_jit_debug_native_return_event const&, ::std::uint64_t, int) noexcept;
    extern "C++" bool llvm_jit_debug_query_native_return_event_host_api(
        llvm_jit_debug_native_return_event const&, void const*, llvm_jit_debug_native_caller_view&) noexcept;
    // Host-private sealed cross-owner return event. The sole issuer freshly
    // authenticates the origin trap under domain/publication through wake.
    // A display identity or copied PC cannot construct this object. No CFA/SP
    // getter exists; only the closed backend transition consumes that evidence.
    class llvm_jit_debug_native_return_event final
    {
        struct resume_window
        {
            ::std::thread::id host{::std::this_thread::get_id()};
            ::std::atomic_bool active{};
        };
        ::std::shared_ptr<resume_window> window_{};
        ::std::shared_ptr<llvm_jit_debug_native_return_continuation const> proof_{};
        llvm_jit_debug_native_activation_cursor_owner parent_{};
        llvm_jit_debug_native_activation_provider provider_{};
        void const* session_{};
        ::std::uint_least64_t thread_{};
        ::std::uintptr_t origin_pc_{}, origin_sp_{}, origin_begin_{}, origin_end_{};
        ::std::uint64_t revision_{};
        ::std::uintptr_t target_{}, stack_{}, begin_{}, end_{};
        ::std::uintptr_t software_skip_{};
        ::std::array<unsigned char,4u> software_original_{}, software_skip_original_{};
        friend class llvm_jit_debug_native_return_continuation;
        friend bool ::uwvm2::runtime::lib::llvm_jit_debug_query_native_return_event_host_api(
            llvm_jit_debug_native_return_event const&, void const*, llvm_jit_debug_native_caller_view&) noexcept;
        llvm_jit_debug_native_return_event(
            ::std::shared_ptr<resume_window> window,
            ::std::shared_ptr<llvm_jit_debug_native_return_continuation const> proof,
            llvm_jit_debug_native_activation_cursor_owner parent,
            llvm_jit_debug_native_activation_provider provider, void const* session,
            ::std::uint_least64_t thread, ::std::uintptr_t origin_pc, ::std::uintptr_t origin_sp,
            ::std::uintptr_t origin_begin, ::std::uintptr_t origin_end, ::std::uint64_t revision,
            ::std::uintptr_t target, ::std::uintptr_t stack, ::std::uintptr_t begin, ::std::uintptr_t end,
            ::std::uintptr_t software_skip=0u, ::std::array<unsigned char,4u> software_original={},
            ::std::array<unsigned char,4u> software_skip_original={}) noexcept
            : window_{window}, proof_{proof}, parent_{parent}, provider_{provider}, session_{session}, thread_{thread},
              origin_pc_{origin_pc}, origin_sp_{origin_sp}, origin_begin_{origin_begin}, origin_end_{origin_end},
              revision_{revision}, target_{target}, stack_{stack}, begin_{begin}, end_{end},
              software_skip_{software_skip},software_original_{software_original},software_skip_original_{software_skip_original} {}
    public:
        llvm_jit_debug_native_return_event() = default;
        llvm_jit_debug_native_return_event(llvm_jit_debug_native_return_event const&) = default;
        llvm_jit_debug_native_return_event& operator=(llvm_jit_debug_native_return_event const&) = default;
        [[nodiscard]] bool valid() const noexcept
        { return proof_ && parent_ && provider_.valid() && session_ && revision_ && revision_ != UINT64_MAX; }
        // Trusted event installer only; never included in console/wire views.
        [[nodiscard]] ::std::uint_least64_t thread() const noexcept { return valid() ? thread_ : 0u; }
        [[nodiscard]] ::std::uintptr_t event_pc() const noexcept { return valid() ? target_ : 0u; }
    private:
        friend bool ::uwvm2::runtime::lib::llvm_jit_debug_continue_native_return_event_host_api(
            void*, llvm_jit_debug_native_return_event const&, ::std::uint64_t, int) noexcept;
        // Closed, synchronous backend borrow: supplied kernel state only tests
        // the seal. No supplied address nominates a target or stack position.
        template<typename Install>
        [[nodiscard]] bool install(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t pc, ::std::uintptr_t sp, ::std::uintptr_t begin,
            ::std::uintptr_t end, ::std::uint64_t revision, Install&& install) const noexcept
        {
            if(!valid() || !window_ || window_->host != ::std::this_thread::get_id() ||
               !window_->active.load(::std::memory_order_acquire) || session != session_ || thread != thread_ || pc != origin_pc_ || sp != origin_sp_ ||
               begin != origin_begin_ || end != origin_end_ || revision != revision_ ||
               target_ < begin_ || target_ >= end_ || stack_ <= origin_sp_ ||
               !provider_.bound_to(session_,thread_,begin_,end_,target_)) { return false; }
            // Host backend only; stack evidence never becomes an inspection API.
            if constexpr(requires { install(provider_,begin_,end_,target_,stack_,software_skip_,software_original_,software_skip_original_); })
            { install(provider_,begin_,end_,target_,stack_,software_skip_,software_original_,software_skip_original_); }
            else { install(provider_, begin_, end_, target_, stack_); }
            return true;
        }
    };
    // Closed runtime-issued software breakpoint plan. No console address,
    // copied instruction or provider callback can construct an executable plan.
    // The private cursor retains the complete engine/code generation owner.
    class llvm_jit_debug_native_breakpoint_plan final
    {
        llvm_jit_debug_native_activation_cursor_owner owner_{};
        void const* session_{};
        ::std::uint_least64_t thread_{};
        ::std::uintptr_t begin_{}, end_{}, pc_{};
        ::std::uint64_t revision_{};
        ::std::array<::std::uintptr_t, 3u> sites_{};
        ::std::array<::std::array<unsigned char, 4u>, 3u> original_{};
        unsigned width_{}, count_{};
        bool initial_{};
        ::std::array<::std::uintptr_t, 2u> delayed_pc_{}, delayed_npc_{};
        ::std::array<bool, 2u> delayed_slot_{};
        friend class llvm_jit_debug_native_activation_cursor;
        llvm_jit_debug_native_breakpoint_plan(llvm_jit_debug_native_activation_cursor_owner owner,
            void const* session, ::std::uint_least64_t thread, ::std::uintptr_t begin, ::std::uintptr_t end,
            ::std::uintptr_t pc, ::std::uint64_t revision, bool initial, unsigned width, unsigned count,
            ::std::array<::std::uintptr_t, 3u> sites,
            ::std::array<::std::array<unsigned char, 4u>, 3u> original,
            ::std::array<::std::uintptr_t, 2u> delayed_pc = {},
            ::std::array<::std::uintptr_t, 2u> delayed_npc = {},
            ::std::array<bool, 2u> delayed_slot = {}) noexcept
            : owner_{owner}, session_{session}, thread_{thread}, begin_{begin}, end_{end}, pc_{pc},
              revision_{revision}, sites_{sites}, original_{original}, width_{width}, count_{count}, initial_{initial}, delayed_pc_{delayed_pc}, delayed_npc_{delayed_npc}, delayed_slot_{delayed_slot} {}
    public:
        llvm_jit_debug_native_breakpoint_plan() = default;
        llvm_jit_debug_native_breakpoint_plan(llvm_jit_debug_native_breakpoint_plan const&) = default;
        llvm_jit_debug_native_breakpoint_plan& operator=(llvm_jit_debug_native_breakpoint_plan const&) = default;
        [[nodiscard]] bool valid() const noexcept { return owner_ && session_ && width_ && count_; }
        [[nodiscard]] bool bound_to(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t begin, ::std::uintptr_t end, ::std::uintptr_t pc, ::std::uint64_t revision, bool initial) const noexcept
        { return valid() && session_ == session && thread_ == thread && begin_ == begin && end_ == end && pc_ == pc && (initial || revision_ == revision) && initial_ == initial; }
        [[nodiscard]] bool accepts_kernel_npc(::std::uintptr_t pc, ::std::uintptr_t npc, bool first) const noexcept
        {
            if(!valid()) { return false; }
            if(first || delayed_npc_[0u] == 0u) { return pc <= UINTPTR_MAX - 4u && npc == pc + 4u; }
            for(unsigned i{}; i != 2u; ++i)
            { if(delayed_npc_[i] != 0u && delayed_pc_[i] == pc && delayed_npc_[i] == npc) { return true; } }
            return false;
        }
        // Closed-plan fact for an already accepted real PC/NPC pair. This
        // cannot qualify a supplied snapshot or mint execution authority.
        [[nodiscard]] bool in_delay_slot(::std::uintptr_t pc, ::std::uintptr_t npc) const noexcept
        {
            if(!valid()) { return false; }
            for(unsigned i{}; i != 2u; ++i)
            { if(delayed_slot_[i] && delayed_npc_[i] != 0u && delayed_pc_[i] == pc && delayed_npc_[i] == npc) { return true; } }
            return false;
        }
        [[nodiscard]] unsigned width() const noexcept { return width_; }
        [[nodiscard]] unsigned count() const noexcept { return count_; }
        [[nodiscard]] ::std::uintptr_t site(unsigned index) const noexcept { return index < count_ ? sites_[index] : 0u; }
        [[nodiscard]] ::std::array<unsigned char, 4u> original(unsigned index) const noexcept
        { return index < count_ ? original_[index] : ::std::array<unsigned char, 4u>{}; }
    };
    extern "C++" bool llvm_jit_debug_native_breakpoint_plan_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const* native_session_identity,
        bool initial, llvm_jit_debug_native_breakpoint_plan&) noexcept;
    class llvm_jit_debug_native_call_continuation;
    class llvm_jit_debug_native_call_event;
    extern "C++" bool llvm_jit_debug_continue_native_call_event_host_api(void*, llvm_jit_debug_native_call_event const&) noexcept;
    // A synchronous resume seal, not an address request. Only the private
    // actual-call issuer constructs it while retaining the canonical cursor.
    class llvm_jit_debug_native_call_event final
    {
        struct resume_window
        {
            ::std::thread::id host{::std::this_thread::get_id()};
            ::std::atomic_bool active{};
        };
        ::std::shared_ptr<resume_window> window_{};
        ::std::shared_ptr<llvm_jit_debug_native_call_continuation const> proof_{};
        llvm_jit_debug_native_breakpoint_plan plan_{};
        ::std::uintptr_t stack_{};
        friend class llvm_jit_debug_native_call_continuation;
        friend bool ::uwvm2::runtime::lib::llvm_jit_debug_continue_native_call_event_host_api(
            void*, llvm_jit_debug_native_call_event const&) noexcept;
        llvm_jit_debug_native_call_event(::std::shared_ptr<resume_window> window,
            ::std::shared_ptr<llvm_jit_debug_native_call_continuation const> proof,
            llvm_jit_debug_native_breakpoint_plan plan, ::std::uintptr_t stack) noexcept
            : window_{window},proof_{proof},plan_{plan},stack_{stack} {}
        template<typename Install>
        [[nodiscard]] bool install(void const* session, ::std::uint_least64_t thread,
            ::std::uintptr_t begin, ::std::uintptr_t end, ::std::uintptr_t pc,
            ::std::uintptr_t stack, ::std::uint64_t revision, Install&& install) const noexcept
        {
            if(!window_ || !proof_ || window_->host!=::std::this_thread::get_id() ||
               !window_->active.load(::std::memory_order_acquire) || stack==0u || stack!=stack_ ||
               !plan_.bound_to(session,thread,begin,end,pc,revision,false) ||
               plan_.count()<2u || plan_.count()>3u || plan_.site(0u)<=pc || plan_.site(0u)>=end) { return false; }
            return install(plan_,stack_);
        }
    public:
        llvm_jit_debug_native_call_event() = default;
    };
}
