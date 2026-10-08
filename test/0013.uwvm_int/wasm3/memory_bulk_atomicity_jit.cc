// Observe explicit full-JIT memory.copy/init traps before module destruction.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "memory_bulk_atomicity_common.h"
#include <signal.h>
#include <string>

namespace
{
    using namespace uwvm2test::wasm3_bulk_atomicity;

    void on_termination_signal(int, siginfo_t*, void*) noexcept { verify_unchanged_at_trap(); }

    [[nodiscard]] bool expect_oob_in_same_instance(auto&& invoke, scenario const& bad, bool copy, bool memory64, unsigned generation)
    {
        int pipe_fds[2]{};
        if(::pipe(pipe_fds) != 0) { return false; }
        auto const child{::fork()};
        if(child < 0)
        {
            ::close(pipe_fds[0]);
            ::close(pipe_fds[1]);
            return false;
        }
        if(child == 0)
        {
            ::alarm(10);
            ::close(pipe_fds[0]);
            if(::dup2(pipe_fds[1], STDERR_FILENO) < 0 || ::dup2(pipe_fds[1], STDOUT_FILENO) < 0) { ::_exit(75); }
            ::close(pipe_fds[1]);
            ::uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
            struct sigaction observer{};
            observer.sa_sigaction = on_termination_signal;
            observer.sa_flags = SA_SIGINFO;
            ::sigemptyset(&observer.sa_mask);
            for(int signal: {SIGILL, SIGABRT, SIGTRAP, SIGSEGV, SIGBUS})
            {
                if(::sigaction(signal, &observer, nullptr) != 0) { ::_exit(75); }
            }
            invoke(bad.dst, bad.src, bad.len);
            ::_exit(73);  // The invalid bulk range must not complete.
        }
        ::close(pipe_fds[1]);
        std::string diagnostic{};
        char chunk[4096]{};
        for(;;)
        {
            auto const n{::read(pipe_fds[0], chunk, sizeof(chunk))};
            if(n > 0) { diagnostic.append(chunk, static_cast<std::size_t>(n)); }
            else if(n == 0) { break; }
            else
            {
                ::close(pipe_fds[0]);
                return false;
            }
        }
        ::close(pipe_fds[0]);
        int status{};
        if(::waitpid(child, &status, 0) != child) { return false; }
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0 || diagnostic.find("access overflow") == std::string::npos ||
           diagnostic.find("READBACK-OK") == std::string::npos)
        {
            std::fprintf(stderr,
                         "FAIL JIT memory.%s %s memory%u generation=%u status=%d output=%s\n",
                         copy ? "copy" : "init",
                         bad.name,
                         memory64 ? 64u : 32u,
                         generation,
                         status,
                         diagnostic.c_str());
            return false;
        }
        std::puts("READBACK-OK");  // Re-emit the verified child callback marker.
        std::printf("PASS same-instance memory.%s %s generation=%u memory%u checked-bytes=%zu\n",
                    copy ? "copy" : "init",
                    bad.name,
                    generation,
                    memory64 ? 64u : 32u,
                    2 * committed_length);
        return true;
    }
}  // namespace

int main(int argc, char** argv)
{
    using namespace uwvm2test::wasm3_bulk_atomicity;
    namespace runtime_mode = uwvm2::uwvm::runtime::runtime_mode;
    if(argc != 5) { return 64; }
    auto const mode{std::string_view{argv[2]}};
    auto const type{std::string_view{argv[3]}};
    auto const policy{std::string_view{argv[4]}};
    if((mode != "copy" && mode != "init") || (type != "memory32" && type != "memory64") || (policy != "instruction" && policy != "unwind")) { return 64; }
    bool const copy{mode == "copy"}, memory64{type == "memory64"};
    runtime_mode::runtime_llvm_jit_full_policy_existed = true;
    runtime_mode::global_runtime_llvm_jit_full_policy = runtime_mode::runtime_llvm_jit_full_policy_t::passbuilder_o3;
    runtime_mode::runtime_llvm_jit_call_stack_existed = true;
    runtime_mode::global_runtime_llvm_jit_call_stack =
        policy == "unwind" ? runtime_mode::runtime_llvm_jit_call_stack_t::unwind : runtime_mode::runtime_llvm_jit_call_stack_t::instruction;
    auto const wasm{read_wasm(argv[1])};
    UWVM2TEST_REQUIRE(!wasm.empty());
    auto features{make_wasm1p1_feature_parameter()};
    auto& wasm1p1{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    wasm1p1.disable_multi_memory = false;
    wasm1p1.disable_memory64 = !memory64;
    auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_bulk_atomicity_jit", {}, features)};
    UWVM2TEST_REQUIRE(prepared.mod->local_defined_memory_vec_storage.size() == 2);
    auto& source{const_cast<uwvm2::object::memory::linear::native_memory_t&>(prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory)};
    auto& target{const_cast<uwvm2::object::memory::linear::native_memory_t&>(prepared.mod->local_defined_memory_vec_storage.index_unchecked(1).memory)};
    reset_memories(source.memory_begin, target.memory_begin, page_size);
    UWVM2TEST_REQUIRE(observer_selftest(source.memory_begin, target.memory_begin));
    std::puts("PASS observer-negative-controls=2");
    auto const invoke{[&](std::uint64_t dst, std::uint64_t src, std::uint64_t count)
                      {
                          auto const args{parameters(copy, memory64, dst, src, count)};
                          uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, 0, nullptr, 0, args.data(), args.size());
                      }};
    for(unsigned generation{}; generation != 2; ++generation)
    {
        auto const length{page_size * (generation + 1u)};
        if(generation != 0)
        {
            auto const old_source{source.memory_begin};
            auto const old_target{target.memory_begin};
            UWVM2TEST_REQUIRE(source.grow_strictly(1, 2 * page_size));
            UWVM2TEST_REQUIRE(target.grow_strictly(1, 2 * page_size));
            if(std::remove_reference_t<decltype(source)>::can_mmap) { UWVM2TEST_REQUIRE(source.memory_begin == old_source); }
            if(std::remove_reference_t<decltype(target)>::can_mmap) { UWVM2TEST_REQUIRE(target.memory_begin == old_target); }
        }
        auto const valid{[&](std::uint64_t dst, std::uint64_t src, std::uint64_t count)
                         {
                             reset_memories(source.memory_begin, target.memory_begin, length);
                             invoke(dst, src, count);
                             return verify_positive(source.memory_begin, target.memory_begin, length, dst, src, count, copy);
                         }};
        UWVM2TEST_REQUIRE(valid(128, 0, 16));
        UWVM2TEST_REQUIRE(valid(length, copy ? length : passive_payload.size(), 0));
        if(generation != 0)
        {
            UWVM2TEST_REQUIRE(valid(page_size - 8, 0, 16));
            std::puts("PASS post-grow-old-boundary destination");
            if(copy)
            {
                UWVM2TEST_REQUIRE(valid(128, page_size - 8, 16));
                std::puts("PASS post-grow-old-boundary source");
            }
        }
        for(auto const& bad: invalid_scenarios(copy, length))
        {
            reset_memories(source.memory_begin, target.memory_begin, length);
            UWVM2TEST_REQUIRE(expect_oob_in_same_instance(invoke, bad, copy, memory64, generation));
        }
    }
    std::printf("PASS 8 JIT same-instance memory.%s fault/readback checks (%s, memory%u), before/after grow\n",
                copy ? "copy" : "init",
                argv[4],
                memory64 ? 64u : 32u);
}
