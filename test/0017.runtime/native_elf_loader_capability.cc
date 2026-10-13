// The real linked RuntimeDyld ABI, independent of version/header macros.
// A link against an unpatched archive is a required negative control.
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <fast_io.h>
int main()
{
    if(::llvm::RuntimeDyld::getUWVMELFLoaderCapabilities()!=3u)
    { ::fast_io::io::perrln("ELF loader library capability mismatch");return 1; }
    ::fast_io::io::println("PASS real linked ELF loader capability ABI");
}
