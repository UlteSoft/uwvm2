/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>

namespace uwvm2::runtime::lib::details
{
    // A live logical stack and a tiered boundary snapshot are both stored oldest-to-newest. Match only activations
    // occupying the same absolute stack position. Function identity alone is not an activation key: recursive calls may
    // have identical (module,function) pairs at several distinct positions and every one must remain visible in diagnostics.
    template <typename Frame>
    [[nodiscard]] inline constexpr ::std::size_t runtime_logical_activation_positional_overlap(
        Frame const* live_frames,
        ::std::size_t live_size,
        Frame const* snapshot_frames,
        ::std::size_t snapshot_size,
        ::std::size_t snapshot_live_begin) noexcept
    {
        if((live_size != 0uz && live_frames == nullptr) || (snapshot_size != 0uz && snapshot_frames == nullptr)) [[unlikely]]
        {
            return 0uz;
        }

        if(snapshot_live_begin > live_size) { return 0uz; }
        auto const live_available{live_size - snapshot_live_begin};
        auto const limit{live_available < snapshot_size ? live_available : snapshot_size};
        ::std::size_t overlap{};
        for(; overlap != limit; ++overlap)
        {
            auto const& live{live_frames[snapshot_live_begin + overlap]};
            auto const& snapshot{snapshot_frames[overlap]};
            if(live.module_id != snapshot.module_id || live.function_index != snapshot.function_index) { break; }
        }
        return overlap;
    }
}  // namespace uwvm2::runtime::lib::details
