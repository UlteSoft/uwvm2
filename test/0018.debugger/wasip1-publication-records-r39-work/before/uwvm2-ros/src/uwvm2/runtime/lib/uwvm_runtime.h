/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <array>
# include <cstddef>
# include <chrono>
# include <cstdint>
# include <memory>
# include <span>
# include <string>
# include <vector>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/runtime/exception/impl.h>
# include <uwvm2/runtime/gc/impl.h>
# include <uwvm2/runtime/checkpoint/materialization.h>
# include <uwvm2/uwvm/debugger/wasm_state.h>
# include <uwvm2/uwvm/debugger/wasm_mutation.h>
# include <uwvm2/uwvm/debugger/wasip1_state.h>
# include <uwvm2/uwvm/debugger/wasip1_portable_checkpoint.h>
# include <uwvm2/uwvm/debugger/checkpoint_state.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifndef UWVM_MODULE
// Textual consumers need only an incomplete descriptor for the pointer API.
// In named-module builds its definition is imported from uwvm2.uwvm.wasm.type:
// repeating this declaration inside uwvm2.runtime would attach the same type
// to two modules, rejected when a host API consumer imports both. Keep the
// descriptor's layout and ABI in preload_api.h; do not duplicate it here.
namespace uwvm2::uwvm::wasm::type { struct uwvm_preload_memory_descriptor_t; }
#endif

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include "native_activation.h"
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{

#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    // Its first declaration belongs to this runtime module. The implementation
    // is native-only and receives authority from a real outer execution scope;
    // no definition, constructor or method is exposed to guest/debug imports.
    class source_exception_guest_bridge;
#endif

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Privileged native foundation entry; never exported as a guest import.
    // Its callback owns/parses/initializes real storage while this same runtime's
    // execution_domain maintenance callback excludes entries and publication.
    // Callback may not execute Wasm, compile, reset/drain, or reenter this API.
    // It must install/seal the actual typed full_source_instance. A void context
    // is only borrowed callback state, never a module/generation authority pin.
    // Failure removes partial source views and leaves admission closed until an
    // explicit successful reset. No-C++-EH builds return false without mutation.
    extern "C++" bool replace_full_source_after_drain_host_api(
        bool (*initialize_source)(void*) noexcept, void* context) noexcept;
#endif
    // Cold serialized native observation only. Integer addresses describe
    // the currently locked publication and never authenticate or pin a module.
    // The selected source and actual publication are compared by control block.
    // No guest memory instruction or execution hot path calls this API.
    struct llvm_jit_full_source_publication_view
    {
        ::std::uint_least64_t runtime_epoch{};
        ::std::size_t module_id{};
        ::std::uintptr_t source_address{}, module_address{}, core3_context_address{};
        ::std::uintptr_t engine_address{}, llvm_context_address{};
        bool initialized_source{}, ready{}, canonical_source{}, publication_owns_source{};
        bool pending_numeric_plan{}; // actual canonical full publication owner
        bool plan_owns_same_source{};
        bool numeric_body_fallback{};
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        bool exception_source_debug_management{}; // actual locked native observation, never authority
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        bool private_eh_leaf_present{}, private_eh_leaf_actual_binding{};
        ::std::size_t private_eh_leaf_clone_count{}; // actual loaded+CFI owner, data only
#endif
        ::std::uintptr_t pending_plan_address{};
    };
    extern "C++" llvm_jit_full_source_publication_view llvm_jit_full_source_publication_host_api(
        ::std::size_t module_id) noexcept;
#if defined(UWVM2TEST_OWNED_FULL_LATE_PUBLICATION_FAILURE)
    // Native witness seam only; absent from normal product builds. The hook is
    // reached after actual initializer callback success, before domain reopening.
    extern "C++" bool uwvm2test_owned_full_late_publication_failure() noexcept;
    extern "C++" bool uwvm2test_owned_full_admission_open_host_api() noexcept;
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#if defined(UWVM2TEST_NATIVE_EH_PRIVATE_LEAF_CFI_FAILURE)
    // Native cold-test seams only, absent from product builds. The rejection
    // occurs after actual loaded extent/FDE proof; notification follows private
    // engine/listener retirement and exact original-public IR restoration.
    extern "C++" bool uwvm2test_private_leaf_reject_after_actual_cfi() noexcept;
    extern "C++" void uwvm2test_private_leaf_failed_engine_retired() noexcept;
#endif
#endif
    inline constexpr ::std::size_t llvm_jit_debug_max_captured_locals{256u};
    inline constexpr ::std::size_t llvm_jit_debug_local_slot_bytes{16u};
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Embedded metadata only. The runtime mints this private owner from its
    // actual full/native publication; callers cannot construct it from integers,
    // aliasing shared_ptrs, guest bytes or publication-view addresses. It owns no
    // independent executable code: every position query acquires a real execution
    // generation lease and checks the current unique publication under its lock.
    class llvm_jit_debug_source_binding;
    using llvm_jit_debug_source_binding_owner = ::std::shared_ptr<llvm_jit_debug_source_binding const>;
    struct llvm_jit_debug_source_section
    {
        ::std::string name{};
        ::std::vector<::std::byte> payload{};
    };
    struct llvm_jit_debug_source_function
    {
        ::std::uint_least64_t function{}, expression_begin{}, expression_size{}, function_generation{};
        // Bounded owned BEFORE-opcode diagnostic bytes, copied only by genuine
        // debug setup. Empty means budget/unavailable; never grants read authority.
        ::std::vector<::std::byte> expression_bytes{};
        // Same private source binding/generation, optional and bounded. These
        // diagnostic boundaries grant no read authority and are never wire input.
        ::std::vector<::std::uint_least8_t> instruction_safe_point_bits{};
        bool instruction_safe_points_complete{};
    };
    struct llvm_jit_debug_source_image
    {
        ::std::uint_least64_t code_section_content_size{}, runtime_epoch{};
        ::std::vector<llvm_jit_debug_source_section> sections{};
        ::std::vector<llvm_jit_debug_source_function> functions{};
    };
    struct llvm_jit_debug_source_position
    {
        ::std::uint_least64_t module{}, function{}, code_offset{}, runtime_epoch{}, function_generation{};
    };
    // Serialized native setup only. Failed/foreign/preloaded sources return {}.
    // The copied image is untrusted metadata and grants no stopped-frame access.
    extern "C++" llvm_jit_debug_source_binding_owner llvm_jit_debug_bind_source_host_api(::std::size_t module_id) noexcept;
    extern "C++" bool llvm_jit_debug_copy_source_image_host_api(llvm_jit_debug_source_binding_owner const& binding,
        llvm_jit_debug_source_image& out) noexcept;
    // Owns the ONE pause-domain guard; callers must not wrap this in
    // while_stopped(). Location comes directly from the real parked participant,
    // never from a request's module/function/decimal offset. Native traps fail.
    extern "C++" bool llvm_jit_debug_source_position_host_api(llvm_jit_debug_source_binding_owner const& binding,
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& control,
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::uint_least64_t participant, llvm_jit_debug_source_position& out) noexcept;
    // Private exact-event capture. Runtime mints it only inside the genuine
    // synchronous before-park callback, storing that callback's real ticket and
    // participant. Numbers, shared_ptr aliases, source metadata and CFA cannot
    // construct one. The controller consumes only a fresh jointly guarded query.
    class llvm_jit_debug_activation_capture;
    using llvm_jit_debug_activation_capture_owner = ::std::shared_ptr<llvm_jit_debug_activation_capture const>;
    struct llvm_jit_debug_activation_identity
    {
        ::std::uint64_t incarnation{}, parent{}, continuation{};
        ::std::uint64_t module{}, function{}, function_generation{}, runtime_epoch{};
    };
    struct llvm_jit_debug_activation_snapshot
    {
        ::std::uint_least64_t participant{};
        ::uwvm2::utils::thread::cooperative_pause_location location{};
        // Owned outer -> inner identities. No caller PC/source scope is guessed.
        ::std::vector<llvm_jit_debug_activation_identity> frames{};
    };
    extern "C++" llvm_jit_debug_activation_capture_owner llvm_jit_debug_capture_activation_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket) noexcept;
    // One real lease -> ONE domain guard -> publication guard validates the
    // captured ticket and actual full code/function generations before copying.
    // This metadata query grants NO source/locals/read authority. The future
    // source next/finish bridge must jointly validate the actual source owner
    // and code-relative PC under this SAME guard, not combine separate queries.
    extern "C++" bool llvm_jit_debug_query_activation_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_activation_snapshot& out) noexcept;
    struct llvm_jit_debug_source_activation_snapshot
    {
        llvm_jit_debug_activation_snapshot activation{};
        llvm_jit_debug_source_position source{};
        bool source_available{};
        // Actual same-publication guest context can remain valid where a
        // generated guest helper has no Code-to-source mapping. No PC is made.
        bool source_context_available{};
    };
    // One fresh stopped transaction validates both the private event capture
    // and actual source binding. A valid chain may have unavailable source (a
    // deeper non-debug function); its location is never guessed from a caller.
    // This copies metadata only, granting no locals/address/expression access.
    extern "C++" bool llvm_jit_debug_query_source_activation_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        llvm_jit_debug_source_activation_snapshot& out) noexcept;
    struct llvm_jit_debug_source_object_copy
    {
        llvm_jit_debug_source_activation_snapshot position{};
        ::std::vector<::std::byte> bytes{};
        ::std::uint_least64_t guest_offset{};
        ::std::uint_least8_t address_bytes{};
    };
    // Cold authenticated copy only: real private capture/source binding -> real
    // lease -> ONE stopped-participant guard -> publication guard -> fresh
    // bounds -> owned bytes. Never accepts a module/native address or caller
    // ticket. Initial producer ABI is a SINGLE locally defined unshared memory;
    // ambiguous multi-memory/imported/shared objects fail closed. Address width
    // must match its actual declaration. No guest callback or raw memory escape.
    extern "C++" bool llvm_jit_debug_copy_source_object_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        ::std::uint_least64_t guest_offset, ::std::size_t size,
        ::std::uint_least8_t address_bytes, llvm_jit_debug_source_object_copy& out) noexcept;
    enum class llvm_jit_debug_configure_result : unsigned { ok, unsupported_mode, already_published, invalid_context };
    // Native trusted manager only; an immutable policy cannot mint a stop or
    // restore authority. This records typed native entry state incrementally;
    // all-opcode continuations/GC/host replay still require separate admission.
    extern "C++" llvm_jit_debug_configure_result llvm_jit_configure_checkpoint_recording_host_api(
        ::uwvm2::runtime::checkpoint::compilation_profile::owner profile) noexcept;
    // Ordinary full debugger/reserved-interface typed-value observation.
    // The immutable observation purpose creates no executable resume entries.
    extern "C++" llvm_jit_debug_configure_result llvm_jit_configure_debug_value_observation_host_api(
        ::uwvm2::runtime::checkpoint::budgets cap = ::uwvm2::runtime::checkpoint::default_observation_budgets()) noexcept;
    struct llvm_jit_checkpoint_recording_observation
    {
        ::uwvm2::runtime::checkpoint::status status{::uwvm2::runtime::checkpoint::status::unmaterialized};
        ::std::uint64_t incarnation{}, runtime_epoch{}, function_generation{}, site{};
        ::std::size_t native_frames{}, typed_slots{};
        bool instrumented{};
        // The retained last recorded site may be older than the current stop.
        // False never permits it to masquerade as current logical VM state.
        bool at_current_opcode{};
        bool executable_restore_available{}; // actual dispatcher/GC/effects qualification, never inferred from slots
    };
    // Cold scalar observation only, exclusively within the runtime's genuine
    // owned before-park callback. No values, native pointers or stop tickets.
    extern "C++" llvm_jit_checkpoint_recording_observation llvm_jit_observe_checkpoint_recording_host_api() noexcept;
    // HOST-ONLY immutable-input diagnostics. The runtime mints a private owner
    // only in the genuine selected on_before_park callback; no guest import,
    // public number, shared_ptr alias, source pin or copied value can mint it.
    class llvm_jit_checkpoint_thread_capture;
    using llvm_jit_checkpoint_thread_capture_owner = ::std::shared_ptr<llvm_jit_checkpoint_thread_capture const>;
    class llvm_jit_debug_source_memory_view;
    using llvm_jit_debug_source_memory_callback = bool (*)(void*, llvm_jit_debug_source_memory_view&) noexcept;
    extern "C++" bool llvm_jit_debug_with_source_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            void*, llvm_jit_debug_source_memory_callback) noexcept;
    // Selected saved physical activation, authenticated by the current typed
    // cohort. The incarnation selects real saved DATA; it creates no read or
    // source owner. A caller Code PC comes only from its sealed current site.
    extern "C++" bool llvm_jit_debug_with_source_frame_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t expected_incarnation, void*, llvm_jit_debug_source_memory_callback) noexcept;
    struct llvm_jit_debug_source_captured_local
    {
        ::std::uint_least8_t wasm_type{};
        ::std::array<::std::byte, 16u> native_bytes{};
        bool available{};
    };
    struct llvm_jit_debug_source_frame_locals
    {
        ::std::uint_least64_t participant{}, incarnation{}, total_count{};
        // Native scalar carrier DATA, not canonical Wasm memory bytes. REF
        // carriers are unavailable and zeroed, never provider/native addresses.
        ::std::vector<llvm_jit_debug_source_captured_local> values{};
        // Same sealed selected frame, bottom-first Wasm operands. Globals are
        // copied under this SAME closed-host/publication transaction. REF and
        // imported-global aliases remain unavailable, never native addresses.
        ::std::vector<llvm_jit_debug_source_captured_local> operands{}, globals{};
        ::std::uint_least64_t operand_count{}, global_count{};
    };
    // HOST-only synchronous bounded reader. Runtime construction is private;
    // public offsets/types/stop labels cannot make a reader. One real canonical
    // capture/source binding and ONE all-stopped publication/memory transaction
    // cover the original pointer bytes and every subsequent guest dereference.
    // Complete privately minted typed captures MUST cover the same actual ticket:
    // the runtime closes host admission and owns the genuine current N-root
    // exclusion before source-position validation and any read. The selected
    // activation's private ticket is checked inside that SAME cohort mutex.
    // No raw memory span/pointer, execution, native-code or cross-module API is
    // exposed. The callback must remain on the original management thread and
    // must not save this borrowed view, reenter runtime/controller, perform IO,
    // call guest code or use it in another thread. Only owned copied bytes may
    // survive the callback. This is not a serialized or guest-import interface.
    class llvm_jit_debug_source_memory_view final
    {
        struct implementation;
        implementation* state_{};
        explicit llvm_jit_debug_source_memory_view(implementation*) noexcept;
        ~llvm_jit_debug_source_memory_view() = default;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_with_source_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            void*, llvm_jit_debug_source_memory_callback) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t, void*, llvm_jit_debug_source_memory_callback) noexcept -> bool;
    public:
        llvm_jit_debug_source_memory_view(llvm_jit_debug_source_memory_view const&) = delete;
        llvm_jit_debug_source_memory_view& operator=(llvm_jit_debug_source_memory_view const&) = delete;
        llvm_jit_debug_source_memory_view(llvm_jit_debug_source_memory_view&&) = delete;
        llvm_jit_debug_source_memory_view& operator=(llvm_jit_debug_source_memory_view&&) = delete;
        [[nodiscard]] ::std::uint_least8_t address_bytes() const noexcept;
        // Owned canonical position only; no memory read or raw activation/frame
        // reference escapes. Does not consume the bounded byte-copy allowance.
        [[nodiscard]] bool copy_position(llvm_jit_debug_source_activation_snapshot&) const noexcept;
        // Only the selected saved-frame API supplies these native typed DATA.
        // copy_position reflects that same activation/call-site, never a VM PC.
        [[nodiscard]] bool copy_frame_locals(llvm_jit_debug_source_frame_locals&) const noexcept;
        // At most32 attempted copies, each<=65536 and aggregate<=2MiB. Checks
        // actual memory bounds before every pointer derivation; output is an
        // owned byte image and actual private position, never an address owner.
        // First byte-reader qualification admits only the actual whole defined-
        // Wasm import closure. Unqualified foreign/provider memory aliases
        // return false; their metadata-only type queries remain available.
        [[nodiscard]] bool copy_guest(::std::uint_least64_t, ::std::size_t,
            llvm_jit_debug_source_object_copy&) noexcept;
    };
    enum class llvm_jit_checkpoint_capture_status : unsigned char
    {
        captured, not_selected, not_actual_before_park, stale_ticket, invalid_activation,
        incomplete_logical_frames, stale_code_generation, invalid_publication,
        invalid_typed_packet, unavailable_reference_roots, unavailable_exception_continuation,
        registry_exhausted, allocation_failed, non_replayable_import, activation_resource_exhausted
    };
    struct llvm_jit_checkpoint_capture_result
    {
        llvm_jit_checkpoint_capture_status status{llvm_jit_checkpoint_capture_status::not_selected};
        llvm_jit_checkpoint_thread_capture_owner capture{};
    };
    enum class llvm_jit_checkpoint_query_status : unsigned char
    {
        coherent_typed_data, not_selected, invalid_management_entry, execution_admission_denied, execution_stopping,
        invalid_capture_owner, incomplete_cohort, wrong_control_or_profile, stale_episode,
        stale_location_or_generation, host_busy, host_untracked, host_admission_refused,
        invalid_current_publication, invalid_typed_data, budget_exceeded, allocation_failed, gc_admission_denied, invalid_gc_population
    };
    enum class llvm_jit_checkpoint_resource_status : unsigned char
    {
        immutable_inputs_only, invalid_management_scope, invalid_cohort,
        invalid_publication, unsupported_source_origin, unsupported_registry,
        unknown_import_census, stale_function_generation, invalid_segment_origin,
        quota_exceeded, allocation_failed
    };
    struct llvm_jit_checkpoint_data_segment_copy
    {
        ::std::uint64_t index{}, source_offset{}, byte_count{};
        bool dropped{};
    };
    struct llvm_jit_checkpoint_module_input_copy
    {
        ::std::uint64_t module{};
        // Exact function/table/memory/global/tag/data/element index order.
        ::std::array<::std::uint64_t, 7u> declaration_counts{};
        ::std::vector<::std::byte> original_module{};
        ::std::vector<llvm_jit_checkpoint_data_segment_copy> data{};
    };
    struct llvm_jit_checkpoint_thread_data_observation
    {
        ::std::uint_least64_t participant{};
        ::uwvm2::utils::thread::cooperative_pause_location location{};
        ::std::size_t frame_count{}, typed_slots{}, initialized_slots{}, unavailable_slots{};
    };
    struct llvm_jit_checkpoint_resource_observation
    {
        llvm_jit_checkpoint_query_status status{llvm_jit_checkpoint_query_status::not_selected};
        llvm_jit_checkpoint_resource_status resource_status{llvm_jit_checkpoint_resource_status::invalid_management_scope};
        ::std::uint_least64_t observed_runtime_epoch{};
        ::std::vector<llvm_jit_checkpoint_thread_data_observation> threads{};
        ::std::vector<llvm_jit_checkpoint_module_input_copy> modules{};
        // Copied immutable inputs and scalar thread observations are DATA. They
        // contain no ticket, control owner, executable PC, native stack or issuer.
        inline static constexpr bool complete_instance = false;
        inline static constexpr bool executable_restore_available = false;
        [[nodiscard]] constexpr bool snapshot_or_restore_authority() const noexcept { return false; }
    };
    extern "C++" llvm_jit_checkpoint_capture_result llvm_jit_checkpoint_capture_thread_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket) noexcept;
    // A management native thread only, NEVER wrapped in while_stopped(). The
    // implementation owns real lease -> ONE all-participant/current-ticket guard
    // -> nonwaiting actual host closure -> publication -> bounded resource copy.
    // Every supplied owner is privately canonicalized before pointee access.
    // All Core3 mutable resources, external exposure census and portable resume
    // are still unavailable; this API opens no endpoint and saves no database.
    extern "C++" llvm_jit_checkpoint_resource_observation llvm_jit_checkpoint_query_resource_inputs_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> captures) noexcept;
    // Native management reader of current Wasm DATA only. Authenticate the
    // complete real capture roster and private pause ticket; implementation owns
    // ONE coherent cohort/root/store/publication borrow. No host pointers, VM
    // state inspection, arbitrary object handles or executable restore are exposed.
    extern "C++" ::uwvm2::uwvm::debugger::wasm_state::view llvm_jit_debug_query_wasm_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> captures,
        ::uwvm2::uwvm::debugger::wasm_state::request const& selection) noexcept;
    // Separate native management mutator. Requests contain only original Wasm
    // selectors/value bits. A VIEW/result is never write permission. The actual
    // implementation reacquires ONE real current cohort, closed host gate, N
    // exclusion and publication; only one canonical global/table slot is changed.
    extern "C++" ::uwvm2::uwvm::debugger::wasm_mutation::result llvm_jit_debug_mutate_wasm_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> captures,
        ::uwvm2::uwvm::debugger::wasm_mutation::request const& selection) noexcept;
    // Same authenticated management-only typed query as Wasm state. This
    // request cannot expose a native descriptor, host path or launch a syscall.
    extern "C++" ::uwvm2::uwvm::debugger::wasip1_state::view llvm_jit_debug_query_wasip1_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> captures,
        ::uwvm2::uwvm::debugger::wasip1_state::request const& selection) noexcept;
    // Same-process native resource lifetime capsule. The incomplete owner can
    // only be issued by the real closed-host/current-cohort capture manager.
    // Its detached DATA never authorizes a restore, descriptor rebinding, host
    // syscall, kernel-offset rollback or deterministic external-effect replay.
    class llvm_jit_wasip1_environment_capsule;
    using llvm_jit_wasip1_environment_capsule_owner = ::std::shared_ptr<llvm_jit_wasip1_environment_capsule const>;
    enum class llvm_jit_wasip1_environment_capsule_status : unsigned char
    {
        captured, requires_llvm_jit_full, not_selected, invalid_recording_label,
        unavailable_capture, unavailable_environment, unavailable_owned_text,
        resource_limit, registry_exhausted, native_duplicate_failed, allocation_failed,
        invalid_capsule_owner, restored, stale_environment, unsupported_resource, native_operation_failed,
        invalid_portable_snapshot, missing_mount, missing_rebind, incompatible_flags, capability_denied
    };
    struct llvm_jit_wasip1_environment_capsule_request
    {
        ::std::uint64_t module{};
        ::std::array<::std::byte,16u> recording_label{};
        ::std::size_t maximum_descriptors{65536u}, maximum_directory_entries{65536u}, maximum_owned_text_bytes{1048576u};
        ::std::size_t maximum_managed_file_bytes{16777216u}, maximum_managed_total_bytes{67108864u};
        bool portable_metadata_only{};
        ::std::shared_ptr<::uwvm2::uwvm::debugger::wasip1_portable::snapshot const> portable_restore{};
        ::std::vector<::uwvm2::uwvm::debugger::wasip1_portable::rebind> portable_rebindings{};
        // Labels and bounds are DATA, not source/env identity or host authority.
    };
    struct llvm_jit_wasip1_environment_capsule_data
    {
        ::std::uint64_t issuer_serial{}, observed_runtime_epoch{}, module{};
        ::std::array<::std::byte,16u> recording_label{};
        ::std::array<::std::byte,32u> original_wasm{}, builtin_interface{};
        ::std::vector<::uwvm2::uwvm::debugger::wasip1_state::text> arguments{}, environment{};
        ::std::vector<::uwvm2::uwvm::debugger::wasip1_state::descriptor_entry> descriptors{};
        ::std::size_t distinct_native_bindings{}, captured_directory_entries{}, duplicated_observers{};
        bool shared_environment{};
        ::std::size_t managed_resources{}, retained_external_resources{}, managed_content_bytes{};
        // No native handle, resource pointer, host path, source/engine/cohort
        // owner or old logical descriptor number can issue restoration rights.
        inline static constexpr bool persisted_resource_restore_available = false;
        inline static constexpr bool kernel_state_rollback_available = false;
        inline static constexpr bool deterministic_external_replay_available = false;
    };
    struct llvm_jit_wasip1_environment_capsule_result
    {
        llvm_jit_wasip1_environment_capsule_status status{llvm_jit_wasip1_environment_capsule_status::not_selected};
        llvm_jit_wasip1_environment_capsule_owner capsule{};
        ::std::shared_ptr<::uwvm2::uwvm::debugger::wasip1_portable::snapshot const> portable{};
        ::uwvm2::uwvm::debugger::wasip1_state::text diagnostic{};
        ::std::uint64_t observed_runtime_epoch{};
    };
    struct llvm_jit_wasip1_portable_environment_group_result
    {
        llvm_jit_wasip1_environment_capsule_status status{llvm_jit_wasip1_environment_capsule_status::not_selected};
        ::uwvm2::uwvm::debugger::wasip1_portable::group_snapshot portable{};
        ::uwvm2::uwvm::debugger::wasip1_state::text diagnostic{};
        ::std::uint64_t observed_runtime_epoch{};
    };
    // Capture 1..16 distinct actual environments under ONE authentic complete
    // current cohort. All requests require the same nonzero label and metadata
    // only; on failure no partial snapshots escape. No file contents are copied.
    extern "C++" llvm_jit_wasip1_portable_environment_group_result llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept;
    struct llvm_jit_wasip1_environment_capsule_data_result
    {
        llvm_jit_wasip1_environment_capsule_status status{llvm_jit_wasip1_environment_capsule_status::invalid_capsule_owner};
        llvm_jit_wasip1_environment_capsule_data data{};
    };
    extern "C++" llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_capture_wasip1_environment_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        llvm_jit_wasip1_environment_capsule_request const&) noexcept;
    extern "C++" llvm_jit_wasip1_environment_capsule_data_result llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(
        llvm_jit_wasip1_environment_capsule_owner const&) noexcept;
    struct llvm_jit_wasip1_environment_restore_request
    {
        ::std::uint64_t module{};
        bool require_managed_resources{}; // Reject retained external resources BEFORE any commit.
    };
    extern "C++" llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        llvm_jit_wasip1_environment_capsule_owner const&, llvm_jit_wasip1_environment_restore_request const&) noexcept;
    // Real saved-leaf execution adapter, used by the private instance
    // restore dispatcher. This does not restore memory/table/global/GC
    // resources by itself and is not a checkpoint file authorization.
    // Atomic current-generation WASIp1 RESOURCE group only. Every table/file
    // is privately prepared before any swap under ONE actual complete cohort,
    // closed host gate, GC exclusion and publication transaction. Strict mode
    // rejects external resources; already committed external effects cannot be
    // retracted. This operation does not restore the complete VM instance.
    extern "C++" llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_owner const>, bool require_managed_resources = true) noexcept;
    // Atomic import of 1..16 independently configured target WASIp1
    // environments. Each request provides its target module and detached
    // portable metadata, plus explicit target-FD bindings when needed. All
    // replacements are prepared under ONE actual current stop before any swap.
    // Snapshots must share a nonzero recording label; labels remain DATA, never
    // proof of a joint Wasm capture. File bytes/external effects are excluded.
    // A failure names its request index in diagnostic and changes no target.
    extern "C++" llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept;
    enum class llvm_jit_checkpoint_continuation_status : unsigned char
    {
        continued, requires_llvm_jit_full, invalid_management_entry, invalid_capture_owner,
        original_execution_still_live, requires_complete_call_dispatch, invalid_typed_packet,
        unavailable_compiled_continuation, stale_publication, invalid_result_buffer,
        invalid_native_environment, allocation_failed, execution_retired
    };
    // Cold host-only copied graph. A private canonical ticket/capture cohort
    // is required; no guest/native pointer, wire ID, debugger VIEW or scalar
    // report can issue this copy or authorize executable new-world restoration.
    enum class llvm_jit_checkpoint_instance_capture_status : unsigned char
    { captured, requires_llvm_jit_full, not_selected, invalid_recording_label, unavailable_capture, allocation_failed };
    struct llvm_jit_checkpoint_instance_capture_result
    {
        llvm_jit_checkpoint_instance_capture_status status{llvm_jit_checkpoint_instance_capture_status::not_selected};
        ::uwvm2::uwvm::debugger::checkpoint::error data_error{
            ::uwvm2::uwvm::debugger::checkpoint::error::unavailable_capability};
        ::std::shared_ptr<::uwvm2::uwvm::debugger::checkpoint::state const> graph{};
        ::std::vector<llvm_jit_wasip1_environment_capsule_owner> wasip1_environments{};
        bool wasip1_checkpoint_required{true}, wasip1_captured_together{};
    };
    // Host-only retirement of ONE complete runtime-owned native cohort.
    // Pending operations retain the old workers/code/closed host and execution
    // admission. A canonical retry can join TLS/kernel exit; it does not restore
    // a candidate world, change the Wasm epoch or expose native addresses.
    class llvm_jit_checkpoint_native_retirement;
    using llvm_jit_checkpoint_native_retirement_owner = ::std::shared_ptr<llvm_jit_checkpoint_native_retirement const>;
    enum class llvm_jit_checkpoint_native_retirement_status : unsigned char
    { retired_and_joined, pending_execution, pending_native_join, requires_llvm_jit_full,
      invalid_context, invalid_deadline, busy, stale_owner, rejected_current_cohort, allocation_failed, failed_closed, preparation_declined, prepared_world_ready_closed };
    struct llvm_jit_checkpoint_native_retirement_result;
    extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_retire_saved_native_workers_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>, ::std::chrono::steady_clock::time_point) noexcept;
    extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_continue_native_retirement_host_api(
        llvm_jit_checkpoint_native_retirement_owner const&, ::std::chrono::steady_clock::time_point) noexcept;
    // Cold preparation rehearsal under ONE actual complete pause/census.
    // New resources/engines/frames/root carriers are unpublished and destroyed before return.
    // Diagnostic DATA does not authorize execution, file restore or ASM access.
    enum class llvm_jit_checkpoint_prepare_status : unsigned char
    { prepared_and_discarded, requires_llvm_jit_full, not_selected, invalid_request,
      unavailable_capture, resource_preparation_declined, engine_preparation_declined, allocation_failed,
      wasip1_preparation_declined, frame_preparation_declined, root_preparation_declined, worker_root_preparation_declined, prepared_and_retained, dispatch_binding_preparation_declined, indirect_binding_preparation_declined, source_initializer_binding_preparation_declined };
    struct llvm_jit_checkpoint_prepare_request
    {
        ::std::array<::std::byte,16u> recording_label{};
        ::uwvm2::uwvm::debugger::checkpoint::limits graph_budget{};
        ::std::uint64_t maximum_native_payload_bytes{256u*1024u*1024u};
        ::std::size_t maximum_original_source_bytes{64u*1024u*1024u};
        // Bound actual private calling-thread root records across all packets.
        ::std::size_t maximum_private_root_frames{65536u};
        // Bound real private native workers; the entire cohort is preflighted.
        ::std::size_t maximum_private_root_workers{256u};
        // Two diagnostic native workers validate private WASIp1 TLS selection.
        ::std::size_t maximum_private_wasip1_workers{2u};
        // Prepare every visible distinct WASIp1 environment in this SAME stop.
        // These bounded candidates are discarded with the candidate Wasm world.
        bool include_wasip1{}, require_managed_wasip1_resources{true};
        // Combined final defined/import cache entries; checked before allocation.
        ::std::size_t maximum_private_dispatch_bindings{1048576u};
        // Combined caller type records, table views and per-caller function slots.
        ::std::size_t maximum_private_indirect_bindings{1048576u};
        // Actual typed+resume native endpoint expectations, across all modules.
        ::std::size_t maximum_private_native_endpoint_functions{65536u};
        // Final main/preload initializer correspondence; no source seal is issued.
        ::std::size_t maximum_private_source_initializer_modules{4096u};
    };
    struct llvm_jit_checkpoint_prepare_result
    {
        llvm_jit_checkpoint_prepare_status status{llvm_jit_checkpoint_prepare_status::not_selected};
        ::uwvm2::uwvm::debugger::checkpoint::error data_error{
            ::uwvm2::uwvm::debugger::checkpoint::error::unavailable_capability};
        ::std::uint64_t native_payload_bytes{};
        ::std::size_t modules{},engines{},functions{};
        unsigned resource_diagnostic{},engine_diagnostic{},frame_diagnostic{};
        ::std::size_t prepared_threads{},prepared_frames{},prepared_root_carriers{};
        // Cold candidate TLS scopes were actually installed and removed. These
        // counts do not imply restored-worker enrollment or live GC publication.
        ::std::size_t validated_private_root_scopes{}, installed_private_root_frames{}, installed_private_root_carriers{};
        bool private_root_chain_restored{};
        // Actual private worker TLS enrollment, simultaneous hold and physical
        // join. These diagnostics do not authorize restored-world execution.
        ::std::size_t started_private_root_workers{}, enrolled_private_root_workers{}, joined_private_root_workers{};
        bool private_worker_cohort_held{}, private_worker_roots_restored{};
        // Genuine private pause/TLS owners, with empty healthy ledgers. No
        // guest activation, saved-frame identity or execution permission.
        ::std::size_t prepared_private_debug_workers{}, enrolled_private_debug_workers{};
        bool private_debug_cohort_held{}, private_debug_tls_restored{};
        // WASIp1 leaf selection is held on THESE same private root/debug
        // workers. Frame visits check nested caller contexts; no guest entry.
        ::std::size_t prepared_private_wasip1_workers{}, enrolled_private_wasip1_workers{}, private_worker_wasip1_frame_visits{};
        bool private_wasip1_cohort_held{}, private_wasip1_tls_restored{};
        llvm_jit_wasip1_environment_capsule_status wasip1_status{llvm_jit_wasip1_environment_capsule_status::unavailable_capture};
        ::std::size_t wasip1_environments{};
        bool wasip1_checkpoint_required{}, wasip1_prepared_together{};
        // Installed into nonmoving PRIVATE candidate environments, with NEW
        // dense module/memory correspondence. No execution/publication token.
        ::std::size_t prepared_wasip1_modules{}, prepared_wasip1_memories{}, prepared_wasip1_shared_modules{};
        bool wasip1_installed_privately{};
        // Final private native cache shape only; not actual world publication.
        ::std::size_t prepared_wasip1_dispatch_contexts{}, prepared_wasip1_trace_bindings{};
        bool wasip1_dispatch_prepared{};
        ::std::size_t verified_wasip1_dispatch_workers{}, verified_wasip1_dispatch_module_visits{};
        bool wasip1_dispatch_tls_restored{}; // Verification only; no execution credential.
        ::std::size_t prepared_runtime_defined_bindings{}, prepared_runtime_import_bindings{};
        ::std::size_t prepared_runtime_defined_pointer_ranges{};
        bool runtime_dispatch_bindings_prepared{}; // Private new-source caches; no publication.
        ::std::size_t prepared_runtime_type_bindings{}, prepared_runtime_table_views{}, prepared_runtime_function_table_views{};
        ::std::size_t prepared_runtime_indirect_targets{}, prepared_runtime_indirect_defined_targets{}, prepared_runtime_indirect_import_targets{};
        ::std::size_t prepared_runtime_indirect_null_targets{}, prepared_runtime_indirect_incompatible_targets{};
        bool runtime_indirect_bindings_prepared{}; // New-source private views; no guest permission.
        ::std::size_t prepared_runtime_native_endpoint_functions{}, prepared_runtime_native_helper_bodies{}, prepared_runtime_native_symbol_claims{};
        bool runtime_native_endpoint_capture_prepared{}; // Private DATA; LIVE seal/epoch are not issued.
        ::std::size_t prepared_source_initializer_modules{}, prepared_source_initializer_preloads{};
        bool runtime_source_initializer_bindings_prepared{}; // Fixed private DATA, no initializer serial.
    };
    struct llvm_jit_checkpoint_native_retirement_result
    {
        llvm_jit_checkpoint_native_retirement_status status{llvm_jit_checkpoint_native_retirement_status::requires_llvm_jit_full};
        llvm_jit_checkpoint_native_retirement_owner operation{};
        ::std::uint_least64_t runtime_epoch{};
        ::std::size_t workers{};
        unsigned cohort_diagnostic{};
        bool execution_drained{}, native_workers_joined{}, all_frames_signalled{};
        llvm_jit_checkpoint_prepare_result preparation{};
        bool candidate_world_retained{}; // Diagnostic only; no world/entry is exposed.
        bool source_initializer_bindings_rechecked_after_physical_join{}; // Actual closed-world preflight only.
    };
    // Cold native management only. Prepare a fresh candidate under the SAME
    // complete stop, then retire/join the actual registered old cohort. Pending
    // owners retain both worlds. Ready leaves ordinary execution CLOSED; it is
    // neither complete publication nor executable restore authority.
    extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_prepare_retire_instance_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        llvm_jit_checkpoint_prepare_request const&, ::std::chrono::steady_clock::time_point) noexcept;
    // Explicitly discard a fully joined private candidate and reopen the SAME
    // retained original instance. Pending execution/TLS must finish first.
    extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_discard_prepared_instance_host_api(
        llvm_jit_checkpoint_native_retirement_owner const&, ::std::chrono::steady_clock::time_point) noexcept;
    extern "C++" llvm_jit_checkpoint_prepare_result llvm_jit_checkpoint_prepare_instance_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        llvm_jit_checkpoint_prepare_request const&) noexcept;

    extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::array<::std::byte, 16u> const& recording_label,
        ::uwvm2::uwvm::debugger::checkpoint::limits const& budget) noexcept;

    extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::array<::std::byte, 16u> const&, ::uwvm2::uwvm::debugger::checkpoint::limits const&) noexcept;

    extern "C++" llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_leaf_host_api(
        llvm_jit_checkpoint_thread_capture_owner const&, void* result, ::std::size_t result_bytes) noexcept;
    // Actual bottom-up native returned-child dispatch for a captured thread.
    // It still does not restore instance resources or authorize file replay.
    extern "C++" llvm_jit_checkpoint_continuation_status llvm_jit_checkpoint_continue_saved_thread_host_api(
        llvm_jit_checkpoint_thread_capture_owner const&, void* result, ::std::size_t result_bytes) noexcept;
    // Host-only actual execution retirement. This preserves current instance
    // resources and runtime epoch; it is not database/whole-instance rollback.
    enum class llvm_jit_checkpoint_execution_retirement_status : unsigned char
    {
        requires_llvm_jit_full, not_selected, rejected_current_cohort,
        execution_retired, execution_drained_without_all_signals, failed_closed
    };
    extern "C++" llvm_jit_checkpoint_execution_retirement_status llvm_jit_checkpoint_retire_saved_execution_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>) noexcept;

    enum class llvm_jit_debug_safe_point_granularity : unsigned { entry_loop, instruction };
    struct llvm_jit_debug_local_view
    {
        // The generated frame owns values until the synchronous park callback
        // returns. The runtime owns the type array on its callback stack. Neither
        // address is ever exposed to the guest or retained by an observer.
        ::std::byte const* values{};
        ::std::uint_least8_t const* types{};
        // One exact compiler-emitted 0/1 flag per original local index. False
        // means unavailable/uninitialized, not zero, null or optimized-out data.
        ::std::uint_least8_t const* availability{};
        ::std::size_t captured_count{}, total_count{};
        // Actual full-entry generation selected by the live bridge/publication.
        // A copied slot is invalid after replacement, even at the same Wasm PC.
        ::std::uint_least64_t function_generation{};
    };
    struct llvm_jit_debug_native_step_site
    {
        // Integer-only host metadata copied synchronously by on_before_park.
        // The manager may arm a machine step only for this exact JIT owner/PC
        // while the originating execution lease keeps its code generation alive.
        ::std::uint_least64_t participant{}, native_thread{};
        ::std::uintptr_t return_pc{}, owner_begin{}, owner_end{};
        bool valid{};
    };
    struct llvm_jit_debug_native_code_site
    {
        // Authentic cooperative BEFORE-park return PC/range DATA. This grants
        // no hardware trap, native thread/register or requested-address right.
        ::std::uint_least64_t participant{};
        ::std::uintptr_t return_pc{}, owner_begin{}, owner_end{};
        bool valid{};
    };
    struct llvm_jit_debug_native_target
    {
        // OWNED exact-engine decoder input, copied under the same code lease.
        // This DATA is never a code/source/frame/restore capability.
        ::std::array<char, 256u> triple{};
        ::std::array<char, 256u> cpu{};
        ::std::array<char, 16384u> features{};
        ::std::size_t triple_size{}, cpu_size{}, features_size{};
        unsigned description_version{}, pointer_bits{}, maximum_instruction_bytes{}, minimum_instruction_alignment{};
        bool little_endian{}, available{};
    };
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" llvm_jit_debug_native_activation_cursor_owner llvm_jit_debug_mint_native_activation_host_api(
        llvm_jit_debug_activation_capture_owner const&, void const* native_session_identity) noexcept;
    extern "C++" bool llvm_jit_debug_native_activation_provider_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, llvm_jit_debug_native_activation_provider&) noexcept;
    // ONE current real trap + execution lease + original participant ticket +
    // publication jointly authenticate the freshly witnessed physical cursor.
    extern "C++" bool llvm_jit_debug_native_activation_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const* native_session_identity) noexcept;
    // Authenticated physical caller identity only. No native RA, SP/FP, saved
    // registers or stack bytes can leave the private runtime unwinder. This is
    // inspection DATA, never a native finish/resume capability.
    struct llvm_jit_debug_native_caller_view
    {
        ::std::uint_least64_t module{}, function{}, function_generation{}, runtime_epoch{};
        ::std::uint64_t incarnation{}, current_incarnation{};
        bool valid{};
    };
    extern "C++" bool llvm_jit_debug_native_caller_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const* native_session_identity,
        llvm_jit_debug_native_caller_view&) noexcept;
    // Bounded physical Wasm parents, nearest first. All frames are individually
    // authenticated. complete means the actual Wasm root was reached; a limit
    // or an unavailable CFI/owner yields only the already proved prefix.
    struct llvm_jit_debug_native_backtrace_view
    {
        ::std::array<llvm_jit_debug_native_caller_view, 32u> frames{};
        ::std::size_t count{};
        bool complete{};
    };
    extern "C++" bool llvm_jit_debug_native_backtrace_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const* native_session_identity,
        llvm_jit_debug_native_backtrace_view&, ::std::size_t maximum_frames = 32u) noexcept;
    // Opaque, revocable physical-return proof, issued only from a genuine
    // current kernel trap and bounded registered CFI. Its query exposes only
    // the authenticated Wasm parent identity. RA/CFA, SP/FP, stack bytes and
    // native code owners remain private. A protected synchronous host resume
    // borrow revalidates all evidence while holding domain/publication through
    // actual wake. It does not install a finish event or publish a parent stop.
    // Copying its identity view cannot nominate a native target.
    class llvm_jit_debug_native_return_continuation;
    using llvm_jit_debug_native_return_continuation_owner = ::std::shared_ptr<llvm_jit_debug_native_return_continuation const>;
    extern "C++" llvm_jit_debug_native_return_continuation_owner llvm_jit_debug_mint_native_return_continuation_host_api(
        llvm_jit_debug_native_activation_cursor_owner const&, void const* native_session_identity) noexcept;
    extern "C++" bool llvm_jit_debug_native_return_continuation_host_api(
        llvm_jit_debug_native_return_continuation_owner const&, void const* native_session_identity,
        llvm_jit_debug_native_caller_view&) noexcept;
    using llvm_jit_debug_native_return_resume_callback = void (*)(void*,
        llvm_jit_debug_native_caller_view const&,
        ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow&) noexcept;
    // One domain scope joins fresh physical-return authentication and backend
    // wake. The callback is trusted synchronous host code, not a console hook;
    // it receives ONLY Wasm parent identity. RA/CFA and native bytes stay private.
    // No domain reentry, allocation or waiting for worker ACK belongs in it.
    // A callback that does not commit, or a false wake, retains the current stop.
    // This foundational transaction does not qualify executable native finish.
    extern "C++" bool llvm_jit_debug_resume_native_return_continuation_host_api(
        llvm_jit_debug_native_return_continuation_owner const&, void const* native_session_identity,
        void* context, llvm_jit_debug_native_return_resume_callback) noexcept;
    using llvm_jit_debug_native_return_event_callback = void (*)(void*,
        llvm_jit_debug_native_return_event const&,
        ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow&) noexcept;
    // Fresh proof -> precise parent event installation -> one-shot actual wake.
    // Retain the sealed event/provider until disable -> real worker ACK -> clear.
    // This qualifies the kernel return event, not a rebased public parent stop.
    extern "C++" bool llvm_jit_debug_resume_native_return_event_host_api(
        llvm_jit_debug_native_return_continuation_owner const&, void const* native_session_identity,
        void* context, llvm_jit_debug_native_return_event_callback) noexcept;
    extern "C++" bool llvm_jit_debug_query_native_return_event_host_api(
        llvm_jit_debug_native_return_event const&, void const* native_session_identity,
        llvm_jit_debug_native_caller_view&) noexcept;
    // Independent native-only parent capture, minted at the exact completed
    // return event. No child locals, source memory or cooperative PC is promoted.
    struct llvm_jit_debug_native_return_stop
    {
        llvm_jit_debug_activation_capture_owner capture{};
        llvm_jit_debug_native_activation_cursor_owner cursor{};
    };
    extern "C++" bool llvm_jit_debug_capture_native_return_stop_host_api(
        llvm_jit_debug_native_return_event const&, void const* native_session_identity,
        llvm_jit_debug_native_return_stop&) noexcept;
    // Host-private near-call capability. The view is DATA only; an earlier
    // query cannot authorize event enable or waking the worker. Only the sole
    // protected resume callback below joins exact trap, publication and wake.
    // Retain every strong owner until actual backend event disable -> worker
    // released ACK -> session clear, including all successful continuation traps.
    // No integer PC, supplied byte copy or register snapshot is accepted.
    class llvm_jit_debug_native_call_continuation;
    using llvm_jit_debug_native_call_continuation_owner = ::std::shared_ptr<llvm_jit_debug_native_call_continuation const>;
    struct llvm_jit_debug_native_call_continuation_view
    {
        ::std::uint_least64_t thread{};
        ::std::uintptr_t origin_pc{}, continuation_pc{}, owner_begin{}, owner_end{};
        ::std::uint_least64_t module{}, function{}, function_generation{}, runtime_epoch{};
        friend constexpr bool operator==(llvm_jit_debug_native_call_continuation_view const&,
            llvm_jit_debug_native_call_continuation_view const&) noexcept = default;
    };
    extern "C++" llvm_jit_debug_native_call_continuation_owner llvm_jit_debug_mint_native_call_continuation_host_api(
        llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_native_activation_cursor_owner const&,
        void const* native_session_identity) noexcept;
    extern "C++" bool llvm_jit_debug_native_call_continuation_host_api(
        llvm_jit_debug_native_call_continuation_owner const&, void const* native_session_identity,
        llvm_jit_debug_native_call_continuation_view&) noexcept;
    using llvm_jit_debug_native_call_event_resume_callback = void (*)(void*,
        llvm_jit_debug_native_call_event const&,llvm_jit_debug_native_call_continuation_view const&,
        ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow&) noexcept;
    extern "C++" bool llvm_jit_debug_resume_native_call_event_host_api(
        llvm_jit_debug_native_call_continuation_owner const&,void const*,void*,
        llvm_jit_debug_native_call_event_resume_callback) noexcept;
    using llvm_jit_debug_native_call_resume_callback = void (*)(void*,
        llvm_jit_debug_native_call_continuation_view const&,
        ::uwvm2::utils::thread::cooperative_pause_domain::external_resume_borrow&) noexcept;
    // Sole protected call execution transition. It validates the exact current
    // trap/caller/callee inside ONE real domain+publication scope. The callback
    // is synchronous trusted host code; copied DATA cannot request a wake.
    extern "C++" bool llvm_jit_debug_resume_native_call_continuation_host_api(
        llvm_jit_debug_native_call_continuation_owner const&, void const* native_session_identity,
        void* context, llvm_jit_debug_native_call_resume_callback) noexcept;
#endif
    inline constexpr ::std::size_t llvm_jit_debug_max_native_code_bytes{480u};
    struct llvm_jit_debug_native_code_bytes
    {
        // Owned copy only. PC is a display origin, never an address parameter.
        llvm_jit_debug_native_target target{};
        bool native_instruction_stop{}; // only an authenticated actual native trap
        ::std::uint8_t bytes[llvm_jit_debug_max_native_code_bytes]{};
        ::std::uint8_t guest_code[llvm_jit_debug_max_native_code_bytes]{}; // complete qualifying instruction bytes only
        ::std::size_t size{};
        ::std::uintptr_t pc{};
        ::std::uint_least64_t participant{}, module{}, function{}, function_generation{}, runtime_epoch{};
    };
    // Cold bounded read of the actual current owned JIT function. No requested
    // address/range reaches the API. Runtime-private before-park capture/control
    // block identity, real lease -> ONE domain -> publication are mandatory.
    // A nonnull session identity is accepted only while the actual active native
    // trap remains externally parked; source/locals authority is NOT extended.
    extern "C++" bool llvm_jit_debug_copy_native_code_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_code_bytes& out) noexcept;
    inline constexpr ::std::size_t llvm_jit_debug_max_native_function_name_bytes{256u};
    struct llvm_jit_debug_native_function_image
    {
        // Separate cold reply type: never stored in the runtime publication,
        // TLS ledger or capture. Only copied, owned bytes leave the real guard.
        llvm_jit_debug_native_target target{};
        bool native_instruction_stop{}; // false means a cooperative safepoint code view
        ::std::vector<::std::uint8_t> bytes{};
        ::std::vector<::std::uint8_t> guest_code{};
        // ARM ELF mapping DATA: 0 is a proved literal pool, 1 is text.
        // Empty retains ordinary forward decoding on other architectures.
        // This grants no display, memory or execution permission.
        ::std::vector<::std::uint8_t> instruction_code{};
        ::std::size_t size{};
        ::std::uintptr_t stop_pc{}, owner_begin{}, owner_end{};
        ::std::uint_least64_t participant{}, module{}, function{}, function_generation{}, runtime_epoch{};
        char8_t function_name[llvm_jit_debug_max_native_function_name_bytes]{};
        ::std::size_t function_name_size{};
        [[nodiscard]] bool complete_storage() const noexcept
        { return size != 0u && bytes.size() == size && guest_code.size() == size &&
            (instruction_code.empty() || instruction_code.size() == size); }
    };
    // No requested PC/offset/range. A current private capture and, for a native
    // park, the real active trap session authorize this exact function only.
    // Name lookup attempts only its actual published Wasm function name; no
    // external loader, arbitrary address resolver, source/locals authority.
    extern "C++" bool llvm_jit_debug_copy_native_function_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_function_image& out) noexcept;
    enum class llvm_jit_debug_native_position_status : unsigned char { unavailable, unknown, exact, ambiguous };
    struct llvm_jit_debug_native_numeric_location { unsigned dwarf_register{}, bits{}; };
    struct llvm_jit_debug_native_position
    {
        llvm_jit_debug_native_position_status status{llvm_jit_debug_native_position_status::unavailable};
        ::std::uintptr_t pc{}, owner_begin{}, owner_end{}, row_begin{}, row_end{};
        ::std::uint_least64_t participant{}, module{}, function{}, function_generation{}, runtime_epoch{};
        ::std::uint_least32_t wasm_offset{};
        llvm_jit_debug_native_numeric_location numeric_locations[64u]{};
        ::std::size_t numeric_location_count{};
    };
    // Host-only current position DATA. No requested native PC/offset/range or
    // filename is accepted. The real private capture/parked trap and actual
    // code-owner lifetime authorize this query, never a DAP numeric address.
    // True authenticates the position even if the optimized provenance is
    // unavailable/unknown/ambiguous. Only `exact` identifies a Wasm byte offset;
    // this API grants no guest-local/source-variable/native-memory authority.
    extern "C++" bool llvm_jit_debug_native_position_host_api(
        llvm_jit_debug_activation_capture_owner const& capture, void const* native_session_identity,
        llvm_jit_debug_native_position& out) noexcept;
    struct llvm_jit_debug_observer
    {
        // HOST-ONLY owning context. A guest value/descriptor is never an observer.
        // The callback executes on the guest's native thread, before it parks, and
        // may request a cooperative pause. It must not enter guest code, perform
        // runtime reset/compilation, wait for other executions, or throw.
        ::std::shared_ptr<void> context{};
        void (*on_safe_point)(void* context, ::std::uint_least64_t participant,
            ::uwvm2::utils::thread::cooperative_pause_location location) noexcept{};
        // Called only on the cold pause path, before the real participant parks,
        // outside the pause-domain lock. Capture this thread's stack here: a
        // manager can request pause just after on_safe_point returned. The same
        // restrictions as on_safe_point apply. A concurrent resume can cancel
        // this park, so the manager still confirms the matching pause ticket.
        void (*on_before_park)(void* context, ::std::uint_least64_t participant,
            ::uwvm2::utils::thread::cooperative_pause_location location, llvm_jit_debug_local_view locals) noexcept{};
        // HOST-COLD ONLY: runtime reset/stop retains context and closes the
        // exact configured domain, then invokes this outside publication/domain
        // locks BEFORE waiting for execution leases. A real native trap does
        // not poll cancellation and must release its backend gate here.
        // Verify the closed-domain identity before retiring a native session.
        // May wait for that handler's release; may not execute guest code,
        // compile, reset/drain the runtime recursively, or throw. False is a
        // trusted-host drain failure: code/session owners must not be destroyed.
        bool (*on_close)(void* context,
            ::uwvm2::utils::thread::cooperative_pause_domain const* closed_domain) noexcept{};
        // Cold real Wasm failure only, outside signal handlers. True requests
        // a terminal cooperative stop; continuation retains fatal semantics.
        // Values are the saved before-opcode inputs, never a successful result.
        bool (*on_trap)(void* context, ::std::uint_least64_t participant,
            ::uwvm2::utils::thread::cooperative_pause_location location) noexcept{};
        // Actual thrown guest exception, before any generated frame unwinds.
        // Exact sealed clauses across the complete current Wasm chain have no
        // matching real tag identity. True requests a terminal read-only park.
        bool (*on_uncaught)(void* context, ::std::uint_least64_t participant,
            ::uwvm2::utils::thread::cooperative_pause_location location) noexcept{};
    };
    /// Host-only launch opt-in, before any runtime registry is published. The controller owns authorization
    /// and must never expose this native API or its domain through guest imports. The runtime retains the
    /// domain through execution drain; reset closes it and removes the opt-in. Configuration, reset and
    /// loader changes are externally serialized host administration operations. No endpoint is opened here.
    /// Only LLVM full supports this option. Existing noninstrumented code cannot acquire safe points later.
    extern "C++" llvm_jit_debug_configure_result llvm_jit_configure_debug_safe_points_host_api(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> control) noexcept;
    /// Cold host-only activation storage budget, set before any publication.
    /// This controls recording memory, not a physical call-depth quota.
    extern "C++" llvm_jit_debug_configure_result llvm_jit_configure_debug_activation_storage_host_api(::std::size_t bytes) noexcept;
    /// Same launch opt-in with an owning host observer for breakpoint/step policy.
    extern "C++" llvm_jit_debug_configure_result llvm_jit_configure_debug_session_host_api(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> control,
        llvm_jit_debug_observer observer,
        llvm_jit_debug_safe_point_granularity granularity = llvm_jit_debug_safe_point_granularity::entry_loop) noexcept;
    /// Compile/publish the configured LLVM full registry without executing a Wasm
    /// function. The host may present a prepared, not-started console after success;
    /// no guest PC/stack exists yet. Entry selection and active segments are handled
    /// by the normal run API. Call during serialized host setup, not from callbacks.
    extern "C++" bool llvm_jit_prepare_debug_host_api() noexcept;
    struct llvm_jit_debug_safe_point_view
    {
        // Borrowed only for one serialized host management operation. Runtime
        // reset or a committed function replacement invalidates this view.
        ::std::uint_least8_t const* bits{};
        ::std::size_t byte_count{}, expression_size{};
        ::std::uint_least64_t function_generation{};
        [[nodiscard]] bool contains(::std::uint_least64_t offset) const noexcept
        {
            if(bits == nullptr || offset >= expression_size || offset / 8u >= byte_count) { return false; }
            // [byte_count complete bitmap bytes] bitmap_end
            // [safe                            ] unsafe (one-past)
            //  ^^ checked index selects one live byte; no pointer advances.
            return (bits[static_cast<::std::size_t>(offset / 8u)] & (1u << (offset % 8u))) != 0u;
        }
        [[nodiscard]] explicit operator bool() const noexcept { return bits != nullptr && byte_count != 0u; }
    };
    // Host management thread only, after prepare_debug_host_api(). A module
    // function index includes imports; imported/unknown functions return {}.
    extern "C++" llvm_jit_debug_safe_point_view llvm_jit_debug_safe_points_host_api(
        ::std::uint_least64_t module_index, ::std::uint_least64_t function_index) noexcept;
    /// Capture an owning innermost-first stack only from the active safe-point
    /// observer on the actual guest thread. The manager thread has no guest stack.
    /// Empty means unavailable (including call-stack=none), invalid context or
    /// allocation failure. Saved names survive reset; native PCs are not exposed.
    extern "C++" ::uwvm2::runtime::exception::diagnostic_trace_ref llvm_jit_capture_debug_stack_host_api() noexcept;
    /// Host-only: usable only synchronously inside on_before_park on the
    /// selected generated-Wasm thread. Resolves the exact JIT code owner and
    /// rejects any PC outside that function's native executable extent.
    extern "C++" llvm_jit_debug_native_code_site llvm_jit_capture_debug_native_code_site_host_api() noexcept;
    extern "C++" llvm_jit_debug_native_step_site llvm_jit_capture_debug_native_step_site_host_api() noexcept;
    /// Host-only opt-in after LLVM-full debug setup. Non-Linux-x86_64 builds
    /// fail closed until a separately qualified native trap adapter exists.
    extern "C++" bool llvm_jit_enable_debug_native_step_host_api() noexcept;
    /// Bounded host debugger read, called only after the controller confirms
    /// every admitted guest participant is stopped. Module and memory indices
    /// are Wasm registry indices, never native addresses. This adds no generated
    /// memory-access instructions or reader-side lock to ordinary execution.
    extern "C++" bool llvm_jit_debug_read_memory_host_api(::std::uint_least64_t module_index,
        ::std::uint_least32_t memory_index, ::std::uint_least64_t offset, void* destination,
        ::std::size_t size) noexcept;

    enum class llvm_jit_debug_replace_status : unsigned
    { replaced, unsupported_mode, invalid_state, invalid_target, stale_generation, invalid_body, abi_mismatch, compile_failure, exhausted };
    struct llvm_jit_debug_replace_result
    {
        llvm_jit_debug_replace_status status{llvm_jit_debug_replace_status::unsupported_mode};
        ::std::uint_least64_t generation{};
    };
    struct llvm_jit_debug_replace_transaction;
    struct llvm_jit_debug_replace_prepare_result
    {
        // `replaced` here means a private generation is ready; it has not been
        // published. Only commit may report a visible replacement.
        llvm_jit_debug_replace_status status{llvm_jit_debug_replace_status::unsupported_mode};
        ::std::uint_least64_t generation{};
        llvm_jit_debug_replace_transaction* transaction{};
    };
    /// Validate and compile one complete Wasm function body without changing any
    /// callable target. Module/function use public Wasm indices, including imports.
    /// A successful prepare returns an owning token that must be discarded once.
    extern "C++" llvm_jit_debug_replace_prepare_result llvm_jit_debug_prepare_function_replacement_host_api(
        ::std::uint_least64_t module_index, ::std::uint_least64_t function_index,
        ::std::uint_least64_t expected_generation, ::std::byte const* body,
        ::std::size_t body_size) noexcept;
    /// The controller calls commit only inside cooperative_pause_domain::while_stopped
    /// after proving that no parked guest stack contains the target function.
    /// Commit never compiles and returns success only after all target aliases have
    /// been atomically published. It does not destroy its token.
    extern "C++" llvm_jit_debug_replace_result llvm_jit_debug_commit_function_replacement_host_api(
        llvm_jit_debug_replace_transaction* transaction) noexcept;
    extern "C++" void llvm_jit_debug_discard_function_replacement_host_api(
        llvm_jit_debug_replace_transaction* transaction) noexcept;
    /// Compatibility host API for embedders without a pause ticket. It must not
    /// publish because an active guest activation cannot be proven absent here.
    extern "C++" llvm_jit_debug_replace_result llvm_jit_debug_replace_function_host_api(
        ::std::uint_least64_t module_index, ::std::uint_least64_t function_index,
        ::std::uint_least64_t expected_generation, ::std::byte const* body,
        ::std::size_t body_size) noexcept;


#endif

    /// Native CLI setup only: derives ownership/phase from actual initialized
    /// storage and launch scope. External modules/raw APIs remain GC-ineligible.
    /// Unsupported/busy/already-published leaves normal execution unchanged.
    extern "C++" ::uwvm2::runtime::gc::managed_gc_configure_result runtime_gc_prepare_cli_collection_host_api() noexcept;
    /// Atomic source-bound counters. In-flight native pressure is accounted at
    /// the next transaction or outer entry leave; final run counters are exact.
    extern "C++" ::uwvm2::runtime::gc::managed_gc_metrics runtime_gc_collection_metrics_host_api() noexcept;

    struct entry_function_abi_buffers
    {
        ::std::byte const* param_buffer{};
        ::std::size_t param_bytes{};
        ::std::byte* result_buffer{};
        ::std::size_t result_bytes{};
    };

    struct full_compile_run_config
    {
        /// @brief The first function index to enter in the main module.
        /// @note  This is the WASM function index space (imports first, then local-defined).
        /// @note  Conventional imported entries must resolve to Wasm; module_start also permits a void host import.
        ::std::size_t entry_function_index{};
        entry_function_abi_buffers entry_abi_buffers{};
        /// @brief A validated start-section invocation may target a host import with signature () -> ().
        bool module_start{};
    };

    struct lazy_compile_run_config
    {
        /// @brief The first function index to enter in the main module.
        /// @note  This is the WASM function index space (imports first, then local-defined).
        /// @note  Imported entries are only supported when they resolve to a wasm-defined `() -> ()` function.
        ::std::size_t entry_function_index{};
        entry_function_abi_buffers entry_abi_buffers{};
        bool assume_full_code_verified{};
        /// @brief A validated start section may invoke a void host import.
        bool module_start{};
    };

    /// @brief Validate and translate every initialized module with the selected full compiler without executing guest code.
    /// @note Call before applying ANY active segment or invoking ANY start/entry function in the module graph.
    ///       Fused validation/translation occurs once; the compatible later full entry reuses this publication.
    /// @note Returns false for unavailable/non-full backends, empty registry or execution/publication/provider re-entry.
    ///       Invalid bodies retain existing full-compiler fatal diagnostics. The host serializes loader/configuration/reset.
    ///       Actual generation/GC admission and floating-point scopes protect this cold operation; no guest frame is fabricated.
    extern "C++" bool full_compile_prepare_host_api() noexcept;

    /// @brief Fuse all initialized lazy-module bodies into checked backend artifacts without entering guest code.
    /// @note The CLI calls before active data/element effects or any module start. A compatible later lazy entry reuses
    ///       the actual ready generation; no pure validation prepass, second body translation or fake guest frame occurs.
    /// @note Returns false for non-lazy/unavailable modes, empty registry or execution/publication/provider re-entry.
    ///       Loader/configuration/reset remain host-serialized. Native codegen may stay deferred or run in owned workers.
    extern "C++" bool lazy_compile_prepare_host_api(::uwvm2::utils::container::u8string_view main_module_name,
                                                   lazy_compile_run_config) noexcept;

    /// @brief Full-compile and run the main module using the configured runtime backend.
    /// @note  This expects uwvm runtime initialization to be complete (runtime storages + import resolution).
    /// @note  After a runtime registry is published, changing full/lazy mode or backend configuration requires a quiescent
    ///        reset_runtime_state_host_api() call before the next run; incompatible reuse fails closed.
    /// @note  This entry is not reentrant on one thread. Host callbacks that need generated-Wasm re-entry must use
    ///        llvm_jit_call_raw_host_api().
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config) noexcept;

    /// @brief Lazily compile and run the main module using the configured lazy-capable backend.
    /// @note  This expects uwvm runtime initialization to be complete (runtime storages + import resolution).
    /// @note  After a runtime registry is published, changing full/lazy mode or backend configuration requires a quiescent
    ///        reset_runtime_state_host_api() call before the next run; incompatible reuse fails closed.
    /// @note  This entry is not reentrant on one thread. Host callbacks that need generated-Wasm re-entry must use
    ///        llvm_jit_call_raw_host_api().
    /// @note Background compiler workers belong to the VM generation and may continue after this entry returns.
    ///       reset_runtime_state_host_api() drains executions and joins them before releasing their metadata.
    extern "C++" void lazy_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, lazy_compile_run_config) noexcept;

    /// @brief Stop every runtime-owned background worker before a WASI proc_exit leaves the normal run loop.
    /// @note On Linux the fast exit path terminates the calling thread directly. Compiler and JIT-cache workers must therefore be
    ///       joined explicitly, otherwise a surviving worker can keep the process alive indefinitely.
    extern "C++" void runtime_stop_before_proc_exit_host_api() noexcept;

    /// @brief Compatibility spelling for callers built against the original lazy-worker shutdown API.
    extern "C++" void lazy_compile_stop_before_proc_exit_host_api() noexcept;

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Normal HOST launch only. The native runtime factory derives and retains
    // the actual current source/profile/control/epoch; no caller-supplied
    // thread, native handle, participant label or serialized ACK is adopted.
    // Its owned trampoline seals each genuine outer debug execution scope on
    // this one OS worker. Graph initialization may enter several such scopes.
    class llvm_jit_debug_guest_worker;
    using llvm_jit_debug_guest_worker_owner = ::std::shared_ptr<llvm_jit_debug_guest_worker const>;
    using llvm_jit_debug_guest_worker_body = void (*)(void*) noexcept;
    enum class llvm_jit_debug_guest_worker_launch_status : unsigned char
    { started, invalid_context, stale_control, admission_closed, quota, allocation_failure, unsupported_mode };
    struct llvm_jit_debug_guest_worker_launch_result
    {
        llvm_jit_debug_guest_worker_launch_status status{llvm_jit_debug_guest_worker_launch_status::unsupported_mode};
        llvm_jit_debug_guest_worker_owner worker{};
    };
    extern "C++" llvm_jit_debug_guest_worker_launch_result runtime_launch_llvm_jit_debug_guest_worker_host_api(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&,
        ::std::shared_ptr<void> launch_state, llvm_jit_debug_guest_worker_body) noexcept;
    // These observations are diagnostics only. The real joint publication
    // manager consumes its own private complete-cohort OS-retirement proof.
    // Pending/error/unsupported retain the genuine native owner and never
    // substitute entry-lease drain, notify_guest_exit or a deadline for OS join.
    extern "C++" ::uwvm2::utils::thread::physical_join_result runtime_join_llvm_jit_debug_guest_worker_until_host_api(
        llvm_jit_debug_guest_worker_owner const&, ::std::chrono::steady_clock::time_point) noexcept;
    extern "C++" bool runtime_llvm_jit_debug_guest_worker_matches_control_host_api(
        llvm_jit_debug_guest_worker_owner const&,
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept;
    // Negative-only actual-trampoline query for between-entry graph work.
    // This cannot manufacture a stop/capture, participant or publication proof.
    extern "C++" bool runtime_llvm_jit_debug_registered_worker_stopping_host_api() noexcept;
    // Completion DATA from the actual initial trampoline after its real outer
    // entry returned; zero for normal return. Never grants stop/join authority.
    extern "C++" ::std::uint_least32_t runtime_llvm_jit_debug_registered_worker_exit_code_host_api() noexcept;
#endif

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    class llvm_jit_debug_shutdown_request;
    using llvm_jit_debug_shutdown_request_owner = ::std::shared_ptr<llvm_jit_debug_shutdown_request const>;
    enum class llvm_jit_debug_shutdown_status : unsigned char
    {
        started, pending_execution, execution_drained, pending_producers, resources_quiescent,
        reentrant, busy, stale_owner, unavailable_cleanup, unsupported_mode
    };
    struct llvm_jit_debug_shutdown_start
    {
        llvm_jit_debug_shutdown_status status{llvm_jit_debug_shutdown_status::unsupported_mode};
        llvm_jit_debug_shutdown_request_owner owner{};
    };
    // Terminal cleanup ABI2 requires actual backend physical retirement.
    // Older runtime objects have no matching symbol: a mixed link fails instead
    // of relabeling the old WORK-only quiescence as physical retirement. The
    // revision is ONLY a rejection guard; all actual owner/entry/OS ACK checks
    // below still independently apply. Query BEFORE arming any actual request.
    extern "C++" ::std::uint_least32_t runtime_debug_shutdown_terminal_cleanup_abi_host_api() noexcept;
    // Trusted management only. Domain ownership is compared BEFORE a supplied
    // pointee is read. The private actual request retains maintenance, source,
    // native code and current function generations until ACTUAL leases drain.
    // The originating native management thread must stay alive until release;
    // wait/release/retry on another real thread return reentrant. A private
    // nonreused native-thread incarnation prevents reused TID/TLS from unlocking
    // somebody else's actual std::mutex maintenance token.
    // Actual lease stop is published without invoking stop_token callbacks:
    // legacy atomic.wait cancellation currently traps inside a noexcept bridge.
    // Blocking waits/syscalls remain actual pending; this API does not claim
    // they woke or force host unwinding. A later qualified debug wait-return
    // cleanup bridge is required before safe cancellation delivery is enabled.
    // Same installed domain returns its canonical pending owner on retry.
    // No raw pointer, saved checkpoint, numeric ACK or guest import arms this.
    extern "C++" llvm_jit_debug_shutdown_start runtime_begin_llvm_jit_debug_shutdown_host_api(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept;
    // wait_milliseconds is clamped to 50; 0 is a nonwaiting actual-state query.
    // Timeout keeps admission closed and ALL actual owners retained. The state
    // resources_quiescent means admitted entry TLS/GC/native cleanup completed
    // and actual full compiler workers/cache native worker physically retired.
    // Cache retirement uses the qualified FastIO native-owner join; timeout or
    // unsupported provider remains pending with its true native owner retained.
    // It is NOT caller guest-thread join, external service destruction or all
    // user finalizer execution. Those caller-owned threads must join separately.
    extern "C++" llvm_jit_debug_shutdown_status runtime_poll_llvm_jit_debug_shutdown_host_api(
        llvm_jit_debug_shutdown_request_owner const&, ::std::uint_least64_t wait_milliseconds) noexcept;
    // Caller must first physically join its own genuine execution worker. This
    // releases only our maintenance/retained request, after rechecking REAL work
    // quiescence. Runtime code/source registries remain owned until normal reset.
    // Pending requests are never destroyed by dropping a caller's shared_ptr.
    extern "C++" bool runtime_release_llvm_jit_debug_shutdown_host_api(
        llvm_jit_debug_shutdown_request_owner const&) noexcept;
#endif

    /// @brief Cooperatively cancel admitted host executions and close admission until reset.
    /// @note Does not wait for execution to finish and may be called from a host callback. It does not forcibly stop guest
    ///       computation or destroy resources. Blocking services use the current execution's cancellation token.
    /// @note On builds without native threads this operation is a no-op.
    extern "C++" void runtime_request_execution_stop_host_api() noexcept;

    /// @brief Close execution admission, cancel/drain active entries, join compiler workers and flush cache writes.
    /// @note Retains code and module registries; admission stays closed until reset_runtime_state_host_api(). This
    ///       is a synchronous host management operation, forbidden from execution or compilation/provider callbacks.
    ///       Nested raw calls belonging to an already admitted execution can finish while the host waits.
    /// @note Cancellation is cooperative; an unbounded computation or uncooperative host callback can delay return.
    ///       The caller still joins its own OS threads and serializes loader/configuration changes. Reset is required
    ///       before destroying external module storage, since retained registries still borrow that storage.
    /// @note On builds without native threads there are no execution leases; owned producers are still stopped.
    extern "C++" void runtime_stop_and_drain_host_api() noexcept;

    /// @brief Query cancellation of this thread's current outermost execution (including callback re-entry).
    /// @note Returns false outside an execution entry or on builds without native threads.
    extern "C++" bool runtime_execution_stop_requested_host_api() noexcept;

    /// @brief Clear backend-neutral and selected-backend runtime state before loading a fresh module set in the same process.
    /// @note  Embedders must call this before destroying or replacing runtime storage referenced by compiled or lazy caches.
    /// @note  On native-thread builds reset closes execution admission, requests cancellation and waits for all outermost
    ///        full/lazy/raw entries to return before joining workers and destroying code/registries. New entries during reset
    ///        fail closed; supported nested raw callback entries reuse their existing admission. Admission reopens after cleanup.
    /// @note  Cancellation is cooperative: an unbounded guest computation can delay reset indefinitely. The host must still
    ///        synchronize loader/configuration changes and joins of its own OS threads; reset does not destroy external storage.
    /// @note  Same-thread reset during any active runtime execution entry fails closed.
    /// @note  Reset also fails closed from provider callbacks made while this thread owns runtime publication.
    extern "C++" void reset_runtime_state_host_api() noexcept;

    /// @brief Observe the actual process-local reset generation for cold native compiler data.
    /// @note A scalar observation supplies no source/instance authorization, execution admission,
    ///       generation lease or publication lifetime. Callers still serialize native configuration,
    ///       source selection and worker drain; a comparison alone cannot protect a racing reset.
    /// @note No guest import or execution hot path calls this API. It reads the same nonwrapping
    ///       counter used to invalidate actual runtime caches during reset, without touching TLS/maps.
    extern "C++" [[nodiscard]] ::std::uint_least64_t observe_compiler_runtime_generation_host_api() noexcept;

#if defined(UWVM_RUNTIME_LLVM_JIT)
    /// @brief Compatibility spelling retained for existing LLVM embedding callers.
    extern "C++" void llvm_jit_reset_runtime_state_host_api() noexcept;

    /// @brief Invoke one generated function through the public raw ABI; this is the sole same-thread execution-callback re-entry API.
    /// @note  Re-entry from provider callbacks during registry publication fails closed because the registry is incomplete.
    extern "C++" void llvm_jit_call_raw_host_api(void const* runtime_module_ptr,
                                                 ::std::uint_least32_t func_index,
                                                 void* result_buffer,
                                                 ::std::size_t result_bytes,
                                                 void const* param_buffer,
                                                 ::std::size_t param_bytes) noexcept;

#endif

    extern "C++" ::std::size_t preload_memory_descriptor_count_host_api() noexcept;
    extern "C++" bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index,
                                                            ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t* out) noexcept;
    extern "C++" bool preload_memory_read_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept;
    extern "C++" bool preload_memory_write_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept;
}  // namespace uwvm2::runtime::lib

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
