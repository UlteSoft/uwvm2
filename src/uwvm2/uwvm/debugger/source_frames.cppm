module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_frames;
import fast_io;
import :source_dwarf_types;
import :source_scope_path;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_frames.h"
