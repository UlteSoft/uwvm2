module;
#include <cstddef>
export module uwvm2.test.fused_typed_select;
import uwvm2.validation.standard.wasm3;
import uwvm2.runtime.compiler.shared.i32_dual_emission;
static_assert(::uwvm2::validation::standard::wasm3::typed_select_carrier_consistent(
    {::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i64},0x7eu));
