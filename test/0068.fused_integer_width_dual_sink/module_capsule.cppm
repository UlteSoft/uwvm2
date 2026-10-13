module;
#include <cstddef>
export module uwvm2test.integer_width_dual_capsule;
import uwvm2.runtime.compiler.shared.i32_dual_emission;
import uwvm2.validation.standard.wasm3.integer_width_event;
static_assert(::uwvm2::validation::standard::wasm3::is_integer_width_event_opcode(0xa7u));
