// Compare this exact harness against the repository HEAD and the working tree.
// Parsing, validation, translation and the reference calculation are outside the
// timed region. Each invocation executes a long Wasm loop and checks its result.
#include "../strict/uwvm_int_translate_strict_common.h"
#include "memory_performance_reference.h"
#include "memory_performance_timing.h"
#include <fstream>

int main(int argc, char** argv)
{
    using namespace uwvm2test::uwvm_int_strict;
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
    install_unexpected_traps();
    auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_performance", {}, features)};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    optable::compile_option options{};
    uwvm2::validation::error::code_validation_error_impl error{};
    auto compiled{compiler::compile_all_from_uwvm_single_func<ring>(*prepared.mod, options, error, &features)};
    UWVM2TEST_REQUIRE(error.err_code == uwvm2::validation::error::code_validation_error_code::ok);
    if(auto const dump{std::getenv("UWVM_TEST_DUMP_BYTECODE")}; dump != nullptr)
    {
        auto const& bytes{compiled.local_funcs.index_unchecked(0).op.operands};
        std::ofstream output(dump, std::ios::binary);
        output.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        UWVM2TEST_REQUIRE(output.good());
        // Test-only address anchor lets the external inspector normalize PIE
        // symbol addresses without changing the executed interpreter stream.
        std::fprintf(stderr, "main=%zx\n", reinterpret_cast<std::uintptr_t>(&main));
    }
    auto arguments{pack_i32(static_cast<std::int32_t>(iterations))};
    auto const expected{memory_performance_checksum(argv[1], iterations)};
    std::printf("{\"iterations\":%u,\"checksum\":%u", iterations, expected);
    memory_performance_timer timer{};
    for(unsigned sample{}; sample != samples + 2u; ++sample)
    {
        timer.begin();
        auto result{interpreter_runner<ring>::run(compiled.local_funcs.index_unchecked(0),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(0), arguments, nullptr, nullptr)};
        timer.end(sample >= 2u ? sample - 2u : 0u, sample >= 2u);
        UWVM2TEST_REQUIRE(result.results.size() == 4u);
        UWVM2TEST_REQUIRE(static_cast<std::uint32_t>(load_i32(result.results)) == expected);

    }
    timer.print(samples);
}
