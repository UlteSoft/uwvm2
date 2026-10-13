#include <fast_io.h>

extern "C" [[noreturn, gnu::noinline]] void uwvm2_test_fast_terminate() noexcept
{
#if defined(UWVM2_TEST_BUILTIN_TRAP_CONTROL)
    __builtin_trap();
#else
    ::fast_io::fast_terminate();
#endif
}

int main() { uwvm2_test_fast_terminate(); }
