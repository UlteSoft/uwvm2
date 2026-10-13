module;
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
export module uwvm2.validation.standard.wasm3.retained_plan;
import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.standard;
import uwvm2.validation.error;
#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif
#include "retained_plan.h"
