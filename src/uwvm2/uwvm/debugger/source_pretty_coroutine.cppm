module;
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_pretty_coroutine;
import fast_io;
import :source_dwarf_types;
import :source_dwarf_values;
import :source_dwarf_objects;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_pretty_coroutine.h"
