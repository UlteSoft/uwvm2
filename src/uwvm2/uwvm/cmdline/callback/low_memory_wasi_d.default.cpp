// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "wasip1_single_socket_udp_connect.h"
#include "wasip1_group_create.h"
#include "wasip1_group_add_module.h"
#include "wasip1_group_enable.h"
#include "wasip1_group_disable.h"
#include "wasip1_group_expose_host_api.h"
#include "wasip1_group_hide_host_api.h"
#include "wasip1_group_noinherit_system_environment.h"
#include "wasip1_group_inherit_system_environment.h"
#include "wasip1_group_disable_utf8_check.h"
#include "wasip1_group_enable_utf8_check.h"
#include "wasip1_group_trace.h"

extern "C" void const* uwvm_low_memory_callback_wasi_d_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
