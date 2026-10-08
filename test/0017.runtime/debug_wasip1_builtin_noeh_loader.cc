// Genuine ordinary typed builtin insertion, independently of debug C++ EH.
// This is not a paused/capture/root/provider-lifetime authority test.
#include <uwvm2/uwvm/runtime/storage/builtin_wasip1_loader.h>
#include <fast_io.h>
#include <string_view>
namespace full=::uwvm2::uwvm::runtime::full;
namespace storage=::uwvm2::uwvm::wasm::storage;
#if defined(__cpp_exceptions)
inline constexpr bool language_cpp_exceptions{true};
#else
inline constexpr bool language_cpp_exceptions{false};
#endif
int main(int argc,char** argv)
{
    if(argc==2)
    {
        if(::std::string_view{argv[1]}!="--require-no-exceptions" || language_cpp_exceptions) { return 90; }
    }
    else if(argc!=1) { return 91; }
    // Fresh actual process; no input-image, typed registry or external bool
    // is fabricated to enable provenance. Ordinary loader insertion is enough.
    if(storage::active_execute_wasm().has_owned_source_image() || !storage::preload_local_imported.empty()) { return 1; }
    if(!full::builtin_wasip1_loader_identity::append_actual_builtin_for_native_loader()) { return 2; }
    if(storage::preload_local_imported.size()!=1u) { return 3; }
    // [actual factory native vector0..1][checked ordinal0] end
    // [safe] extent checked BEFORE real wrapper read; implementation not called.
    if(storage::preload_local_imported.index_unchecked(0u).ptr==nullptr) { return 4; }
    ::fast_io::io::println("WASIp1 ordinary typed builtin insertion PASS cpp-eh=",language_cpp_exceptions ? 1u:0u,
        " debug-provenance-assertion=not-tested native-capture=false");
}
