// Time actual full-JIT code after translation, optimization and publication.
// HEAD and current builds use their own runtime object and the same LLVM build.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../strict/uwvm_int_translate_strict_common.h"
#include "memory_performance_reference.h"
#include "memory_performance_timing.h"
#include <fstream>
#include <string_view>

int main(int argc, char** argv)
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace mode = uwvm2::uwvm::runtime::runtime_mode;
    if(argc != 4) { return 64; }
    auto const iterations{static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10))};
    auto const samples{static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10))};
    if(iterations == 0 || iterations > 100000000u || samples == 0 || samples > 100u) { return 64; }
    static_assert(uwvm2::object::memory::linear::native_memory_t::can_mmap);
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) { return 65; }
    byte_vec wasm{};
    for(char c; input.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
    if(!input.eof()) { return 65; }
    auto features{make_wasm1p1_feature_parameter()};
#ifndef UWVM2TEST_BASELINE
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_multi_memory = false;
#endif
    mode::runtime_llvm_jit_full_policy_existed = true;
    mode::global_runtime_llvm_jit_full_policy = mode::runtime_llvm_jit_full_policy_t::passbuilder_o3;
    mode::runtime_llvm_jit_call_stack_existed = true;
    auto const policy{std::getenv("UWVM_TEST_JIT_CALL_STACK")};
    if(policy == nullptr || std::string_view{policy} == "unwind")
    { mode::global_runtime_llvm_jit_call_stack = mode::runtime_llvm_jit_call_stack_t::unwind; }
    else if(std::string_view{policy} == "instruction")
    { mode::global_runtime_llvm_jit_call_stack = mode::runtime_llvm_jit_call_stack_t::instruction; }
    else { return 64; }
    auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_performance", {}, features)};
    // Optional inspection run: the real full runtime writes its authenticated
    // object cache. Timing runs leave this unset and keep caching disabled.
    if(auto const directory{std::getenv("UWVM_TEST_JIT_OBJECT_CACHE_DIR")}; directory != nullptr)
    {
        auto const length{std::strlen(directory)};
        UWVM2TEST_REQUIRE(length != 0);
        mode::global_runtime_llvm_jit_cache_path = uwvm2::utils::container::u8string{
            reinterpret_cast<char8_t const*>(directory), reinterpret_cast<char8_t const*>(directory) + length};
        mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::custom_path;
    }

    auto const expected{memory_performance_checksum(argv[1], iterations)};
    std::printf("{\"iterations\":%u,\"checksum\":%u", iterations, expected);
    memory_performance_timer timer{};
    for(unsigned sample{}; sample != samples + 2u; ++sample)
    {
        std::uint32_t actual{};
        timer.begin();
        uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, 0,
            reinterpret_cast<std::byte*>(&actual), sizeof(actual),
            reinterpret_cast<std::byte const*>(&iterations), sizeof(iterations));
        timer.end(sample >= 2u ? sample - 2u : 0u, sample >= 2u);
        UWVM2TEST_REQUIRE(actual == expected);
        // The first call compiles/publishes; the second warms code and data.

    }
    timer.print(samples);
}
