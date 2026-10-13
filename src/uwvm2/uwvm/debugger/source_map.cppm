module;
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <algorithm>
#include <bit>
#include <climits>
#include <cerrno>
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
# include <fcntl.h>
#endif
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_map;
import fast_io;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_map.h"
