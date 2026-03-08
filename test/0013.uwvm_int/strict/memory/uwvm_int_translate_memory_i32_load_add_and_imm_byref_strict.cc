#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32_1(::std::int32_t a)
    {
        return pack_i32(a);
    }

    [[nodiscard]] byte_vec build_i32_load_add_and_imm_byref_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32 addr) -> i32
        //   local.get addr; i32.load; i32.const 7; i32.add   (expects i32_load_add_imm byref fptr)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i32 addr) -> i32
        //   local.get addr; i32.load; i32.const 0xff; i32.and  (expects i32_load_and_imm byref fptr)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_load);
            u32(c, 2u);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 0xff);
            op(c, wasm_op::i32_and);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_i32_load_add_and_imm_byref_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);

        // Verify we emitted the byref (non-tailcall) fused opfunc pointers.
        {
            optable::uwvm_interpreter_stacktop_currpos_t curr{};
            auto const exp_add =
                optable::translate::get_uwvmint_i32_load_add_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
            auto const exp_and =
                optable::translate::get_uwvmint_i32_load_and_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_add));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_and));
        }

        using Runner = interpreter_runner<Opt>;

        ::std::int32_t const got_add =
            load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                 rt.local_defined_function_vec_storage.index_unchecked(0),
                                 pack_i32_1(0),
                                 nullptr,
                                 nullptr)
                         .results);
        UWVM2TEST_REQUIRE(got_add == 7);

        ::std::int32_t const got_and =
            load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                 rt.local_defined_function_vec_storage.index_unchecked(1),
                                 pack_i32_1(0),
                                 nullptr,
                                 nullptr)
                         .results);
        UWVM2TEST_REQUIRE(got_and == 0);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_i32_load_add_and_imm_byref() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i32_load_add_and_imm_byref_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_load_add_and_imm_byref");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref (non-tailcall): covers translate.h `CompileOption.is_tail_call == false` emission branch.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_i32_load_add_and_imm_byref_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_i32_load_add_and_imm_byref();
}
