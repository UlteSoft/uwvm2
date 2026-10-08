// PRIVATE source-only dual-mode adapter. No compiler/native qualification is inherited.
module;
#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

export module uwvm2.runtime.exception.pending_island;
import uwvm2.runtime.exception.value;
import uwvm2.runtime.exception.roots;
import uwvm2.object.global;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "pending_island.h"
