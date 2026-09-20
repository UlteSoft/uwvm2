#include <uwvm2/runtime/lib/uwvm_runtime_state_signature.h>

int main()
{
    using namespace ::uwvm2::runtime::lib::details;

    constexpr char8_t first_cache_path[]{u8"/tmp/uwvm2-cache-a"};
    constexpr char8_t second_cache_path[]{u8"/tmp/uwvm2-cache-b"};

    runtime_state_signature empty{};
    runtime_state_signature full{};
    full.kind = runtime_state_kind::full;
    full.runtime_compiler = 1u;
    full.runtime_mode = 3u;
    full.runtime_scheduling_size = 4096uz;
    full.llvm_jit_cache_path_size = sizeof(first_cache_path) - 1uz;
    full.llvm_jit_cache_path_hash = stable_runtime_state_u8_hash(first_cache_path, full.llvm_jit_cache_path_size);

    if(classify_runtime_state_transition(empty, full) != runtime_state_transition::initialize) { return 1; }
    if(classify_runtime_state_transition(full, full) != runtime_state_transition::reuse) { return 2; }

    auto rejects_reuse{[&](runtime_state_signature const& changed) noexcept
                       { return classify_runtime_state_transition(full, changed) == runtime_state_transition::reject_requires_reset; }};

    auto changed{full};
    changed.kind = runtime_state_kind::uwvm_int_lazy;
    if(!rejects_reuse(changed)) { return 3; }
    changed = full;
    ++changed.runtime_compiler;
    if(!rejects_reuse(changed)) { return 4; }
    changed = full;
    ++changed.runtime_mode;
    if(!rejects_reuse(changed)) { return 5; }
    changed = full;
    changed.assume_full_code_verified = !changed.assume_full_code_verified;
    if(!rejects_reuse(changed)) { return 6; }

    changed = full;
    changed.runtime_compile_threads_existed = !changed.runtime_compile_threads_existed;
    if(!rejects_reuse(changed)) { return 7; }
    changed = full;
    ++changed.runtime_compile_threads_policy;
    if(!rejects_reuse(changed)) { return 8; }
    changed = full;
    ++changed.runtime_compile_threads_resolved;
    if(!rejects_reuse(changed)) { return 9; }
    changed = full;
    changed.runtime_scheduling_policy_existed = !changed.runtime_scheduling_policy_existed;
    if(!rejects_reuse(changed)) { return 10; }
    changed = full;
    ++changed.runtime_scheduling_policy;
    if(!rejects_reuse(changed)) { return 11; }
    changed = full;
    ++changed.runtime_scheduling_size;
    if(!rejects_reuse(changed)) { return 12; }

    changed = full;
    changed.uwvm_int_disable_loop_unwind = !changed.uwvm_int_disable_loop_unwind;
    if(!rejects_reuse(changed)) { return 13; }
    changed = full;
    ++changed.uwvm_int_opcode_conbination_level;
    if(!rejects_reuse(changed)) { return 14; }
    changed = full;
    changed.uwvm_int_disable_delay_local = !changed.uwvm_int_disable_delay_local;
    if(!rejects_reuse(changed)) { return 15; }
    changed = full;
    changed.uwvm_int_enable_instruction_reorder = !changed.uwvm_int_enable_instruction_reorder;
    if(!rejects_reuse(changed)) { return 16; }
    changed = full;
    ++changed.uwvm_int_loop_unwind_max_size;
    if(!rejects_reuse(changed)) { return 17; }

    changed = full;
    changed.llvm_jit_policy_existed = !changed.llvm_jit_policy_existed;
    if(!rejects_reuse(changed)) { return 18; }
    changed = full;
    ++changed.llvm_jit_policy;
    if(!rejects_reuse(changed)) { return 19; }
    changed = full;
    changed.llvm_jit_lazy_policy_existed = !changed.llvm_jit_lazy_policy_existed;
    if(!rejects_reuse(changed)) { return 20; }
    changed = full;
    ++changed.llvm_jit_lazy_policy;
    if(!rejects_reuse(changed)) { return 21; }
    changed = full;
    changed.llvm_jit_full_policy_existed = !changed.llvm_jit_full_policy_existed;
    if(!rejects_reuse(changed)) { return 22; }
    changed = full;
    ++changed.llvm_jit_full_policy;
    if(!rejects_reuse(changed)) { return 23; }
    changed = full;
    changed.llvm_jit_call_stack_existed = !changed.llvm_jit_call_stack_existed;
    if(!rejects_reuse(changed)) { return 24; }
    changed = full;
    ++changed.llvm_jit_call_stack;
    if(!rejects_reuse(changed)) { return 25; }
    changed = full;
    changed.llvm_jit_disable_ir_verification = !changed.llvm_jit_disable_ir_verification;
    if(!rejects_reuse(changed)) { return 26; }

    changed = full;
    ++changed.llvm_jit_cache_path_mode;
    if(!rejects_reuse(changed)) { return 27; }
    changed = full;
    changed.llvm_jit_cache_path_size = sizeof(second_cache_path) - 1uz;
    changed.llvm_jit_cache_path_hash = stable_runtime_state_u8_hash(second_cache_path, changed.llvm_jit_cache_path_size);
    if(!rejects_reuse(changed)) { return 28; }
    changed = full;
    changed.llvm_jit_cache_no_sign = !changed.llvm_jit_cache_no_sign;
    if(!rejects_reuse(changed)) { return 29; }
    changed = full;
    changed.llvm_jit_cache_no_verify = !changed.llvm_jit_cache_no_verify;
    if(!rejects_reuse(changed)) { return 30; }

    changed = full;
    changed.tiered_disable_uwvm_int_lazy_interpreter = !changed.tiered_disable_uwvm_int_lazy_interpreter;
    if(!rejects_reuse(changed)) { return 31; }
    changed = full;
    changed.tiered_disable_llvm_full_jit = !changed.tiered_disable_llvm_full_jit;
    if(!rejects_reuse(changed)) { return 32; }

    // reset_runtime_state_host_api publishes the empty signature again, after which a different mode/backend may initialize.
    auto lazy{full};
    lazy.kind = runtime_state_kind::uwvm_int_lazy;
    lazy.runtime_mode = 1u;
    if(classify_runtime_state_transition(runtime_state_signature{}, lazy) != runtime_state_transition::initialize) { return 33; }
    if(classify_runtime_state_transition(lazy, lazy) != runtime_state_transition::reuse) { return 34; }
    if(classify_runtime_state_transition(lazy, runtime_state_signature{}) != runtime_state_transition::reject_requires_reset) { return 35; }
}
