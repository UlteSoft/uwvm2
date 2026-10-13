module;
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:checkpoint_envelope;
import fast_io;
import fast_io_crypto;
import uwvm2.utils.control;
import :checkpoint_binding;
import :checkpoint_codec;
import :checkpoint_state_identity;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "checkpoint_envelope.h"
