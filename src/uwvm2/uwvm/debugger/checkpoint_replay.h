/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "checkpoint_state.h"
# include <algorithm>
# include <memory>
# include <span>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint
{
    struct checkpoint_index
    {
        ::std::uint64_t identifier{}, parent{}, instruction{}, event_cursor{};
        friend bool operator==(checkpoint_index const&, checkpoint_index const&) = default;
    };
    struct reverse_plan
    {
        error status{error::unavailable_capability};
        checkpoint_index origin{};
        ::std::uint64_t target_instruction{};
    };
    // Planning is not execution or restore authorization. Reverse execution is
    // restore(the nearest ancestor checkpoint) followed by deterministic forward
    // replay to an instruction boundary, as in rr/QEMU. Native PCs and debugger
    // observation-only trace entries cannot satisfy this contract.
    [[nodiscard]] inline reverse_plan plan_reverse(::std::span<checkpoint_index const> catalog,
        ::std::uint64_t current_checkpoint, ::std::uint64_t current_instruction, ::std::uint64_t target_instruction) noexcept
    {
        if(catalog.empty() || target_instruction >= current_instruction) { return {}; }
        ::std::uint64_t previous{};
        for(auto const& item : catalog)
        {
            if(item.identifier <= previous || item.parent >= item.identifier) { return {error::malformed}; }
            previous = item.identifier;
        }
        auto lookup = [&](::std::uint64_t id) -> checkpoint_index const*
        {
            auto const position{::std::lower_bound(catalog.begin(), catalog.end(), id,
                [](checkpoint_index const& item, ::std::uint64_t key) { return item.identifier < key; })};
            if(position == catalog.end() || position->identifier != id) { return nullptr; }
            // Same live catalog owner; checked iterator borrow is local only.
            return ::std::addressof(*position);
        };
        auto const* current{lookup(current_checkpoint)};
        if(current == nullptr || current->instruction > current_instruction) { return {error::malformed}; }
        for(::std::size_t visited{}; current != nullptr && visited != catalog.size(); ++visited)
        {
            if(current->instruction <= target_instruction) { return {error::none, *current, target_instruction}; }
            if(current->parent == 0u) { return {}; }
            auto const* parent{lookup(current->parent)};
            if(parent == nullptr || parent->instruction > current->instruction || parent->event_cursor > current->event_cursor)
            { return {error::malformed}; }
            current = parent; // checked same-catalog ancestry borrow, no wire pointer.
        }
        return {error::malformed};
    }
    struct replay_request
    {
        ::std::uint64_t sequence{}, instruction{}, logical_thread{}, adapter_operation{};
        ::std::uint16_t event_kind{};
        ::std::span<object_id const> capability_ids{};
        ::std::span<value const> arguments{};
    };
    struct replay_result
    {
        error status{error::replay_diverged};
        ::std::span<value const> results{};
        ::std::span<::std::byte const> adapter_payload{};
        ::std::uint64_t error_bits{}, effect_ordinal{};
    };
    // Owns copied immutable event data. It grants no host capability, never
    // invokes an import and never writes an external resource. A runtime import
    // adapter must validate and apply the returned payload to its detached
    // virtual resource/guest memory under the real world-stop/replay owner.
    class replay_log
    {
        ::std::vector<object> events_{};
        ::std::size_t cursor_{};
    public:
        [[nodiscard]] error bind(state const& snapshot, limits const& cap = {})
        {
            if(auto const status{validate_graph(snapshot, cap)}; status != error::none) { return status; }
            for(auto const& item : snapshot.objects)
            { if(item.kind == object_kind::host_resource && item.flags == 3u) { return error::non_replayable_import; } }
            ::std::vector<object> candidate{};
            for(auto const& item : snapshot.objects) { if(item.kind == object_kind::event) { candidate.push_back(item); } }
            if(snapshot.replay_event_cursor > candidate.size()) { return error::invalid_shape; }
            events_ = ::std::move(candidate); cursor_ = static_cast<::std::size_t>(snapshot.replay_event_cursor);
            return error::none;
        }
        [[nodiscard]] ::std::size_t cursor() const noexcept { return cursor_; }
        [[nodiscard]] bool exhausted() const noexcept { return cursor_ == events_.size(); }
        [[nodiscard]] replay_result consume(replay_request const& request) noexcept
        {
            if(cursor_ >= events_.size()) { return {}; }
            auto const& item{events_[cursor_]};
            if(item.words[0] != request.sequence || item.words[1] != request.instruction || item.words[2] != request.logical_thread ||
               item.words[3] != request.adapter_operation || item.flags != request.event_kind ||
               item.links.size() != request.capability_ids.size() || item.words[4] != request.arguments.size()) { return {}; }
            for(::std::size_t i{}; i != item.links.size(); ++i) { if(item.links[i] != request.capability_ids[i]) { return {}; } }
            for(::std::size_t i{}; i != request.arguments.size(); ++i) { if(item.values[i] != request.arguments[i]) { return {}; } }
            auto const arguments{static_cast<::std::size_t>(item.words[4])};
            // [owned exact argument values][owned exact result values] end
            // [safe                                                  ] unsafe (one-past)
            //                                ^^ subspan start: validated counts
            // ensure arguments<=size BEFORE this synchronous borrow. No native
            // addresses exist in the returned typed file-local values.
            ::std::span<value const> values{item.values};
            ++cursor_; // integer event cursor advances only after exact match.
            return {error::none, values.subspan(arguments), item.bytes, item.words[6], item.words[7]};
        }
    };
}
