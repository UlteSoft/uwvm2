// Included inside the emitter namespace. Exact signature/noexcept wrappers
// are instantiated only as native targets selected for a debug-full function.
// They do not replace the original provider FP/generated-depth suspend guards.
template<auto Function, typename Signature = decltype(Function)>
struct llvm_jit_debug_host_bridge_wrapper;
template<auto Function, typename Return, typename... Args>
struct llvm_jit_debug_host_bridge_wrapper<Function, Return (*)(Args...)>
{
    static Return invoke(Args... args)
    {
        ::uwvm2::runtime::lib::details::llvm_jit_debug_host_scope scope{};
        return Function(::std::forward<Args>(args)...);
    }
};
template<auto Function, typename Return, typename... Args>
struct llvm_jit_debug_host_bridge_wrapper<Function, Return (*)(Args...) noexcept>
{
    static Return invoke(Args... args) noexcept
    {
        ::uwvm2::runtime::lib::details::llvm_jit_debug_host_scope scope{};
        return Function(::std::forward<Args>(args)...);
    }
};
// A SysV annotated native target can appear in a Win64 GNU/Clang bridge. Keep
// that attribute on the underlying call; the generated wrapper itself uses the
// qualified host convention selected by apply_llvm_jit_host_calling_conv.
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64)) && !defined(__arm64ec__) && \
    (defined(__GNUC__) || defined(__clang__))
template<auto Function, typename Return, typename... Args>
struct llvm_jit_debug_host_bridge_wrapper<Function, Return (__attribute__((sysv_abi)) *)(Args...)>
{
    static Return invoke(Args... args)
    {
        ::uwvm2::runtime::lib::details::llvm_jit_debug_host_scope scope{};
        return Function(::std::forward<Args>(args)...);
    }
};
template<auto Function, typename Return, typename... Args>
struct llvm_jit_debug_host_bridge_wrapper<Function, Return (__attribute__((sysv_abi)) *)(Args...) noexcept>
{
    static Return invoke(Args... args) noexcept
    {
        ::uwvm2::runtime::lib::details::llvm_jit_debug_host_scope scope{};
        return Function(::std::forward<Args>(args)...);
    }
};
#endif
template<auto A, auto B>
inline constexpr bool llvm_jit_same_debug_bridge{[]() constexpr noexcept
{
    if constexpr(::std::is_same_v<decltype(A), decltype(B)>) { return A == B; }
    else { return false; }
}()};
template<auto Function>
inline constexpr bool llvm_jit_debug_control_bridge{
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_activation_enter_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_activation_leave_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_host_enter_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_host_leave_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_debug_raw_target_abi_bridge> ||
    // Classify the actual target before any foreign-host island is entered.
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_call_raw_from_generated_wasm_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_raw_target_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observe_call_raw_from_generated_wasm_abi_bridge> ||
    // Internal checkpoint DATA/activation leaves preserve the actual generated
    // frame; wrapping them as a foreign island would invalidate their issuer.
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_workspace_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_enter_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::llvm_jit_push_call_stack_frame> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame> ||
    // Strict native-frame TLS leaves never park or call guest/host callbacks.
    // In particular GC enter runs BEFORE the genuine activation-enter hook:
    // a pending typed tail must cross this leaf without creating a host island.
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::gc::uwvm_gc_root_frame_enter_checked_abi> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::gc::uwvm_gc_root_frame_leave_checked_abi> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::llvm_jit_runtime_trap> ||
    llvm_jit_same_debug_bridge<Function, ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap>};
