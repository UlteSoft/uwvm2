// Genuine selected LLVM-full before-park -> private capture -> ONE actual
// coherent manager -> immutable source/data inputs. No mocked runtime registry,
// constructor bypass, memory/GC export or complete-instance restore acceptance.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
namespace image = ::uwvm2::utils::control;
namespace full = ::uwvm2::uwvm::runtime::full;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, unsigned line)
{
    if(valid) { return; }
    ::fast_io::print(::fast_io::err(), "debug_checkpoint_resource_inputs_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct source_setup
{
    ::std::u8string path{};
    image::owned_file_image::owner immutable{};
    full::full_source_instance::mutable_owner source{};
    bool ready{};
    static bool prepare(void* pointer) noexcept
    {
        // [actual main-owned synchronous drained setup context] owner_end
        // [safe] retained until replace_full_source_after_drain callback returns.
        auto& state{*static_cast<source_setup*>(pointer)};
        try
        {
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"checkpoint-inputs")};
            if(!full::select_unparsed_full_source_after_drain(candidate)) { return false; }
            // SAME exclusive source image is adopted BEFORE magic/section parse,
            // not copied from a mutable file mapping after compiler validation.
            auto const loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(candidate->file_for_native_initialization(),
                candidate->owned_file_name(), candidate->owned_rename(), ::uwvm2::uwvm::wasm::storage::wasm_parameter,
                ::std::move(state.immutable))};
            if(loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok ||
               ::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() !=
                   ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok ||
               ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() !=
                   ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return false; }
            ::uwvm2::uwvm::runtime::initializer::initialize_runtime(true);
            if(!candidate->seal_actual_initializer()) { return false; }
            state.source = ::std::move(candidate); state.ready = true; return true;
        }
        catch(...) { return false; }
    }
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t ordinal{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::array<domain::pause_ticket, 3u> tickets{};
    ::std::array<threads::cooperative_pause_location, 3u> locations{};
    ::std::array<lib::llvm_jit_checkpoint_capture_result, 3u> captures{};
    ::std::size_t requested{}; bool done{};
    static void point(void* pointer, ::std::uint_least64_t, threads::cooperative_pause_location location) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        auto const ordinal{self.ordinal.fetch_add(1u, ::std::memory_order_relaxed)};
        ::std::lock_guard lock{self.mutex};
        // First call stops at actual ordinals0 and7, after data.drop. A second
        // normal entry stops at the SAME actual location as episode0. A copied
        // PC/epoch cannot grant a park or capture; only the real request does.
        if(!((self.requested == 0u && ordinal == 0u) ||
             (self.requested == 1u && ordinal == 7u) ||
             (self.requested == 2u && location == self.locations[0u]))) { return; }
        REQUIRE(self.requested < self.tickets.size());
        auto ticket{self.control->request_pause()}; REQUIRE(ticket);
        auto const episode{self.requested++};
        // [three main-owned event cells] end; episode<3 BEFORE indexing/mutation.
        self.tickets[episode] = ::std::move(ticket); self.locations[episode] = location;
        self.changed.notify_all();
    }
    static void before_park(void* pointer, ::std::uint_least64_t participant,
        threads::cooperative_pause_location location, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex}; REQUIRE(self.requested != 0u && self.requested <= self.tickets.size());
        auto const episode{self.requested - 1u}; REQUIRE(self.locations[episode] == location && participant != 0u);
        // Genuine runtime-minted opaque owner, no private test friend/constructor.
        self.captures[episode] = lib::llvm_jit_checkpoint_capture_thread_host_api(self.tickets[episode]);
        REQUIRE(self.captures[episode].status == lib::llvm_jit_checkpoint_capture_status::captured && self.captures[episode].capture);
        self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    // ONLY keeper-created disposable Wasm input and private output paths. The
    // owned branch deliberately truncates/overwrites that disposable input.
    if(argc != 5) { return 64; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    auto const origin{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    REQUIRE(policy == "instruction" || policy == "unwind"); REQUIRE(origin == "owned" || origin == "mapped");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_memory64 = false; features.explicit_enable_memory64 = true;
    features.disable_table64 = false; features.explicit_enable_table64 = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    features.disable_bulk_memory = false; features.explicit_enable_bulk_memory = true;
    source_setup setup{}; setup.path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // Retire the borrowed cursor BEFORE clear/reallocation. The path remains
    // main-owned through actual initializer, both guest entries and reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-resource-inputs", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(setup.path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual final global argument allocation] end
    // [safe] borrow AFTER both emplacements; no later growth before reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    // [actual immutable owner original.data..original.size] end
    // [safe] factory/size checked BEFORE taking an independent test DATA copy.
    ::std::vector<::std::byte> expected{original.begin(), original.end()};
    if(origin == "owned") { setup.immutable = ::std::move(bytes.image); }
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) && setup.ready && setup.source);
    REQUIRE(setup.source->file().has_owned_source_image() == (origin == "owned"));
    auto state{::std::make_shared<observer>()};
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile);
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(!lib::llvm_jit_checkpoint_capture_thread_host_api({}).capture); // external thread cannot mint
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // actual SAME fused validate+translate
    ::std::array<::std::uint32_t, 2u> outputs{};
    ::std::thread guest{[&]
    {
        for(auto& output : outputs)
        {
            lib::full_compile_run_config config{}; config.entry_function_index = 0u;
            config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
            config.entry_abi_buffers.result_bytes = sizeof(output);
            lib::full_compile_and_run_main_module(u8"checkpoint-inputs", config);
        }
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 3u> captured{};
    ::std::uint_least64_t epoch{};
    for(::std::size_t episode{}; episode != captured.size(); ++episode)
    {
        domain::pause_ticket ticket{};
        {
            ::std::unique_lock lock{state->mutex};
            REQUIRE(state->changed.wait_until(lock, deadline(), [&] { return state->requested > episode || state->done; }));
            REQUIRE(!state->done && state->requested > episode); ticket = state->tickets[episode];
        }
        REQUIRE(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused);
        { ::std::lock_guard lock{state->mutex}; captured[episode] = state->captures[episode].capture; REQUIRE(captured[episode]); }
        auto observed{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {::std::addressof(captured[episode]), 1u})};
        REQUIRE(observed.status == lib::llvm_jit_checkpoint_query_status::coherent_typed_data && observed.threads.size() == 1u);
        REQUIRE(!observed.complete_instance && !observed.executable_restore_available && !observed.snapshot_or_restore_authority());
        auto const& thread{observed.threads[0u]}; REQUIRE(thread.frame_count == 1u && thread.typed_slots == 2u);
        bool const entry{episode != 1u};
        REQUIRE(thread.unavailable_slots == (entry ? 1u : 0u) && thread.initialized_slots == (entry ? 1u : 2u));
        if(episode == 2u)
        {
            // Same function/PC/runtime generation, distinct genuine request.
            REQUIRE(state->locations[2u] == state->locations[0u]);
            REQUIRE(captured[2u].get() != captured[0u].get());
        }
        if(episode == 0u) { epoch = observed.observed_runtime_epoch; } else { REQUIRE(epoch == observed.observed_runtime_epoch); }
        if(origin == "owned")
        {
            REQUIRE(observed.resource_status == lib::llvm_jit_checkpoint_resource_status::immutable_inputs_only && observed.modules.size() == 1u);
            auto const& module{observed.modules[0u]}; REQUIRE(module.module == 0u && module.original_module == expected && module.data.size() == 2u);
            REQUIRE(module.declaration_counts == (::std::array<::std::uint64_t, 7u>{1u, 1u, 1u, 2u, 1u, 2u, 0u}));
            auto const& passive{module.data[0u]}; REQUIRE(passive.index == 0u && passive.dropped == (episode != 0u));
            if(episode == 0u)
            {
                REQUIRE(passive.byte_count == 4u && passive.source_offset <= expected.size() && passive.byte_count <= expected.size() - passive.source_offset);
                // [owned original bytes ... offset..offset+4<=size] end
                // [safe] subtraction range check BEFORE indexed payload reads.
                constexpr ::std::array<::std::byte, 4u> data{::std::byte{1u}, ::std::byte{2u}, ::std::byte{0xffu}, ::std::byte{0u}};
                for(::std::size_t i{}; i != data.size(); ++i) { REQUIRE(module.original_module[static_cast<::std::size_t>(passive.source_offset) + i] == data[i]); }
                { ::fast_io::native_file truncate{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::out | ::fast_io::open_mode::trunc}; truncate.close(); }
                { ::fast_io::obuf_file overwrite{::fast_io::mnp::os_c_str(argv[1])}; ::fast_io::print(overwrite, "modified disposable source after real pause"); ::fast_io::flush(overwrite); overwrite.close(); }
                auto again{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {::std::addressof(captured[episode]), 1u})};
                REQUIRE(again.status == observed.status && again.modules.size() == 1u && again.modules[0u].original_module == expected);
            }
            else { REQUIRE(passive.source_offset == 0u && passive.byte_count == 0u); }
            // Detached DATA oracle output only; not a protected checkpoint asset.
            REQUIRE(!module.original_module.empty() && module.original_module.size() <= PTRDIFF_MAX);
            ::fast_io::obuf_file out{::fast_io::mnp::os_c_str(argv[4])};
            // [complete owned detached module bytes] exclusive_end
            // [safe] vector+PTRDIFF bounds above BEFORE one-past write cursor.
            ::fast_io::operations::write_all_bytes(out, module.original_module.data(), module.original_module.data() + module.original_module.size());
            ::fast_io::flush(out); out.close();
        }
        else { REQUIRE(observed.resource_status == lib::llvm_jit_checkpoint_resource_status::unsupported_source_origin && observed.modules.empty()); }
        auto forged_owner{::std::make_shared<unsigned>(1u)};
        // Native hostile alias uses same address but a DIFFERENT control block.
        lib::llvm_jit_checkpoint_thread_capture_owner forged{forged_owner, captured[episode].get()};
        auto invalid{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {::std::addressof(forged), 1u})};
        REQUIRE(invalid.status == lib::llvm_jit_checkpoint_query_status::invalid_capture_owner && invalid.modules.empty() && invalid.threads.empty());
        if(episode != 0u)
        {
            auto stale{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {::std::addressof(captured[0u]), 1u})};
            REQUIRE(stale.status == lib::llvm_jit_checkpoint_query_status::stale_episode && stale.modules.empty() && stale.threads.empty());
        }
        // Repeated canonical owner != a second actual participant. Empty
        // owners and a different real domain request also cannot grant DATA.
        ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> duplicate{captured[episode], captured[episode]};
        auto duplicated{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, duplicate)};
        REQUIRE(duplicated.status == lib::llvm_jit_checkpoint_query_status::incomplete_cohort && duplicated.threads.empty());
        auto empty{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {})};
        REQUIRE(empty.status == lib::llvm_jit_checkpoint_query_status::incomplete_cohort && empty.modules.empty() && empty.threads.empty());
        auto foreign_control{::std::make_shared<domain>(1u)};
        auto foreign_ticket{foreign_control->request_pause()}; REQUIRE(foreign_ticket);
        auto foreign{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(foreign_ticket, {::std::addressof(captured[episode]), 1u})};
        REQUIRE(foreign.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data && foreign.modules.empty() && foreign.threads.empty());
        REQUIRE(foreign_control->resume(foreign_ticket));
        REQUIRE(state->control->resume(ticket));
        auto resumed{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {::std::addressof(captured[episode]), 1u})};
        REQUIRE(resumed.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data && resumed.modules.empty() && resumed.threads.empty());
    }
    guest.join(); REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
    lib::reset_runtime_state_host_api(); REQUIRE(state->control->is_closed());
    auto retired{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(state->tickets[2u], {::std::addressof(captured[2u]), 1u})};
    REQUIRE(retired.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data && retired.modules.empty() && retired.threads.empty());
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(), "CHECKPOINT_RESOURCE_INPUTS origin=", origin, " policy=", policy,
        " source_data_only=1 genuine_producer=1 same_pc_new_episode=1 whole_restore=0\n");
}
