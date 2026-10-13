// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "wasip1_group_set_argv0.h"
#include "wasip1_group_force_args.h"
#include "wasip1_group_set_fd_limit.h"
#include "wasip1_group_add_or_replace_environment.h"
#include "wasip1_group_delete_system_environment.h"
#include "wasip1_group_mount_dir.h"
#include "wasip1_group_socket_tcp_listen.h"
#include "wasip1_group_socket_tcp_connect.h"
#include "wasip1_group_socket_udp_bind.h"
#include "wasip1_group_socket_udp_connect.h"

extern "C" void const* uwvm_low_memory_callback_wasi_e_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
