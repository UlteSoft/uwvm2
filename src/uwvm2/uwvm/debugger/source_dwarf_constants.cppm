module;
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
# include <llvm/BinaryFormat/Dwarf.h>
# include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#endif
export module uwvm2.uwvm.debugger:source_dwarf_constants;
import fast_io;
import :source_dwarf_types;
import :source_dwarf_variants;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "source_dwarf_constants.h"
