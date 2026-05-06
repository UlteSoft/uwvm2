#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;

    struct runtime_smoke_module
    {
        byte_vec wasm{};
        ::std::uint32_t entry_index{};
    };

    [[nodiscard]] runtime_smoke_module build_runtime_smoke_module()
    {
        module_builder mb{};

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

        func_body entry{};
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_a_index);
        op(entry.code, wasm_op::call);
        u32(entry.code, callee_b_index);
        op(entry.code, wasm_op::end);
        auto const entry_index{mb.add_func(void_ty, ::std::move(entry))};

        return runtime_smoke_module{.wasm = mb.build(), .entry_index = entry_index};
    }

    [[nodiscard]] int test_lazy_runtime()
    {
        auto sm{build_runtime_smoke_module()};
        auto prep{prepare_runtime_from_wasm(sm.wasm, u8"uwvm2test_lazy_runtime")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        configure_lazy_runtime(0uz, 5uz);
        ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(
            u8"uwvm2test_lazy_runtime",
            ::uwvm2::runtime::lib::lazy_compile_run_config{.entry_function_index = sm.entry_index, .assume_full_code_verified = false});

        return 0;
    }
}  // namespace

int main()
{
    return test_lazy_runtime();
}
