// Source-only selection/signature witness, not a runtime activation credential.
// Compile with the same qualified LLVM-full textual header closure as product.
// No wrapper is called; all checks use the actual product bridge declarations.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate.h>
#include <fast_io.h>
#include <type_traits>
#include <cstdint>
#if !defined(UWVM_USE_LLVM_JIT) && !defined(UWVM_USE_DEFAULT_JIT)
# error "debug_activation_bridge_selection requires a qualified LLVM-full build"
#endif
namespace selection = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace native = uwvm2::runtime::lib::details;
namespace gc = uwvm2::runtime::gc;
namespace fixtures
{
    int throwing(int value) { return value; }
    ::std::uint64_t nonthrowing(::std::uint64_t value) noexcept { return value; }
}
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_activation_enter_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_activation_leave_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_safe_point_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_host_enter_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_host_leave_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<native::llvm_jit_debug_raw_target_abi_bridge>);
static_assert(selection::llvm_jit_debug_control_bridge<uwvm2::runtime::lib::llvm_jit_push_call_stack_frame>);
static_assert(selection::llvm_jit_debug_control_bridge<uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>);
static_assert(selection::llvm_jit_debug_control_bridge<gc::uwvm_gc_root_frame_enter_checked_abi>);
static_assert(selection::llvm_jit_debug_control_bridge<gc::uwvm_gc_root_frame_leave_checked_abi>);
static_assert(!selection::llvm_jit_debug_control_bridge<native::llvm_jit_call_raw_from_generated_wasm_abi_bridge>);
static_assert(!selection::llvm_jit_debug_control_bridge<native::llvm_jit_throw_numeric_abi_bridge>);
static_assert(!selection::llvm_jit_debug_control_bridge<fixtures::throwing>);
static_assert(!selection::llvm_jit_debug_control_bridge<fixtures::nonthrowing>);
static_assert(::std::is_same_v<decltype(&selection::llvm_jit_debug_host_bridge_wrapper<fixtures::throwing>::invoke), decltype(&fixtures::throwing)>);
static_assert(::std::is_same_v<decltype(&selection::llvm_jit_debug_host_bridge_wrapper<fixtures::nonthrowing>::invoke), decltype(&fixtures::nonthrowing)>);
int main()
{
    ::fast_io::io::println("debug_activation_bridge_selection: PASS strict control leaves and exact scalar/noexcept signatures");
}
