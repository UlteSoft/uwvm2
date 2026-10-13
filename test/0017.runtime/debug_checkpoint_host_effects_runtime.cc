// Real, canonical LLVM-full source and native-provider callback reentry. The
// new six-call-form WAT is assembled by the official wasm-tools CLI remotely.
// Recording must reject replay BEFORE every unadapted native side effect,
// including a host tail whose caller has already retired. No whole VM restore.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
namespace types = ::uwvm2::uwvm::wasm::type;
namespace container = ::uwvm2::utils::container;
namespace full = ::uwvm2::uwvm::runtime::full;
using domain = threads::cooperative_pause_domain;
static constexpr ::std::uint_least32_t reentry_function{7u};
static void require(bool valid, char const* message)
{
    if(!valid)
    {
        ::fast_io::io::perrln("debug_checkpoint_host_effects_runtime: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
static ::std::atomic<void const*> actual_reentry_module{};
static ::std::atomic_uint native_calls{}, native_returns{};
#if !defined(UWVM2_TEST_CHECKPOINT_HOST_EFFECT_PRECALL) || UWVM2_TEST_CHECKPOINT_HOST_EFFECT_PRECALL != 1
# error This actual pre-provider fixture requires a fresh three-TU private test build with UWVM2_TEST_CHECKPOINT_HOST_EFFECT_PRECALL=1.
#endif
static ::std::atomic_uint admitted_before_native{};
namespace uwvm2::runtime::lib
{
    extern "C++" void uwvm2test_checkpoint_before_native_effect(unsigned reason, bool admitted) noexcept
    {
        require(admitted && reason == static_cast<unsigned>(checkpoint::status::non_replayable_import) &&
            native_calls.load(::std::memory_order_relaxed) == 0u &&
            admitted_before_native.fetch_add(1u, ::std::memory_order_release) == 0u,
            "actual sticky non-replayable marker and genuine foreign-operation admission precede first provider effect");
    }
}

using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
using host_features = types::feature_list<wasm1>;
struct effect
{
    inline static constexpr container::u8string_view function_name{u8"effect"};
    using result_tuple = types::import_function_result_tuple_t<host_features, value_type::i32>;
    using parameter_tuple = types::import_function_parameter_tuple_t<host_features>;
    using local_imported_function_type = types::local_imported_function_type_t<result_tuple, parameter_tuple>;
    static void call(local_imported_function_type& call) noexcept
    {
        require(admitted_before_native.load(::std::memory_order_acquire) == 1u,
            "provider entry has actual pre-call checkpoint gate receipt");
        auto const module{actual_reentry_module.load(::std::memory_order_acquire)};
        require(module != nullptr && native_calls.fetch_add(1u, ::std::memory_order_relaxed) == 0u,
            "actual one unadapted native effect, with canonical source still owned");
        // [actual main module in main-owned canonical source registry] end
        // [safe                                                     ] caller
        // owns the source through outer/nested entry, no replacement/reset race;
        // this synchronous raw API validates the exact void -> void ABI.
        lib::llvm_jit_call_raw_host_api(module, reentry_function, nullptr, 0u, nullptr, 0u);
        native_returns.fetch_add(1u, ::std::memory_order_relaxed);
        container::get<0>(call.res) = 0;
    }
};
struct native_module
{
    container::u8string_view module_name{u8"checkpoint-host"};
    using local_function_tuple = container::tuple<effect>;
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_bool requested{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{}; threads::cooperative_pause_location location{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    bool captured{}, done{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location location) noexcept
    {
        // [actual runtime-retained observer owner] owner_end
        // [safe                                  ] callback-only native borrow;
        // guest scalar IDs or guest addresses never choose this context.
        auto& self{*static_cast<observer*>(opaque)};
        if(location.function != reentry_function || self.requested.exchange(true, ::std::memory_order_relaxed)) { return; }
        auto ticket{self.control->request_pause()}; require(static_cast<bool>(ticket), "actual reentered guest pause request");
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.location = location;
        self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location location,
        lib::llvm_jit_debug_local_view local) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        require(location.function == reentry_function && local.total_count == 1u && local.captured_count == 1u &&
            local.availability != nullptr, "actual callback stop retains original nondefaultable local index");
        // [actual runtime-owned availability[1]] exclusive_end
        // [safe                                ] captured_count==1 validated
        // before indexing; NEVER read the uninitialized native payload.
        require(local.availability[0u] == 0u, "unset Core3 reference local unavailable");
        auto const record{lib::llvm_jit_observe_checkpoint_recording_host_api()};
        require(record.instrumented && record.status == checkpoint::status::non_replayable_import &&
            !record.executable_restore_available,
            "unadapted native effect is recorded before callback and cannot authorize replay/restore");
        ::std::lock_guard lock{self.mutex}; require(location == self.location, "same actual stop episode");
        self.recording = record; self.captured = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy == "instruction" || policy == "unwind", "explicit instruction/unwind policy");
    auto const entry_text{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    ::std::uint_least32_t entry{};
    // [actual main-owned terminated argument bytes] end
    // [safe                                      ] owning string's exact extent
    // bounds the end pointer before parse_by_scan; require complete consumption.
    auto const entry_end{entry_text.data() + entry_text.size()};
    auto const parsed{::fast_io::parse_by_scan(entry_text.data(), entry_end, ::fast_io::mnp::dec_get<true, true>(entry))};
    require(parsed.code == ::fast_io::parse_code::ok && parsed.iter == entry_end && entry >= 1u && entry <= 6u,
        "actual six-call-form fixture function index 1..6");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument owner] no running guest/borrowed cursor
    // [safe             ] invalidate old cursor BEFORE reallocating the owner.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-host-effects", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete global argument allocation] end
    // [safe                              ] borrow final slot only after every
    // emplacement; path owner and vector remain stable through actual reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_tail_call = false;
    auto& preloaded{::uwvm2::uwvm::wasm::storage::preload_local_imported};
    require(preloaded.empty(), "isolated native test setup");
    // [empty actual local-provider owner] no parser/runtime borrowers
    // [safe                            ] reserve custom + builtin provider
    // before any provider address may escape; no test growth after preparation.
    preloaded.reserve(2u); preloaded.emplace_back(types::local_imported_t{native_module{}});
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-host-effects";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "actual canonical source parse/native-provider link/Core3 initializer");
    // Only cold, externally serialized native administration reads this owner.
    // No guest has entered and no replacement/metadata workers are running.
    auto const source{full::selected_full_source_owner_pin()};
    require(full::full_source_instance::has_canonical_owner(source) && source->initialized_from_actual_state(),
        "actual retained canonical source control block");
    auto const module{source->initialized_main_module()}; require(module != nullptr, "actual initialized main registry member");
    // Owned CLI preparation defers active segments to the start dispatcher.
    // This embedding fixture owns that cold startup before any guest enters.
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"checkpoint-host-effects");
    actual_reentry_module.store(module, ::std::memory_order_release);
    auto state{::std::make_shared<observer>()};
    auto const profile{checkpoint::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "owned immutable profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual full-only debug session");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok,
        "actual immutable checkpoint compile policy selected before publication");
    require(lib::llvm_jit_prepare_debug_host_api(), "actual single-pass validate/emit full engine");
    ::std::uint32_t output{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{}; config.entry_function_index = entry;
        // [main-owned initialized i32 result] end
        // [safe                             ] exact actual host ABI width;
        // joined guest cannot outlive this result owner or source pin.
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
        config.entry_abi_buffers.result_bytes = sizeof(output);
        lib::full_compile_and_run_main_module(u8"checkpoint-host-effects", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock, deadline(), [&] { return static_cast<bool>(state->ticket) || state->done; }), "actual callback pause event");
        require(!state->done && static_cast<bool>(state->ticket), "callback really parked before native provider returned"); ticket = state->ticket;
    }
    require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused,
        "genuine enrolled participant parked inside native callback reentry");
    {
        ::std::lock_guard lock{state->mutex};
        require(state->captured && state->recording.instrumented && state->recording.status == checkpoint::status::non_replayable_import &&
            state->location.function == reentry_function && state->location.offset == 0u && native_calls.load() == 1u && native_returns.load() == 0u,
            "real callback pause belongs to pending unadapted native operation");
    }
    // Cooperative pause is not a coherent worldstop: an outer native operation
    // is suspended on this callback's native stack. No publication is attempted.
    require(state->control->resume(ticket), "actual callback resumes"); guest.join();
    require(output == (entry < 4u ? 42u : 0u) && native_calls.load() == 1u && native_returns.load() == 1u,
        "normal/tail native transfer preserves original result ABI and one side effect");
    require(!lib::llvm_jit_observe_checkpoint_recording_host_api().instrumented, "external host cannot mint activation observation");
    // [main's native source/module borrow] no live guest/foreign operation
    // [safe                             ] withdraw callback borrow before actual
    // reset invalidates runtime publication; strong source remains pinned locally.
    actual_reentry_module.store(nullptr, ::std::memory_order_release);
    lib::reset_runtime_state_host_api(); require(state->control->is_closed(), "actual reset closes debug/profile owner");
    ::fast_io::io::println("debug_checkpoint_host_effects_runtime: PASS entry=", entry,
        " policy=", policy, " non-replayable-before-callback=1 native-calls=1 whole-restore=0");
}
