module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <uwvm2/utils/macro/push_macros.h>
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#endif
export module uwvm2.uwvm.runtime.storage:builtin_wasip1_loader;
import fast_io;
import fast_io_crypto;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.imported.wasi.wasip1.local_imported;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "builtin_wasip1_loader.h"
