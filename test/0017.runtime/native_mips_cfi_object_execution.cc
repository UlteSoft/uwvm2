// Execute the exact N64 object exported by debug_native_registered_cfi_mips64.
// This is a compiler/ABI oracle, not a Wasm debugger activation issuer.
#include <fast_io.h>
#include <cstdint>
#if !defined(__linux__) || !defined(__mips__) || __SIZEOF_POINTER__ != 8
# error This fixture requires MIPS64 Linux N64.
#endif
extern "C" ::std::uint64_t uwvm_cfi_execute();
extern "C" ::std::uint64_t uwvm_cfi_branch(::std::uint64_t);
int main()
{
    auto const result{uwvm_cfi_execute()};
    auto const early{uwvm_cfi_branch(0u)}, later{uwvm_cfi_branch(1u)};
    if(result!=66u || early!=0u || later!=66u)
    {
        ::fast_io::io::perrln("FAIL actual N64 emitted object: result=",result," early=",early," later=",later);
        return 1;
    }
    ::fast_io::io::println("PASS actual Linux N64 emitted object: stack-args=64 result=",result,
        " early=",early," later=",later," Wasm-caller-qualified=false finish-qualified=false");
}
