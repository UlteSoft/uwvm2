#include <uwvm2/runtime/lib/uwvm_runtime_llvm_lazy_worker_policy.h>

#include <limits>

using ::uwvm2::runtime::lib::details::llvm_lazy_urgent_worker_allowed;

// Both logical-stack and native-unwind modes obey an explicit zero budget.
static_assert(!llvm_lazy_urgent_worker_allowed(0uz, false));
static_assert(!llvm_lazy_urgent_worker_allowed(0uz, true));
// Keep the existing positive-budget lane and independent native-unwind veto.
static_assert(llvm_lazy_urgent_worker_allowed(1uz, false));
static_assert(!llvm_lazy_urgent_worker_allowed(1uz, true));
static_assert(llvm_lazy_urgent_worker_allowed(16uz, false));
static_assert(!llvm_lazy_urgent_worker_allowed(16uz, true));
static_assert(llvm_lazy_urgent_worker_allowed((::std::numeric_limits<::std::size_t>::max)(), false));
static_assert(!llvm_lazy_urgent_worker_allowed((::std::numeric_limits<::std::size_t>::max)(), true));

int main()
{
    for(::std::size_t budget{}; budget != 17uz; ++budget)
    {
        if(llvm_lazy_urgent_worker_allowed(budget, false) != (budget != 0uz)) { return 1; }
        if(llvm_lazy_urgent_worker_allowed(budget, true)) { return 2; }
    }
}
