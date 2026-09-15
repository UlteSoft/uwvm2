// Freestanding codegen isolates MSACSR save/restore from SDK availability.
// Inspect emitted control instructions as well as runtime tests; successful
// assembly does not establish coverage for a build that never enables MSA.
// Build only explicitly for a MIPS MSA Linux target with -nostdlib -static -Wl,-e,_start.
// This checks the actual auxiliary guard without substituting libc fenv stubs or requiring a cross C++ runtime.
#if defined(UWVM_TEST_FREESTANDING_MIPS_MSA) && defined(__mips_msa)
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_aux_environment.h>
namespace
{
    unsigned read_control() { unsigned value; __asm__ volatile("cfcmsa %0, $1" : "=r"(value) : : "memory"); return value; }
    void write_control(unsigned value) { __asm__ volatile("ctcmsa $1, %0" : : "r"(value) : "memory"); }
    int test()
    {
        using guard = uwvm2::runtime::lib::details::scoped_wasm_auxiliary_fp_environment;
        unsigned const original{read_control()};
        write_control(0x01000002u); // Flush and round upwards, without enabling exceptions.
        int result{};
        {
            guard entry{true, true};
            if(read_control() != 0u) { result = 1; }
            {
                guard callback{true};
                write_control(0x01000001u);
            }
            if(read_control() != 0u) { result = 2; }
        }
        if(read_control() != 0x01000002u) { result = 3; }
        write_control(original);
        return result;
    }
}
extern "C" [[noreturn]] void _start()
{
    register long result __asm__("$4"){test()};
#if defined(__mips64)
    register long syscall_number __asm__("$2"){5058}; // Linux n64 exit.
#else
    register long syscall_number __asm__("$2"){4001}; // Linux o32 exit.
#endif
    __asm__ volatile("syscall" : "+r"(syscall_number) : "r"(result) : "$3", "$7", "memory");
    __builtin_unreachable();
}
#else
#error "Build this explicit probe for MIPS MSA with UWVM_TEST_FREESTANDING_MIPS_MSA, not as a native test."
#endif
