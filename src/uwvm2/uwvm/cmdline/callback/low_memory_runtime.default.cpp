// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "runtime_compiler_log.h"
#include "runtime_compile_threads.h"
#include "runtime_scheduling_policy.h"
#include "runtime_llvm_jit_policy.h"
#include "runtime_llvm_jit_lazy_policy.h"
#include "runtime_llvm_jit_full_policy.h"
#include "runtime_llvm_jit_exception_dispatch.h"
#include "runtime_llvm_jit_call_stack.h"
#include "runtime_llvm_jit_cache_path.h"
#include "runtime_debug_int.h"
#include "runtime_int.h"
#include "runtime_jit.h"
#include "runtime_aot.h"
#include "runtime_tiered.h"
#include "runtime_uwvm_int_set_opcode_conbination_level.h"
#include "runtime_uwvm_int_loop_unwind_max_size.h"

extern "C" void const* uwvm_low_memory_callback_runtime_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
