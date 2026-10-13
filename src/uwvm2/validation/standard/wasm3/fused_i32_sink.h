/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include "i32_numeric_event.h"
# include "i64_numeric_event.h"
# include "integer_width_event.h"
# include "integer_compare_event.h"
# include "table_access_event.h"
# include "typed_select_event.h"
# include "scalar_memory_event.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Compiler-only synchronous operation channel, NOT a validation certificate,
    // native publication, source seal, saved-state credential, or guest authority.
    // Only the original fused walk supplies accepted operation DATA. A selected
    // sink cannot cause a second body read: neither this channel nor its events
    // carry a Wasm source pointer. Default instantiation has no runtime members.
    struct discard_fused_i32_operations
    { static constexpr bool receives_fused_i32_operations{false}; };

    struct validated_i32_provider_event
    {
        unsigned opcode{}; // i32.const 0x41 or local.get 0x20
        ::std::uint_least32_t value{}; // original unsigned bits / checked local index
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
        bool stack_polymorphic{};
    };

    // The original bounded i64.const/local.get issuer supplies this only AFTER
    // its accepted typed push. Native sinks receive unsigned bits / local index,
    // not any raw body pointer or abstract validation credential.
    struct validated_i64_provider_event
    {
        unsigned opcode{}; // i64.const 0x42 or local.get 0x20
        ::std::uint_least64_t value{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
        bool stack_polymorphic{};
    };

    template<typename Sink, bool = Sink::receives_fused_i32_operations>
    class fused_i32_function_transaction;

    template<typename Sink>
    class fused_i32_function_transaction<Sink, false>
    {
    public:
        explicit constexpr fused_i32_function_transaction(Sink*) noexcept {}
    };

    template<typename Sink>
    class fused_i32_function_transaction<Sink, true>
    {
        Sink* sink_{};
        bool completed_{};
    public:
        // This nonowning compiler borrow lives through the lexical fused call.
        // The caller owns the sink before the transaction and destroys it after.
        explicit constexpr fused_i32_function_transaction(Sink* sink) noexcept : sink_{sink} {}
        fused_i32_function_transaction(fused_i32_function_transaction const&) = delete;
        fused_i32_function_transaction& operator=(fused_i32_function_transaction const&) = delete;
        constexpr ~fused_i32_function_transaction()
        { if(sink_ != nullptr && !completed_) { sink_->abort_unsealed(); } }

        template<typename Module>
        constexpr void begin(Module const& module, ::std::size_t module_id, ::std::size_t local_index)
        { if(sink_ != nullptr) { sink_->begin(module, module_id, local_index); } }
        constexpr void opcode(unsigned opcode, ::std::size_t offset)
        { if(sink_ != nullptr) { sink_->opcode(opcode, offset); } }
        constexpr void provider(validated_i32_provider_event const& event)
        { if(sink_ != nullptr) { sink_->provider(event); } }
        constexpr void provider64(validated_i64_provider_event const& event)
        {
            // Existing selected i32-only clients remain source-compatible. A
            // skipped extension still cannot seal their physical native slice:
            // the following opcode/terminal detects its outstanding event.
            if constexpr(requires(Sink& actual) { actual.provider64(event); })
            { if(sink_ != nullptr) { sink_->provider64(event); } }
        }
        constexpr void scalar_memory(validated_scalar_memory_event const& event)
        {
            if constexpr(requires(Sink& actual) { actual.scalar_memory(event); })
            { if(sink_ != nullptr) { sink_->scalar_memory(event); } }
        }
        constexpr void numeric(validated_i32_numeric_event const& event)
        { if(sink_ != nullptr) { sink_->numeric(event); } }
        constexpr void numeric64(validated_i64_numeric_event const& event)
        {
            // The selected extension consumes only the same accepted transition.
            // Skipping a callback cannot seal a pending physical LLVM operation.
            if constexpr(requires(Sink& actual) { actual.numeric64(event); })
            { if(sink_ != nullptr) { sink_->numeric64(event); } }
        }
        constexpr void integer_width(validated_integer_width_event const& event)
        {
            if constexpr(requires(Sink& actual) { actual.integer_width(event); })
            { if(sink_ != nullptr) { sink_->integer_width(event); } }
        }
        constexpr void integer_compare(validated_integer_compare_event const& event)
        {
            if constexpr(requires(Sink& actual) { actual.integer_compare(event); })
            { if(sink_ != nullptr) { sink_->integer_compare(event); } }
        }
        constexpr void table_access(validated_table_access_event const& event)
        {
            if constexpr(requires(Sink& actual) { actual.table_access(event); })
            { if(sink_ != nullptr) { sink_->table_access(event); } }
        }
        constexpr void typed_select(validated_typed_select_event const& event)
        {
            if constexpr(requires(Sink& actual) { actual.typed_select(event); })
            { if(sink_ != nullptr) { sink_->typed_select(event); } }
        }
        constexpr void complete(::std::size_t expression_bytes)
        {
            // Called AFTER the original terminal end/type checks and pointer
            // fixups succeeded and the actual ring artifact was moved into its
            // compiler owner. Sink availability is distinct from Wasm validity.
            if(sink_ != nullptr) { sink_->complete(expression_bytes); }
            completed_ = true;
        }
    };
    // A speculative scanner owns only DATA it decoded during its ORIGINAL pass.
    // This is not a body parser, module certificate or native permission. Nothing
    // is published until the original complete window and physical ring commit.
    // The default discarded channel has no buffers, allocations or callbacks.
    template<bool Selected>
    class committed_integer_add_batch {};

    template<>
    class committed_integer_add_batch<true> final
    {
        struct provider_record
        { ::std::uint_least32_t index{}; ::std::size_t offset{}, bytes{}; };
        provider_record providers_[8u]{};
        validated_i32_numeric_event i32_adds_[7u]{};
        validated_i64_numeric_event i64_adds_[7u]{};
        ::std::size_t provider_count_{}, add_count_{}, value_count_{}, next_offset_{}, depth_{};
        bool wide_{}, failed_{};
        struct owned_operand
        {
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
            bool unknown{};
        };
    public:
        constexpr void begin(bool wide, ::std::uint_least32_t index,
            ::std::size_t offset, ::std::size_t bytes, ::std::size_t depth) noexcept
        {
            wide_ = wide; depth_ = depth; next_offset_ = offset;
            record_provider(index, offset, bytes);
        }
        constexpr void record_provider(::std::uint_least32_t index,
            ::std::size_t offset, ::std::size_t bytes) noexcept
        {
            if(failed_) { return; }
            if(provider_count_ >= 8u || add_count_ != 0u || offset != next_offset_ || bytes == 0u ||
               bytes > (::std::numeric_limits<::std::size_t>::max)() - offset)
            { failed_ = true; return; }
            // provider_count < 8 proves the destination owned-record slot BEFORE
            // its indexed write. The source pointer is never retained or moved.
            providers_[provider_count_] = {index, offset, bytes};
            ++provider_count_; ++value_count_; next_offset_ = offset + bytes;
        }
        constexpr void record_add(::std::size_t offset) noexcept
        {
            if(failed_) { return; }
            if(provider_count_ != 8u || add_count_ >= 7u || offset != next_offset_ ||
               offset == (::std::numeric_limits<::std::size_t>::max)())
            { failed_ = true; return; }
            namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
            auto const count{[&]() constexpr noexcept { return value_count_; }};
            auto const consume{[&]() constexpr noexcept
            {
                // The common first-arity kernel proves value_count > 0 BEFORE
                // consuming one concrete typed provider in this owned suffix.
                --value_count_;
                return owned_operand{{wide_ ? type::value_kind::i64 : type::value_kind::i32}, false};
            }};
            auto const push{[&](type::core_value_type) constexpr noexcept { ++value_count_; }};
            // The original scanner has just accepted this add byte. These are
            // its FIRST numeric type transitions, from the same shared kernel;
            // no raw replay or access to operands below these eight gets occurs.
            typed_stack_sequence_result result{};
            if(wide_)
            { result = transition_i64_numeric_event<0x7cu>(i64_adds_[add_count_],
                false, count, consume, push, offset, depth_); }
            else
            { result = transition_i32_numeric_event<0x6au>(i32_adds_[add_count_],
                false, count, consume, push, offset, depth_); }
            if(result.error != typed_stack_error::ok) { failed_ = true; return; }
            ++add_count_; next_offset_ = offset + 1u;
        }
        [[nodiscard]] constexpr bool complete() const noexcept
        { return !failed_ && provider_count_ == 8u && add_count_ == 7u && value_count_ == 1u; }

        [[nodiscard]] constexpr bool complete_providers(::std::size_t count) const noexcept
        {
            // The original preload scanner accepted this exact count (2..8).
            // No trailing numeric event belongs to this shape. Prefix operands
            // remain in the original frame; only newly checked gets are stored.
            return !failed_ && count >= 2u && count <= 8u && provider_count_ == count &&
                add_count_ == 0u && value_count_ == count;
        }
        template<typename Transaction>
        constexpr void publish_providers_after_ring_commit(Transaction& transaction, ::std::size_t count) const
        {
            if(!complete_providers(count)) { return; }
            for(::std::size_t i{}; i != count; ++i)
            {
                // complete_providers proves count <= eight owned slots BEFORE
                // indexed reads. There are no Wasm cursors or validation pops.
                auto const& provider{providers_[i]};
                if(i != 0u) { transaction.opcode(0x20u, provider.offset); }
                if(wide_)
                { transaction.provider64({.opcode = 0x20u, .value = provider.index,
                    .source_offset = provider.offset, .source_bytes = provider.bytes,
                    .control_depth = depth_, .stack_polymorphic = false}); }
                else
                { transaction.provider({.opcode = 0x20u, .value = provider.index,
                    .source_offset = provider.offset, .source_bytes = provider.bytes,
                    .control_depth = depth_, .stack_polymorphic = false}); }
            }
        }

        template<typename Transaction>
        constexpr void publish_after_ring_commit(Transaction& transaction) const
        {
            if(!complete()) { return; }
            // The original dispatcher already announced the first local.get.
            // Every later opcode uses exactly the owned offset from the first
            // scanner. This loop reads DATA only, never a source slice/type stack.
            for(::std::size_t i{}; i != 8u; ++i)
            {
                auto const& provider{providers_[i]}; // fixed complete eight slots
                if(i != 0u) { transaction.opcode(0x20u, provider.offset); }
                if(wide_)
                { transaction.provider64({.opcode = 0x20u, .value = provider.index,
                    .source_offset = provider.offset, .source_bytes = provider.bytes,
                    .control_depth = depth_, .stack_polymorphic = false}); }
                else
                { transaction.provider({.opcode = 0x20u, .value = provider.index,
                    .source_offset = provider.offset, .source_bytes = provider.bytes,
                    .control_depth = depth_, .stack_polymorphic = false}); }
            }
            for(::std::size_t i{}; i != 7u; ++i)
            {
                if(wide_)
                { auto const& event{i64_adds_[i]}; transaction.opcode(event.opcode, event.source_offset);
                    transaction.numeric64(event); }
                else
                { auto const& event{i32_adds_[i]}; transaction.opcode(event.opcode, event.source_offset);
                    transaction.numeric(event); }
            }
        }
    };

}
