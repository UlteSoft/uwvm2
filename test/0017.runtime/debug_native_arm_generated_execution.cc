// Executes actual LLVM-produced ARM C ABI -> AAPCS/TailCC -> AAPCS/TailCC
// code, linked from debug_native_registered_cfi_arm.cc's original object.
// This fixture grants no public Wasm debugger or native frame/read authority.
#include <fast_io.h>
#include <cstdint>
#if !defined(__linux__) || !defined(__arm__) || !defined(__ARMEL__) || !defined(__ARM_PCS_VFP)
# error This fixture requires actual little-endian Linux ARM hard-float execution.
#endif
extern "C" ::std::uint32_t uwvm_cfi_arm_execution_bridge();
int main()
{
    static_assert(sizeof(void*) == 4u);
    unsigned completed{};
    for(unsigned n{}; n != 1024u; ++n)
    {
        ::std::uint32_t sp_before{}, fp_before{}, sp_after{}, fp_after{};
        asm volatile("mov %0, sp\n\tmov %1, r11" : "=r"(sp_before), "=r"(fp_before) : : "memory");
        auto const result{uwvm_cfi_arm_execution_bridge()};
        asm volatile("mov %0, sp\n\tmov %1, r11" : "=r"(sp_after), "=r"(fp_after) : : "memory");
        if(result != 64u || sp_before != sp_after || fp_before != fp_after)
        {
            ::fast_io::io::perrln("actual ARM generated execution: FAIL result/stack/frame preservation");
            return 1;
        }
        ++completed;
    }
    ::fast_io::io::println("actual ARM generated execution: PASS calls=",completed,
        " result=64 stack-preserved=true frame-preserved=true public-Wasm-caller-qualified=false native-finish-qualified=false");
}
