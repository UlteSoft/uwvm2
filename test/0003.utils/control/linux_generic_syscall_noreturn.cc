#include "linux_generic_test_abi.h"
#include <fast_io.h>
#if !defined(__NR_getpid) || !defined(__NR_exit)
# error "Actual generic target syscall declarations are required"
#endif
// Keeper must run this as a NEW child in the existing shared cgroup. Never
// execute it inside a persistent uwvm process, debugger, keeper or supervisor.
// Default: getpid returns and the new cold fatal trap must terminate the child.
// UWVM_TEST_GENERIC_EXPECT_EXIT: true exit does not return; child status is 93.
// Target trap signal is established by actual target execution, not guessed
// from x86 instruction/signal names and reused for PowerPC or another ISA.
int main()
{
#if defined(UWVM_TEST_GENERIC_EXPECT_EXIT)
    ::fast_io::system_call_no_return<__NR_exit>(93L);
#else
    ::fast_io::system_call_no_return<__NR_getpid>(0L);
#endif
}
