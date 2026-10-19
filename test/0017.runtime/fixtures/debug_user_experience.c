// Actual DWARF + nested Wasm calls + WASIp1 output for the PTY UX audit.
typedef unsigned int u32;
struct iovec { char const* data; u32 size; };
struct Record { int count; float ratio; };
__attribute__((import_module("wasi_snapshot_preview1"), import_name("fd_write")))
extern u32 guest_write(u32, struct iovec const*, u32, u32*);
__attribute__((noinline)) int leaf(int value)
{
    volatile int counter = value;
    volatile struct Record record = {value, 1.5f};
    volatile int* pointer = &counter;
    counter += 1; // UX_LEAF_READY: stop after initialization, before increment
    return counter + (record.count - value) + (*pointer - counter);
}
__attribute__((noinline)) int outer(int value) { return leaf(value); }
__attribute__((noinline)) int future(void) { return 4; }
void _start(void)
{
    int result = outer(2);
    char bytes[2] = {(char)('0' + result), '\n'};
    struct iovec output = {bytes, 2}; u32 written;
    guest_write(1, &output, 1, &written);
}
