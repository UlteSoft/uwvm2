// --target=wasm32 -g -O0 -nostdlib -Wl,--no-entry,--export=_start,--global-base=65536
// The only initialized data object starts at the explicit linker global base.
unsigned char memory_sample[16]={0x41,0x42,0x00,0xff,0x80,0,0,0,0x43,0x44,0x45,0x46,0,0,0,0};
struct iovec { void *data; unsigned size; };
__attribute__((import_module("wasi_snapshot_preview1"),import_name("fd_write")))
extern int fd_write(unsigned,struct iovec*,unsigned,unsigned*);
__attribute__((noinline)) int leaf(int value) { return value+3; }
__attribute__((noinline)) int main(void)
{
    volatile int a=7;
    a=leaf(a);
    struct iovec output;output.data=memory_sample;output.size=2;unsigned written;
    fd_write(1,&output,1,&written);
    return a;
}
void _start(void) { (void)main(); }
