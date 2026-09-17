// Link with the cross-target object emitted by simd_direct_lowering.cc. A store that traps must not modify its
// in-bounds prefix, including when LLVM splits a vector into multiple target stores. No fork/core dump needed.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DECL(N) extern void guarded_store_##N(unsigned char*, uint32_t, const unsigned char*);
DECL(1) DECL(2) DECL(4) DECL(8) DECL(16) DECL(32) DECL(64)
static sigjmp_buf resume;
static volatile sig_atomic_t faulted;
static void fault(int signal_number) { faulted = signal_number; siglongjmp(resume, 1); }

int main(void)
{
    const unsigned page = 65536;
    if(sysconf(_SC_PAGESIZE) > page) { puts("SKIP: host pages exceed Wasm page size"); return 77; }
    unsigned char* memory = (unsigned char*)mmap(0, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(memory == MAP_FAILED || mprotect(memory + page, page, PROT_NONE)) { return 1; }
    struct sigaction action = {0}; action.sa_handler = fault; sigemptyset(&action.sa_mask);
    if(sigaction(SIGSEGV, &action, 0) || sigaction(SIGBUS, &action, 0)) { return 2; }
    void (*const functions[])(unsigned char*, uint32_t, const unsigned char*) = {
        guarded_store_1, guarded_store_2, guarded_store_4, guarded_store_8, guarded_store_16, guarded_store_32, guarded_store_64};
    unsigned char input[64]; memset(input, 0x5a, sizeof(input));
    unsigned cases = 0;
    for(unsigned index = 0; index < 7; ++index)
    {
        const unsigned width = 1u << index;
        for(unsigned shift = 0; shift <= width; ++shift)
        {
            memset(memory + page - 128, 0xa5, 128); faulted = 0;
            if(!sigsetjmp(resume, 1)) { functions[index](memory, page - width + shift, input); }
            if((faulted != 0) != (shift != 0)) { printf("FAIL fault width=%u shift=%u\n", width, shift); return 3; }
            for(unsigned i = 0; i < 128; ++i)
            {
                unsigned char expected = !shift && i >= 128 - width ? 0x5a : 0xa5;
                if(memory[page - 128 + i] != expected)
                { printf("FAIL partial write width=%u shift=%u byte=%u\n", width, shift, i); return 4; }
            }
            ++cases;
        }
    }
    munmap(memory, page * 2);
    printf("PASS %u guarded store boundary/no-partial-write cases\n", cases);
    return 0;
}
