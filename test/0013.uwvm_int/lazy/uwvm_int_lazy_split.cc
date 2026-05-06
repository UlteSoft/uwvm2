#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;

    [[nodiscard]] byte_vec build_lazy_split_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::local_get);
            u32(c, 0u);
            for(::std::int32_t i{}; i != 12; ++i)
            {
                op(c, wasm_op::i32_const);
                i32(c, i);
                op(c, wasm_op::drop);
            }
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c{fb.code};

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            strict::append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::else_);
            op(c, wasm_op::block);
            strict::append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 22);
            op(c, wasm_op::end);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_lazy_split()
    {
        auto wasm{build_lazy_split_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_split")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        {
            lazy_split_config_t function_cfg{.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only,
                                             .cu_policy = lazy::lazy_compile_unit_split_policy_t::function,
                                             .linear_eu_code_size = 0uz,
                                             .cu_code_size = 0uz};
            auto function_storage{initialize_lazy_storage(*prep.mod, function_cfg)};
            UWVM2TEST_REQUIRE(function_storage.functions.size() == 2uz);

            auto const& fn{function_storage.functions.index_unchecked(0)};
            UWVM2TEST_REQUIRE(fn.eu_count == 1uz);
            UWVM2TEST_REQUIRE(fn.cu_count == 1uz);
            UWVM2TEST_REQUIRE(fn.primary_cu_index == fn.first_cu_index);

            auto const& eu{function_storage.execution_units.index_unchecked(fn.first_eu_index)};
            UWVM2TEST_REQUIRE(eu.kind == lazy::lazy_execution_unit_kind::function);
            UWVM2TEST_REQUIRE(eu.parent_eu_index == SIZE_MAX);

            auto const& cu{function_storage.compile_units.index_unchecked(fn.primary_cu_index)};
            UWVM2TEST_REQUIRE(cu.kind == lazy::lazy_compile_unit_kind::function);
            UWVM2TEST_REQUIRE(cu.begin_eu_index == fn.first_eu_index);
            UWVM2TEST_REQUIRE(cu.end_eu_index == fn.first_eu_index + 1uz);
            UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
        }

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 2uz);

        auto const& fn0{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn0.eu_count > 3uz);
        UWVM2TEST_REQUIRE(fn0.cu_count > 1uz);
        UWVM2TEST_REQUIRE(fn0.primary_cu_index != SIZE_MAX);

        ::std::size_t linear_count{};
        for(::std::size_t i{fn0.first_eu_index}; i != fn0.first_eu_index + fn0.eu_count; ++i)
        {
            auto const& eu{storage.execution_units.index_unchecked(i)};
            if(eu.kind == lazy::lazy_execution_unit_kind::linear_chunk) { ++linear_count; }
            UWVM2TEST_REQUIRE(eu.function_index == fn0.function_index);
            UWVM2TEST_REQUIRE(eu.local_function_index == 0uz);
            UWVM2TEST_REQUIRE(eu.code_begin <= eu.code_end);
        }
        UWVM2TEST_REQUIRE(linear_count > 1uz);

        auto const& fn1{storage.functions.index_unchecked(1)};
        ::std::size_t if_eu{SIZE_MAX};
        ::std::size_t else_eu{SIZE_MAX};
        ::std::size_t block_in_else{SIZE_MAX};

        for(::std::size_t i{fn1.first_eu_index}; i != fn1.first_eu_index + fn1.eu_count; ++i)
        {
            auto const& eu{storage.execution_units.index_unchecked(i)};
            if(eu.kind == lazy::lazy_execution_unit_kind::if_) { if_eu = i; }
            if(eu.kind == lazy::lazy_execution_unit_kind::else_) { else_eu = i; }
        }

        UWVM2TEST_REQUIRE(if_eu != SIZE_MAX);
        UWVM2TEST_REQUIRE(else_eu != SIZE_MAX);

        auto const& if_unit{storage.execution_units.index_unchecked(if_eu)};
        auto const& else_unit{storage.execution_units.index_unchecked(else_eu)};
        UWVM2TEST_REQUIRE(else_unit.parent_eu_index == if_eu);
        UWVM2TEST_REQUIRE(if_unit.code_end == else_unit.code_begin);

        for(::std::size_t i{fn1.first_eu_index}; i != fn1.first_eu_index + fn1.eu_count; ++i)
        {
            auto const& eu{storage.execution_units.index_unchecked(i)};
            if(eu.kind == lazy::lazy_execution_unit_kind::block && eu.parent_eu_index == else_eu) { block_in_else = i; }
        }
        UWVM2TEST_REQUIRE(block_in_else != SIZE_MAX);

        for(::std::size_t i{fn0.first_cu_index}; i != fn0.first_cu_index + fn0.cu_count; ++i)
        {
            auto const& cu{storage.compile_units.index_unchecked(i)};
            UWVM2TEST_REQUIRE(cu.function_index == fn0.function_index);
            UWVM2TEST_REQUIRE(cu.local_function_index == 0uz);
            UWVM2TEST_REQUIRE(cu.begin_eu_index < cu.end_eu_index);
            UWVM2TEST_REQUIRE(cu.code_begin <= cu.code_end);
            UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
        }

        {
            lazy_split_config_t eu_cfg{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control_and_linear_chunks,
                                       .cu_policy = lazy::lazy_compile_unit_split_policy_t::execution_unit,
                                       .linear_eu_code_size = 5uz,
                                       .cu_code_size = 0uz};
            auto eu_storage{initialize_lazy_storage(*prep.mod, eu_cfg)};
            auto const& fn{eu_storage.functions.index_unchecked(0)};

            ::std::size_t linear_eu_count{};
            for(::std::size_t i{fn.first_eu_index}; i != fn.first_eu_index + fn.eu_count; ++i)
            {
                if(eu_storage.execution_units.index_unchecked(i).kind == lazy::lazy_execution_unit_kind::linear_chunk) { ++linear_eu_count; }
            }

            UWVM2TEST_REQUIRE(linear_eu_count > 1uz);
            UWVM2TEST_REQUIRE(fn.cu_count == linear_eu_count);

            for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
            {
                auto const& cu{eu_storage.compile_units.index_unchecked(i)};
                UWVM2TEST_REQUIRE(cu.kind == lazy::lazy_compile_unit_kind::execution_unit);
                UWVM2TEST_REQUIRE(cu.end_eu_index == cu.begin_eu_index + 1uz);
                UWVM2TEST_REQUIRE(eu_storage.execution_units.index_unchecked(cu.begin_eu_index).kind == lazy::lazy_execution_unit_kind::linear_chunk);
                UWVM2TEST_REQUIRE(cu.materialization_scope == lazy::lazy_materialization_scope::whole_function);
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_lazy_split();
}
