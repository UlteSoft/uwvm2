// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "log_output.h"
#include "log_color.h"
#include "log_enable_warning.h"
#include "log_disable_warning.h"
#include "log_convert_warn_to_fatal.h"

extern "C" void const* uwvm_low_memory_callback_log_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
