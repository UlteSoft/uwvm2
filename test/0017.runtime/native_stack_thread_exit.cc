// Reproduce native stack registration/TSD teardown under TSan without LLVM or
// intentional faults. The normal native_stack_guard test covers exhaustion and
// host-destructor reentry; this test must complete with live sanitizer state.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <thread>
#include <cstdlib>
#include <cstdio>
int main()
{
    for(unsigned i{};i!=32;++i)
    {
        std::thread thread{[]
        {
            for(unsigned j{};j!=4;++j)
            {
                uwvm2::runtime::lib::native_stack::scope outer;
                if(!outer.ready()) {std::abort();}
                uwvm2::runtime::lib::native_stack::scope nested;
                if(!nested.ready()) {std::abort();}
            }
        }};
        thread.join();
    }
    std::puts("PASS native stack thread exit: nested/cache entry and pthread teardown");
}
