// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "wasi_disable_utf8_check.h"
#include "wasip1_global_trace.h"
#include "wasip1_global_expose_host_api.h"
#include "wasip1_global_disable.h"
#include "wasip1_global_set_fd_limit.h"
#include "wasip1_global_mount_dir.h"
#include "wasip1_global_set_argv0.h"
#include "wasip1_global_force_args.h"
#include "wasip1_global_delete_system_environment.h"
#include "wasip1_global_add_or_replace_environment.h"
#include "wasip1_global_socket_tcp_listen.h"
#include "wasip1_global_socket_tcp_connect.h"

extern "C" void const* uwvm_low_memory_callback_wasi_a_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
