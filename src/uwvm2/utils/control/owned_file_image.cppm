module;
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
# include <cerrno>
# include <fcntl.h>
#endif
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.utils.control:owned_file_image;
import fast_io;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "owned_file_image.h"
