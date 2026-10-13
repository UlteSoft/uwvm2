module;
#include <algorithm>
#include <limits>
#include <new>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_dwarf_query;
import fast_io;
import :source_dwarf_types;
import :source_scope_path;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_dwarf_query.h"
