// Built with --target=wasm32 -g -O0 -nostdlib --no-entry --export=_start.
typedef unsigned u32;
typedef unsigned long long u64;
struct iovec { void *data; u32 size; };
#define WASI(name) __attribute__((import_module("wasi_snapshot_preview1"), import_name(name)))
WASI("path_open") extern int path_open(u32,u32,const char*,u32,u32,u64,u64,u32,u32*);
WASI("fd_read") extern int fd_read(u32,struct iovec*,u32,u32*);
WASI("fd_write") extern int fd_write(u32,struct iovec*,u32,u32*);
volatile int phase, counter=7, error_code;
u32 file_fd, input_size;
char input_bytes[32];
__attribute__((noinline)) void leaf(int amount)
{
    counter += amount; // LEAF_STATEMENT
}
__attribute__((noinline)) int main(void)
{
    error_code=path_open(3,0,"open.txt",8,0,0x60006e,0,0,&file_fd);
    phase=1; // FILE_OPENED
    struct iovec in={input_bytes,sizeof(input_bytes)};
    error_code=fd_read(0,&in,1,&input_size);
    struct iovec out={"TUI_STDOUT\n",11}; u32 written;
    fd_write(1,&out,1,&written);
    out.data="TUI_STDERR\n";fd_write(2,&out,1,&written);
    out.data=input_bytes;out.size=input_size;fd_write(1,&out,1,&written);
    leaf(3); // ENTER_LEAF
    phase=2; // AFTER_LEAF
    return 0;
}
void _start(void) { (void)main(); }
