module;
#include "language_debug_features.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
export module uwvm2.uwvm.debugger:source_pretty_printer;
import fast_io;
import :source_dwarf_types;
import :source_dwarf_values;
import :source_dwarf_objects;
#if UWVM_DEBUG_LANGUAGE_CPP
import :source_pretty_coroutine;
#endif
import uwvm2.uwvm.debugger.source_pretty_registry;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_pretty_printer.h"
