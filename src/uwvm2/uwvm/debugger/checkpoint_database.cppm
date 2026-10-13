module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:checkpoint_database;
import fast_io;
import fast_io_crypto;
import :checkpoint_state;
import :checkpoint_codec;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "checkpoint_database.h"
