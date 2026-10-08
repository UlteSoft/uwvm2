module;
#include <uwvm2/utils/macro/push_macros.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#endif
export module uwvm2.utils.control:session;
import fast_io;
import :protocol;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "session.h"
