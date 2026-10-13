// Check memory.copy/init trap atomicity in the still-live interpreter module.
#include "memory_bulk_atomicity_common.h"

namespace
{
    using namespace uwvm2test::wasm3_bulk_atomicity;
    std::size_t expected_wasm_length{};

    void UWVM2TEST_WASM_ABI on_oob(uwvm2::object::memory::error::memory_error_t const& error) noexcept
    {
        if(error.memory_type_size != expected_wasm_length || (error.memory_length != committed_length && error.memory_length != passive_payload.size()))
        {
            ::_exit(74);
        }
        verify_unchanged_at_trap();
    }

    template <optable::uwvm_interpreter_translate_option_t Option>
    int check(byte_vec const& wasm, bool copy, bool memory64)
    {
        auto features{make_wasm1p1_feature_parameter()};
        auto& wasm1p1{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        wasm1p1.disable_multi_memory = false;
        wasm1p1.disable_memory64 = !memory64;
        auto prepared{prepare_runtime_from_wasm(wasm, u8"memory_bulk_atomicity_int", {}, features)};
        UWVM2TEST_REQUIRE(prepared.mod->local_defined_memory_vec_storage.size() == 2);
        auto& source{const_cast<uwvm2::object::memory::linear::native_memory_t&>(prepared.mod->local_defined_memory_vec_storage.index_unchecked(0).memory)};
        auto& target{const_cast<uwvm2::object::memory::linear::native_memory_t&>(prepared.mod->local_defined_memory_vec_storage.index_unchecked(1).memory)};
        optable::compile_option options{};
        uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, options, error, &features)};
        UWVM2TEST_REQUIRE(error.err_code == uwvm2::validation::error::code_validation_error_code::ok);
        auto const& function{compiled.local_funcs.index_unchecked(0)};
        auto const& runtime_function{prepared.mod->local_defined_function_vec_storage.index_unchecked(0)};
        reset_memories(source.memory_begin, target.memory_begin, page_size);
        UWVM2TEST_REQUIRE(observer_selftest(source.memory_begin, target.memory_begin));
        std::puts("PASS observer-negative-controls=2");
        auto const invoke{[&](std::uint64_t dst, std::uint64_t src, std::uint64_t count)
                          {
                              auto const args{parameters(copy, memory64, dst, src, count)};
                              auto const result{interpreter_runner<Option>::run(function, runtime_function, args, nullptr, nullptr)};
                              return result.results.empty();
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
                                 return invoke(dst, src, count) && verify_positive(source.memory_begin, target.memory_begin, length, dst, src, count, copy);
                             }};
            UWVM2TEST_REQUIRE(valid(128, 0, 16));
            UWVM2TEST_REQUIRE(valid(length, copy ? length : passive_payload.size(), 0));
            if(generation != 0)
            {
                // The exact old 64 KiB boundary is now ordinary committed memory.
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
                auto const child{::fork()};
                UWVM2TEST_REQUIRE(child >= 0);
                if(child == 0)
                {
                    ::alarm(10);
                    expected_wasm_length = static_cast<std::size_t>(bad.len);
                    optable::trap_memory_out_of_bounds_func = on_oob;
                    invoke(bad.dst, bad.src, bad.len);
                    ::_exit(73);  // An invalid range must trap before a write.
                }
                int status{};
                UWVM2TEST_REQUIRE(::waitpid(child, &status, 0) == child);
                if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
                {
                    std::fprintf(stderr,
                                 "FAIL int memory.%s %s generation=%u memory%u status=%d\n",
                                 copy ? "copy" : "init",
                                 bad.name,
                                 generation,
                                 memory64 ? 64u : 32u,
                                 status);
                    return 1;
                }
                std::printf("PASS same-instance memory.%s %s generation=%u memory%u checked-bytes=%zu\n",
                            copy ? "copy" : "init",
                            bad.name,
                            generation,
                            memory64 ? 64u : 32u,
                            2 * length);
            }
        }
        return 0;
    }
}  // namespace

int main(int argc, char** argv)
{
    if(argc != 4) { return 64; }
    auto const mode{std::string_view{argv[2]}};
    auto const type{std::string_view{argv[3]}};
    if((mode != "copy" && mode != "init") || (type != "memory32" && type != "memory64")) { return 64; }
    auto const wasm{read_wasm(argv[1])};
    UWVM2TEST_REQUIRE(!wasm.empty());
    install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call = false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{.is_tail_call = true,
                                                                .i32_stack_top_begin_pos = 3uz,
                                                                .i32_stack_top_end_pos = 5uz,
                                                                .i64_stack_top_begin_pos = 3uz,
                                                                .i64_stack_top_end_pos = 5uz,
                                                                .f32_stack_top_begin_pos = 5uz,
                                                                .f32_stack_top_end_pos = 7uz,
                                                                .f64_stack_top_begin_pos = 5uz,
                                                                .f64_stack_top_end_pos = 7uz};
    UWVM2TEST_REQUIRE(check<uncached>(wasm, mode == "copy", type == "memory64") == 0);
#if !defined(UWVM2TEST_UNCACHED_ONLY)
    UWVM2TEST_REQUIRE(check<ring>(wasm, mode == "copy", type == "memory64") == 0);
    std::puts("PASS 16 same-instance fault/readback checks, 2 interpreter dispatches, before/after grow");
#else
    std::puts("PASS 8 same-instance fault/readback checks, uncached interpreter, before/after grow");
#endif
}
