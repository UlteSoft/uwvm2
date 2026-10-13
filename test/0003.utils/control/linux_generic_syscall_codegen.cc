#include "linux_generic_test_abi.h"
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif
#if !defined(__NR_getpid) || !defined(__NR_close) || !defined(__NR_fdatasync)
# error "The actual generic target SDK must provide these syscall declarations"
#endif
// Source-only O3/EH/noEH caller probes. Keeper emits target IR and assembly;
// these functions are never executed on the development host.
extern "C" [[gnu::noinline,gnu::used]] long generic_codegen_pid() noexcept
{ return ::fast_io::system_call<__NR_getpid, long>(); }
extern "C" [[gnu::noinline,gnu::used]] long generic_codegen_badfd(long descriptor) noexcept
{ return ::fast_io::system_call<__NR_close, long>(descriptor); }
extern "C" [[gnu::noinline,gnu::used]] int generic_codegen_fdatasync(int descriptor) noexcept
{ return ::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{descriptor}).error; }
#if !defined(UWVM_TEST_IMPORT_FAST_IO)
// no_return was not publicly exported before this patch; preserve module API.
extern "C" [[gnu::noinline,gnu::used]] void generic_codegen_trap() noexcept
{ ::fast_io::system_call_no_return<__NR_getpid>(0L); }
#endif
