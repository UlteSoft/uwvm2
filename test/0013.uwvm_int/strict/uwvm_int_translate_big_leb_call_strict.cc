#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] inline ::std::uint32_t load_u32(::std::byte const* p) noexcept
    {
        ::std::uint32_t v{};
        ::std::memcpy(::std::addressof(v), p, sizeof(v));
        return v;
    }

    inline void store_u32(::std::byte* p, ::std::uint32_t v) noexcept
    {
        ::std::memcpy(p, ::std::addressof(v), sizeof(v));
    }

    static void call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        using kind_t = optable::trivial_defined_call_kind;
        using info_t = optable::compiled_defined_call_info;

        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(wasm_module_id != SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const* const info = reinterpret_cast<info_t const*>(call_function);
        if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(info->trivial_kind == kind_t::none) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const top = *stack_top_ptr;
        auto const top_addr = reinterpret_cast<::std::uintptr_t>(top);
        if(top_addr < info->param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::byte* const base = reinterpret_cast<::std::byte*>(top_addr - info->param_bytes);

        ::std::uint_least32_t const imm_u32{::std::bit_cast<::std::uint_least32_t>(info->trivial_imm)};

        auto finish_void = [&]() noexcept { *stack_top_ptr = base; };
        auto finish_i32 = [&](::std::uint32_t v) noexcept
        {
            store_u32(base, v);
            *stack_top_ptr = base + 4;
        };

        switch(info->trivial_kind)
        {
            case kind_t::nop_void:
                finish_void();
                break;
            case kind_t::const_i32:
                finish_i32(static_cast<::std::uint32_t>(imm_u32));
                break;
            case kind_t::param0_i32:
                finish_i32(load_u32(base));
                break;
            default:
                ::fast_io::fast_terminate();
        }
    }

    [[nodiscard]] byte_vec build_big_leb_call_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // Build 129 functions so `call 128` uses a 2-byte LEB.
        // f0: call f128
        // f1..f127: return 0
        // f128: return 777
        for(::std::uint32_t i = 0u; i < 129u; ++i)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            if(i == 0u)
            {
                op(c, wasm_op::call);
                u32(c, 128u);
                op(c, wasm_op::end);
            }
            else if(i == 128u)
            {
                op(c, wasm_op::i32_const);
                i32(c, 777);
                op(c, wasm_op::end);
            }
            else
            {
                op(c, wasm_op::i32_const);
                i32(c, 0);
                op(c, wasm_op::end);
            }

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_big_leb_call_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f128: callee
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(128),
                                              rt.local_defined_function_vec_storage.index_unchecked(128),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 777);

        // f0: caller (big leb call index)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 777);

        // f1: dummy
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 0);

        return 0;
    }

    [[nodiscard]] int test_translate_big_leb_call() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +call_bridge;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_big_leb_call_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_big_leb_call");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_big_leb_call_suite<opt>(rt) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_big_leb_call_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_big_leb_call();
}
