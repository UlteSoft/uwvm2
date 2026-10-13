// Inspect the still-committed bytes from the actual trap callback. A process
// merely reporting an OOB trap cannot prove that a split store wrote no prefix.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    std::byte const volatile* untouched{};
    std::byte const volatile* selected{};
    std::size_t selected_length{65536};
    std::size_t expected_width{}, expected_address{};
    // A distinct source prefix lets fused same-memory copies demonstrably alter
    // a destination. Copying the original fill byte would hide a partial write.
    constexpr std::byte initial_byte(std::size_t index) noexcept
    { return index < 16 ? std::byte{0x3c} : std::byte{0xa5}; }
    void reset_selected(std::byte* base, std::size_t length)
    {
        std::memset(base, 0xa5, length);
        std::memset(base, 0x3c, 16);
    }

    [[noreturn]] void check_prefix() noexcept
    {
        // Both complete committed mappings remain owned by the prepared module
        // until _exit. Volatile reads observe any partial native store before
        // the synchronous trap. No allocation or stdio occurs in this callback.
        for(std::size_t i{}; i != 2 * 65536; ++i)
        { if(untouched[i] != std::byte{0x11}) { ::_exit(71); } }
        for(std::size_t i{}; i != selected_length; ++i)
        { if(selected[i] != initial_byte(i)) { ::_exit(72); } }
        ::_exit(0);
    }
    void fault(uwvm2::object::memory::error::mmap_memory_error_t const& error) noexcept
    {
        if(error.memory_idx != 1 || error.memory_length != selected_length || error.memory_offset < selected_length ||
           error.memory_offset - selected_length >= expected_width) { ::_exit(74); }
        check_prefix();
    }
    void UWVM2TEST_WASM_ABI software_fault(uwvm2::object::memory::error::memory_error_t const& error) noexcept
    {
        if(error.memory_idx != 1 || error.memory_length != selected_length || error.memory_type_size != expected_width ||
           error.memory_offset.offset_65_bit || error.memory_offset.offset != expected_address) { ::_exit(74); }
        check_prefix();
    }

    template <optable::uwvm_interpreter_translate_option_t Option>
    int check(byte_vec const& wasm, unsigned width, bool growth, bool memory64)
    {
        auto features{make_wasm1p1_feature_parameter()};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_multi_memory = false;
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_memory64 = !memory64;
        auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_store_atomicity", {}, features)};
        auto& memory0{prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory};
        // The harness exposes a const module view; initializer-owned memory
        // storage itself is mutable, as in the runtime memory.grow path.
        auto& memory1{const_cast<uwvm2::object::memory::linear::native_memory_t&>(
            prepared.mod->local_defined_memory_vec_storage.index_unchecked(1).memory)};
        std::memset(memory0.memory_begin, 0x11, 2 * 65536);
        reset_selected(memory1.memory_begin, 65536);
        untouched = memory0.memory_begin;
        selected = memory1.memory_begin;
        selected_length = 65536;
        optable::compile_option options{};
        uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, options, error, &features)};
        UWVM2TEST_REQUIRE(error.err_code == uwvm2::validation::error::code_validation_error_code::ok);
        // Capture the store's actual bit pattern in a valid execution. The
        // same compiled function must remain valid when growth commits pages.
        auto const& function{compiled.local_funcs.index_unchecked(0)};
        auto const& runtime_function{prepared.mod->local_defined_function_vec_storage.index_unchecked(0)};
        auto const valid_address{memory64 ? pack_i64(32) : pack_i32(32)};
        auto const valid_result{interpreter_runner<Option>::run(function, runtime_function, valid_address, nullptr, nullptr)};
        UWVM2TEST_REQUIRE(valid_result.results.size() == 4 && load_i32(valid_result.results) == 0);
        std::byte pattern[16]{};
        std::memcpy(pattern, memory1.memory_begin + 32, width);
        expected_width = width;
        reset_selected(memory1.memory_begin, selected_length);
        for(unsigned generation{}; generation != (growth ? 2u : 1u); ++generation)
        {
            if(generation != 0)
            {
                auto const old_base{memory1.memory_begin};
                UWVM2TEST_REQUIRE(memory1.grow_strictly(1, 2 * 65536));
                UWVM2TEST_REQUIRE(!std::remove_reference_t<decltype(memory1)>::can_mmap || memory1.memory_begin == old_base);
                selected = memory1.memory_begin;
                selected_length = 2 * 65536;
                for(unsigned prefix{1}; prefix < width; ++prefix)
                {
                    reset_selected(memory1.memory_begin, selected_length);
                    auto const address{65536u - prefix};
                    auto const arguments{memory64 ? pack_i64(address) : pack_i32(address)};
                    auto const result{interpreter_runner<Option>::run(function, runtime_function, arguments, nullptr, nullptr)};
                    UWVM2TEST_REQUIRE(result.results.size() == 4 && load_i32(result.results) == 0);
                    for(std::size_t i{}; i != selected_length; ++i)
                    {
                        auto const expected{i >= address && i - address < width ? pattern[i - address] : initial_byte(i)};
                        UWVM2TEST_REQUIRE(memory1.memory_begin[i] == expected);
                        UWVM2TEST_REQUIRE(memory0.memory_begin[i] == std::byte{0x11});
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
                    uwvm2::object::memory::signal::set_mmap_memory_out_of_bounds_handler(fault);
                    optable::trap_memory_out_of_bounds_func = software_fault;
                    expected_address = selected_length - prefix;
                    auto const arguments{memory64 ? pack_i64(expected_address) : pack_i32(expected_address)};
                    interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
                        prepared.mod->local_defined_function_vec_storage.index_unchecked(0), arguments, nullptr, nullptr);
                    ::_exit(73); // The selected-memory store must trap.
                }
                int status{};
                UWVM2TEST_REQUIRE(::waitpid(child, &status, 0) == child);
                if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
                {
                    std::fprintf(stderr, "FAIL split store width=%u prefix=%u tail=%u memory64=%u status=%d\n",
                                 width, prefix, unsigned(Option.is_tail_call), unsigned(memory64), status);
                    std::abort();
                }
            }
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if(argc < 3 || argc > 5) { return 64; }
    bool growth{}, memory64{};
    for(int i{3}; i < argc; ++i)
    {
        if(std::strcmp(argv[i], "growth") == 0 && !growth) { growth = true; }
        else if(std::strcmp(argv[i], "memory64") == 0 && !memory64) { memory64 = true; }
        else { return 64; }
    }
    auto const width{static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10))};
    if(width != 2 && width != 4 && width != 8 && width != 16) { return 64; }
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) { return 65; }
    byte_vec wasm{};
    for(char c; input.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
    UWVM2TEST_REQUIRE(input.eof());
    install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call = false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    UWVM2TEST_REQUIRE(check<uncached>(wasm, width, growth, memory64) == 0);
#if !defined(UWVM2TEST_UNCACHED_ONLY)
    UWVM2TEST_REQUIRE(check<ring>(wasm, width, growth, memory64) == 0);
    std::printf("PASS %u split-store prefix checks, uncached and register-ring, memory%s\n",
                2 * (growth ? 2 : 1) * (width - 1), memory64 ? "64" : "32");
#else
    std::printf("PASS %u split-store prefix checks, uncached only, memory%s\n",
                (growth ? 2 : 1) * (width - 1), memory64 ? "64" : "32");
#endif
    if(growth) { std::puts("PASS unchanged compiled function across memory.grow; old split boundary becomes writable"); }
}
