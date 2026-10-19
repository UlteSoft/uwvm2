module;
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>
#include <new>
#include <string_view>
#include <utility>
#if defined(__linux__)
# include <fcntl.h>
# include <linux/keyctl.h>
# include <sys/random.h>
# include <sys/syscall.h>
# include <sys/prctl.h>
# include <sys/stat.h>
# include <sys/statfs.h>
# include <sys/sysmacros.h>
# include <unistd.h>
#endif
#include "runtime_image_anchor.h"
#include "runtime_origin.h"
export module uwvm2.runtime.llvm_jit_cache:self_executable;
import fast_io_crypto;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "self_executable.h"
#include "secure_file.h"

#include "kernel_origin.h"
#include "origin_auth.h"
