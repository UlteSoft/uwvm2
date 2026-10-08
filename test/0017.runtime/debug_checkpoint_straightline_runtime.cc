// Actual LLVM-full opt-in entry producer/activation qualification. This does
// not accept whole-instance restore, replay, GC graph export or native-PC jump.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io.h>
#include <uwvm2/utils/container/string_concat.h>
#include <atomic>
#include <array>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* text)
{
    if(!valid) { ::fast_io::io::perrln("debug_checkpoint_straightline_runtime: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t point_count{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::array<domain::pause_ticket, 2u> tickets{};
    ::std::array<threads::cooperative_pause_location, 2u> locations{};
    ::std::array<lib::llvm_jit_checkpoint_recording_observation, 2u> recordings{};
    ::std::array<bool, 2u> captured{};
    ::std::size_t requests{}; bool unsupported{}, overquota{}, done{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained native observer owner] owner_end
        // [safe] pointer is the owned host callback context, never a guest ID.
        auto& self{*static_cast<observer*>(opaque)};
        auto const ordinal{self.point_count.fetch_add(1u, ::std::memory_order_relaxed)};
        if(self.overquota ? ordinal != 0u : self.unsupported ? ordinal != 1u : ordinal != 0u && ordinal != 6u) { return; }
        auto ticket{self.control->request_pause()}; require(static_cast<bool>(ticket), "actual current-opcode pause request");
        ::std::lock_guard lock{self.mutex};
        require(self.requests < self.tickets.size(), "bounded actual pause episode slots");
        auto const episode{self.requests++};
        // [actual host-owned ticket/location arrays[2] ... episode] end
        // [safe] episode<2 checked BEFORE preserving the real private ticket;
        // integer ordinals/locations are not substitutes for its control owner.
        self.tickets[episode] = ::std::move(ticket); self.locations[episode] = where;
        self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view local) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        auto const recording{lib::llvm_jit_observe_checkpoint_recording_host_api()};
        if(self.overquota)
        {
            require(local.total_count == 1000u && local.captured_count == 256u &&
                local.availability != nullptr && local.values != nullptr,
                "legacy bounded display remains usable when checkpoint workspace declines");
            // [actual display availability0 ... index ... 256] exclusive_end
            // [safe] complete captured_count checked BEFORE original-index reads.
            // These defaultable i32 display locals grant no checkpoint values.
            for(::std::size_t index{}; index != local.captured_count; ++index)
            { require(local.availability[index] == 1u, "genuine defaultable display slot remains initialized"); }
            ::std::lock_guard lock{self.mutex};
            require(self.requests == 1u, "single genuine over-quota pause episode");
            self.recordings[0u] = recording; self.captured[0u] = true; self.changed.notify_all(); return;
        }
        require(local.captured_count == 2u && local.total_count == 2u && local.availability != nullptr && local.values != nullptr,
            "actual Core3 two original local indices remain stable");
        // [actual captured availability[2]] exclusive_end
        // [safe] complete count before flag reads; a false local's bytes are
        // not interpreted as any value, including a null reference.
        require(local.availability[0u] <= 1u && local.availability[1u] == 1u,
            "actual unset/initialized flags are canonical");
        ::std::lock_guard lock{self.mutex};
        require(self.requests > 0u && self.requests <= self.tickets.size(), "actual retained current pause episode");
        auto const episode{self.requests - 1u};
        require(self.locations[episode] == where && !self.captured[episode], "same real stop location, no stale episode reuse");
        require(local.availability[0u] == ((!self.unsupported && episode == 1u) ? 1u : 0u),
            "nonnull i31 is unavailable before set and genuinely available after its store");
        ::std::uint64_t numeric{};
        // [actual borrowed local packet slots0..2 of16bytes] packet_end
        // [safe] slot1 index and payload width<=16 checked above BEFORE byte
        // pointer advance; flag1==1 is required before interpreting those bytes.
        ::std::memcpy(::std::addressof(numeric), local.values + 16u, sizeof(numeric));
        require(numeric == ((!self.unsupported && episode == 1u) ? 70u : 0u),
            "real original-index i64 local contains default then assigned bits");
        self.recordings[episode] = recording; self.captured[episode] = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    // The selected immutable recording profile overrides this legacy opt-out
    // in per-function, task, merged optimizer and object-emission verification.
    // ROS always verifies full compilation and has no corresponding option.
    mode::runtime_llvm_jit_disable_ir_verifaction = true;
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy == "instruction" || policy == "unwind", "explicit instruction/unwind policy");
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument allocation] no running guest or borrowed cursor
    // [safe                   ] retire cursor before clear/reallocation.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-straightline", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual complete global argument array] end
    // [safe                                ] no later growth through reset;
    // borrow only after both emplacements, backed by main-owned UTF8 path.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-straightline";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source() == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "actual owned source parse/initializer with new GC/nondefaultable-local syntax");
    auto const kind{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    require(kind == "straightline" || kind == "unsupported" || kind == "overquota", "explicit fixture scope");
    auto state{::std::make_shared<observer>()}; state->unsupported = kind == "unsupported"; state->overquota = kind == "overquota";
    auto const profile{checkpoint::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "owned immutable profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual full-only session");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok,
        "actual prepublication opt-in retains immutable compile policy");
    require(!lib::llvm_jit_observe_checkpoint_recording_host_api().instrumented, "external scalar IDs cannot query an activation");
    require(lib::llvm_jit_prepare_debug_host_api(), "same fused validate+emit full compilation");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::already_published,
        "published engine cannot mutate checkpoint compilation policy");
    ::std::uint32_t output{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{}; config.entry_function_index = 0u;
        // [main-owned real result slot] output_end
        // [safe                      ] normal host ABI exact sizeof extent.
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
        config.entry_abi_buffers.result_bytes = sizeof(output);
        lib::full_compile_and_run_main_module(u8"checkpoint-straightline", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    auto const episodes{state->unsupported || state->overquota ? 1u : 2u};
    ::std::uint64_t actual_epoch{}, actual_incarnation{};
    for(::std::size_t episode{}; episode != episodes; ++episode)
    {
        domain::pause_ticket ticket{};
        {
            ::std::unique_lock lock{state->mutex};
            require(state->changed.wait_until(lock, deadline(), [&] { return state->requests > episode || state->done; }),
                "actual requested current-opcode event");
            require(!state->done && state->requests > episode && static_cast<bool>(state->tickets[episode]),
                "guest really stops at actual selected opcode"); ticket = state->tickets[episode];
        }
        require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused,
            "genuine actual participant parks under that private episode");
        {
            ::std::lock_guard lock{state->mutex}; auto const& record{state->recordings[episode]};
            auto const where{state->locations[episode]};
            require(state->captured[episode] && where.function == 0u, "actual callback reports the same Wasm function");
            require(record.instrumented && record.status == (state->overquota ? checkpoint::status::quota_exceeded : checkpoint::status::ok) && record.incarnation != 0u &&
                record.runtime_epoch == where.code_generation && record.function_generation == 1u && record.native_frames == (state->overquota ? 0u : 1u),
                "canonical current publication, sealed plan, actual frame and immutable profile match");
            if(state->overquota)
            {
                require(where.offset == 0u && !record.at_current_opcode && record.site == 0u && record.typed_slots == 0u,
                    "oversized recording explicitly declines without a native packet or invented entry site");
            }
            else if(state->unsupported)
            {
                require(where.offset != 0u && !record.at_current_opcode && record.site == 1u && record.typed_slots == 2u,
                    "active nested control declines checkpoint current-site availability without rejecting valid Wasm");
            }
            else if(episode == 0u)
            {
                require(where.offset == 0u && record.at_current_opcode && record.site == 1u && record.typed_slots == 2u,
                    "genuine entry captures exact pre-op locals with unset nondefaultable state");
                actual_epoch = record.runtime_epoch; actual_incarnation = record.incarnation;
            }
            else
            {
                require(where.offset != 0u && record.at_current_opcode && record.site == 7u && record.typed_slots == 3u &&
                    record.runtime_epoch == actual_epoch && record.incarnation == actual_incarnation,
                    "same real frame reaches new same-walk site with initialized locals plus actual i64 operand");
            }
            require(!record.executable_restore_available, "logical typed producer does not fabricate full-instance restore authority");
        }
        require(state->control->resume(ticket), "same actual episode resumes");
    }
    guest.join();
    require(output == 42u, "new Core3 store, unsupported control and checkpoint-overquota guests execute normally");
    require(!lib::llvm_jit_observe_checkpoint_recording_host_api().instrumented, "retired native observer context has no activation authority");
    lib::reset_runtime_state_host_api(); require(state->control->is_closed(), "actual reset retires session/profile/code generation");
    ::fast_io::io::println("debug_checkpoint_straightline_runtime: PASS actual same-walk typed producer; whole-state restore/reverse/replay acceptance=false");
}
