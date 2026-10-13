// Diagnostic only: vary the address of an unchanged, already compiled loop in
// one process. This separates packed-stream cache-line splits from native code
// layout/ASLR. These relocated-stream timings are NOT production acceptance.
#include "../strict/uwvm_int_translate_strict_common.h"
#include "memory_performance_reference.h"
#include <array>
#include <chrono>
#include <fstream>
#include <random>

int main(int argc, char** argv)
{
    using namespace uwvm2test::uwvm_int_strict;
    if(argc != 2) { return 64; }
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) { return 65; }
    byte_vec wasm{};
    for(char c; input.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
    UWVM2TEST_REQUIRE(input.eof());
    auto features{make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_multi_memory = false;
    install_unexpected_traps();
    auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_layout_probe", {}, features)};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    optable::compile_option options{};
    uwvm2::validation::error::code_validation_error_impl error{};
    auto compiled{compiler::compile_all_from_uwvm_single_func<ring>(*prepared.mod, options, error, &features)};
    UWVM2TEST_REQUIRE(error.err_code == uwvm2::validation::error::code_validation_error_code::ok);
    UWVM2TEST_REQUIRE(compiled.local_funcs.size() == 1);
    auto const& fn{compiled.local_funcs.index_unchecked(0)};
    auto const& original{fn.op.operands};
    auto const old_begin{reinterpret_cast<std::uintptr_t>(original.data())};
    auto const old_end{old_begin + original.size()};
    byte_vec storage(original.size() + 128);
    auto const aligned{(reinterpret_cast<std::uintptr_t>(storage.data()) + 63) & ~std::uintptr_t{63}};
    constexpr std::uint32_t iterations{1000003};
    // Use the same independent oracle as the throughput harness, including its
    // disjoint initialized loads. Relocation must not silently drop a read.
    auto const expected{memory_performance_checksum(argv[1], iterations)};
    auto const arguments{pack_i32(iterations)};
    std::array<unsigned, 16> offsets{};
    for(unsigned i{}; i != offsets.size(); ++i) { offsets[i] = i * 4; }
    std::mt19937 random{0x5741534d};
    for(unsigned round{}; round != 7; ++round)
    {
        std::shuffle(offsets.begin(), offsets.end(), random);
        for(auto offset: offsets)
        {
            // Allocation has 128 spare bytes: round-up <= 63, offset <= 60.
            // [padding][complete relocated stream][remaining storage] | unsafe
            //          ^^ entry; all instruction bytes remain inside storage
            auto* entry{reinterpret_cast<std::byte*>(aligned + offset)};
            std::memcpy(entry, original.data(), original.size());
            unsigned relocations{};
            // These fixtures contain exactly one internal pointer: br_if's
            // loop back-edge. Native handlers and memory objects are external.
            // Refuse any other layout instead of silently inventing fixups.
            for(std::size_t i{}; i + sizeof(std::uintptr_t) <= original.size(); ++i)
            {
                std::uintptr_t value{};
                std::memcpy(&value, original.data() + i, sizeof(value));
                if(value >= old_begin && value < old_end)
                {
                    auto const replacement{reinterpret_cast<std::uintptr_t>(entry) + (value - old_begin)};
                    // [safe prefix][complete pointer immediate][safe suffix]
                    //               ^^ entry+i, checked above for pointer width
                    std::memcpy(entry + i, &replacement, sizeof(replacement));
                    ++relocations;
                }
            }
            UWVM2TEST_REQUIRE(relocations == 1);
            for(unsigned sample{}; sample != 3; ++sample)
            {
                auto const begin{std::chrono::steady_clock::now()};
                auto const result{interpreter_runner<ring>::run(fn,
                    prepared.mod->local_defined_function_vec_storage.index_unchecked(0), arguments, nullptr, nullptr, entry)};
                auto const end{std::chrono::steady_clock::now()};
                UWVM2TEST_REQUIRE(result.results.size() == 4 && static_cast<std::uint32_t>(load_i32(result.results)) == expected);
                if(sample == 2)
                {
                    auto const elapsed{std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()};
                    std::printf("{\"round\":%u,\"offset\":%u,\"nanoseconds\":%lld,\"checksum\":%u}\n",
                                round, offset, static_cast<long long>(elapsed), expected);
                }
            }
        }
    }
}
