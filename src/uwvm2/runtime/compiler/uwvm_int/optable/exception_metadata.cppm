module;
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:exception_metadata;
import fast_io;
import uwvm2.runtime.exception;
import uwvm2.uwvm.runtime.storage;
import :define;
import :exception;
import :exception_throw;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "exception_metadata.h"
