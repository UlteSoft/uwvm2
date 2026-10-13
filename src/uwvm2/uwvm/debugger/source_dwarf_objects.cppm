module;
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_dwarf_objects;
import fast_io;
import :source_dwarf_types;
import :source_dwarf_query;
import :source_dwarf_values;
import :source_dwarf_variants;
import :source_frames;
import :source_type_declarators;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_dwarf_objects.h"
