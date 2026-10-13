module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <memory>
#include <limits>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

export module uwvm2.runtime.compiler.uwvm_int.optable:gc;

import fast_io;
import uwvm2.utils.container;
import uwvm2.object;
import uwvm2.uwvm.runtime.storage;
import uwvm2.parser.wasm.standard.wasm1;
import :define;
import :stack;
import :wasm1p1;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "gc.h"
