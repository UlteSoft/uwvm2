/* GCC/system-linker control for the isolated LLVM-generated-code fixture.
 * Identical four modes and recursion count; failure here is not evidence of a
 * RuntimeDyld regression. Build with -fno-optimize-sibling-calls so all ten
 * frames remain observable. This is diagnostic code, not a production handler.
 */
#include <stdint.h>
#include <string.h>
extern struct { uint32_t mode, padding; uint64_t float_bits; } uwvm_remote_state;
extern void uwvm_host_capture(void);

__attribute__((noinline)) void uwvm_remote_recursive(unsigned depth)
{
    if(depth) { uwvm_remote_recursive(depth - 1); return; }
    switch(*(volatile uint32_t*)&uwvm_remote_state.mode)
    {
    case 1: __builtin_trap();
    case 2: (void)*(volatile uint32_t*)(uintptr_t)0x30000000; return;
    case 3: {
        uint64_t bits=*(volatile uint64_t*)&uwvm_remote_state.float_bits;
        double value;
        memcpy(&value, &bits, sizeof(value));
        if(!(value > -2147483649.0 && value < 2147483648.0)) { __builtin_trap(); }
        *(volatile uint32_t*)&uwvm_remote_state.mode=(uint32_t)(int32_t)value;
        return;
    }
    default: uwvm_host_capture(); return;
    }
}

__attribute__((noinline)) void uwvm_remote_entry(void)
{
    uwvm_remote_recursive(8);
}
