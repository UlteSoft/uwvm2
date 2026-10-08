// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "wasip1_global_socket_udp_bind.h"
#include "wasip1_global_socket_udp_connect.h"
#include "wasip1_module_common.h"
#include "wasip1_single_common.h"
#include "wasip1_group_common.h"
#include "wasip1_single_create.h"
#include "wasip1_single_enable.h"
#include "wasip1_single_disable.h"
#include "wasip1_single_expose_host_api.h"
#include "wasip1_single_hide_host_api.h"
#include "wasip1_single_noinherit_system_environment.h"
#include "wasip1_single_inherit_system_environment.h"

extern "C" void const* uwvm_low_memory_callback_wasi_b_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
