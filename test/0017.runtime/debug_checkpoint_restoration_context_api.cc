// Actual private API/type boundary and preserved ordinary helper smoke ONLY.
// Never defines a fake transaction friend or source/restore credential.
#include <memory>
#include <type_traits>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/initializer/init.h>

namespace in = ::uwvm2::uwvm::runtime::initializer;
namespace st = ::uwvm2::uwvm::runtime::storage;
namespace full = ::uwvm2::uwvm::runtime::full;
using ordinary = in::details::initialization_context<in::details::initialization_purpose::ordinary>;
using unpublished = in::details::initialization_context<in::details::initialization_purpose::unpublished_restore>;
template<typename Context>
concept has_public_original_source_copy = requires(full::full_source_instance::owner const& source)
{ Context::prepare_owned_original_source(source, ::std::uint_least64_t{1u}, ::std::size_t{1024u}); };
static_assert(!has_public_original_source_copy<in::restoration_context>);
template<typename Context>
concept has_public_compiler_module_owner = requires(Context& context, st::wasm_module_storage_t const* module)
{ context.prepare_compiler_module_owner(module); };
static_assert(!has_public_compiler_module_owner<in::restoration_context>);
template<typename Context>
concept has_public_resource_roster = requires(Context& context)
{ context.begin_resource_fixups(::std::size_t{1u}, ::std::size_t{1024u}); context.all_actual_resource_records_filled(); };
static_assert(!has_public_resource_roster<in::restoration_context>);
template<typename Context>
concept has_public_saved_data_fill = requires(Context& context, st::local_defined_data_storage_t* data)
{ context.stage_saved_data_segment(data, false, ::std::span<::std::byte const>{}); };
static_assert(!has_public_saved_data_fill<in::restoration_context>);
template<typename Context>
concept has_public_saved_element_fill = requires(Context& context, st::local_defined_element_storage_t* element)
{ context.stage_saved_element_segment(element, true,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{}, ::std::span<st::gc_reference const>{}); };
static_assert(!has_public_saved_element_fill<in::restoration_context>);

static_assert(!::std::is_default_constructible_v<in::staged_compiler_module_owner>);
static_assert(!::std::is_constructible_v<in::staged_compiler_module_owner,
    full::full_source_instance::owner, st::wasm_module_storage_t const*, ::uwvm2::uwvm::wasm::type::wasm_file_t const*>);
static_assert(::std::is_copy_constructible_v<in::staged_compiler_module_owner>);
static_assert(!::std::is_copy_assignable_v<in::staged_compiler_module_owner>);
static_assert(::std::is_same_v<decltype(::std::declval<in::staged_compiler_module_owner const&>().source()),
    full::full_source_instance::owner const&>);
static_assert(::std::is_same_v<decltype(::std::declval<in::staged_compiler_module_owner const&>().module()),
    st::wasm_module_storage_t const*>);
static_assert(::std::is_same_v<decltype(::std::declval<in::staged_compiler_module_owner const&>().file()),
    ::uwvm2::uwvm::wasm::type::wasm_file_t const*>);

static_assert(::std::is_empty_v<ordinary>);
static_assert(::std::is_default_constructible_v<ordinary>);
static_assert(!::std::is_default_constructible_v<unpublished>);
static_assert(!::std::is_copy_constructible_v<unpublished>);
static_assert(!::std::is_constructible_v<in::restoration_context, full::full_source_instance::mutable_owner, st::checkpoint_gc_staging&>);
static_assert(!::std::is_copy_constructible_v<in::restoration_context>);
static_assert(!::std::is_move_constructible_v<in::restoration_context>);
static_assert(::std::is_same_v<decltype(&in::details::linked_gc_function_reference_type_matches), st::gc_function_type_match_callback>);
static_assert(::std::is_same_v<decltype(&in::initialize_runtime), void(*)(bool) noexcept>);

static void require(bool passed)
{
    if(!passed) { ::fast_io::io::perrln("restoration context API/ordinary smoke FAIL"); ::fast_io::fast_terminate(); }
}
int main()
{
    ordinary world{};
    require(::std::addressof(world.modules()) == ::std::addressof(st::active_runtime_registry()));
    require(::std::addressof(world.declarations()) == ::std::addressof(::uwvm2::uwvm::wasm::storage::all_module));
    require(::std::addressof(world.exports()) == ::std::addressof(::uwvm2::uwvm::wasm::storage::all_module_export));
    require(::std::addressof(world.current_module()) == ::std::addressof(in::details::current_initializing_module_name));
    require(::std::addressof(world.alias_sanity()) == ::std::addressof(in::details::import_alias_sanity_checked));
    require(::std::addressof(world.import_reset_policy()) == ::std::addressof(::uwvm2::uwvm::wasm::storage::configured_module_import_reset));
    require(::std::addressof(world.memory_limit_policy()) == ::std::addressof(::uwvm2::uwvm::wasm::storage::configured_module_memory_limit));
    st::gc_function_type_match_callback const actual{&in::details::linked_gc_function_reference_type_matches};
    require(!actual({}, nullptr, 0u)); // Native malformed input retained, no staged sentinel in ordinary callback.
    ::fast_io::io::println("restoration context API + ordinary helper smoke PASS; fresh VM/restore=false");
}
