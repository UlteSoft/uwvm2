/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <uwvm2/utils/macro/push_macros.h>

namespace uwvm2::runtime::lib::details
{
    inline constexpr ::std::size_t llvm_jit_debug_max_captured_locals{256u};
    inline constexpr ::std::size_t llvm_jit_debug_local_slot_bytes{16u};
    // Internal debug-full table serialization. The guard is inert for every
    // ordinary backend. It is shared by the initializer and LLVM table helpers
    // so imported aliases refer to the same actual-table lock domain.
    extern "C++" bool llvm_jit_debug_table_lock_abi_bridge() noexcept;
    extern "C++" void llvm_jit_debug_table_unlock_abi_bridge() noexcept;
    class llvm_jit_debug_table_guard
    {
        bool locked_{llvm_jit_debug_table_lock_abi_bridge()};
    public:
        llvm_jit_debug_table_guard() noexcept = default;
        llvm_jit_debug_table_guard(llvm_jit_debug_table_guard const&) = delete;
        llvm_jit_debug_table_guard& operator=(llvm_jit_debug_table_guard const&) = delete;
        ~llvm_jit_debug_table_guard() noexcept
        { if(locked_) { llvm_jit_debug_table_unlock_abi_bridge(); } }
    };
    // Generated debug-full call_indirect only. Return 0 after copying one
    // consistent target to the host JIT alloca; return 1 for out-of-bounds.
    // All narrow integer parameters use register-width ABI carriers.
    extern "C++" ::std::uintptr_t llvm_jit_debug_call_indirect_target_abi_bridge(
        ::std::uintptr_t module_address, ::std::uintptr_t table_index,
        ::std::uint64_t selector, ::std::uintptr_t target_output_address) noexcept;
    // Generated call_ref/return_call_ref passes a complete host-side funcref
    // alloca and an aligned target-record alloca. 0 means copied target; 1
    // means null reference. Invalid storage identities terminate in the host.
    extern "C++" ::std::uintptr_t llvm_jit_resolve_call_ref_target_abi_bridge(
        ::std::uintptr_t caller_module_address, ::std::uintptr_t funcref_address,
        ::std::uintptr_t target_output_address) noexcept;
    /// Internal import bridge emitted only into generated Wasm code. This is deliberately separate from the public
    /// host/re-entry API: callers must already own the generated-bridge depth token, canonical LLVM-Wasm FP scope and,
    /// when selected, native-unwind execution gate. Native-provider callbacks suspend that token before invoking host code.
    extern "C++" void llvm_jit_call_raw_from_generated_wasm(void const* runtime_module_ptr,
                                                             ::std::uint_least32_t func_index,
                                                             void* result_buffer,
                                                             ::std::size_t result_bytes,
                                                             void const* param_buffer,
                                                             ::std::size_t param_bytes) UWVM_THROWS;

    /// Materialize a generated local tail target without invoking it. The caller
    /// subsequently emits musttail with the validated tail-compatible typed prototype.
    /// This internal bridge requires an existing generated-code execution token.
    extern "C++" ::std::uintptr_t llvm_jit_resolve_tail_target_abi_bridge(
        ::std::uintptr_t runtime_module_address, ::std::uintptr_t function_index) noexcept;

    /// Resolve a checked table target without invoking it. Zero denotes a host
    /// leaf; defined Wasm targets always return a published typed native entry.
    extern "C++" ::std::uintptr_t llvm_jit_resolve_indirect_tail_target_abi_bridge(
        ::std::uintptr_t runtime_module_address, ::std::uintptr_t table_index, ::std::uintptr_t selector) noexcept;

    /// Resolve the copied call_ref identity to a typed native musttail target.
    /// This may materialize a lazy/tiered definition after releasing the debug
    /// table lock. Zero denotes an imported host leaf handled by the raw adapter.
    extern "C++" ::std::uintptr_t llvm_jit_resolve_call_ref_tail_target_abi_bridge(
        ::std::uintptr_t caller_module_address, ::std::uintptr_t funcref_address) noexcept;

    /// Exact machine-level ABI used by generated LLVM code. Keep every integer operand register-wide: some ABIs attach
    /// target-specific extension attributes even to uint32_t parameters/returns. The wrapper validates and narrows the
    /// Wasm function index before forwarding to the typed implementation above.
    extern "C++" void llvm_jit_call_raw_from_generated_wasm_abi_bridge(::std::uintptr_t runtime_module_address,
                                                                        ::std::uintptr_t func_index,
                                                                        ::std::uintptr_t result_buffer_address,
                                                                        ::std::size_t result_bytes,
                                                                        ::std::uintptr_t param_buffer_address,
                                                                        ::std::size_t param_bytes) UWVM_THROWS;
    // Debug-full exact event hooks. Only validated generated code under the
    // genuine entry token may call them. Returned identity never enters Wasm.
    extern "C++" ::std::uint64_t llvm_jit_debug_activation_enter_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t function_index, ::std::uint64_t compiled_generation) noexcept;
    extern "C++" void llvm_jit_debug_activation_leave_abi_bridge(
        ::std::uint64_t incarnation, ::std::uintptr_t exit_kind) noexcept;

    // Explicit checkpoint-profile-only producer. Incarnation is the private
    // genuine entry result; runtime resolves the canonical sealed plan from
    // actual publication before interpreting this frame-owned native range.
    extern "C++" void llvm_jit_checkpoint_materialize_abi_bridge(
        ::std::uint64_t incarnation, ::std::uint64_t site, ::std::uintptr_t native_slots, ::std::size_t bytes) noexcept;
    // Selected immutable checkpoint policy only: genuine current frame-owned
    // N*16 typed native values plus N-local executed 0/1 flags. Both borrowed
    // native ranges are copied synchronously after canonical owner/plan checks.
    extern "C++" void llvm_jit_checkpoint_materialize_dynamic_abi_bridge(
        ::std::uint64_t incarnation, ::std::uint64_t site, ::std::uintptr_t native_slots,
        ::std::size_t bytes, ::std::uintptr_t native_flags, ::std::size_t flag_count) noexcept;
    // Observer-only allocation/cleanup leaves. Validated compiler sites determine
    // one final constant extent. Private SSA addresses never become Wasm DATA
    // or recording/restore authority.
    extern "C++" ::std::uintptr_t llvm_jit_checkpoint_observer_workspace_abi_bridge(
        ::std::size_t compiler_bytes) noexcept;
    extern "C++" void llvm_jit_checkpoint_observer_workspace_leave_abi_bridge(::std::uintptr_t compiler_buffer) noexcept;
    extern "C++" ::std::uint64_t llvm_jit_checkpoint_activation_enter_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t function_index, ::std::uint64_t compiled_generation) noexcept;
    extern "C++" void llvm_jit_checkpoint_activation_leave_abi_bridge(
        ::std::uint64_t incarnation, ::std::uintptr_t exit_kind) noexcept;

    // Selected resumable full-JIT import edges only. Resolve the actual owned
    // final leaf and count a foreign operation BEFORE provider entry; these
    // internal ABI declarations grant no native/file/restore capability.
    extern "C++" void llvm_jit_checkpoint_call_raw_from_generated_wasm_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::size_t,
        ::std::uintptr_t, ::std::size_t) UWVM_THROWS;
    extern "C++" void llvm_jit_checkpoint_raw_target_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::size_t,
        ::std::uintptr_t, ::std::size_t) UWVM_THROWS;

    // Selected resumable compilation only. This is a distinct potentially
    // throwing bridge after the old noexcept pause returns. The private real
    // manager alone can arm its request; scalars/serialized DATA grant none.
    // Resume ABI2 only: the raw seven-argument entry first binds every range
    // to the canonical running dispatcher's prepared owner. The exact-original
    // typed clone then obtains inputs under its NEW genuine activation token.
    // Neither bridge can install TLS or accept serialized execution authority.
    extern "C++" void llvm_jit_checkpoint_bind_resume_entry_abi_bridge(
        ::std::uintptr_t module,::std::uintptr_t function,::std::uint64_t generation,::std::uint64_t site,
        void const* payload,::std::uintptr_t payload_bytes,void const* flags,::std::uintptr_t flag_count,
        ::std::uintptr_t output,::std::uintptr_t output_bytes) noexcept;
    extern "C++" [[nodiscard]] ::std::uint64_t llvm_jit_checkpoint_resume_input_abi_bridge(
        ::std::uint64_t incarnation,::std::uintptr_t component) noexcept;

    // Potentially throwing INTERNAL resume edge; canonical dispatcher TLS
    // alone owns the child packet/code/result. No guest/FILE pointer is accepted.
    extern "C++" [[nodiscard]] ::std::uintptr_t llvm_jit_checkpoint_resume_child_abi_bridge(
        ::std::uint64_t parent_incarnation, ::std::uint64_t waiting_site, ::std::uintptr_t result_bytes) UWVM_THROWS;

    // Pure internal child-result→actual parent precise-root handoff. Only the
    // canonical running continuation TLS can release its real registered roots.
    extern "C++" void llvm_jit_checkpoint_release_child_roots_abi_bridge(
        ::std::uint64_t parent_incarnation,::std::uint64_t waiting_site) noexcept;

    extern "C++" void llvm_jit_checkpoint_retirement_poll_abi_bridge(
        ::std::uint64_t incarnation, ::std::uintptr_t module_id,
        ::std::uintptr_t function_index, ::std::size_t opcode_offset) UWVM_THROWS;

    // Trusted debug-full cancellation after the noexcept observer/park bridge
    // returns. Only actual native cleanup Invokes call it; no guest ABI or
    // serialized/native-address authority is accepted. Ordinary code emits none.
    // Trusted compiler-only negative gate. Null on unsupported atomic ABI.
    extern "C++" void const* llvm_jit_debug_shutdown_flag_address_host_api() noexcept;
    extern "C++" void llvm_jit_debug_shutdown_poll_abi_bridge(::std::uint64_t incarnation,
        ::std::uintptr_t module_id, ::std::uintptr_t function_index, ::std::size_t opcode_offset) UWVM_THROWS;
    // Called only after a debug-selected blocking wait releases its real queue
    // node/callbacks/locks. Actual worker TLS supplies the private cancellation
    // witness; no guest result or caller-provided frame identity is accepted.
    extern "C++" void llvm_jit_debug_cancelled_wait_abi_bridge() UWVM_THROWS;

    // Used ONLY by debug-full-selected native wrappers. Ordinary generated
    // code keeps its original bridge address and host FP/depth suspend scope.
    extern "C++" ::std::uint64_t llvm_jit_debug_host_enter_abi_bridge() noexcept;
    extern "C++" void llvm_jit_debug_host_leave_abi_bridge(::std::uint64_t token) noexcept;
    extern "C++" void llvm_jit_debug_host_unwind_abi_bridge(::std::uint64_t token) noexcept;
    class llvm_jit_debug_host_scope
    {
        ::std::uint64_t token_{llvm_jit_debug_host_enter_abi_bridge()};
        int const uncaught_{::std::uncaught_exceptions()};
    public:
        llvm_jit_debug_host_scope() noexcept = default;
        llvm_jit_debug_host_scope(llvm_jit_debug_host_scope const&) = delete;
        llvm_jit_debug_host_scope& operator=(llvm_jit_debug_host_scope const&) = delete;
        ~llvm_jit_debug_host_scope()
        {
            if(token_ == 0u) { return; }
            if(::std::uncaught_exceptions() > uncaught_) { llvm_jit_debug_host_unwind_abi_bridge(token_); }
            else { llvm_jit_debug_host_leave_abi_bridge(token_); }
        }
    };
    // Dynamic raw-target path has a distinct host ABI, forwarding to the
    // original qualified SysV/fastcall raw entry inside the private scope.
    extern "C++" void llvm_jit_debug_raw_target_abi_bridge(::std::uintptr_t raw_entry,
        ::std::uintptr_t context, ::std::uintptr_t result, ::std::size_t result_bytes,
        ::std::uintptr_t parameters, ::std::size_t parameter_bytes) UWVM_THROWS;

    // Observation dispatch classifies the authentic Wasm/foreign leaf before host-island admission.
    extern "C++" void llvm_jit_checkpoint_observe_call_raw_from_generated_wasm_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t) UWVM_THROWS;

    // Optional full-JIT cooperative debug point. IDs and expression-relative
    // byte offset come from the validated compiler; the host supplies the pinned
    // code generation and authorization context. The extra generated-frame
    // snapshot pointer is consumed synchronously inside the runtime only.
    // Packet ABI v2: captured_count complete 16-byte payloads, followed by
    // captured_count one-byte 0/1 availability flags. Five native arguments
    // remain unchanged; every producer/runtime/observer TU must be rebuilt.
    extern "C++" void llvm_jit_debug_safe_point_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t function_index,
        ::std::size_t wasm_expr_byte_offset, ::std::uintptr_t local_snapshot,
        ::std::size_t captured_local_count) noexcept;

    // Host-only exception bridges. Addresses are emitted only by validated LLVM code; no guest
    // memory contains native exception objects or registration capabilities. Every scalar uses
    // the target register width, matching the generated C ABI on all supported native targets.
    extern "C++" [[noreturn]] void llvm_jit_throw_numeric_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
        ::std::uintptr_t buffer_address, ::std::size_t bytes) UWVM_THROWS;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
    // Independent staged prototype: never a public Wasm-call/trace permission.
    extern "C++" [[noreturn]] void llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1(
        ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
        ::std::uintptr_t buffer_address, ::std::uintptr_t byte_count) UWVM_THROWS;
#endif
    extern "C++" ::std::uintptr_t llvm_jit_exception_matches_tag_abi_bridge(
        ::std::uintptr_t caught_guest_object, ::std::uintptr_t module_id, ::std::uintptr_t tag_index) noexcept;
    extern "C++" void llvm_jit_exception_copy_numeric_payload_abi_bridge(
        ::std::uintptr_t caught_guest_object, ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
        ::std::uintptr_t destination, ::std::size_t bytes) noexcept;
    // Core 3 reference-bearing exception tuples preserve the complete Wasm carrier and a
    // validated owner root. These cold bridges are separate from the numeric-only fast path.
    extern "C++" [[noreturn]] void llvm_jit_throw_tuple_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
        ::std::uintptr_t buffer_address, ::std::size_t bytes) UWVM_THROWS;
    extern "C++" void llvm_jit_exception_copy_tuple_abi_bridge(
        ::std::uintptr_t caught_guest_object, ::std::uintptr_t module_id, ::std::uintptr_t tag_index,
        ::std::uintptr_t destination, ::std::size_t bytes) noexcept;
    extern "C++" void llvm_jit_exception_make_ref_abi_bridge(
        ::std::uintptr_t caught_guest_object, ::std::uintptr_t module_id,
        ::std::uintptr_t destination) noexcept;
    extern "C++" [[noreturn]] void llvm_jit_throw_ref_abi_bridge(
        ::std::uintptr_t module_id, ::std::uintptr_t carrier_address) UWVM_THROWS;
    // Selected ONLY by instrumented full-JIT emission. Ordinary throw bridges
    // retain their existing code and never read an uncaught-debug TLS flag.
    extern "C++" [[noreturn]] void llvm_jit_debug_throw_numeric_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::size_t) UWVM_THROWS;
    extern "C++" [[noreturn]] void llvm_jit_debug_throw_tuple_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::size_t) UWVM_THROWS;
    extern "C++" [[noreturn]] void llvm_jit_debug_throw_ref_abi_bridge(
        ::std::uintptr_t, ::std::uintptr_t) UWVM_THROWS;
}
#include <uwvm2/utils/macro/pop_macros.h>
