module;
#include <cstddef>
export module uwvm2test.table_access_capsule;
import uwvm2.validation.standard.wasm3;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
static_assert(::uwvm2::validation::standard::wasm3::is_table_access_event_opcode(0x26u));
