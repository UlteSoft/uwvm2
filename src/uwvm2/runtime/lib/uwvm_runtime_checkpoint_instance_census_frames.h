// Include ONLY inside runtime_checkpoint_gc_state_borrow after the actual
// resource/reference methods. The sole manager already proved every capture in
// this lexical ONE cohort, genuine N roots and current publication/typed plan.
// These methods construct portable DATA, never executable restoration rights.
#pragma once
[[nodiscard]] bool census_copy_frames(complete_census_work& state) const
{
    namespace native = ::uwvm2::runtime::checkpoint;
    if(!current_scope() || state.actual.modules.empty() || cohort_.size() != captures_.size() ||
        cohort_.size() > state.cap.max_threads || cohort_.size() >= (::std::numeric_limits<::std::uint64_t>::max)())
    { return state.fail(census_error::unavailable_capability); }
    state.snapshot.next_logical_thread = static_cast<::std::uint64_t>(cohort_.size()) + 1u;
    ::std::uint64_t frame_count{};
    for(::std::size_t ordinal{}; ordinal != cohort_.size(); ++ordinal)
    {
        capture::owner actual{};
        for(auto const& candidate : captures_)
        { if(candidate && candidate->participant_ == cohort_[ordinal].id) { actual = candidate; break; } }
        if(!actual || actual->location_ != cohort_[ordinal].location || actual->frames_.empty() ||
            frame_count > state.cap.max_frames || actual->frames_.size() > state.cap.max_frames - frame_count)
        { return state.fail(census_error::unavailable_capability); }
        frame_count += actual->frames_.size(); // Complete cap check BEFORE sum.
        auto const thread{state.allocate(census_object_kind::thread)};
        if(thread == 0u) { return false; }
        // The ordinal comes from the genuine retained participant roster; it
        // is not an OS TID, CPU run-queue position or replay scheduling witness.
        state.object(thread)->words[0u] = ordinal + 1u;
        state.object(thread)->words[1u] = ordinal;
        for(auto const& saved : actual->frames_)
        {
            auto const& activation{saved.activation}; auto const& logical{saved.logical};
            if(activation.module >= state.actual.modules.size() || activation.module >= state.function_ids.size() ||
                activation.function >= state.function_ids[activation.module].size() ||
                capture::check_owned_values(logical, capture::value_use::live_observation) != checkpoint_thread_capture_status::captured)
            { return state.fail(census_error::incompatible_type); }
            auto const owner{static_cast<::std::size_t>(activation.module)};
            // [actual compiler-sealed sites ... site-1<size] end
            // [safe] check_owned_values proved 0<site<=size BEFORE subtraction.
            auto const& plan{logical.plan->get()};
            auto const& site{plan.sites[static_cast<::std::size_t>(logical.site - 1u)]};
            auto const function{state.function_ids[owner][activation.function]};
            auto const* callable{state.object(function)};
            if(callable == nullptr || callable->kind != census_object_kind::function ||
                callable->words[1u] != activation.function_generation || plan.function_generation != activation.function_generation ||
                plan.module != activation.module || plan.function != activation.function ||
                site.local_count > logical.values.size() || site.operand_count > logical.values.size() - site.local_count ||
                site.saved_parameter_count != logical.values.size() - site.local_count - site.operand_count)
            { return state.fail(census_error::stale_generation); }
            auto const frame{state.allocate(census_object_kind::frame)};
            if(frame == 0u || !state.append_link(thread, frame) || !state.append_link(frame, function)) { return false; }
            state.object(frame)->flags = static_cast<::std::uint16_t>(site.phase);
            state.object(frame)->words = {site.opcode_offset, site.local_count, site.operand_count,
                site.controls.size(), site.handlers.size(), activation.function_generation, site.caller_return_offset, 0u};
            auto const live{site.local_count + site.operand_count}; // Both bounded by values before sum.
            for(::std::size_t slot{}; slot != live; ++slot)
            {
                auto const& actual_value{logical.values[slot]}; census_value value{};
                if(!copy_complete_native(state, owner, actual_value.declaration.type, actual_value.bits.data(),
                    actual_value.declaration.initialized, value) || !state.append_value(frame, value)) { return false; }
            }
            for(auto const& layout : site.controls)
            {
                if(layout.first_saved_parameter > site.saved_parameter_count ||
                    layout.saved_parameter_count > site.saved_parameter_count - layout.first_saved_parameter)
                { return state.fail(census_error::incompatible_type); }
                auto const control{state.allocate(census_object_kind::control)};
                if(control == 0u || !state.append_link(frame, control)) { return false; }
                ::std::uint16_t kind{};
                switch(layout.kind)
                {
                    case native::control_kind::function: kind = 3u; break;
                    case native::control_kind::block: kind = 0u; break;
                    case native::control_kind::loop: kind = 1u; break;
                    case native::control_kind::if_then: case native::control_kind::if_else: kind = 2u; break;
                    default: return state.fail(census_error::incompatible_type);
                }
                state.object(control)->flags = kind;
                state.object(control)->words = {layout.entry_offset, layout.end_offset, layout.saved_parameter_count,
                    layout.declared_results.size(), 0u, 0u, 0u, 0u};
                auto const first{live + layout.first_saved_parameter}; // first_saved<=remaining proved BEFORE +.
                for(::std::size_t parameter{}; parameter != layout.saved_parameter_count; ++parameter)
                {
                    // [actual owned values ... live+saved_first+parameter<size] end
                    // [safe] complete saved window proved BEFORE index addition.
                    auto const& actual_value{logical.values[first + parameter]}; census_value value{};
                    if(!actual_value.declaration.initialized || !copy_complete_native(state, owner,
                        actual_value.declaration.type, actual_value.bits.data(), true, value) ||
                        !state.append_value(control, value)) { return state.fail(census_error::incompatible_type); }
                }
            }
            for(auto const& layout : site.handlers)
            {
                if(layout.target_control >= site.controls.size() || owner >= state.tag_ids.size() ||
                    (!layout.catch_all && layout.tag_index >= state.tag_ids[owner].size()))
                { return state.fail(census_error::incompatible_type); }
                auto const handler{state.allocate(census_object_kind::handler)};
                if(handler == 0u || !state.append_link(frame, handler)) { return false; }
                state.object(handler)->flags = static_cast<::std::uint16_t>((layout.catch_all ? 2u : 0u) | (layout.with_reference ? 1u : 0u));
                state.object(handler)->words = {site.controls.size() - 1u - layout.target_control, layout.target_offset, 0u, 0u, 0u, 0u, 0u, 0u};
                if(!layout.catch_all && !state.append_link(handler, state.tag_ids[owner][layout.tag_index])) { return false; }
                // Actual before-opcode/awaiting-call captures have no in-flight
                // native catch owner. exception_continuation remains refused by
                // the genuine producer until its owned caught-value exists.
            }
        }
    }
    return current_scope() || state.fail(census_error::stale_generation);
}

[[nodiscard]] bool census_copy_complete_graph(complete_census_work& state) const
{
    // Actual module/source/store owners in state precede this list guard and
    // remain alive after unlock. Genuine N exclusion and current publication
    // are the mutation/lifetime proofs; the list mutex is membership only.
    store_type::cohort_guard stores{};
    if(!copy_complete_instance_resources(state) || !census_copy_frames(state) ||
       !drain_complete_reference_graph(state)) { return false; }
    auto const valid{::uwvm2::uwvm::debugger::checkpoint::validate_graph(state.snapshot, state.cap)};
    if(valid != census_error::none) { return state.fail(valid); }
    return current_scope() || state.fail(census_error::stale_generation);
}
