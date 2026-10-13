#include "retained_artifacts.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#include <uwvm2/uwvm/wasm/loader/wasm_file.h>
#include <fast_io_unit/string.h>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif
namespace
{
    namespace proof = ::uwvm2test::checked_artifacts;
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace lib = ::uwvm2::runtime::lib;
    namespace wasm = ::uwvm2::uwvm::wasm;
    namespace runtime = ::uwvm2::uwvm::runtime;
    namespace mode = runtime::runtime_mode;
    void require(bool value, unsigned line)
    {
        if(!value)
        {
            ::fast_io::print(::fast_io::err(), "artifact_retention: FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto enabled_features()
    {
        auto out{strict::make_wasm1p1_feature_parameter()};
        auto& p{wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(out)};
        p.disable_gc = false; p.disable_exceptions = false; p.disable_function_references = false;
        p.disable_memory64 = false; p.disable_table64 = false; p.disable_multi_memory = false;
        p.disable_tail_call = false; p.disable_extended_const = false; p.disable_table_initializer = false;
        return out;
    }
    proof::source_owner load_actual_source(char const* path)
    {
        // Actual synchronous runtime drain occurs before selecting the new
        // nonmoving native owner. This single-threaded fixture serializes all
        // loader/configuration/producer/handoff/reset operations externally.
        lib::reset_runtime_state_host_api();
        wasm::storage::all_module.clear(); wasm::storage::all_module_export.clear();
        wasm::storage::preloaded_wasm.clear(); wasm::storage::preload_local_imported.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        wasm::storage::weak_symbol.clear();
#endif
        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;
        auto source{proof::full::full_source_instance::create_unparsed(
            ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))),
            ::fast_io::u8concat_std(u8"checked-artifact-owner"))};
        REQUIRE(proof::full::select_unparsed_full_source_after_drain(source));
        wasm::type::wasm_parameter_t parameters{};
        parameters.binfmt1_para = enabled_features();
        REQUIRE(wasm::loader::load_wasm_file(source->file_for_native_initialization(), source->owned_file_name(),
            source->owned_rename(), parameters) == wasm::loader::load_wasm_file_rtl::ok);
        REQUIRE(wasm::loader::construct_all_module_and_check_duplicate_module() == wasm::loader::load_and_check_modules_rtl::ok);
        REQUIRE(wasm::loader::check_import_exist_and_detect_cycles() == wasm::loader::load_and_check_modules_rtl::ok);
        runtime::initializer::initialize_runtime(true);
        REQUIRE(source->seal_actual_initializer());
        REQUIRE(source->initialized_main_module() != nullptr && source->registry().size() == 1uz);
        REQUIRE(source->initialized_main_module()->local_defined_function_vec_storage.size() == 2uz);
        return source;
    }
    bool is_expected_local_init_rejection(proof::error const& error) noexcept
    {
        // The discriminator is checked before the corresponding diagnostic
        // union member is read. Allocator/native/LLVM failures cannot qualify.
        return error.err_code == proof::error_code::br_value_type_mismatch &&
            error.err_selectable.br_value_type_mismatch.op_code_name == u8"local.get (unset non-null local)";
    }
    template<typename Produce, typename Consume>
    void observe(proof::source_owner source, bool expected_valid, char const* backend, Produce produce, Consume consume)
    {
        proof::error failure{};
        bool parse_rejected{};
        unsigned actual_fused_factory_calls{};
        auto artifact = [&]
        {
            try
            {
                ++actual_fused_factory_calls;
                return produce(source, failure);
            }
            catch(::fast_io::error const& error)
            {
                if(error.domain != ::fast_io::parse_domain_value || error.code !=
                    static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
                parse_rejected = true;
                return decltype(produce(source, failure)){};
            }
        }();
        bool const accepted{static_cast<bool>(artifact) && !parse_rejected && failure.err_code == proof::error_code::ok};
        REQUIRE(accepted == expected_valid && actual_fused_factory_calls == 1u);
        if(!expected_valid)
        {
            REQUIRE(parse_rejected && !artifact && is_expected_local_init_rejection(failure));
            lib::reset_runtime_state_host_api();
            ::fast_io::print(::fast_io::out(), "ARTIFACT backend=", ::fast_io::mnp::os_c_str(backend),
                " actual_unused_invalid_rejected=1 private_artifact_minted=0 actual_fused_factory_calls=1\n");
            return;
        }
        ::std::weak_ptr<proof::full::full_source_instance const> lifetime{source};
        source.reset();
        consume(artifact, lifetime);
        REQUIRE(actual_fused_factory_calls == 1u);
        REQUIRE(!artifact->current());
        REQUIRE(!lifetime.expired());
        artifact.reset();
        REQUIRE(lifetime.expired());
        ::fast_io::print(::fast_io::out(), "ARTIFACT backend=", ::fast_io::mnp::os_c_str(backend),
            " actual_fused_factory_calls=", ::fast_io::mnp::dec(actual_fused_factory_calls),
            " retained_source_survived_reset=1 retired_handoff_rejected=1 last_owner_released=1",
            " deferred_raw_wasm_decoder_entrypoints=none-by-construction guest_execution=not-tested\n");
    }
}
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    auto const backend{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])}};
    auto const expected{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(expected != "accept" && expected != "reject") { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compile_threads = 0;
    mode::runtime_compile_threads_existed = true;
#if !defined(UWVM_DISABLE_INT) && !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    if(backend == "int")
    {
        mode::global_runtime_compiler = mode::runtime_compiler_t::uwvm_interpreter_only;
        using plan = proof::retained_int_artifact<strict::k_test_byref_opt>;
        observe(load_actual_source(argv[3]), expected == "accept", "uwvm-int-retained-opstream",
            [](auto source, auto& error) { return plan::compile_initial(::std::move(source), error); },
            [](plan::owner const& artifact, auto const& lifetime)
            {
                REQUIRE(artifact->current() && artifact->function_count() == 2uz && !lifetime.expired());
                ::std::byte const* initial_program{};
                ::std::size_t initial_size{};
                for(unsigned request{}; request != 4u; ++request)
                {
                    auto view{plan::defer_function(artifact, 0uz)};
                    REQUIRE(view.has_value());
                    auto const program{view->program()};
                    REQUIRE(!program.empty() && program.data() != nullptr);
                    if(request == 0u)
                    {
                        // [actual retained immutable emitted stream] program_end
                        // [safe                                    ] copy owner-backed endpoint/length, no byte read or cursor advance.
                        initial_program = program.data(); initial_size = program.size();
                    }
                    else { REQUIRE(program.data() == initial_program && program.size() == initial_size); }
                }
                REQUIRE(!plan::defer_function(artifact, 2uz));
                auto retained_view{plan::defer_function(artifact, 0uz)};
                REQUIRE(retained_view && !retained_view->program().empty());
                lib::reset_runtime_state_host_api();
                REQUIRE(!artifact->current() && !lifetime.expired());
                REQUIRE(retained_view->program().empty() && !plan::defer_function(artifact, 0uz));
                // Immutable data retains its actual source after reset, but the
                // retired selection supplies no new lowering/execution authority.
                retained_view.reset();
            });
        return 0;
    }
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    if(backend == "llvm")
    {
        mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
        mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
        using plan = proof::retained_ir_artifact;
        observe(load_actual_source(argv[3]), expected == "accept", "llvm-retained-real-IR",
            [](auto source, auto& error) { return plan::compile_initial(::std::move(source), error); },
            [](plan::owner const& artifact, auto const& lifetime)
            {
                REQUIRE(artifact->current() && !lifetime.expired());
                auto deferred{artifact->defer_lowering()};
                REQUIRE(deferred && deferred->current());
                REQUIRE(!artifact->defer_lowering());
                auto const& data{deferred->data()};
                REQUIRE(data.local_funcs.size() == 2uz && data.llvm_jit_module.emitted);
                REQUIRE(data.llvm_jit_module.llvm_context_holder != nullptr && data.llvm_jit_module.llvm_module != nullptr);
                auto const& module{*data.llvm_jit_module.llvm_module};
                REQUIRE(::std::addressof(module.getContext()) == data.llvm_jit_module.llvm_context_holder.get());
                // Genuine LLVM verification consumes the transferred actual IR;
                // no source decoder or Wasm validator can enter this handoff.
                REQUIRE(!::llvm::verifyModule(module, &::llvm::errs()));
                ::std::size_t real_definitions{};
                for(auto const& function : module) { real_definitions += !function.isDeclaration(); }
                REQUIRE(real_definitions >= 2uz);
                lib::reset_runtime_state_host_api();
                REQUIRE(!artifact->current() && !deferred->current() && !lifetime.expired());
                REQUIRE(!artifact->defer_lowering());
                // Already-retained IR remains owned diagnostic data even after
                // reset. It is never installed into a runtime/JIT engine here.
                REQUIRE(!::llvm::verifyModule(module, &::llvm::errs()));
                deferred.reset();
            });
        return 0;
    }
#endif
    return 3; // An unavailable backend cannot masquerade as a successful probe.
}
