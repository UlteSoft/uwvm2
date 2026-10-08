// Check actual full-JIT stores from the guard-fault callback, after compilation
// and a safe warm-up call. The generated function receives a dynamic address.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../strict/uwvm_int_translate_strict_common.h"
#include <fstream>
#include <signal.h>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    std::byte const volatile* untouched{};
    std::byte const volatile* selected{};
    std::size_t selected_length{65536};
    constexpr std::byte initial_byte(std::size_t index) noexcept
    { return index < 16 ? std::byte{0x3c} : std::byte{0xa5}; }
    void reset_selected(std::byte* base, std::size_t length)
    {
        std::memset(base, 0xa5, length);
        std::memset(base, 0x3c, 16);
    }
    void fault(int, siginfo_t* info, void*) noexcept
    {
        auto const address{reinterpret_cast<std::uintptr_t>(info->si_addr)};
        auto const boundary{reinterpret_cast<std::uintptr_t>(selected) + selected_length};
        if(address < boundary || address - boundary >= 16) { ::_exit(74); }
        // These mappings remain committed and module-owned until _exit. Observe
        // every byte before runtime termination can discard the guest state.
        for(std::size_t i{}; i != 2 * 65536; ++i)
        { if(untouched[i] != std::byte{0x11}) { ::_exit(71); } }
        for(std::size_t i{}; i != selected_length; ++i)
        { if(selected[i] != initial_byte(i)) { ::_exit(72); } }
        ::_exit(0);
    }
}

int main(int argc, char** argv)
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace mode = uwvm2::uwvm::runtime::runtime_mode;
    if(argc < 4 || argc > 6) { return 64; }
    auto const width{static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10))};
    if(width != 2 && width != 4 && width != 8 && width != 16) { return 64; }
    auto const policy{std::string_view{argv[3]}};
    if(policy != "instruction" && policy != "unwind") { return 64; }
    bool growth{}, memory64{};
    for(int i{4}; i < argc; ++i)
    {
        if(std::strcmp(argv[i], "growth") == 0 && !growth) { growth = true; }
        else if(std::strcmp(argv[i], "memory64") == 0 && !memory64) { memory64 = true; }
        else { return 64; }
    }
    mode::runtime_llvm_jit_full_policy_existed = true;
    mode::global_runtime_llvm_jit_full_policy = mode::runtime_llvm_jit_full_policy_t::passbuilder_o3;
    mode::runtime_llvm_jit_call_stack_existed = true;
    mode::global_runtime_llvm_jit_call_stack = policy == "unwind" ? mode::runtime_llvm_jit_call_stack_t::unwind
                                                               : mode::runtime_llvm_jit_call_stack_t::instruction;
    static_assert(uwvm2::object::memory::linear::native_memory_t::can_mmap);
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) { return 65; }
    byte_vec wasm{};
    for(char c; input.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
    UWVM2TEST_REQUIRE(input.eof());
    auto features{make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_multi_memory = false;
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64 = !memory64;
    auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_store_atomicity_jit", {}, features)};
    std::uint32_t address32{}, result{};
    std::uint64_t address64{};
    auto const invoke{[&]()
    {
        if(memory64)
        { uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, 0, &result, sizeof(result), &address64, sizeof(address64)); }
        else
        { uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, 0, &result, sizeof(result), &address32, sizeof(address32)); }
    }};
    invoke();
    UWVM2TEST_REQUIRE(result == 0);
    auto& memory0{prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory};
    auto& memory1{const_cast<uwvm2::object::memory::linear::native_memory_t&>(
        prepared.mod->local_defined_memory_vec_storage.index_unchecked(1).memory)};
    std::memset(memory0.memory_begin, 0x11, 2 * 65536);
    reset_selected(memory1.memory_begin, 65536);
    untouched = memory0.memory_begin;
    selected = memory1.memory_begin;
    selected_length = 65536;
    address32 = 32;
    address64 = 32;
    invoke();
    UWVM2TEST_REQUIRE(result == 0);
    std::byte pattern[16]{};
    std::memcpy(pattern, memory1.memory_begin + 32, width);
    reset_selected(memory1.memory_begin, selected_length);
    for(unsigned generation{}; generation != (growth ? 2u : 1u); ++generation)
    {
        if(generation != 0)
        {
            auto const old_base{memory1.memory_begin};
            UWVM2TEST_REQUIRE(memory1.grow_strictly(1, 2 * 65536));
            UWVM2TEST_REQUIRE(memory1.memory_begin == old_base);
            selected = memory1.memory_begin;
            selected_length = 2 * 65536;
            for(unsigned prefix{1}; prefix < width; ++prefix)
            {
                reset_selected(memory1.memory_begin, selected_length);
                auto const start{65536u - prefix};
                address32 = start;
                address64 = start;
                invoke();
                UWVM2TEST_REQUIRE(result == 0);
                for(std::size_t i{}; i != selected_length; ++i)
                {
                    auto const expected{i >= start && i - start < width ? pattern[i - start] : initial_byte(i)};
                    UWVM2TEST_REQUIRE(memory1.memory_begin[i] == expected);
                }
            }
            reset_selected(memory1.memory_begin, selected_length);
        }
        for(unsigned prefix{1}; prefix < width; ++prefix)
        {
            auto const child{::fork()};
            UWVM2TEST_REQUIRE(child >= 0);
            if(child == 0)
            {
                ::alarm(10);
                // Public runtime entry re-establishes its reporting callback.
                // Observe the synchronous OS fault independently: all committed
                // bytes in memory 0 and memory 1 must still equal their fills.
                struct sigaction observer{};
                observer.sa_sigaction = fault;
                observer.sa_flags = SA_SIGINFO;
                ::sigemptyset(&observer.sa_mask);
                if(::sigaction(SIGSEGV, &observer, nullptr) != 0 || ::sigaction(SIGBUS, &observer, nullptr) != 0) { ::_exit(75); }
                address32 = selected_length - prefix;
                address64 = selected_length - prefix;
                invoke();
                ::_exit(73);
            }
            int status{};
            UWVM2TEST_REQUIRE(::waitpid(child, &status, 0) == child);
            if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                std::fprintf(stderr, "FAIL JIT split store width=%u prefix=%u policy=%s memory64=%u generation=%u status=%d\n",
                             width, prefix, argv[3], unsigned(memory64), generation, status);
                return 1;
            }
        }
    }
    std::printf("PASS %u full-JIT split-store prefix checks (%s, memory%s); %u old-boundary positive writes after grow\n",
                (growth ? 2u : 1u) * (width - 1), argv[3], memory64 ? "64" : "32", growth ? width - 1 : 0u);
}
