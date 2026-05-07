#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;

    struct runtime_smoke_module
    {
        byte_vec wasm{};
        ::std::uint32_t entry_index{};
    };

    [[nodiscard]] runtime_smoke_module build_runtime_smoke_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        mb.table_has_max = true;
        mb.table_max = 1u;
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        func_type void_ty{{}, {}};

        func_body callee_a{};
        op(callee_a.code, wasm_op::i32_const);
        i32(callee_a.code, 17);
        op(callee_a.code, wasm_op::drop);
        op(callee_a.code, wasm_op::end);
        auto const callee_a_index{mb.add_func(void_ty, ::std::move(callee_a))};

        func_body callee_b{};
        op(callee_b.code, wasm_op::block);
        strict::append_u8(callee_b.code, k_block_empty);
        op(callee_b.code, wasm_op::loop);
        strict::append_u8(callee_b.code, k_block_empty);
        op(callee_b.code, wasm_op::br);
        u32(callee_b.code, 1u);
        op(callee_b.code, wasm_op::end);
        op(callee_b.code, wasm_op::end);
        op(callee_b.code, wasm_op::end);
        auto const callee_b_index{mb.add_func(void_ty, ::std::move(callee_b))};

        func_type branch_ty{{k_val_i32}, {}};
        func_body callee_c{};
        op(callee_c.code, wasm_op::local_get);
        u32(callee_c.code, 0u);
        op(callee_c.code, wasm_op::if_);
        strict::append_u8(callee_c.code, k_block_empty);
        op(callee_c.code, wasm_op::i32_const);
        i32(callee_c.code, 11);
        op(callee_c.code, wasm_op::drop);
        op(callee_c.code, wasm_op::else_);
        op(callee_c.code, wasm_op::block);
        strict::append_u8(callee_c.code, k_block_empty);
        op(callee_c.code, wasm_op::i32_const);
        i32(callee_c.code, 22);
        op(callee_c.code, wasm_op::drop);
        op(callee_c.code, wasm_op::end);
        op(callee_c.code, wasm_op::end);
        op(callee_c.code, wasm_op::end);
        auto const callee_c_index{mb.add_func(branch_ty, ::std::move(callee_c))};

        func_body callee_d{};
        op(callee_d.code, wasm_op::i32_const);
        i32(callee_d.code, 4);
        op(callee_d.code, wasm_op::i32_const);
        i32(callee_d.code, static_cast<::std::int32_t>(0x11223344u));
        op(callee_d.code, wasm_op::i32_store);
        u32(callee_d.code, 2u);
        u32(callee_d.code, 0u);
        op(callee_d.code, wasm_op::i32_const);
        i32(callee_d.code, 4);
        op(callee_d.code, wasm_op::i32_load);
        u32(callee_d.code, 2u);
        u32(callee_d.code, 0u);
        op(callee_d.code, wasm_op::drop);
        op(callee_d.code, wasm_op::end);
        auto const callee_d_index{mb.add_func(void_ty, ::std::move(callee_d))};

        func_body callee_e{};
        op(callee_e.code, wasm_op::i32_const);
        i32(callee_e.code, 0);
        op(callee_e.code, wasm_op::call_indirect);
        u32(callee_e.code, 0u);
        u32(callee_e.code, 0u);
        op(callee_e.code, wasm_op::end);
        auto const callee_e_index{mb.add_func(void_ty, ::std::move(callee_e))};

        strict::element_segment seg{};
        op(seg.offset_expr, wasm_op::i32_const);
        i32(seg.offset_expr, 0);
        op(seg.offset_expr, wasm_op::end);
        seg.func_indices.push_back(callee_a_index);
        mb.elements.push_back(::std::move(seg));

        func_body entry{};
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_a_index);
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_b_index);
        op(entry.code, wasm_op::i32_const);
        i32(entry.code, 1);
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_c_index);
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_d_index);
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_e_index);
        op(entry.code, wasm_op::end);
        auto const entry_index{mb.add_func(void_ty, ::std::move(entry))};

        return runtime_smoke_module{.wasm = mb.build(), .entry_index = entry_index};
    }

    struct runtime_scenario
    {
        ::std::size_t worker_count{};
        ::std::size_t scheduling_size{};
        mode::runtime_scheduling_policy_t policy{mode::runtime_scheduling_policy_t::code_size};
        bool assume_full_code_verified{};
    };

    [[nodiscard]] int run_runtime_scenario(runtime_scenario sc)
    {
        auto sm{build_runtime_smoke_module()};
        auto prep{prepare_runtime_from_wasm(sm.wasm, u8"uwvm2test_lazy_runtime")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        configure_lazy_runtime(sc.worker_count, sc.scheduling_size);
        mode::global_runtime_scheduling_policy = sc.policy;
        mode::global_runtime_mode = sc.assume_full_code_verified ? mode::runtime_mode_t::lazy_compile_with_full_code_verification
                                                                 : mode::runtime_mode_t::lazy_compile;
        ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(
            u8"uwvm2test_lazy_runtime",
            ::uwvm2::runtime::lib::lazy_compile_run_config{.entry_function_index = sm.entry_index,
                                                           .assume_full_code_verified = sc.assume_full_code_verified});

        return 0;
    }

#if defined(__unix__) || defined(__APPLE__)
    template <typename Fn>
    [[nodiscard]] int run_child_expect_zero(Fn&& fn)
    {
        pid_t const pid = ::fork();
        if(pid == 0)
        {
            auto const rc{fn()};
            _exit(rc == 0 ? 0 : 1);
        }
        if(pid < 0) { return strict::fail(__LINE__, "fork"); }

        int status{};
        if(::waitpid(pid, &status, 0) < 0) { return strict::fail(__LINE__, "waitpid"); }
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) { return strict::fail(__LINE__, "child runtime scenario failed"); }
        return 0;
    }
#endif

    [[nodiscard]] int test_lazy_runtime()
    {
        constexpr runtime_scenario scenarios[]{
            runtime_scenario{.worker_count = 1uz,
                             .scheduling_size = 5uz,
                             .policy = mode::runtime_scheduling_policy_t::code_size,
                             .assume_full_code_verified = false},
            runtime_scenario{.worker_count = 2uz,
                             .scheduling_size = 3uz,
                             .policy = mode::runtime_scheduling_policy_t::code_size,
                             .assume_full_code_verified = false},
            runtime_scenario{.worker_count = 1uz,
                             .scheduling_size = 1uz,
                             .policy = mode::runtime_scheduling_policy_t::function_count,
                             .assume_full_code_verified = false},
            runtime_scenario{.worker_count = 0uz,
                             .scheduling_size = 4uz,
                             .policy = mode::runtime_scheduling_policy_t::code_size,
                             .assume_full_code_verified = true},
        };

        for(auto const& sc: scenarios)
        {
#if defined(__unix__) || defined(__APPLE__)
            UWVM2TEST_REQUIRE(run_child_expect_zero([&]() noexcept { return run_runtime_scenario(sc); }) == 0);
#else
            UWVM2TEST_REQUIRE(run_runtime_scenario(sc) == 0);
#endif
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_lazy_runtime();
}
