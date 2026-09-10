#include <uwvm2/runtime/lib/uwvm_runtime_execution_entry.h>

#include <cstddef>
#include <limits>

int main()
{
    namespace execution = ::uwvm2::runtime::lib::details;
    using execution::runtime_execution_entry_leave_result;
    using execution::runtime_execution_entry_reentry;

    ::std::size_t depth{};
    bool outer_fp_state{true};
    ::std::size_t outer_bridge_token{3u};
    bool outer_scratch_state{true};
    ::std::size_t cleanup_count{};

    // This models either the full interpreter entry or the full LLVM entry. Both use the same reject policy.
    if(!execution::runtime_execution_entry_enter(depth) || depth != 1u) { return 1; }
    if(execution::runtime_execution_entry_reset_allowed(depth)) { return 2; }

    // A nested full entry uses `reject` too; it must fail without changing ownership of the outer state.
    if(execution::runtime_execution_entry_enter(depth, runtime_execution_entry_reentry::reject) || depth != 1u) { return 3; }

    // The public LLVM raw API is the only supported callback re-entry. Its exit is nested and therefore cannot clean
    // the outer token, FP marker, or interpreter scratch allocation.
    if(!execution::runtime_execution_entry_enter(depth, runtime_execution_entry_reentry::allow_public_llvm_raw) || depth != 2u) { return 4; }
    if(execution::runtime_execution_entry_leave(depth) != runtime_execution_entry_leave_result::nested || depth != 1u) { return 5; }
    if(!outer_fp_state || outer_bridge_token != 3u || !outer_scratch_state || cleanup_count != 0u) { return 6; }
    if(execution::runtime_execution_entry_reset_allowed(depth)) { return 7; }

    if(execution::runtime_execution_entry_leave(depth) != runtime_execution_entry_leave_result::outermost || depth != 0u) { return 8; }
    outer_fp_state = false;
    outer_bridge_token = 0u;
    outer_scratch_state = false;
    ++cleanup_count;
    if(outer_fp_state || outer_bridge_token != 0u || outer_scratch_state || cleanup_count != 1u) { return 9; }
    if(!execution::runtime_execution_entry_reset_allowed(depth)) { return 10; }

    if(execution::runtime_execution_entry_leave(depth) != runtime_execution_entry_leave_result::invalid) { return 11; }

    depth = (::std::numeric_limits<::std::size_t>::max)();
    if(execution::runtime_execution_entry_enter(depth, runtime_execution_entry_reentry::reject)) { return 12; }
    if(execution::runtime_execution_entry_enter(depth, runtime_execution_entry_reentry::allow_public_llvm_raw)) { return 13; }
    if(depth != (::std::numeric_limits<::std::size_t>::max)()) { return 14; }

    ::std::size_t publication_depth{};
    if(!execution::runtime_state_publication_access_allowed(publication_depth)) { return 15; }
    if(!execution::runtime_state_publication_enter(publication_depth) || publication_depth != 1u) { return 16; }
    // Public raw execution and reset share this pre-lock gate; neither may recursively wait on an owned publication lock.
    if(execution::runtime_state_publication_access_allowed(publication_depth)) { return 17; }
    if(execution::runtime_state_publication_enter(publication_depth) || publication_depth != 1u) { return 18; }
    if(!execution::runtime_state_publication_leave(publication_depth) || publication_depth != 0u) { return 19; }
    if(!execution::runtime_state_publication_access_allowed(publication_depth)) { return 20; }
    if(execution::runtime_state_publication_leave(publication_depth)) { return 21; }

    ::std::size_t metadata_callback_depth{};
    if(!execution::runtime_compilation_metadata_callback_access_allowed(metadata_callback_depth)) { return 22; }
    if(!execution::runtime_compilation_metadata_callback_enter(metadata_callback_depth) || metadata_callback_depth != 1u) { return 23; }
    // Public raw and reset must fail closed for compilation metadata even when no publication lock is held. Nested
    // metadata helpers are permitted, but access remains blocked until the outermost provider callback has returned.
    if(execution::runtime_compilation_metadata_callback_access_allowed(metadata_callback_depth)) { return 24; }
    if(!execution::runtime_compilation_metadata_callback_enter(metadata_callback_depth) || metadata_callback_depth != 2u) { return 25; }
    if(!execution::runtime_compilation_metadata_callback_leave(metadata_callback_depth) || metadata_callback_depth != 1u) { return 26; }
    if(execution::runtime_compilation_metadata_callback_access_allowed(metadata_callback_depth)) { return 27; }
    if(!execution::runtime_compilation_metadata_callback_leave(metadata_callback_depth) || metadata_callback_depth != 0u) { return 28; }
    if(!execution::runtime_compilation_metadata_callback_access_allowed(metadata_callback_depth)) { return 29; }
    if(execution::runtime_compilation_metadata_callback_leave(metadata_callback_depth)) { return 30; }
    metadata_callback_depth = (::std::numeric_limits<::std::size_t>::max)();
    if(execution::runtime_compilation_metadata_callback_enter(metadata_callback_depth) ||
       metadata_callback_depth != (::std::numeric_limits<::std::size_t>::max)())
    {
        return 31;
    }
    return 0;
}
