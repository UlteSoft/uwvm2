module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.runtime.exception.value;
import fast_io;
import uwvm2.object.global;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#include "immutable_value.h"
#else
#include "value.h"
#endif
