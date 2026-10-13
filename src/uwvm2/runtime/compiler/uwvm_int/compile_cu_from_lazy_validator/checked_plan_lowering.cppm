module;
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
export module uwvm2.runtime.compiler.uwvm_int.compile_cu_from_lazy_validator:checked_plan_lowering;
import fast_io;
import uwvm2.utils.container;
import uwvm2.object.global;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.standard;
import uwvm2.uwvm.wasm.feature;
import uwvm2.validation.standard.wasm3;
import uwvm2.uwvm.runtime.storage;
import uwvm2.uwvm.runtime.checked_source;
import uwvm2.runtime;
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;
import uwvm2.runtime.compiler.uwvm_int.optable;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "checked_plan_lowering.h"
