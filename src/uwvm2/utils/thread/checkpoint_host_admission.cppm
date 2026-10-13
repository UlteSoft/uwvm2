module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# include <mutex>
#endif
export module uwvm2.utils.thread:checkpoint_host_admission;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "checkpoint_host_admission.h"
