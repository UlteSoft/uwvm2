/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "source_scope_path.h"
# include <algorithm>
# include <limits>
# include <new>
# include <string_view>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::source_dwarf
{
    enum class inline_query_error { none, unavailable, malformed, ambiguous, limit_exceeded, allocation_failure };
    struct inline_frame
    {
        // Owned display strings only. call_file/line/column are DWARF call-site
        // metadata, not a fabricated caller Wasm PC or the current source line.
        ::std::string name{}, call_file{};
        ::std::uint64_t call_line{}, call_column{};
    };
    struct inline_query_limits
    {
        ::std::size_t max_scopes{65536u}, max_ranges{65536u}, max_depth{64u}, max_frames{64u}, max_string_bytes{16384u};
    };
    // Pure metadata query. PC is Code-relative; this function supplies NO runtime
    // stop, generation, source, frame or read authority. Production first obtains
    // it from the native runtime's genuine paused participant/publication query.
    // Only the current physical frame can be expanded. No LLVM API/expression,
    // source file, native pointer, caller PC, local or memory value is accessed.
    [[nodiscard]] inline inline_query_error query_inline_frames(::std::span<scope_record const> scopes,
        ::std::uint64_t pc, ::std::vector<inline_frame>& out, inline_query_limits const& cap = {}) noexcept
    {
        out.clear();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            // The shared selector owns physical/inline identities and performs
            // every range, parent and enum check before display copies. The
            // physical key is additional to this API's inline-frame budget.
            if(cap.max_frames == (::std::numeric_limits<::std::size_t>::max)())
            { return inline_query_error::limit_exceeded; }
            concrete_scope_path path{};
            scope_path_limits const path_cap{cap.max_scopes, cap.max_ranges, cap.max_depth, cap.max_frames + 1u};
            auto const selected{query_concrete_scope_path(scopes, pc, path, path_cap)};
            switch(selected)
            {
                case scope_path_error::none: break;
                case scope_path_error::unavailable: return inline_query_error::unavailable;
                case scope_path_error::malformed: return inline_query_error::malformed;
                case scope_path_error::ambiguous: return inline_query_error::ambiguous;
                case scope_path_error::limit_exceeded: return inline_query_error::limit_exceeded;
                case scope_path_error::allocation_failure: return inline_query_error::allocation_failure;
                default: return inline_query_error::malformed;
            }
            ::std::vector<inline_frame> pending{};
            ::std::size_t strings{};
            for(::std::size_t i{1u}; i < path.record_indices.size(); ++i)
            {
                // [owned checked path: physical, inline ... i ... end]
                // [safe                                             ] i < size.
                //  ^^ scalar index advances only; no pointer is modified.
                auto const index{path.record_indices[i]};
                // [same immutable scope span ... index ... end]
                // [safe                                      ] selector bounded index.
                //  ^^ no borrowed scope/frame pointer escapes the display copy.
                auto const& scope{scopes[index]};
                if(!budget::charge(scope.name.size(), cap.max_string_bytes, strings) ||
                   !budget::charge(scope.call_file.size(), cap.max_string_bytes, strings))
                { return inline_query_error::limit_exceeded; }
                pending.push_back({::fast_io::concat_std(::std::string_view{scope.name}),
                    ::fast_io::concat_std(::std::string_view{scope.call_file}), scope.call_line, scope.call_column});
            }
            ::std::reverse(pending.begin(), pending.end());
            out = ::std::move(pending); return inline_query_error::none;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out.clear(); return inline_query_error::allocation_failure; }
#endif
    }
}
