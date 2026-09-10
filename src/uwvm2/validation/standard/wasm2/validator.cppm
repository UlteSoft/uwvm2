module;

#include <cstddef>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.validation.standard.wasm2:validator;

import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.parser.wasm.standard.wasm2.features;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm1;
import uwvm2.validation.standard.wasm1p1;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "validator.h"
