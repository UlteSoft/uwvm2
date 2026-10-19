#pragma once
// TU-scope includes; the separate adapter header is included inside Runtime lib
// AFTER its real resolve_wasip1_env_for_runtime_module_id implementation.
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <climits>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>
#if defined(__linux__)
# include <cerrno>
# include <csignal>
# include <fcntl.h>
# include <poll.h>
# include <pthread.h>
# include <sched.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/stat.h>
# include <sys/syscall.h>
# include <sys/timerfd.h>
# include <time.h>
# include <unistd.h>
#endif
#include <uwvm2/runtime/wasm_threads/thread_group.h>
#include <uwvm2/uwvm/runtime/initializer/threaded_child_world.h>
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
#include <uwvm2/runtime/compiler/shared/wasm_threads.h>
#include <uwvm2/runtime/wasm_threads/wait.h>
