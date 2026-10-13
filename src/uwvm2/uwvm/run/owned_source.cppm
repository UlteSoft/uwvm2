module;
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2.uwvm.run:owned_source;
import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.control;
import uwvm2.runtime;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.wasm;
import uwvm2.uwvm.runtime.storage;
import uwvm2.uwvm.runtime.initializer;
import uwvm2.uwvm.runtime.runtime_mode;
import :loader;
import :retval;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "owned_source.h"
