module;
#include <cstddef>
#include <type_traits>
#include <llvm/Target/TargetMachine.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
export module uwvm2test.real_fused_i32_dual_sink;
import uwvm2.validation.standard.wasm3;
import uwvm2.validation.error;
import uwvm2.runtime.compiler.uwvm_int.optable;
import uwvm2.runtime.compiler.shared.i32_dual_emission;
using default_sink = ::uwvm2::validation::standard::wasm3::discard_fused_i32_operations;
static_assert(!default_sink::receives_fused_i32_operations);
static_assert(::std::is_empty_v<::uwvm2::validation::standard::wasm3::fused_i32_function_transaction<default_sink>>);
export auto actual_optional_dual_factory(
    ::uwvm2::runtime::compiler::shared::i32_dual_emission::module_type const& module,
    ::uwvm2::runtime::compiler::shared::i32_dual_emission::ring_options options,
    ::uwvm2::runtime::compiler::shared::i32_dual_emission::emission_policy policy,
    ::uwvm2::validation::error::code_validation_error_impl& error,
    ::uwvm2::runtime::compiler::shared::i32_dual_emission::feature_type const* features) UWVM_THROWS
{
    constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t option{.is_tail_call = false};
    return ::uwvm2::runtime::compiler::shared::i32_dual_emission::compile<option>(module, options, policy, error, features);
}
