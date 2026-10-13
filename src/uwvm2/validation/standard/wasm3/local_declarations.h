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
# include <span>
# include <utility>
# include "recursive_type_binary.h"
# include "recursive_type_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    [[nodiscard]] inline constexpr bool core3_value_is_defaultable(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type) noexcept
    {
        using kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        return type.kind <= kind::v128 || (type.kind == kind::reference && type.nullable);
    }
    struct core3_local_run
    {
        ::std::uint_least32_t count{};
        ::std::uint_least64_t first_index{};
        ::std::size_t binary_offset{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
    };
    struct core3_local_declarations
    {
        // Keep binary runs compressed. A tiny declaration of billions of locals must not allocate billions
        // of validator entries. Storage/frame resource limits are applied when a runtime layout is requested.
        ::uwvm2::utils::container::vector<core3_local_run> runs{};
        ::std::uint_least64_t parameter_count{}, total_count{};
        [[nodiscard]] inline constexpr bool find(::std::uint_least64_t index,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type& result) const noexcept
        {
            if(index < parameter_count || index >= total_count) { return false; }
            ::std::size_t first{}, last{runs.size()};
            while(first != last)
            {
                auto const middle{first + (last - first) / 2uz};
                // [first, last) lies inside runs; middle is a live element, never a one-past access.
                auto const& run{runs.index_unchecked(middle)};
                if(index < run.first_index) { last = middle; }
                else if(index - run.first_index >= run.count) { first = middle + 1uz; }
                else { result = run.type; return true; }
            }
            return false;
        }
    };
    enum class core3_local_error : unsigned { ok, binary, too_many_locals, unknown_type };
    struct core3_local_result
    {
        core3_local_error error{};
        ::std::size_t binary_offset{};
        recursive_type_binary_error binary_error{};
    };
    // Decode just the local declaration prefix, leaving the first instruction (possibly body_end) unread.
    // Function parameters are already validated by the type section; parameter_count reserves their indices.
    [[nodiscard]] inline constexpr core3_local_result scan_core3_local_declarations(
        ::std::byte const*& cursor, ::std::byte const* body_end, ::std::uint_least64_t parameter_count,
        recursive_type_context const& context, core3_local_declarations& output) noexcept
    {
        constexpr ::std::uint_least64_t maximum{0xffff'ffffull};
        if(parameter_count > maximum) { return {core3_local_error::too_many_locals, 0}; }
        // [cursor ... body_end) is the caller-proven function-body allocation. Equal endpoints may be null;
        // do not subtract null pointers, and do not dereference the end while borrowing the byte range.
        recursive_binary_details::reader input{{cursor, cursor == body_end ? 0uz : static_cast<::std::size_t>(body_end - cursor)}};
        core3_local_declarations result{};
        result.parameter_count = result.total_count = parameter_count;
        core3_local_result semantic{};
        if(!input.list(result.runs, 2uz, [&](core3_local_run& run) constexpr noexcept
        {
            run.binary_offset = input.position;
            run.first_index = result.total_count;
            if(!input.u32(run.count) || !input.value(run.type)) { return false; }
            // Type uses must exist even in a zero-count declaration; do not erase the encoded type first.
            if(run.type.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference && run.type.heap.is_defined() &&
               !context.contains(static_cast<::std::uint_least64_t>(run.type.heap.code)))
            { semantic = {core3_local_error::unknown_type, run.binary_offset}; return false; }
            if(run.count > maximum - result.total_count)
            { semantic = {core3_local_error::too_many_locals, run.binary_offset}; return false; }
            result.total_count += run.count;
            return true;
        }))
        {
            if(semantic.error != core3_local_error::ok) { return semantic; }
            return {core3_local_error::binary, input.status.error_offset, input.status.error};
        }
        output = ::std::move(result);
        // [complete locals prefix][first instruction ... body_end)
        // [safe                  ] unsafe (the first instruction may equal body_end)
        // ^^ cursor is the caller-proven body begin; input.position is bounded by that same body.
        // A successful list consumes its count byte, so cursor is non-null before this addition.
        cursor += input.position;
        // [complete locals prefix][first instruction ... body_end)
        // [safe                  ] unsafe (could be body_end)
        //                         ^^ cursor is first unconsumed byte; opcode reads must check it.
        return {};
    }
    class core3_local_initialization
    {
        // Only locals without a default value need tracking. Sparse state is bounded by actual local.set/tee
        // operations, not by a hostile declared local count. The ordered set avoids chosen integer hash collisions.
        ::uwvm2::utils::container::set<::std::uint_least32_t> assigned_{};
        ::uwvm2::utils::container::vector<::std::uint_least32_t> undo_{};
    public:
        [[nodiscard]] inline ::std::size_t checkpoint() const noexcept { return undo_.size(); }
        [[nodiscard]] inline bool is_initialized(::std::uint_least32_t index, bool initially_initialized) const noexcept
        { return initially_initialized || assigned_.contains(index); }
        inline void initialize(::std::uint_least32_t index, bool initially_initialized) noexcept
        {
            if(initially_initialized) { return; }
            auto const [position, inserted]{assigned_.insert(index)};
            if(inserted) { undo_.push_back(index); }
        }
        // Save a checkpoint on entry to each block/loop/if/try_table. Restore it at else and end.
        // Core 3 structured instructions do not export their inner local initialization sets, even when both
        // if branches assign a local. A get in unreachable code still requires initialization.
        [[nodiscard]] inline bool restore(::std::size_t checkpoint) noexcept
        {
            if(checkpoint > undo_.size()) { return false; }
            while(undo_.size() != checkpoint)
            {
                // The size check proves a live undo entry; pop changes only owned container cursors.
                auto const index{undo_.back_unchecked()};
                assigned_.erase(index);
                undo_.pop_back_unchecked();
            }
            return true;
        }
    };
}
