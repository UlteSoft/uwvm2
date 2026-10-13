// Genuine parser -> linked initializer(defer active segments) -> production run graph.
// No synthetic module/type/reference metadata and no preliminary body validator.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/run/run.h>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace lib = ::uwvm2::runtime::lib;
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
    namespace wasm = ::uwvm2::uwvm::wasm;
    namespace runtime = ::uwvm2::uwvm::runtime;
    using bytes = strict::byte_vec;
    constexpr auto consumer_name{u8"full-admission-consumer"};
    constexpr auto provider_name{u8"full-admission-provider"};

    void require(bool value, unsigned line) noexcept
    {
        if(!value)
        {
            ::fast_io::print(::fast_io::err(), "full_graph_effects: FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    bytes read_file(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        bytes source;
        source.resize(input.size());
        // [actual mapped file, size) / [actual owning source, same size)
        // [safe                                                     ] equal checked lengths bound this copy.
        ::std::memcpy(source.data(), input.data(), input.size());
        return source;
    }
    auto features()
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        auto& p{wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(out)};
        p.disable_gc = false; p.disable_function_references = false;
        p.disable_exceptions = false; p.disable_extended_const = false;
        p.disable_table_initializer = false; p.disable_tail_call = false;
        p.disable_memory64 = false; p.disable_table64 = false;
        p.disable_multi_memory = false; p.disable_threads = false;
        return out;
    }
    auto parse(bytes const& source, ::uwvm2::utils::container::u8string_view name,
        strict::wasm_feature_parameter_t const& policy)
    {
        REQUIRE(source.size() >= 8uz && source.size() <= 65536uz);
        ::uwvm2::parser::wasm::base::error_impl error{};
        // [owning source array ... source.size()] section_end
        // [safe                                ] actual live array has this checked length.
        //  ^^ Both parser endpoints borrow the same owner; only end advances by its size.
        auto const begin{source.data()};
        auto const end{begin + source.size()};
        auto parsed{wasm::feature::binfmt_ver1_handler(begin, end, error, policy)};
        wasm::type::wasm_file_t out{1u};
        out.file_name = u8"full-admission.wasm";
        out.module_name = name;
        out.wasm_parameter.binfmt1_para = policy;
        out.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(parsed);
        return out;
    }
    void initialize_deferred(bytes const& source, bytes const& provider)
    {
        lib::reset_runtime_state_host_api();
        wasm::storage::all_module.clear(); wasm::storage::all_module_export.clear();
        wasm::storage::preloaded_wasm.clear(); wasm::storage::preload_local_imported.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        wasm::storage::weak_symbol.clear();
#endif
        auto enabled{features()};
        wasm::storage::preloaded_wasm.emplace_back(parse(provider, provider_name, enabled));
        wasm::storage::execute_wasm = parse(source, consumer_name, enabled);
        REQUIRE(wasm::loader::construct_all_module_and_check_duplicate_module() == wasm::loader::load_and_check_modules_rtl::ok);
        REQUIRE(wasm::loader::check_import_exist_and_detect_cycles() == wasm::loader::load_and_check_modules_rtl::ok);
        // Real allocate/link/global initialization, but NO active data/element writes.
        runtime::initializer::initialize_runtime(true);
        auto const& registry{runtime::storage::active_runtime_registry()};
        auto const c{registry.find(consumer_name)};
        auto const p{registry.find(provider_name)};
        REQUIRE(c != registry.end() && p != registry.end());
        REQUIRE(c->second.local_defined_function_vec_storage.size() == 3uz);
        REQUIRE(c->second.imported_memory_vec_storage.size() == 1uz && c->second.imported_table_vec_storage.size() == 1uz);
        REQUIRE(p->second.local_defined_memory_vec_storage.size() == 1uz && p->second.local_defined_table_vec_storage.size() == 1uz);
        // [actual registry-owned vectors][checked sole import/provider slots]
        // [safe                                                          ] no registry/vector changes follow these borrows.
        auto const& memory_import{c->second.imported_memory_vec_storage.index_unchecked(0uz)};
        auto const& table_import{c->second.imported_table_vec_storage.index_unchecked(0uz)};
        REQUIRE(memory_import.link_kind == runtime::storage::imported_memory_storage_t::imported_memory_link_kind::defined);
        REQUIRE(table_import.link_kind == runtime::storage::imported_table_storage_t::imported_table_link_kind::defined);
        REQUIRE(memory_import.target.defined_ptr == ::std::addressof(p->second.local_defined_memory_vec_storage.index_unchecked(0uz)));
        REQUIRE(table_import.target.defined_ptr == ::std::addressof(p->second.local_defined_table_vec_storage.index_unchecked(0uz)));
    }
    void snapshot(char const* phase, bool expect_segments, bool expect_start,
        bool expect_provider_segments = false, bool expect_provider_start = false)
    {
        auto const& registry{runtime::storage::active_runtime_registry()};
        auto const c{registry.find(consumer_name)};
        auto const p{registry.find(provider_name)};
        REQUIRE(c != registry.end() && p != registry.end());
        REQUIRE(p->second.local_defined_memory_vec_storage.size() == 1uz);
        REQUIRE(p->second.local_defined_table_vec_storage.size() == 1uz);
        REQUIRE(c->second.local_defined_function_vec_storage.size() == 3uz);
        // [actual provider vectors ... end)
        // [safe                           ] checked size one proves slot zero.
        auto const& memory{p->second.local_defined_memory_vec_storage.index_unchecked(0uz).memory};
        auto const& table{p->second.local_defined_table_vec_storage.index_unchecked(0uz)};
        REQUIRE(memory.memory_begin != nullptr && memory.memory_length >= 5uz && table.elems.size() == 1uz);
        // [actual linear-memory allocation ... byte 0,1,2,3,4 ... memory_length)
        // [safe                                                          ] checked live range covers all five observations.
        auto const a{::std::to_integer<unsigned>(memory.memory_begin[0uz])};
        auto const b{::std::to_integer<unsigned>(memory.memory_begin[1uz])};
        auto const started{::std::to_integer<unsigned>(memory.memory_begin[2uz])};
        auto const provider_started{::std::to_integer<unsigned>(memory.memory_begin[3uz])};
        auto const provider_data{::std::to_integer<unsigned>(memory.memory_begin[4uz])};
        // [actual live table elements][sole checked slot] table_end
        // [safe                                                   ] no pointer advances; each union is read only for its kind.
        auto const& element{table.elems.index_unchecked(0uz)};
        bool const table_set{element.type == runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined &&
            element.storage.defined_ptr == ::std::addressof(c->second.local_defined_function_vec_storage.index_unchecked(1uz))};
        bool const table_null{element.type == runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported &&
            element.storage.imported_ptr == nullptr};
        REQUIRE(a == (expect_segments ? 165u : 0u) && b == (expect_segments ? 90u : 0u));
        bool table_seed{};
        if(expect_provider_segments)
        {
            REQUIRE(p->second.local_defined_function_vec_storage.size() == 2uz);
            // [actual provider functions][checked function zero] function_end
            // [safe                                                      ] kind selects the initialized union member.
            table_seed = element.type == runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined &&
                element.storage.defined_ptr == ::std::addressof(p->second.local_defined_function_vec_storage.index_unchecked(0uz));
        }
        REQUIRE(expect_segments ? table_set : expect_provider_segments ? table_seed : table_null);
        REQUIRE(provider_started == (expect_provider_start ? 113u : 0u));
        REQUIRE(provider_data == (expect_provider_segments ? 195u : 0u));
        REQUIRE(started == (expect_start ? 158u : 0u));
        ::fast_io::print(::fast_io::out(), "FULL_GRAPH phase=", ::fast_io::mnp::os_c_str(phase),
            " imported_memory0=", ::fast_io::mnp::dec(a), " imported_memory1=", ::fast_io::mnp::dec(b),
            " imported_table_replacement=", ::fast_io::mnp::dec(table_set),
            " real_guest_start_marker=", ::fast_io::mnp::dec(started),
            " provider_seed=", ::fast_io::mnp::dec(table_seed),
            " provider_segment_marker=", ::fast_io::mnp::dec(provider_data),
            " provider_start_marker=", ::fast_io::mnp::dec(provider_started), "\n");
    }
    struct runtime_owner
    {
        ~runtime_owner() { lib::reset_runtime_state_host_api(); }
    };
}
int main(int argc, char** argv)
{
    if(argc != 4 && argc != 5) { return 2; }
    auto const selected{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])}};
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    if(selected == "int")
    {
#if !defined(UWVM_DISABLE_INT)
        mode::global_runtime_compiler = mode::runtime_compiler_t::uwvm_interpreter_only;
#else
        return 3;
#endif
    }
    else if(selected == "llvm")
    {
#if !defined(UWVM_DISABLE_JIT)
        mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
        mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
#else
        return 3;
#endif
    }
    else if(selected == "tiered")
    {
#if !defined(UWVM_DISABLE_INT) && !defined(UWVM_DISABLE_JIT)
        mode::global_runtime_compiler = mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
#else
        return 3;
#endif
    }
    else { return 2; }
    mode::global_runtime_compile_threads = 0;
    mode::runtime_compile_threads_existed = true;
    auto const source{read_file(argv[2])};
    auto const provider{read_file(argv[3])};
    // [actual original source owners] remain alive until this guard resets every code/registry borrow.
    runtime_owner owner;
    initialize_deferred(source, provider);
    snapshot("initialized-deferred", false, false);
#if defined(UWVM2TEST_FULL_PREPARE_API)
    if(argc == 5)
    {
        REQUIRE(::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[4])} == "prepare-twice");
        REQUIRE(lib::full_compile_prepare_host_api());
        snapshot("prepared-once-no-segments-or-guest", false, false);
        REQUIRE(lib::full_compile_prepare_host_api());
        snapshot("prepared-twice-no-segments-or-guest", false, false);
    }
#else
    if(argc == 5) { return 3; }
#endif
    auto const& registry{runtime::storage::active_runtime_registry()};
    auto const p{registry.find(provider_name)};
    REQUIRE(p != registry.end());
    auto const provider_has_start{p->second.local_defined_function_vec_storage.size() == 2uz};
    REQUIRE(provider_has_start || p->second.local_defined_function_vec_storage.empty());
    lib::full_compile_run_config cfg{};
    cfg.entry_function_index = 2uz;
    ::uwvm2::uwvm::run::run_initialized_module_graph(consumer_name, cfg,
        [provider_has_start](::uwvm2::utils::container::u8string_view name, lib::full_compile_run_config entry) noexcept
        {
            // This callback is reached by the actual graph ONLY after its active segments.
            // On invalid modules old source prints effects before full compilation fails;
            // after admission preparation it must not reach this callback at all.
            if(name == provider_name)
            {
                REQUIRE(provider_has_start);
                snapshot("actual-preload-after-segments-before-start", false, false, true, false);
            }
            else
            {
                REQUIRE(name == consumer_name);
                snapshot("actual-graph-after-segments-before-entry", true, false, provider_has_start, provider_has_start);
            }
            lib::full_compile_and_run_main_module(name, entry);
        });
    snapshot("actual-graph-after-real-guest", true, true, provider_has_start, provider_has_start);
    ::fast_io::print(::fast_io::out(), "full_graph_effects: PASS actual graph, linked provider memory/table, guest Core3 probe\n");
}
