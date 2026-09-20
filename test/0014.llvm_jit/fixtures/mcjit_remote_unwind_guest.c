/* Execute trusted test-only relocated bytes, registering CFI with the target's
 * actual libgcc unwinder. This must not be exposed as an untrusted object loader.
 * MAP_FIXED_NOREPLACE and bounds reject collisions, never overwrite guest maps.
 * Non-PIE links make the externally resolved callback address reproducible. */
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <unwind.h>
#include <signal.h>
#include <setjmp.h>

extern void __register_frame(void const*);
extern void __deregister_frame(void const*);
static struct { uintptr_t address; size_t size; } code[32];
static unsigned code_count, frames_seen, captures;
struct { uint32_t mode, padding; uint64_t float_bits; } uwvm_remote_state;
static sigjmp_buf escape;
static volatile sig_atomic_t caught;

#ifdef UWVM_REMOTE_LINKED_CONTROL
/* Positive control: let the target's ordinary linker register the SAME object
 * instead of RuntimeDyld and __register_frame. This separates loader/CFI bugs
 * from guest libgcc/QEMU signal-frame or this diagnostic handler's limitations.
 * The driver obtains exact function sizes from the object's symbol table. */
extern void uwvm_remote_recursive(unsigned);
extern void uwvm_remote_entry(void);
static uintptr_t code_address(void (*function)(void))
{
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    /* ELFv1 C function pointers name descriptors, not instruction addresses. */
    return *(uintptr_t const*)(void*)function;
#else
    return (uintptr_t)function;
#endif
}
#endif

static _Unwind_Reason_Code inspect_frame(struct _Unwind_Context* context, void* ignored)
{
    (void)ignored;
    uintptr_t ip = (uintptr_t)_Unwind_GetIP(context);
    printf("IP %llx\n", (unsigned long long)ip);
    for(unsigned i=0; i<code_count; ++i)
    {
        /* Return IP may point just after a call at the end of its function. */
        if(ip > code[i].address && ip <= code[i].address + code[i].size) { ++frames_seen; break; }
    }
    return _URC_NO_REASON;
}

__attribute__((noinline)) void uwvm_host_capture(void)
{
    ++captures;
    _Unwind_Backtrace(inspect_frame, NULL);
}

static void trap_handler(int signal_number, siginfo_t* info, void* context)
{
    (void)info; (void)context;
    caught=signal_number;
    // Diagnostic-only: libgcc/printf from this isolated single-threaded handler
    // are not a production async-signal-safety contract or ROS trap handling.
    printf("caught signal=%d address=%p\n", signal_number, info->si_addr);
    uwvm_host_capture();
    siglongjmp(escape, 1);
}

int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    /* Preserve evidence when a diagnostic unwinder itself faults. Buffered
     * stdout would otherwise hide all frames before the second signal. */
    setvbuf(stdout, NULL, _IONBF, 0);
    unsigned mode=(unsigned)strtoul(argv[2], NULL, 10);
    if(mode > 3) { return 2; }
    if(argc == 4 && (mode != 3 || strcmp(argv[3], "quiet-nan"))) { return 2; }
    uwvm_remote_state.mode=mode;
    /* This is target-native floating-point IR, not a Wasm bit-reinterpretation
     * oracle. Legacy MIPS/PA-RISC/SH reverse the quiet-bit convention: the
     * common 0x7ff0000000000001 is a quiet NaN there. Construct an actual sNaN
     * for the default probe, and optionally repeat with a qNaN. Both must take
     * the conversion guard's trap path and preserve the same physical frames.
     * Do not change Wasm's fixed bit encoding to follow this native fixture. */
#if defined(__hppa__) || defined(__hppa) || defined(__sh__) || (defined(__mips__) && !defined(__mips_nan2008))
    uint64_t signaling_nan=UINT64_C(0x7ff8000000000001);
#else
    uint64_t signaling_nan=UINT64_C(0x7ff0000000000001);
#endif
    uwvm_remote_state.float_bits=signaling_nan ^ (argc == 4 ? UINT64_C(0x0008000000000000) : 0);
    if(mode == 3) { printf("native_nan=%s bits=%016llx\n", argc == 4 ? "quiet" : "signaling",
                          (unsigned long long)uwvm_remote_state.float_bits); }
    long page = sysconf(_SC_PAGESIZE);
    if(page <= 0 || page > 65536 || (page & (page-1))) { return 4; }
    uintptr_t entry = 0;
    void* frames[32]; unsigned frame_count = 0;
#ifdef UWVM_REMOTE_LINKED_CONTROL
    entry=(uintptr_t)&uwvm_remote_entry;
    code[0].address=code_address((void(*)(void))&uwvm_remote_recursive);
    code[0].size=UWVM_REMOTE_RECURSIVE_SIZE;
    code[1].address=code_address(&uwvm_remote_entry);
    code[1].size=UWVM_REMOTE_ENTRY_SIZE;
    code_count=2;
#else
    if(chdir(argv[1])) { return 2; }
    FILE* manifest = fopen("manifest.txt", "r");
    if(!manifest) { return 3; }
    char line[256];
    while(fgets(line, sizeof(line), manifest))
    {
        unsigned long long address=0, size=0; unsigned is_code=0;
        char filename[64];
        if(sscanf(line, "E %llx", &address) == 1) { entry=(uintptr_t)address; continue; }
        if(sscanf(line, "S %llx %llx %x %63s", &address, &size, &is_code, filename) == 4)
        {
            if(address < 0x20000000 || address >= 0x20200000 || (address & 65535) || size > 32768 ||
               code_count >= 32 || strstr(filename, "..") || strchr(filename, '/')) { return 5; }
            size_t mapped = ((size_t)size + (size_t)page - 1) & ~((size_t)page - 1);
            if(!mapped) { mapped=(size_t)page; }
            void* memory = mmap((void*)(uintptr_t)address, mapped, PROT_READ|PROT_WRITE,
                                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);
            if(memory == MAP_FAILED) { perror("mmap"); return 6; }
            FILE* file = fopen(filename, "rb");
            if(!file || fread(memory, 1, (size_t)size, file) != size) { return 7; }
            fclose(file);
            __builtin___clear_cache((char*)memory, (char*)memory + size);
            if(mprotect(memory, mapped, PROT_READ|(is_code ? PROT_EXEC : 0))) { return 8; }
            if(is_code) { code[code_count].address=(uintptr_t)address; code[code_count++].size=(size_t)size; }
            continue;
        }
        if(sscanf(line, "F %llx %llx", &address, &size) == 2)
        {
            if(frame_count >= 32 || !size || address < 0x20000000 || address+size > 0x20200000) { return 9; }
            frames[frame_count++]=(void*)(uintptr_t)address;
            continue;
        }
        return 10;
    }
    fclose(manifest);
    if(!entry || !frame_count || !code_count) { return 11; }
#endif
    if(mmap((void*)(uintptr_t)0x30000000, (size_t)page, PROT_NONE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED) { return 13; }
    stack_t alternate={0};
    alternate.ss_size=65536;
    alternate.ss_sp=malloc(alternate.ss_size);
    if(!alternate.ss_sp || sigaltstack(&alternate, NULL)) { return 14; }
    struct sigaction action={0};
    action.sa_sigaction=trap_handler;
    action.sa_flags=SA_SIGINFO;
#ifndef UWVM_REMOTE_NO_ALTSTACK
    action.sa_flags|=SA_ONSTACK;
#endif
    sigemptyset(&action.sa_mask);
    if(sigaction(SIGILL, &action, NULL) || sigaction(SIGSEGV, &action, NULL) || sigaction(SIGTRAP, &action, NULL)) { return 15; }
    for(unsigned i=0; i<frame_count; ++i) { __register_frame(frames[i]); }
    if(sigsetjmp(escape, 1) == 0)
    {
#ifdef UWVM_REMOTE_INDIRECT_BRIDGE
        /* Target-native C function-pointer ABI, including ELFv1 descriptors
         * and MIPS t9/GP rules, is distinct from direct external-symbol calls. */
        ((void(*)(void*, void(*)(void)))entry)(&uwvm_remote_state, uwvm_host_capture);
#else
        ((void(*)(void))entry)();
#endif
    }
    for(unsigned i=0; i<frame_count; ++i) { __deregister_frame(frames[i]); }
    /* depth 8..0 gives nine recursive frames plus the explicit entry frame. */
    printf("mode=%u signal=%d captures=%u dynamic_frames=%u expected=10\n", mode, (int)caught, captures, frames_seen);
    int signal_ok=mode == 0 ? caught == 0 : mode == 2 ? caught == SIGSEGV : (caught == SIGILL || caught == SIGTRAP);
    return captures == 1 && frames_seen == 10 && signal_ok ? 0 : 12;
}
