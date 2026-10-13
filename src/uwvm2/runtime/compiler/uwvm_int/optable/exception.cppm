module;
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.runtime.compiler.uwvm_int.optable:exception;
import fast_io;
import uwvm2.utils.container;
import uwvm2.runtime.exception;
import :define;
import :call;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "exception.h"
