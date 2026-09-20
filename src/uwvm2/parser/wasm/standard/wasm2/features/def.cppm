module;

#include <concepts>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.parser.wasm.standard.wasm2.features:def;

import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.standard.wasm1p1.features;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "def.h"
