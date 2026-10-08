// Match real unwinder regions to callable addresses on direct and descriptor
// ABIs. This is a native ABI regression, not a substitute for actual JIT tests.
#include <uwvm2/runtime/lib/uwvm_runtime_native_function_address.h>
#include <atomic>
#include <cstdio>
#include <unwind.h>

struct trace_state
{
    std::uintptr_t recursive_code{}, root_code{};
    unsigned recursive_frames{}, root_frames{};
    bool bad_order{};
};

static _Unwind_Reason_Code inspect(_Unwind_Context* context, void* opaque)
{
    auto& state{*static_cast<trace_state*>(opaque)};
    auto const region{static_cast<std::uintptr_t>(_Unwind_GetRegionStart(context))};
    if(region == state.recursive_code)
    {
        state.bad_order |= state.root_frames != 0;
        ++state.recursive_frames;
    }
    else if(region == state.root_code)
    {
        state.bad_order |= state.recursive_frames != 3;
        ++state.root_frames;
    }
    return _URC_NO_REASON;
}

extern "C" __attribute__((noinline)) void recursive(unsigned depth, trace_state* state)
{
    if(depth) { recursive(depth - 1, state); }
    else { static_cast<void>(_Unwind_Backtrace(inspect, state)); }
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

extern "C" __attribute__((noinline)) void root(trace_state* state)
{
    recursive(2, state);
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

int main()
{
    using uwvm2::runtime::lib::details::native_function_code_address;
    if(native_function_code_address(0) != 0) { return 1; }
    auto const root_callable{reinterpret_cast<std::uintptr_t>(&root)};
    trace_state state{native_function_code_address(reinterpret_cast<std::uintptr_t>(&recursive)),
                      native_function_code_address(root_callable)};
#if defined(__ELF__) && defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    if(state.root_code == root_callable) { return 2; }
#else
    if(state.root_code != root_callable) { return 2; }
#endif
    // Invoke the callable, not its normalized code address: ELFv1 needs its TOC.
    auto volatile invoke{reinterpret_cast<void (*)(trace_state*)>(root_callable)};
    invoke(&state);
    std::printf("recursive=%u root=%u order=%s callable=%llx code=%llx\n",
                state.recursive_frames, state.root_frames, state.bad_order ? "bad" : "ok",
                static_cast<unsigned long long>(root_callable), static_cast<unsigned long long>(state.root_code));
    return state.bad_order || state.recursive_frames != 3 || state.root_frames != 1;
}
