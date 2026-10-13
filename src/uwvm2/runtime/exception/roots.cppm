module;
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

export module uwvm2.runtime.exception.roots;
import uwvm2.runtime.exception.value;
import uwvm2.object.global;

#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "roots.h"
