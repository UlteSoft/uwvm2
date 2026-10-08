#include <uwvm2/uwvm/crtmain/low_memory_entry.h>

// The callback definitions are emitted by the split command-line callback
// objects; this translation unit owns only the execution driver.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/run/impl.h>

namespace uwvm2::uwvm::crtmain::low_memory
{
    int run() noexcept { return ::uwvm2::uwvm::run::run(); }
}
