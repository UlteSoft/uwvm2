// Test-only link shim for the CLI / loader / initializer probe. The probe
// exercises secure native-ingress rejection and does not expose host APIs.
// A full VM build links host_api.default.cpp instead of this translation unit.
#include <uwvm2/uwvm/wasm/type/preload_api.h>

namespace uwvm2::uwvm::wasm::type
{
    extern "C" uwvm_preload_host_api_v1 const* uwvm_get_preload_host_api_v1() noexcept
    { return nullptr; }
}
