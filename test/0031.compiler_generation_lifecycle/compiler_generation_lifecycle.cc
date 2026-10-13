#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/uwvm/runtime/initializer/init.h>
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
#include <uwvm2/uwvm/wasm/loader/wasm_file.h>
namespace
{
    namespace lib = ::uwvm2::runtime::lib;
    namespace full = ::uwvm2::uwvm::runtime::full;
    namespace wasm = ::uwvm2::uwvm::wasm;
    namespace runtime = ::uwvm2::uwvm::runtime;
    void require(bool condition, unsigned line)
    {
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "compiler_generation_lifecycle: FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    bool same_selected_owner(full::full_source_instance::owner const& source)
    {
        auto selected{full::selected_full_source_owner_pin()};
        return selected && source && selected.get() == source.get() &&
            !selected.owner_before(source) && !source.owner_before(selected);
    }
    auto enabled_features()
    {
        wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t features{};
        auto& p{wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        p.disable_multi_value = false; p.disable_reference_types = false;
        p.disable_table_instructions = false; p.disable_multiple_tables = false;
        p.disable_bulk_memory = false; p.disable_sign_extension = false;
        p.disable_nontrapping_float_to_int = false; p.disable_simd = false;
        p.controllable_allow_multi_result_vector = false; p.controllable_allow_multi_table = false;
        p.disable_gc = false; p.disable_exceptions = false; p.disable_function_references = false;
        p.disable_memory64 = false; p.disable_table64 = false; p.disable_multi_memory = false;
        p.disable_tail_call = false; p.disable_extended_const = false; p.disable_table_initializer = false;
        return features;
    }
    full::full_source_instance::owner load_actual_source(char const* path)
    {
        lib::reset_runtime_state_host_api();
        // Actual runtime reset has returned after draining workers/entries.
        // Native loader/configuration stays externally serialized in this test.
        full::retire_selected_full_source_after_drain();
        wasm::storage::all_module.clear(); wasm::storage::all_module_export.clear();
        wasm::storage::preloaded_wasm.clear(); wasm::storage::preload_local_imported.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        wasm::storage::weak_symbol.clear();
#endif
        ::uwvm2::uwvm::io::show_verbose = false; ::uwvm2::uwvm::io::show_depend_warning = false;
        auto source{full::full_source_instance::create_unparsed(
            ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))),
            ::fast_io::u8concat_std(u8"compiler-generation-actual-source"))};
        REQUIRE(full::select_unparsed_full_source_after_drain(source));
        wasm::type::wasm_parameter_t parameters{}; parameters.binfmt1_para = enabled_features();
        REQUIRE(wasm::loader::load_wasm_file(source->file_for_native_initialization(), source->owned_file_name(),
            source->owned_rename(), parameters) == wasm::loader::load_wasm_file_rtl::ok);
        REQUIRE(wasm::loader::construct_all_module_and_check_duplicate_module() == wasm::loader::load_and_check_modules_rtl::ok);
        REQUIRE(wasm::loader::check_import_exist_and_detect_cycles() == wasm::loader::load_and_check_modules_rtl::ok);
        runtime::initializer::initialize_runtime(true);
        REQUIRE(source->seal_actual_initializer());
        return source;
    }
}
int main(int argc, char** argv)
{
    if(argc != 2) { return 64; }
    auto source{load_actual_source(argv[1])};
    REQUIRE(source && same_selected_owner(source) && source->registry().size() == 1uz);
    // [strong source -> actual initialized registry module]
    // [safe] initializer seal proves membership; the source remains owned here.
    // ^^ module: metadata borrow, never an executable/admission capability.
    auto const* module{source->initialized_main_module()};
    REQUIRE(module != nullptr && module->local_defined_function_vec_storage.size() == 1uz);
    auto const& local{module->local_defined_function_vec_storage.index_unchecked(0uz)};
    REQUIRE(local.wasm_code_ptr != nullptr && local.function_type_ptr != nullptr);
    auto const* code_identity{local.wasm_code_ptr};
    auto const* type_identity{local.function_type_ptr};
    ::std::weak_ptr<full::full_source_instance const> weak_source{source};
    auto const first_generation{lib::observe_compiler_runtime_generation_host_api()};
    REQUIRE(first_generation != 0u);
    lib::reset_runtime_state_host_api();
    auto const second_generation{lib::observe_compiler_runtime_generation_host_api()};
    REQUIRE(second_generation > first_generation && same_selected_owner(source) && !weak_source.expired());
    // Standalone reset intentionally preserves selected parsing source ownership.
    // The source pin permits metadata observation/destruction, not guest entry.
    REQUIRE(source->initialized_main_module() == module && source->initialized_from_actual_state());
    REQUIRE(module->local_defined_function_vec_storage.index_unchecked(0uz).wasm_code_ptr == code_identity &&
            module->local_defined_function_vec_storage.index_unchecked(0uz).function_type_ptr == type_identity);
    lib::reset_runtime_state_host_api();
    auto const third_generation{lib::observe_compiler_runtime_generation_host_api()};
    REQUIRE(third_generation > second_generation && same_selected_owner(source) && !weak_source.expired());
    full::retire_selected_full_source_after_drain();
    REQUIRE(!full::selected_full_source_owner_pin() && !weak_source.expired());
    // Every borrowed module/code/type identity above is now retired. No code
    // below reads or forms a pointer from that metadata after the last owner drops.
    source.reset();
    REQUIRE(weak_source.expired());
    lib::reset_runtime_state_host_api();
    auto const fourth_generation{lib::observe_compiler_runtime_generation_host_api()};
    REQUIRE(fourth_generation > third_generation && !full::selected_full_source_owner_pin());
    ::fast_io::print(::fast_io::out(),
        "COMPILER_GENERATION actual_reset_counter=1 selected_owner_retained=1 original_metadata_retained=1",
        " explicit_drained_source_retirement=1 final_weak_expired=1 empty_reset_counter=1 concurrent_reset_qualified=0",
        " first=", ::fast_io::mnp::dec(first_generation), " second=", ::fast_io::mnp::dec(second_generation),
        " third=", ::fast_io::mnp::dec(third_generation), " fourth=", ::fast_io::mnp::dec(fourth_generation), "\n");
    return 0;
}
