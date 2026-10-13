module;
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
export module uwvm2.uwvm.runtime.checked_source;
import fast_io;
import fast_io_crypto;
import uwvm2.utils.control;
import uwvm2.utils.container;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.standard;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.wasm.loader;
import uwvm2.uwvm.runtime.storage;
import uwvm2.runtime;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "parsed_integer_plan.h"
