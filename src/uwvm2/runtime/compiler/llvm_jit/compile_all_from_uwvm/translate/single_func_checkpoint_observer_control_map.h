#pragma once
// Same-fused-walk compilation DATA only. The vector below cannot authenticate
// a publication, native address, paused participant or executable continuation.
// Pending lexical ends are NEVER accepted by sealed_function_plan: only the
// original validator's actual `end` transition closes and backpatches them.
struct llvm_jit_checkpoint_observer_control_map
{
    struct link { ::std::size_t site{}, item{}; bool handler{}; };
    struct scope
    {
        ::std::size_t entry{}, end{SIZE_MAX};
        ::std::vector<link> pending{};
    };
    ::uwvm2::runtime::checkpoint::function_plan* plan{};
    ::std::vector<scope> scopes{};
    [[nodiscard]] bool selected() const noexcept
    {
        namespace cp = ::uwvm2::runtime::checkpoint;
        return plan != nullptr && plan->profile;
    }
    [[nodiscard]] ::std::size_t begin(::std::size_t offset)
    {
        if(!selected() || plan->producer_availability != ::uwvm2::runtime::checkpoint::status::ok ||
           offset >= plan->expression_bytes || scopes.size() >= plan->expression_bytes) { return SIZE_MAX; }
        scopes.push_back({offset}); return scopes.size()-1u;
    }
    [[nodiscard]] bool close(::std::size_t index, ::std::size_t offset) noexcept
    {
        if(!selected()) { return true; }
        if(index == SIZE_MAX && plan->producer_availability == ::uwvm2::runtime::checkpoint::status::quota_exceeded) { return true; }
        if(index >= scopes.size() || offset >= plan->expression_bytes) { return false; }
        // [actual same-walk scope records0 ... index ... N] end
        // [safe] full ordinal bound BEFORE indexing; scopes do not escape.
        auto& current{scopes[index]};
        if(current.end != SIZE_MAX || current.entry > offset) { return false; }
        for(auto const fix : current.pending)
        {
            if(fix.site >= plan->sites.size()) { return false; }
            // [actual tentative compiler site cells0 ... fix.site ... N] end
            // [safe] current vector count is checked before selecting a cell.
            auto& site{plan->sites[fix.site]};
            if(fix.handler)
            {
                if(fix.item >= site.handlers.size()) { return false; }
                auto& field{site.handlers[fix.item].target_offset};
                if(field != SIZE_MAX) { return false; } field = offset;
            }
            else
            {
                if(fix.item >= site.controls.size()) { return false; }
                auto& control{site.controls[fix.item]};
                if(control.entry_offset != current.entry || control.end_offset != SIZE_MAX) { return false; }
                control.end_offset = offset;
            }
        }
        current.end = offset; current.pending.clear(); return true;
    }
    [[nodiscard]] ::std::size_t find(::std::size_t entry) const noexcept
    {
        ::std::size_t lo{}, hi{scopes.size()};
        while(lo != hi)
        {
            auto const mid{lo+(hi-lo)/2u};
            // [same-walk ascending lexical entries0 ... mid ... N] end
            // [safe] lo<=mid<hi<=N BEFORE indexing; subtraction cannot overflow.
            if(scopes[mid].entry <= entry) { lo=mid+1u; }else{ hi=mid; }
        }
        if(lo != 0u && scopes[lo-1u].entry == entry) { return lo-1u; }
        return SIZE_MAX;
    }
    [[nodiscard]] ::uwvm2::runtime::checkpoint::status validate_tentative(
        ::uwvm2::runtime::checkpoint::safepoint_layout const& actual) const
    {
        namespace cp = ::uwvm2::runtime::checkpoint;
        if(!selected()) { return plan == nullptr ? cp::status::invalid_plan : cp::validate_site(actual,*plan); }
        // Only a PRIVATE compiler copy normalizes unresolved fields for the
        // ordinary type/count checker. The real plan retains SIZE_MAX until
        // actual `end`, so seal_compiler_metadata still fails while pending.
        // This helper does not read source bytes or create runtime authority.
        auto checked{actual};
        for(auto& control : checked.controls)
        {
            if(control.end_offset != SIZE_MAX) { continue; }
            auto const index{find(control.entry_offset)};
            if(index >= scopes.size() || scopes[index].end != SIZE_MAX || control.entry_offset > actual.opcode_offset)
            { return cp::status::invalid_layout; }
            control.end_offset = actual.opcode_offset;
        }
        for(auto& handler : checked.handlers)
        {
            if(handler.target_offset != SIZE_MAX) { continue; }
            if(handler.target_control >= checked.controls.size()) { return cp::status::invalid_layout; }
            auto const& target{actual.controls[handler.target_control]};
            if(target.kind == cp::control_kind::loop || target.end_offset != SIZE_MAX || find(target.entry_offset) >= scopes.size())
            { return cp::status::invalid_layout; }
            handler.target_offset = actual.opcode_offset;
        }
        return cp::validate_site(checked,*plan);
    }
    [[nodiscard]] bool link_site(::std::size_t ordinal)
    {
        if(!selected()) { return true; }
        if(ordinal >= plan->sites.size()) { return false; }
        auto const& site{plan->sites[ordinal]};
        if(validate_tentative(site) != ::uwvm2::runtime::checkpoint::status::ok) { return false; }
        for(::std::size_t i{}; i != site.controls.size(); ++i)
        {
            auto const& control{site.controls[i]};
            if(control.end_offset != SIZE_MAX) { continue; }
            auto const index{find(control.entry_offset)};
            if(index >= scopes.size()) { return false; }
            scopes[index].pending.push_back({ordinal,i,false});
        }
        for(::std::size_t i{}; i != site.handlers.size(); ++i)
        {
            auto const& handler{site.handlers[i]};
            if(handler.target_offset != SIZE_MAX) { continue; }
            if(handler.target_control >= site.controls.size()) { return false; }
            auto const index{find(site.controls[handler.target_control].entry_offset)};
            if(index >= scopes.size()) { return false; }
            scopes[index].pending.push_back({ordinal,i,true});
        }
        return true;
    }
    void rollback_sites_from(::std::size_t first) noexcept
    {
        for(auto& current : scopes)
        {
            ::std::size_t kept{};
            for(::std::size_t i{}; i != current.pending.size(); ++i)
            {
                // [actual pending links0 ... kept<=i ... N] end
                // [safe] i<N, then kept<=i BEFORE reading/writing one cell.
                auto const item{current.pending[i]};
                if(item.site < first) { current.pending[kept++]=item; }
            }
            current.pending.resize(kept); // remove only tentative failed sites
        }
    }
    [[nodiscard]] bool complete() const noexcept
    {
        if(!selected()) { return true; }
        for(auto const& current : scopes) { if(current.end == SIZE_MAX || !current.pending.empty()) { return false; } }
        return true;
    }
};
struct runtime_local_func_llvm_jit_emit_state_t;
struct llvm_jit_checkpoint_observer_site_query
{
    void* context{};
    bool (*stage)(void*, runtime_local_func_llvm_jit_emit_state_t&){};
};
