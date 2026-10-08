module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
# include <llvm/ADT/StringMap.h>
# include <llvm/BinaryFormat/Dwarf.h>
# include <llvm/DebugInfo/DWARF/DWARFContext.h>
# include <llvm/DebugInfo/DWARF/DWARFDie.h>
# include <llvm/DebugInfo/DWARF/DWARFDebugLine.h>
# include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
# include <llvm/DebugInfo/DWARF/DWARFUnit.h>
# include <llvm/Support/Error.h>
# include <llvm/Support/MemoryBuffer.h>
#endif
export module uwvm2.uwvm.debugger:source_dwarf_index;
import fast_io;
import :source_dwarf_types;
import :source_dwarf_variants;
import :source_dwarf_constants;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_dwarf_index.h"
