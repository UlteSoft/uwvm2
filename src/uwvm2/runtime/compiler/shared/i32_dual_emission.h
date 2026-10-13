/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <bit>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <span>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <llvm/IR/Verifier.h>
# include <llvm/Target/TargetMachine.h>
# include <uwvm2/validation/standard/wasm3/fused_i32_sink.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::i32_dual_emission
{
    namespace ring = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
    namespace native = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace typed = ::uwvm2::validation::standard::wasm3;
    using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using ring_options = ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option;
    using ring_symbol = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
    using feature_type = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

    // SOURCE compiler research slice. No runtime/tiered/CLI publication consumes
    // this artifact yet. The exact native target is caller-owned through emission;
    // no target/triple/CPU is guessed and no native entry is minted here.
    struct emission_policy
    {
        ::llvm::TargetMachine const* target{};
        ::std::size_t function_operations{65536uz};
        ::std::size_t function_locals{4096uz};
        ::std::size_t module_functions{4096uz};
        ::std::size_t module_operations{262144uz};
        // Optional SOURCE slice only. False preserves the initial i32 sink's
        // accepted signatures/opcodes; normal runtime compiler never selects it.
        bool emit_integer_scalar_memory{};
        // Independent numeric extension. False keeps its prior admitted slice;
        // selecting it does not imply all Core 3 or runtime READY support.
        bool emit_i64_numeric{};
        bool emit_integer_width{};
        bool emit_integer_compare{};
        bool emit_typed_select{};
    };
    enum class native_unavailability : unsigned
    {
        none, target_unavailable, unsupported_signature, unsupported_opcode,
        noncontiguous_first_events, operation_quota, local_quota,
        inconsistent_event, native_emission_declined
    };
    namespace details
    {
        // The callback receives only accepted DATA directly in the original INT
        // fused walk. It neither owns nor receives a raw instruction slice. LLVM
        // then consumes these exact events with its existing physical SSA helpers.
        // Any lexical skip/replay or other legal opcode only loses LLVM availability;
        // the authoritative ring walk continues validation and normal emission.
        class function_sink final
        {
            native::llvm_jit_module_storage_t module_{};
            native::local_func_storage_t metadata_{};
            native::details::runtime_local_func_llvm_jit_emit_state_t state_{};
            emission_policy policy_{};
            ::std::size_t expression_bytes_{}, next_offset_{}, operation_count_{}, numeric_count_{};
            ::std::size_t current_offset_{}, memory_count_{}, numeric64_count_{};
            unsigned current_opcode_{};
            native_unavailability reason_{};
            bool started_{}, active_{}, operation_pending_{}, terminal_seen_{}, completed_{};
            void decline(native_unavailability reason) noexcept
            {
                if(reason_ == native_unavailability::none) { reason_ = reason; }
                active_ = false; operation_pending_ = false;
                // State references LLVM-owned objects. Destroy it BEFORE its module
                // and context, including every failed validation/unsealed path.
                state_ = {};
                module_.discard_emission_preserving_first_decline();
            }
            bool accept_extent(unsigned opcode, ::std::size_t offset, ::std::size_t bytes,
                               ::std::size_t depth, bool polymorphic) noexcept
            {
                if(!active_) { return false; }
                if(!operation_pending_ || current_opcode_ != opcode || current_offset_ != offset ||
                   offset != next_offset_ || bytes == 0uz || offset >= expression_bytes_ ||
                   bytes > expression_bytes_ - offset || depth != 1uz || polymorphic || terminal_seen_)
                { decline(native_unavailability::inconsistent_event); return false; }
                // Numeric accounting only, bounded subtraction proved addition.
                // No pointer is formed and no event/immediate bytes are decoded.
                next_offset_ = offset + bytes;
                operation_pending_ = false;
                state_.current_wasm_op_offset = offset;
                return true;
            }
        public:
            static constexpr bool receives_fused_i32_operations{true};
            explicit function_sink(emission_policy policy) noexcept : policy_{policy} {}
            function_sink(function_sink const&) = delete;
            function_sink& operator=(function_sink const&) = delete;
            ~function_sink() = default;

            void begin(module_type const& module, ::std::size_t module_id, ::std::size_t local_index) noexcept
            {
                if(started_) { decline(native_unavailability::inconsistent_event); return; }
                started_ = true;
                if(local_index >= module.local_defined_function_vec_storage.size() ||
                   local_index > SIZE_MAX - module.imported_function_vec_storage.size())
                { decline(native_unavailability::inconsistent_event); return; }
                // [actual runtime-owned finalized local entries ...] local_end
                // [safe] index checked BEFORE borrowing its immutable type/code.
                auto const& actual{module.local_defined_function_vec_storage.index_unchecked(local_index)};
                if(actual.function_type_ptr == nullptr || actual.wasm_code_ptr == nullptr)
                { decline(native_unavailability::inconsistent_event); return; }
                auto const& type{*actual.function_type_ptr};
                auto const& code{*actual.wasm_code_ptr};
                // Source identity only, from the parser-owned same runtime entry.
                // [expr_begin ... expression bytes] | code_end (one-past)
                // The actual parser/runtime storage admission owns this one range;
                // this callback never reads bytes or advances either pointer.
                auto const begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
                auto const end{reinterpret_cast<::std::byte const*>(code.body.code_end)};
                if(begin == nullptr || end == nullptr || end <= begin)
                { decline(native_unavailability::inconsistent_event); return; }
                expression_bytes_ = static_cast<::std::size_t>(end - begin);
                if(policy_.target == nullptr || policy_.target->createDataLayout().isDefault() ||
                   policy_.target->createDataLayout().getPointerSizeInBits() != sizeof(::std::uintptr_t) * 8uz ||
                   policy_.target->createDataLayout().isLittleEndian() != (::std::endian::native == ::std::endian::little))
                { decline(native_unavailability::target_unavailable); return; }
                using value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;
                if((type.parameter.begin == nullptr && type.parameter.begin != type.parameter.end) ||
                   (type.result.begin == nullptr && type.result.begin != type.result.end))
                { decline(native_unavailability::inconsistent_event); return; }
                // [actual finalized tuple ...] tuple_end (one-past)
                // Each nonempty tuple belongs to one parser-owned allocation;
                // null empty ranges avoid null-pointer subtraction entirely.
                auto const parameter_count{type.parameter.begin == nullptr ? 0uz :
                    static_cast<::std::size_t>(type.parameter.end - type.parameter.begin)};
                auto const result_count{type.result.begin == nullptr ? 0uz :
                    static_cast<::std::size_t>(type.result.end - type.result.begin)};
                auto const parameters{::std::span<value_type const>{type.parameter.begin, parameter_count}};
                auto const results{::std::span<value_type const>{type.result.begin, result_count}};
                if(parameters.size() > policy_.function_locals)
                { decline(native_unavailability::local_quota); return; }
                for(auto value: parameters)
                { if(value != value_type::i32 && (!(policy_.emit_integer_scalar_memory || policy_.emit_i64_numeric || policy_.emit_integer_width || policy_.emit_integer_compare || policy_.emit_typed_select) || value != value_type::i64))
                  { decline(native_unavailability::unsupported_signature); return; } }
                for(auto value: results)
                { if(value != value_type::i32 && (!(policy_.emit_integer_scalar_memory || policy_.emit_i64_numeric || policy_.emit_integer_width || policy_.emit_integer_compare || policy_.emit_typed_select) || value != value_type::i64))
                  { decline(native_unavailability::unsupported_signature); return; } }
                if(results.size() > 1uz)
                { decline(native_unavailability::unsupported_signature); return; }
                auto locals{parameters.size()};
                for(auto const& run: code.locals)
                {
                    if(run.type != value_type::i32 && (!(policy_.emit_integer_scalar_memory || policy_.emit_i64_numeric || policy_.emit_integer_width || policy_.emit_integer_compare || policy_.emit_typed_select) || run.type != value_type::i64))
                    { decline(native_unavailability::unsupported_signature); return; }
                    if(run.count > policy_.function_locals - locals)
                    { decline(native_unavailability::local_quota); return; }
                    locals += run.count;
                }
                metadata_.function_type_ptr = actual.function_type_ptr;
                metadata_.wasm_code_ptr = actual.wasm_code_ptr;
                metadata_.code_begin = begin; metadata_.code_end = end;
                metadata_.module_id = module_id;
                metadata_.function_index = module.imported_function_vec_storage.size() + local_index;
                metadata_.runtime_module_ptr = &module; // same borrowed owner, not executable publication.
                if(!native::details::try_prepare_runtime_llvm_jit_module_storage(module, module_, false, policy_.target))
                { decline(native_unavailability::native_emission_declined); return; }
#if LLVM_VERSION_MAJOR >= 21
                module_.llvm_module->setTargetTriple(policy_.target->getTargetTriple());
#else
                module_.llvm_module->setTargetTriple(policy_.target->getTargetTriple().str());
#endif
                module_.llvm_module->setDataLayout(policy_.target->createDataLayout());
                // No debug/checkpoint/tiered/call-stack policy is silently inherited.
                // This selected compiler slice lowers straight-line integer functions
                // through the real default full typed ABI, with mandatory verification.
                active_ = native::details::try_prepare_runtime_local_func_llvm_jit_emit_state(
                    metadata_, module_, state_, true, false, 0u, 0uz, 0u, 0uz, false,
                    false, false, false, false, native::llvm_jit_compilation_mode::full);
                if(!active_) { decline(native_unavailability::native_emission_declined); }
            }
            void opcode(unsigned opcode, ::std::size_t offset) noexcept
            {
                if(!active_) { return; }
                if(operation_pending_ || terminal_seen_ || offset != next_offset_)
                { decline(native_unavailability::noncontiguous_first_events); return; }
                if(operation_count_ >= policy_.function_operations)
                { decline(native_unavailability::operation_quota); return; }
                ++operation_count_;
                bool const extended{((policy_.emit_integer_scalar_memory || policy_.emit_i64_numeric || policy_.emit_integer_width || policy_.emit_integer_compare || policy_.emit_typed_select) && opcode == 0x42u) ||
                    (policy_.emit_integer_scalar_memory && opcode >= 0x28u && opcode <= 0x3eu) ||
                    (policy_.emit_i64_numeric && typed::is_i64_numeric_event_opcode(opcode)) ||
                    (policy_.emit_integer_width && typed::is_integer_width_event_opcode(opcode)) ||
                    (policy_.emit_integer_compare && typed::is_integer_compare_event_opcode(opcode)) ||
                    (policy_.emit_typed_select && opcode == 0x1cu)};
                if(opcode != 0x41u && opcode != 0x20u && opcode != 0x0bu &&
                   !typed::is_i32_numeric_event_opcode(opcode) && !extended)
                { decline(native_unavailability::unsupported_opcode); return; }
                current_opcode_ = opcode; current_offset_ = offset;
                operation_pending_ = true;
            }
            void provider(typed::validated_i32_provider_event const& event) noexcept
            {
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                bool emitted{};
                if(event.opcode == 0x41u)
                {
                    auto const bits{event.value};
                    emitted = native::details::try_emit_runtime_local_func_llvm_jit_constant(
                        state_, native::details::runtime_operand_stack_value_type::i32,
                        [bits](::llvm::LLVMContext& context) noexcept -> ::llvm::Value*
                        { return ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(context), bits, false); });
                }
                else if(event.opcode == 0x20u)
                { emitted = native::details::try_emit_runtime_local_func_llvm_jit_local_get(state_, event.value); }
                if(!emitted) { decline(native_unavailability::native_emission_declined); }
            }
            void provider64(typed::validated_i64_provider_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_integer_scalar_memory && !policy_.emit_i64_numeric && !policy_.emit_integer_width && !policy_.emit_integer_compare && !policy_.emit_typed_select)
                { decline(native_unavailability::unsupported_opcode); return; }
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                bool emitted{};
                if(event.opcode == 0x42u)
                {
                    auto const bits{event.value};
                    emitted = native::details::try_emit_runtime_local_func_llvm_jit_constant(
                        state_, native::details::runtime_operand_stack_value_type::i64,
                        [bits](::llvm::LLVMContext& context) noexcept -> ::llvm::Value*
                        { return ::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(context), bits, false); });
                }
                else if(event.opcode == 0x20u && event.value <= 0xffff'ffffull)
                { emitted = native::details::try_emit_runtime_local_func_llvm_jit_local_get(
                    state_, static_cast<::std::uint_least32_t>(event.value)); }
                if(!emitted) { decline(native_unavailability::native_emission_declined); }
            }
            void scalar_memory(typed::validated_scalar_memory_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_integer_scalar_memory ||
                   (event.value_kind != typed::scalar_memory_value_kind::i32 &&
                    event.value_kind != typed::scalar_memory_value_kind::i64))
                { decline(native_unavailability::unsupported_opcode); return; }
                // The exact first typed producer already checked declaration,
                // address width, alignment and whole operand arity. This is a
                // synchronous physical SSA sink, never a second body decoder.
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, !event.reachable)) { return; }
                ++memory_count_;
                if(!native::details::try_emit_runtime_local_func_llvm_jit_scalar_memory(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void numeric(typed::validated_i32_numeric_event const& event) noexcept
            {
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                ++numeric_count_;
                // Same already checked numeric transition, no abstract type stack
                // or raw immediate matcher in this second physical lowering sink.
                if(!native::details::try_emit_runtime_local_func_llvm_jit_i32_numeric(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void numeric64(typed::validated_i64_numeric_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_i64_numeric)
                { decline(native_unavailability::unsupported_opcode); return; }
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                ++numeric64_count_;
                // This accepted SAME typed event is the only physical sink input.
                // No second byte decode, abstract type check or memory guard.
                if(!native::details::try_emit_runtime_local_func_llvm_jit_i64_numeric(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void integer_width(typed::validated_integer_width_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_integer_width)
                { decline(native_unavailability::unsupported_opcode); return; }
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                // SAME typed event enters only the real physical SSA emitter.
                // The sink does not pop/match abstract types or reread a body.
                if(!native::details::try_emit_runtime_local_func_llvm_jit_integer_width(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void integer_compare(typed::validated_integer_compare_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_integer_compare)
                { decline(native_unavailability::unsupported_opcode); return; }
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                // Actual first-typed suffix was consumed only by the INT walker.
                // This physical sink never matches abstract types or reads raw bytes.
                if(!native::details::try_emit_runtime_local_func_llvm_jit_integer_compare(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void typed_select(typed::validated_typed_select_event const& event) noexcept
            {
                if(!active_) { return; }
                if(!policy_.emit_typed_select || (event.result_type.kind !=
                    ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i32 &&
                    event.result_type.kind != ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64))
                { decline(native_unavailability::unsupported_opcode); return; }
                if(!accept_extent(event.opcode, event.source_offset, event.source_bytes,
                    event.control_depth, event.stack_polymorphic)) { return; }
                // Same accepted typed event enters physical SSA, never a second
                // abstract type transition/raw valtype count or body decoder.
                if(!native::details::try_emit_runtime_local_func_llvm_jit_typed_select(state_, event))
                { decline(native_unavailability::native_emission_declined); }
            }
            void complete(::std::size_t expression_bytes) noexcept
            {
                // Only the original ring function's successful terminal/fixup path
                // invokes this. Completion can be valid with native unavailable.
                completed_ = true;
                if(!active_) { return; }
                if(expression_bytes != expression_bytes_ || current_opcode_ != 0x0bu ||
                   !accept_extent(0x0bu, current_offset_, 1uz, 1uz, false) || next_offset_ != expression_bytes_)
                { decline(native_unavailability::inconsistent_event); return; }
                terminal_seen_ = true;
                if(!native::details::try_emit_runtime_local_func_llvm_jit_end(state_) ||
                   !native::details::finalize_runtime_local_func_llvm_jit_emit_state(state_, module_) ||
                   !native::details::finalize_runtime_llvm_jit_module_storage(module_, true))
                { decline(native_unavailability::native_emission_declined); return; }
                // Retire descriptor/build-state borrows before moving their module.
                state_ = {};
            }
            void abort_unsealed() noexcept
            {
                completed_ = false;
                decline(native_unavailability::inconsistent_event);
            }
            [[nodiscard]] bool validated_complete() const noexcept { return completed_; }
            [[nodiscard]] native_unavailability reason() const noexcept { return reason_; }
            [[nodiscard]] ::std::size_t operation_count() const noexcept { return operation_count_; }
            [[nodiscard]] ::std::size_t numeric_count() const noexcept { return numeric_count_; }
            [[nodiscard]] ::std::size_t memory_count() const noexcept { return memory_count_; }
            [[nodiscard]] ::std::size_t numeric64_count() const noexcept { return numeric64_count_; }
            [[nodiscard]] native::llvm_jit_module_storage_t take_ir() noexcept
            {
                if(!completed_ || !active_ || !terminal_seen_ || !module_.emitted) { return {}; }
                active_ = false;
                return ::std::move(module_);
            }
        };
    }

    struct function_diagnostic
    {
        native_unavailability reason{};
        ::std::size_t operations{}, i32_numeric_transitions{}, integer_memory_transitions{}, i64_numeric_transitions{};
    };
    template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t Option>
    class checked_module;
    template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t Option>
    [[nodiscard]] checked_module<Option> compile(module_type const&, ring_options,
        emission_policy, ::uwvm2::validation::error::code_validation_error_impl&, feature_type const* = nullptr) UWVM_THROWS;

    // Exact-NTTP compiler artifact. The factory alone seals it after EVERY body
    // completed the original fused walker and all pointer fixups. No native engine,
    // code publication, lazy READY, module-source identity credential or execution
    // domain is constructed. Runtime module/source must outlive normal code use.
    template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t Option>
    class checked_module final
    {
        template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t Actual>
        friend checked_module<Actual> compile(module_type const&, ring_options, emission_policy,
            ::uwvm2::validation::error::code_validation_error_impl&, feature_type const*) UWVM_THROWS;
        ring_symbol ring_{};
        ::uwvm2::utils::container::vector<native::llvm_jit_module_storage_t> fragments_{};
        ::uwvm2::utils::container::vector<function_diagnostic> diagnostics_{};
        bool sealed_{};
        checked_module() = default;
    public:
        checked_module(checked_module const&) = delete;
        checked_module& operator=(checked_module const&) = delete;
        checked_module(checked_module&& other) noexcept : ring_{::std::move(other.ring_)},
            fragments_{::std::move(other.fragments_)}, diagnostics_{::std::move(other.diagnostics_)},
            sealed_{::std::exchange(other.sealed_, false)} {}
        checked_module& operator=(checked_module&& other) noexcept
        {
            if(this != &other)
            {
                ring_ = ::std::move(other.ring_);
                fragments_ = ::std::move(other.fragments_);
                diagnostics_ = ::std::move(other.diagnostics_);
                sealed_ = ::std::exchange(other.sealed_, false);
            }
            return *this;
        }
        [[nodiscard]] bool typed_admission_complete() const noexcept { return sealed_; }
        [[nodiscard]] ring_symbol const& ring_artifact() const noexcept { return ring_; }
        [[nodiscard]] ::std::span<function_diagnostic const> diagnostics() const noexcept
        { return {diagnostics_.data(), diagnostics_.size()}; }
        [[nodiscard]] bool native_ir_available(::std::size_t local_index) const noexcept
        {
            return sealed_ && local_index < fragments_.size() &&
                fragments_.index_unchecked(local_index).emitted &&
                fragments_.index_unchecked(local_index).llvm_context_holder != nullptr &&
                fragments_.index_unchecked(local_index).llvm_module != nullptr;
        }
        [[nodiscard]] native::llvm_jit_module_storage_t take_checked_ir(::std::size_t local_index) noexcept
        {
            if(!native_ir_available(local_index)) { return {}; }
            return ::std::move(fragments_.index_unchecked(local_index));
        }
    };
    template<::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t Option>
    [[nodiscard]] inline checked_module<Option> compile(module_type const& module, ring_options options,
        emission_policy policy, ::uwvm2::validation::error::code_validation_error_impl& error,
        feature_type const* features) UWVM_THROWS
    {
        checked_module<Option> owned{};
        // Module declaration admission is required even with zero local bodies.
        // This metadata check is not a pure code validator or a first body walk.
        ring::details::validate_runtime_module_storage(module);
        feature_type const defaults{};
        ring::details::require_runtime_module_declaration_policy(module, features == nullptr ? defaults : *features, error);
        ring::details::initialize_local_defined_call_info(module, options, owned.ring_);
        auto const count{module.local_defined_function_vec_storage.size()};
        auto const retain_module{count <= policy.module_functions};
        if(retain_module) { owned.fragments_.resize(count); owned.diagnostics_.resize(count); }
        auto remaining_operations{policy.module_operations};
        for(::std::size_t local{}; local != count; ++local)
        {
            auto function_policy{policy};
            if(function_policy.function_operations > remaining_operations)
            { function_policy.function_operations = remaining_operations; }
            if(!retain_module) { function_policy.target = nullptr; }
            details::function_sink sink{function_policy};
            ring::details::compile_all_from_uwvm_local_func<Option, details::function_sink>(
                module, options, owned.ring_, local, features, error, &sink);
            if(!sink.validated_complete()) { ::fast_io::fast_terminate(); }
            // [presized native/diagnostic owners ...] owner_end
            // [safe] local<count proven before both owned indexed stores.
            if(retain_module)
            {
                owned.diagnostics_.index_unchecked(local) = {
                    sink.reason(), sink.operation_count(), sink.numeric_count(), sink.memory_count(), sink.numeric64_count()};
                auto fragment{sink.take_ir()};
                if(fragment.emitted)
                {
                    if(sink.operation_count() > remaining_operations) { ::fast_io::fast_terminate(); }
                    remaining_operations -= sink.operation_count();
                }
                owned.fragments_.index_unchecked(local) = ::std::move(fragment);
            }
        }
        ring::details::aggregate_local_function_storage(owned.ring_);
        // Call-bridge ABI metadata uses the same finalized type/layout helper.
        // Do NOT call the old raw-body trivial matcher: that would add a body
        // replay after this one typed walk. This initial optional compiler slice
        // deliberately leaves trivial_kind=none (normal generic ring calls).
        // Ordinary full/lazy/tiered paths remain byte-for-byte unselected.
        for(::std::size_t local{}; local != count; ++local)
        {
            // [actual finalized local/ring/call-info owners ...] owner_end
            // [safe] local<count BEFORE each owned indexed borrow.
            auto const& runtime{module.local_defined_function_vec_storage.index_unchecked(local)};
            if(runtime.function_type_ptr == nullptr) { ::fast_io::fast_terminate(); }
            auto const layout{native::details::get_runtime_wasm_call_abi_layout(*runtime.function_type_ptr)};
            if(!layout.valid) { ::fast_io::fast_terminate(); }
            auto& info{owned.ring_.local_defined_call_info.index_unchecked(local)};
            info.runtime_func = &runtime;
            info.compiled_func = &owned.ring_.local_funcs.index_unchecked(local);
            info.param_bytes = layout.parameter_bytes; info.result_bytes = layout.result_bytes;
        }
        owned.sealed_ = true;
        return owned;
    }
}
#endif

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
