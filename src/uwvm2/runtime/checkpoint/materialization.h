/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <utility>
# include <vector>
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    inline constexpr ::std::uint32_t state_schema_revision{6u};
    namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
    inline constexpr ::std::size_t native_slot_bytes{16u};
    // Actual runtime/compiler continuation protocol, shared by both selected
    // purposes and the build/cache binding. ABI1 objects must not be reused.
    inline constexpr ::std::uint64_t native_resume_abi_revision{2u};
    // Per-function checkpoint storage only, not the complete native call stack.
    // Four extra native owners: legacy entry payload, max-live dynamic payload,
    // executed flags and saved flags. Reserve 16 bytes per owner for alignment;
    // actual native frame extent still requires target/assembly qualification.
    inline constexpr ::std::size_t native_workspace_limit{32768u};
    inline constexpr ::std::size_t native_workspace_alignment_reserve{64u};
    // Observation uses activation-owned heap storage: two local flag ranges,
    // a private local-address table, then the maximum live typed packet. The
    // table never crosses the host snapshot API or public ASM projection.
    // This bounds arithmetic only; the observer profile bounds allocation.
    inline constexpr ::std::size_t observer_local_metadata_bytes{2u + sizeof(::std::uintptr_t)};
    [[nodiscard]] inline constexpr bool observer_workspace_fits(::std::size_t locals, ::std::size_t slots) noexcept
    {
        auto const limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
        return slots >= locals && locals <= limit / observer_local_metadata_bytes &&
            slots <= (limit - locals * observer_local_metadata_bytes) / native_slot_bytes;
    }
    [[nodiscard]] inline constexpr bool native_workspace_fits(::std::size_t locals, ::std::size_t slots) noexcept
    {
        if(slots < locals) { return false; }
        auto remaining{native_workspace_limit - native_workspace_alignment_reserve};
        if(locals > remaining / (native_slot_bytes + 2u)) { return false; }
        remaining -= locals * (native_slot_bytes + 2u); // proven product/subtraction first
        return slots <= remaining / native_slot_bytes;
    }
    [[nodiscard]] inline constexpr bool native_workspace_with_saved_fits(
        ::std::size_t locals, ::std::size_t slots, ::std::size_t saved) noexcept
    {
        if(slots < locals) { return false; }
        auto remaining{native_workspace_limit-native_workspace_alignment_reserve-native_slot_bytes};
        if(locals > remaining/(native_slot_bytes+2u)) { return false; }
        remaining -= locals*(native_slot_bytes+2u);
        if(slots > remaining/native_slot_bytes) { return false; }
        remaining -= slots*native_slot_bytes;
        return saved <= remaining/native_slot_bytes;
    }
    // Resolved original-module syntax/validation policy DATA. Every effective
    // field that changes Core parsing/validation is represented: cli_mode,
    // nineteen proposal switches and two legacy controllable restrictions.
    // CLI ownership bookkeeping and parser resource quotas are separate; this
    // record is not a parser budget, compilation owner or restore credential.
    struct module_feature_policy
    {
        inline static constexpr ::std::uint64_t revision{1u};
        inline static constexpr ::std::uint64_t disable_mask{(::std::uint64_t{1u} << 19u) - 1u};
        ::std::uint64_t cli_mode{}, disabled{}, controlled{};
        [[nodiscard]] constexpr bool valid() const noexcept
        { return cli_mode <= 4u && (disabled & ~disable_mask) == 0u && controlled <= 3u; }
        template<typename Parameter>
        [[nodiscard]] static constexpr module_feature_policy from_actual_parameter(Parameter const& p) noexcept
        {
            module_feature_policy copied{};
            copied.cli_mode = static_cast<::std::uint64_t>(p.cli_mode);
            ::std::array<bool, 19u> const fields{p.disable_multi_value, p.disable_reference_types,
                p.disable_table_instructions, p.disable_multiple_tables, p.disable_bulk_memory,
                p.disable_sign_extension, p.disable_nontrapping_float_to_int, p.disable_simd,
                p.disable_extended_const, p.disable_table_initializer, p.disable_relaxed_simd,
                p.disable_multi_memory, p.disable_tail_call, p.disable_memory64, p.disable_table64,
                p.disable_function_references, p.disable_gc, p.disable_exceptions, p.disable_threads};
            for(::std::size_t n{}; n != fields.size(); ++n)
            { if(fields[n]) { copied.disabled |= ::std::uint64_t{1u} << n; } }
            copied.controlled = (p.controllable_allow_multi_result_vector ? 1u : 0u) |
                (p.controllable_allow_multi_table ? 2u : 0u);
            return copied;
        }
        // Compatibility requirements are derived from this actual enabled
        // policy, not all capabilities supported by the product. The exact
        // per-module record remains required: this mask cannot distinguish
        // table instruction/initializer switches or the MVP grammar family.
        [[nodiscard]] constexpr ::std::uint64_t compatibility_mask() const noexcept
        {
            ::std::uint64_t result{::std::uint64_t{1u} << 5u}; // Mutable globals are Core MVP, no proposal switch.
            auto add = [&](unsigned parameter_bit, unsigned file_bit) constexpr noexcept
            { if((disabled & (::std::uint64_t{1u} << parameter_bit)) == 0u) { result |= ::std::uint64_t{1u} << file_bit; } };
            if((controlled & 1u) == 0u) { add(0u, 0u); }
            add(1u, 1u); add(7u, 2u); add(10u, 3u); add(4u, 4u);
            add(6u, 6u); add(5u, 7u); add(12u, 8u); add(13u, 9u);
            add(11u, 10u); add(14u, 11u); add(16u, 12u); add(15u, 13u);
            add(17u, 14u); add(8u, 15u); add(18u, 16u); add(17u, 17u);
            return result;
        }
        using identity = ::std::array<::std::uint64_t, 4u>;
        [[nodiscard]] constexpr identity cache_identity() const noexcept
        { return {revision, cli_mode, disabled, controlled}; }
        template<typename Output>
        inline void print_cache_identity_to(Output&& output) const
        {
            ::fast_io::print(::std::forward<Output>(output), ::fast_io::mnp::dec(revision), u8"/",
                ::fast_io::mnp::dec(cli_mode), u8"/", ::fast_io::mnp::dec(disabled), u8"/",
                ::fast_io::mnp::dec(controlled));
        }
        friend constexpr bool operator==(module_feature_policy const&, module_feature_policy const&) = default;
    };

    enum class status : unsigned
    {
        ok, invalid_profile, invalid_plan, invalid_activation, invalid_layout,
        invalid_reference, quota_exceeded, unmaterialized, unavailable_resume,
        non_replayable_import, stale_generation, allocation_failed
    };
    struct budgets
    {
        ::std::size_t frames{4096u}, slots_per_frame{1048576u}, controls_per_frame{65536u}, handlers_per_frame{65536u};
        friend bool operator==(budgets const&, budgets const&) = default;
    };
// Zero physical frames means unlimited observation; resumable budgets stay finite.
    [[nodiscard]] inline constexpr budgets default_observation_budgets() noexcept
    { budgets result{}; result.frames = 0u; return result; }
    enum class compilation_purpose : unsigned { observe_values, resumable };
    // Immutable per-engine compilation identity, NEVER management authority.
    // The runtime still requires its actual full-only configuration gate,
    // canonical source/publication owners and privately minted pause ticket.
    class compilation_profile
    {
        budgets cap_{};
        compilation_purpose purpose_{compilation_purpose::resumable};
        explicit compilation_profile(budgets cap, compilation_purpose purpose) noexcept : cap_{cap}, purpose_{purpose} {}
    public:
        using owner = ::std::shared_ptr<compilation_profile const>;
        [[nodiscard]] static owner create_for_trusted_manager(budgets cap = {},
            compilation_purpose purpose = compilation_purpose::resumable)
        {
            if((purpose != compilation_purpose::observe_values && purpose != compilation_purpose::resumable) ||
               (cap.frames == 0u && purpose != compilation_purpose::observe_values) || cap.slots_per_frame == 0u || cap.controls_per_frame == 0u || cap.handlers_per_frame == 0u ||
               cap.slots_per_frame > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / native_slot_bytes)
            { return {}; }
            return owner{new compilation_profile{cap, purpose}};
        }
        [[nodiscard]] static owner create_for_trusted_observer(budgets cap = default_observation_budgets())
        { return create_for_trusted_manager(cap, compilation_purpose::observe_values); }
        [[nodiscard]] budgets const& limits() const noexcept { return cap_; }
        [[nodiscard]] compilation_purpose purpose() const noexcept { return purpose_; }
        // This exact tuple enters the object-cache key (fast_io canonical numeric formatting).
        // A global flag, a borrowed address or a mutable runtime bool cannot
        // distinguish ordinary, observation-only and restorable generated IR.
        using cache_identity_type = ::std::array<::std::uint64_t, 10u>;
        [[nodiscard]] cache_identity_type cache_identity() const noexcept
        {
            // Explicit codegen policy versions distinguish identical caps:
            // observation v13 uses activation-owned heap packets and flags;
            // resumable v17 combines actual retirement, nested native call/EH and
            // owned saved-control tuples, returned-reference root handoff and
            // the exact original-prototype continuation ABI2.
            // Fixed ten-field wire identity remains complete and bounded.
            auto const policy{purpose_ == compilation_purpose::observe_values ? 13u : 17u};
            return {0x554357504c4f4731u, policy, state_schema_revision, native_resume_abi_revision, native_slot_bytes, cap_.frames, cap_.slots_per_frame, cap_.controls_per_frame, cap_.handlers_per_frame, native_workspace_limit};
        }
        // One canonical serializer visits the ENTIRE declared tuple, including
        // the native workspace limit. Callers cannot omit a new trailing field.
        // The static overload formats DATA for comparison only: a tuple never
        // creates this profile or authenticates compilation/capture/restore.
        template<typename Output>
        static void print_cache_identity_to(Output& output, cache_identity_type const& identity)
        {
            bool separator{};
            for(auto const field : identity)
            {
                if(separator) { ::fast_io::io::print(output, u8"/"); }
                ::fast_io::io::print(output, ::fast_io::mnp::dec(field));
                separator = true;
            }
        }
        template<typename Output>
        void print_cache_identity_to(Output& output) const
        { print_cache_identity_to(output, cache_identity()); }
    };
    enum class frame_phase : unsigned { before_opcode, awaiting_call_return, exception_continuation };
    enum class control_kind : unsigned { function, block, loop, if_then, if_else };
    struct typed_slot
    {
        types::core_value_type type{};
        // A nondefaultable local can still be uninitialized at a valid stop.
        // Its alloca must not be read. Operands/control params are always live.
        bool initialized{true};
        friend bool operator==(typed_slot const&, typed_slot const&) = default;
    };
    struct control_layout
    {
        control_kind kind{};
        ::std::size_t entry_offset{}, end_offset{}, outer_operand_height{}, first_saved_parameter{}, saved_parameter_count{};
        ::std::vector<types::core_value_type> declared_parameters{}, declared_results{};
        friend bool operator==(control_layout const&, control_layout const&) = default;
    };
    struct handler_layout
    {
        bool catch_all{}, with_reference{};
        ::std::size_t tag_index{}, target_control{}, target_offset{};
        ::std::vector<types::core_value_type> parameters{};
        friend bool operator==(handler_layout const&, handler_layout const&) = default;
    };
    struct safepoint_layout
    {
        ::std::uint64_t identifier{}; // Dense compiler ordinal, never a native PC.
        ::std::size_t opcode_offset{}, local_count{}, operand_count{}, saved_parameter_count{}, caller_return_offset{};
        frame_phase phase{};
        // Locals, actual operand SSA values, then saved if/else entry params.
        // Exact types come from fused validation BEFORE this edge consumes them.
        ::std::vector<typed_slot> slots{};
        ::std::vector<control_layout> controls{};
        ::std::vector<handler_layout> handlers{};
        friend bool operator==(safepoint_layout const&, safepoint_layout const&) = default;
    };
    struct function_plan
    {
        compilation_profile::owner profile{};
        ::std::size_t module{}, function{}, expression_bytes{};
        ::std::uint64_t function_generation{};
        ::std::vector<safepoint_layout> sites{};
        status compiler_failure{status::ok}; // real malformed producer metadata/emission
        status producer_availability{status::ok}; // quota decline never rejects legal guest code
        // Exact compiled logical landings, sealed after real LLVM verifier
        // succeeds. DATA alone never authenticates a native resume entry.
        ::std::vector<::std::uint64_t> resume_sites{};
        ::std::uint64_t resume_abi_revision{};
        // Only this fused compiler may record actual cleanup Invoke sites.
        // These immutable IDs are DATA until the sole current-cohort manager
        // authenticates original live captures, code owners and host closure.
        ::std::vector<::std::uint64_t> retirement_sites{};
        ::std::uint64_t retirement_abi_revision{};
        // Source plans are DATA, never executable resume authorization. An
        // actual LLVM dispatcher plus every PHI/call/EH/tail edge proof must
        // be retained by canonical full_code_publication before restore exists.
    };
    [[nodiscard]] inline bool known_type(types::core_value_type type) noexcept
    {
        if(type.kind > types::value_kind::reference) { return false; }
        if(type.kind != types::value_kind::reference) { return true; }
        if(type.heap.is_defined()) { return static_cast<::std::uint64_t>(type.heap.code) <= 0xffffffffu; }
        switch(static_cast<types::abstract_heap_type>(type.heap.code))
        {
            case types::abstract_heap_type::noexn: case types::abstract_heap_type::nofunc:
            case types::abstract_heap_type::noextern: case types::abstract_heap_type::none:
            case types::abstract_heap_type::func: case types::abstract_heap_type::extern_:
            case types::abstract_heap_type::any: case types::abstract_heap_type::eq:
            case types::abstract_heap_type::i31: case types::abstract_heap_type::struct_:
            case types::abstract_heap_type::array: case types::abstract_heap_type::exn: return true;
        }
        return false; // Validation-only Bot never has a materialized live value.
    }
    [[nodiscard]] inline status validate_site(safepoint_layout const& site, function_plan const& plan) noexcept
    {
        if(plan.compiler_failure != status::ok) { return plan.compiler_failure; }
        if(!plan.profile || plan.function_generation == 0u || plan.expression_bytes == 0u || site.identifier == 0u ||
           site.opcode_offset >= plan.expression_bytes || site.phase > frame_phase::exception_continuation)
        { return status::invalid_plan; }
        auto const& cap{plan.profile->limits()};
        if(site.slots.size() > cap.slots_per_frame || site.controls.size() > cap.controls_per_frame || site.handlers.size() > cap.handlers_per_frame)
        { return status::quota_exceeded; }
        if(site.local_count > site.slots.size() || site.operand_count > site.slots.size() - site.local_count ||
           site.saved_parameter_count != site.slots.size() - site.local_count - site.operand_count || site.controls.empty())
        { return status::invalid_layout; }
        for(::std::size_t i{}; i != site.slots.size(); ++i)
        {
            auto const& slot{site.slots[i]};
            if(!known_type(slot.type) || (!slot.initialized &&
               (i >= site.local_count || slot.type.kind != types::value_kind::reference || slot.type.nullable)))
            { return status::invalid_layout; }
        }
        ::std::size_t captured{};
        for(auto const& control : site.controls)
        {
            if(control.kind > control_kind::if_else || control.entry_offset >= plan.expression_bytes ||
               control.end_offset >= plan.expression_bytes || control.declared_parameters.size() > cap.slots_per_frame ||
               control.declared_results.size() > cap.slots_per_frame || control.outer_operand_height > site.operand_count ||
               control.first_saved_parameter != captured || control.saved_parameter_count > site.saved_parameter_count - captured)
            { return status::invalid_layout; }
            captured += control.saved_parameter_count;
            for(auto const& type : control.declared_parameters) { if(!known_type(type)) { return status::invalid_layout; } }
            for(auto const& type : control.declared_results) { if(!known_type(type)) { return status::invalid_layout; } }
        }
        if(captured != site.saved_parameter_count || site.controls[0].kind != control_kind::function) { return status::invalid_layout; }
        for(auto const& handler : site.handlers)
        {
            if(handler.target_control >= site.controls.size() || handler.target_offset >= plan.expression_bytes ||
               (handler.catch_all && (!handler.parameters.empty() || handler.tag_index != 0u))) { return status::invalid_layout; }
            for(auto const& type : handler.parameters) { if(!known_type(type)) { return status::invalid_layout; } }
        }
        if(site.phase == frame_phase::awaiting_call_return && site.caller_return_offset >= plan.expression_bytes)
        { return status::invalid_layout; }
        return status::ok;
    }
    [[nodiscard]] inline status validate_plan(function_plan const& plan) noexcept
    {
        if(plan.compiler_failure != status::ok) { return plan.compiler_failure; }
        if(!plan.profile || plan.function_generation == 0u || plan.expression_bytes == 0u ||
           (plan.producer_availability != status::ok && plan.producer_availability != status::quota_exceeded)) { return status::invalid_plan; }
        if(plan.sites.empty())
        { return plan.producer_availability == status::quota_exceeded && plan.resume_sites.empty() &&
                 plan.resume_abi_revision == 0u && plan.retirement_sites.empty() &&
                 plan.retirement_abi_revision == 0u ? status::ok : status::invalid_plan; }
        for(::std::size_t i{}; i != plan.sites.size(); ++i)
        {
            if(plan.sites[i].identifier != i + 1u) { return status::invalid_plan; }
            auto const result{validate_site(plan.sites[i], plan)}; if(result != status::ok) { return result; }
        }
        // An observation profile may never seal executable continuation
        // metadata, even if a caller manufactures otherwise valid DATA.
        if((plan.profile->purpose() == compilation_purpose::observe_values &&
            (!plan.resume_sites.empty() || plan.resume_abi_revision != 0u ||
             !plan.retirement_sites.empty() || plan.retirement_abi_revision != 0u)) ||
           (plan.resume_abi_revision != 0u && plan.resume_abi_revision != native_resume_abi_revision) || (plan.resume_sites.empty() != (plan.resume_abi_revision == 0u)))
        { return status::invalid_plan; }
        ::std::uint64_t previous{};
        for(::std::size_t i{}; i != plan.resume_sites.size(); ++i)
        {
            auto const id{plan.resume_sites[i]};
            if(id == 0u || id <= previous || id > plan.sites.size()) { return status::invalid_plan; }
            // [complete sealed site IDs ... id-1 ... N] end
            // [safe] full nonzero ordinal bound BEFORE metadata access.
            auto const& site{plan.sites[static_cast<::std::size_t>(id-1u)]};
            if(site.phase == frame_phase::awaiting_call_return)
            {
                if(id == UINT64_MAX || id >= plan.sites.size()) { return status::invalid_plan; }
                // [complete dense waiting site | paired after-call site] end
                // [safe] id<N BEFORE selecting its immediately following cell.
                // This is the same fused producer's normal-return edge. Static
                // lexical clauses stay identical; no pending native catch or
                // guest exception owner is fabricated by these DATA checks.
                auto const& after{plan.sites[static_cast<::std::size_t>(id)]};
                if(after.phase != frame_phase::before_opcode || after.identifier != id+1u ||
                   after.opcode_offset != site.caller_return_offset || after.local_count != site.local_count ||
                   after.operand_count < site.operand_count || after.saved_parameter_count != site.saved_parameter_count ||
                   after.controls != site.controls || after.handlers != site.handlers || after.slots.size() < site.slots.size())
                { return status::invalid_plan; }
                auto const results{after.operand_count-site.operand_count};
                auto const prefix{site.local_count+site.operand_count}; // validate_site bounded both against owned slots.
                if(after.slots.size()-site.slots.size() != results || prefix > after.slots.size() ||
                   results > after.slots.size()-prefix) { return status::invalid_plan; }
                for(::std::size_t n{}; n != prefix; ++n)
                {
                    // [bounded identical waiting/after local+operand prefix] end
                    // [safe] prefix<=both owned sizes BEFORE either selection.
                    if(site.slots[n] != after.slots[n]) { return status::invalid_plan; }
                }
                for(::std::size_t n{}; n != site.saved_parameter_count; ++n)
                {
                    // [waiting prefix | saved0..K] -> [after prefix | results | saved0..K]
                    // [safe] exact bounded count equations BEFORE either offset;
                    // after-prefix-results == K proves each complete suffix cell.
                    if(site.slots[prefix+n] != after.slots[prefix+results+n]) { return status::invalid_plan; }
                }
            }
            else if(site.phase != frame_phase::before_opcode) { return status::invalid_plan; }
            previous = id; // same fused walk installs ascending dense IDs
        }
        if(plan.retirement_abi_revision > 1u || (plan.retirement_sites.empty() != (plan.retirement_abi_revision == 0u)))
        { return status::invalid_plan; }
        previous = 0u;
        for(auto const id : plan.retirement_sites)
        {
            if(id == 0u || id <= previous || id > plan.sites.size()) { return status::invalid_plan; }
            // [complete sealed site IDs0 ... id-1 ... N] end
            // [safe] positive in-range ordinal proved before subtract/index.
            if(plan.sites[static_cast<::std::size_t>(id-1u)].phase != frame_phase::before_opcode)
            { return status::invalid_plan; }
            previous = id;
        }
        return status::ok;
    }
    // Independently owned and validated immutable metadata. Sealing this data
    // once avoids O(all function sites) work on each instrumented native entry.
    // It is still NOT the canonical runtime/code/world-stop authority.
    class sealed_function_plan
    {
        function_plan plan_{};
        explicit sealed_function_plan(function_plan&& plan) : plan_{::std::move(plan)} {}
    public:
        using owner = ::std::shared_ptr<sealed_function_plan const>;
        [[nodiscard]] static owner seal_compiler_metadata(function_plan plan)
        { if(validate_plan(plan) != status::ok) { return {}; } return owner{new sealed_function_plan{::std::move(plan)}}; }
        [[nodiscard]] function_plan const& get() const noexcept { return plan_; }
    };
}
