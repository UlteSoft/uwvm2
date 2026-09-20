#include <uwvm2/runtime/lib/uwvm_runtime_state_signature.h>

int main()
{
    using namespace ::uwvm2::runtime::lib::details;

    runtime_state_signature empty{};
    runtime_state_signature published{};
    published.kind = runtime_state_kind::full;
    published.runtime_compiler = 1u;
    published.runtime_mode = 1u;
    published.runtime_compile_threads_resolved = 1uz;
    published.runtime_scheduling_size = 4096uz;
    published.uwvm_int_opcode_conbination_level = 1u;
    published.uwvm_int_loop_unwind_max_size = 4096uz;
    published.llvm_jit_cache_enabled = true;
    published.llvm_jit_cache_generate_signature = true;
    published.llvm_jit_cache_verify_signature = true;

    if(classify_runtime_state_transition(empty, published) != runtime_state_transition::initialize) { return 1; }
    if(classify_runtime_state_transition(published, published) != runtime_state_transition::reuse) { return 2; }
    if(classify_runtime_state_transition(published, empty) != runtime_state_transition::reject_requires_reset) { return 3; }

    auto expect_reject{[&](runtime_state_signature requested) noexcept
                       { return classify_runtime_state_transition(published, requested) == runtime_state_transition::reject_requires_reset; }};

    auto changed{published};
    changed.runtime_compiler = 2u;
    if(!expect_reject(changed)) { return 4; }
    changed = published;
    changed.runtime_compile_threads_resolved = 2uz;
    if(!expect_reject(changed)) { return 5; }
    changed = published;
    changed.runtime_scheduling_size = 2048uz;
    if(!expect_reject(changed)) { return 6; }
    changed = published;
    changed.uwvm_int_disable_delay_local = true;
    if(!expect_reject(changed)) { return 7; }
    changed = published;
    changed.uwvm_int_opcode_conbination_level = 0u;
    if(!expect_reject(changed)) { return 8; }
    changed = published;
    changed.uwvm_int_enable_instruction_reorder = true;
    if(!expect_reject(changed)) { return 9; }
    changed = published;
    changed.uwvm_int_disable_loop_unwind = true;
    if(!expect_reject(changed)) { return 10; }
    changed = published;
    changed.llvm_jit_policy = 2u;
    if(!expect_reject(changed)) { return 11; }
    changed = published;
    changed.llvm_jit_full_policy = 3u;
    if(!expect_reject(changed)) { return 12; }
    changed = published;
    changed.llvm_jit_call_stack = 1u;
    if(!expect_reject(changed)) { return 13; }
    changed = published;
    changed.llvm_jit_cache_enabled = false;
    if(!expect_reject(changed)) { return 14; }
    changed = published;
    changed.llvm_jit_cache_generate_signature = false;
    if(!expect_reject(changed)) { return 15; }
    changed = published;
    changed.llvm_jit_cache_verify_signature = false;
    if(!expect_reject(changed)) { return 16; }

    constexpr char8_t first_cache_path[]{u8"/tmp/uwvm2-cache-a"};
    constexpr char8_t second_cache_path[]{u8"/tmp/uwvm2-cache-b"};
    published.llvm_jit_cache_path_size = sizeof(first_cache_path) - 1uz;
    published.llvm_jit_cache_path_hash = stable_runtime_state_u8_hash(first_cache_path, published.llvm_jit_cache_path_size);
    changed = published;
    changed.llvm_jit_cache_path_size = sizeof(second_cache_path) - 1uz;
    changed.llvm_jit_cache_path_hash = stable_runtime_state_u8_hash(second_cache_path, changed.llvm_jit_cache_path_size);
    if(!expect_reject(changed)) { return 17; }

    // Clearing the publication during reset permits a different full-runtime configuration to initialize.
    if(classify_runtime_state_transition(runtime_state_signature{}, changed) != runtime_state_transition::initialize) { return 18; }
}
