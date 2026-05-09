#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    using errc = ::uwvm2::validation::error::code_validation_error_code;
    using lazy_compile_request_t = ::uwvm2::utils::thread::lazy_compile_request;
    using lazy_function_storage_t = lazy::lazy_function_storage_t;

    enum : ::std::size_t
    {
        k_fn_add = 0uz,
        k_fn_structured = 1uz,
        k_fn_memory = 2uz,
        k_fn_call_indirect = 3uz,
        k_fn_count = 4uz,
    };

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_strategy_matrix_module()
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

        func_type i32_i32_to_i32{{strict::k_val_i32, strict::k_val_i32}, {strict::k_val_i32}};
        func_type i32_to_i32{{strict::k_val_i32}, {strict::k_val_i32}};
        func_type void_to_i32{{}, {strict::k_val_i32}};

        {
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(i32_i32_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            auto& c{fb.code};
            op(c, wasm_op::block);
            strict::append_u8(c, strict::k_val_i32);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            strict::append_u8(c, strict::k_val_i32);
            op(c, wasm_op::block);
            strict::append_u8(c, strict::k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::end);
            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const);
            i32(c, 22);
            op(c, wasm_op::end);
            op(c, wasm_op::end);
            op(c, wasm_op::end);
            (void)mb.add_func(i32_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 12);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, static_cast<::std::int32_t>(0x55667788u));
            op(fb.code, wasm_op::i32_store);
            u32(fb.code, 2u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 12);
            op(fb.code, wasm_op::i32_load);
            u32(fb.code, 2u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(void_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0);
            op(fb.code, wasm_op::call_indirect);
            u32(fb.code, 0u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(i32_i32_to_i32, ::std::move(fb));
        }

        strict::element_segment seg{};
        op(seg.offset_expr, wasm_op::i32_const);
        i32(seg.offset_expr, 0);
        op(seg.offset_expr, wasm_op::end);
        seg.func_indices.push_back(k_fn_add);
        mb.elements.push_back(::std::move(seg));

        return mb.build();
    }

    [[nodiscard]] inline ::uwvm2::utils::container::u8string_view module_name_view(char const* ascii) noexcept
    {
        return ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(ascii), ::std::strlen(ascii)};
    }

    [[nodiscard]] ::std::size_t count_non_function_eus(lazy_module_t const& storage, lazy_function_storage_t const& fn) noexcept
    {
        ::std::size_t count{};
        for(::std::size_t i{fn.first_eu_index}; i != fn.first_eu_index + fn.eu_count; ++i)
        {
            if(storage.execution_units.index_unchecked(i).kind != lazy::lazy_execution_unit_kind::function) { ++count; }
        }
        return count;
    }

    [[nodiscard]] int verify_split_invariants(lazy_module_t const& storage, lazy_split_config_t cfg)
    {
        UWVM2TEST_REQUIRE(storage.functions.size() == k_fn_count);

        auto const& add_fn{storage.functions.index_unchecked(k_fn_add)};
        UWVM2TEST_REQUIRE(add_fn.eu_count == 1uz);
        UWVM2TEST_REQUIRE(add_fn.cu_count == 1uz);
        UWVM2TEST_REQUIRE(add_fn.primary_cu_index == add_fn.first_cu_index);

        auto const& structured_fn{storage.functions.index_unchecked(k_fn_structured)};
        if(cfg.eu_policy == lazy::lazy_execution_unit_split_policy_t::function_only)
        {
            UWVM2TEST_REQUIRE(structured_fn.eu_count == 1uz);
            UWVM2TEST_REQUIRE(structured_fn.cu_count == 1uz);
            return 0;
        }

        auto const non_function_eus{count_non_function_eus(storage, structured_fn)};
        UWVM2TEST_REQUIRE(structured_fn.eu_count == non_function_eus + 1uz);
        UWVM2TEST_REQUIRE(non_function_eus == 3uz);

        switch(cfg.cu_policy)
        {
            case lazy::lazy_compile_unit_split_policy_t::function:
            {
                UWVM2TEST_REQUIRE(structured_fn.cu_count == 1uz);
                break;
            }
            case lazy::lazy_compile_unit_split_policy_t::execution_unit:
            {
                UWVM2TEST_REQUIRE(structured_fn.cu_count == non_function_eus);
                break;
            }
            case lazy::lazy_compile_unit_split_policy_t::code_size:
            {
                if(cfg.cu_code_size <= 1uz) { UWVM2TEST_REQUIRE(structured_fn.cu_count == non_function_eus); }
                else if(cfg.cu_code_size >= 4096uz) { UWVM2TEST_REQUIRE(structured_fn.cu_count == 1uz); }
                else { UWVM2TEST_REQUIRE(structured_fn.cu_count >= 1uz && structured_fn.cu_count <= non_function_eus); }
                break;
            }
        }

        UWVM2TEST_REQUIRE(structured_fn.primary_cu_index != SIZE_MAX);
        for(::std::size_t i{structured_fn.first_cu_index}; i != structured_fn.first_cu_index + structured_fn.cu_count; ++i)
        {
            auto const& cu{storage.compile_units.index_unchecked(i)};
            UWVM2TEST_REQUIRE(cu.local_function_index == k_fn_structured);
            UWVM2TEST_REQUIRE(cu.begin_eu_index < cu.end_eu_index);
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int verify_results(prepared_runtime const& prep, lazy_module_t const& storage)
    {
        using Runner = interpreter_runner<Opt>;

        {
            auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_add),
                                prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_add),
                                pack_i32x2(19, 23),
                                nullptr,
                                nullptr)};
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
        }

        {
            auto rr_true{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_structured),
                                     prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_structured),
                                     strict::pack_i32(1),
                                     nullptr,
                                     nullptr)};
            auto rr_false{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_structured),
                                      prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_structured),
                                      strict::pack_i32(0),
                                      nullptr,
                                      nullptr)};
            UWVM2TEST_REQUIRE(load_i32(rr_true.results) == 11);
            UWVM2TEST_REQUIRE(load_i32(rr_false.results) == 22);
        }

        {
            auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_memory),
                                prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_memory),
                                strict::pack_no_params(),
                                nullptr,
                                nullptr)};
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == 0x55667788u);
        }

        {
            auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_call_indirect),
                                prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_call_indirect),
                                pack_i32x2(20, 22),
                                nullptr,
                                nullptr)};
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_remaining_functions(prepared_runtime const& prep,
                                                  lazy_module_t& storage,
                                                  lazy_compile_options_t& options,
                                                  ::std::size_t already_compiled_index)
    {
        for(::std::size_t i{}; i != storage.functions.size(); ++i)
        {
            if(i == already_compiled_index) { continue; }
            if(storage.functions.index_unchecked(i).materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled) { continue; }

            ::uwvm2::validation::error::code_validation_error_impl local_err{};
            auto const& fn{storage.functions.index_unchecked(i)};
            UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);
            ::std::fprintf(stderr, "  compile_remaining_functions fn=%zu\n", i);
            try
            {
                lazy::compile_cu_from_lazy_validator<Opt>(*prep.mod, storage, options, fn.primary_cu_index, local_err);
            }
            catch(::fast_io::error const&)
            {
                switch(i)
                {
                    case 0uz: return strict::fail(__LINE__, "compile_remaining_functions_fn0");
                    case 1uz: return strict::fail(__LINE__, "compile_remaining_functions_fn1");
                    case 2uz: return strict::fail(__LINE__, "compile_remaining_functions_fn2");
                    case 3uz: return strict::fail(__LINE__, "compile_remaining_functions_fn3");
                    default: return strict::fail(__LINE__, "compile_remaining_functions_fn?");
                }
            }
            catch(...)
            {
                switch(i)
                {
                    case 0uz: return strict::fail(__LINE__, "compile_remaining_functions_uncaught_fn0");
                    case 1uz: return strict::fail(__LINE__, "compile_remaining_functions_uncaught_fn1");
                    case 2uz: return strict::fail(__LINE__, "compile_remaining_functions_uncaught_fn2");
                    case 3uz: return strict::fail(__LINE__, "compile_remaining_functions_uncaught_fn3");
                    default: return strict::fail(__LINE__, "compile_remaining_functions_uncaught_fn?");
                }
            }
            UWVM2TEST_REQUIRE(local_err.err_code == errc::ok);
            UWVM2TEST_REQUIRE(storage.functions.index_unchecked(i).materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled);
        }
        return 0;
    }

    struct compile_case
    {
        char const* module_name_ascii{};
        lazy_split_config_t split{};
        lazy_validation_mode_t validation_mode{lazy_validation_mode_t::validate_on_lazy_compile};
        ::std::size_t worker_count{};
        unsigned priority{};
        bool use_direct_compile{};
        ::std::size_t compile_function_index{};
        bool expect_primary_request{true};
    };

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_compile_case(compile_case cc)
    {
        auto wasm{build_strategy_matrix_module()};
        auto prep{prepare_runtime_from_wasm(wasm, module_name_view(cc.module_name_ascii))};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, cc.split)};
        UWVM2TEST_REQUIRE(verify_split_invariants(storage, cc.split) == 0);

        auto options{make_lazy_options(module_name_view(cc.module_name_ascii), cc.validation_mode)};
        if(cc.validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        runner_call_indirect_bridge_scope<Opt> bridge_scope{prep, storage};

        auto const& fn{storage.functions.index_unchecked(cc.compile_function_index)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto const compile_unit_index =
            cc.expect_primary_request || fn.cu_count == 1uz ? fn.primary_cu_index : (fn.first_cu_index + fn.cu_count - 1uz);

        ::uwvm2::validation::error::code_validation_error_impl err{};

        if(cc.use_direct_compile)
        {
            ::std::fprintf(stderr, "  direct_compile fn=%zu\n", compile_unit_index);
            try
            {
                lazy::compile_cu_from_lazy_validator<Opt>(*prep.mod, storage, options, compile_unit_index, err);
            }
            catch(::fast_io::error const&)
            {
                return strict::fail(__LINE__, cc.module_name_ascii);
            }
            catch(...)
            {
                return strict::fail(__LINE__, "direct_compile_uncaught");
            }
        }
        else
        {
            lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                                   .lazy_storage = ::std::addressof(storage),
                                                   .options = options,
                                                   .compile_unit_index = compile_unit_index,
                                                   .err = ::std::addressof(err),
                                                   .module_name = module_name_view(cc.module_name_ascii)};

            auto request{lazy::make_lazy_compile_request<Opt>(ctx, cc.priority)};
            UWVM2TEST_REQUIRE(request.unit != nullptr);
            UWVM2TEST_REQUIRE(request.compile != nullptr);

            ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
            ::std::fprintf(stderr, "  scheduler_compile fn=%zu worker=%zu priority=%u\n", compile_unit_index, cc.worker_count, cc.priority);
            try
            {
                scheduler.start({.worker_count = cc.worker_count});
                UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
                scheduler.stop();
            }
            catch(::fast_io::error const&)
            {
                return strict::fail(__LINE__, cc.module_name_ascii);
            }
            catch(...)
            {
                return strict::fail(__LINE__, "scheduler_uncaught");
            }
        }

        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(storage.functions.index_unchecked(cc.compile_function_index).materialization_state.state.load(::std::memory_order_acquire) ==
                          lazy_compile_state_t::compiled);

        for(::std::size_t i{fn.first_cu_index}; i != fn.first_cu_index + fn.cu_count; ++i)
        {
            UWVM2TEST_REQUIRE(storage.compile_units.index_unchecked(i).state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled);
        }

        UWVM2TEST_REQUIRE(compile_remaining_functions<Opt>(prep, storage, options, cc.compile_function_index) == 0);
        UWVM2TEST_REQUIRE(!storage.compiled.local_funcs.index_unchecked(cc.compile_function_index).op.operands.empty());
        UWVM2TEST_REQUIRE(verify_results<Opt>(prep, storage) == 0);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int test_invalid_request_guards()
    {
        auto wasm{build_strategy_matrix_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_strategy_invalid_request")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        auto options{make_lazy_options(u8"uwvm2test_lazy_strategy_invalid_request", lazy_validation_mode_t::validate_on_lazy_compile)};
        UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = storage.compile_units.size(),
                                               .err = ::std::addressof(err),
                                               .module_name = u8"uwvm2test_lazy_strategy_invalid_request"};
        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 0u)};
        UWVM2TEST_REQUIRE(request.unit == nullptr);
        UWVM2TEST_REQUIRE(request.compile == nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 1uz});
        UWVM2TEST_REQUIRE(!scheduler.try_request({}));
        UWVM2TEST_REQUIRE(!scheduler.ensure_ready({}));
        scheduler.request_or_compile_inline({});
        scheduler.stop();
        return 0;
    }

    [[nodiscard]] int test_lazy_strategy_matrix()
    {
        configure_unexpected_traps();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};

        constexpr compile_case cases[]{
            compile_case{.module_name_ascii = "uwvm2test_lazy_strategy_function_only_validate",
                         .split = lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only,
                                                      .cu_policy = lazy::lazy_compile_unit_split_policy_t::function,
                                                      .cu_code_size = 0uz},
                         .validation_mode = lazy_validation_mode_t::validate_on_lazy_compile,
                         .worker_count = 0uz,
                         .priority = 1u,
                         .use_direct_compile = true,
                         .compile_function_index = k_fn_add},
            compile_case{.module_name_ascii = "uwvm2test_lazy_strategy_structured_function_assume",
                         .split = lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                                      .cu_policy = lazy::lazy_compile_unit_split_policy_t::function,
                                                      .cu_code_size = 0uz},
                         .validation_mode = lazy_validation_mode_t::assume_full_code_verified,
                         .worker_count = 0uz,
                         .priority = 1u,
                         .use_direct_compile = true,
                         .compile_function_index = k_fn_structured},
            compile_case{.module_name_ascii = "uwvm2test_lazy_strategy_eu_validate",
                         .split = lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                                      .cu_policy = lazy::lazy_compile_unit_split_policy_t::execution_unit,
                                                      .cu_code_size = 0uz},
                         .validation_mode = lazy_validation_mode_t::validate_on_lazy_compile,
                         .worker_count = 2uz,
                         .priority = 0u,
                         .use_direct_compile = false,
                         .compile_function_index = k_fn_structured,
                         .expect_primary_request = false},
            compile_case{.module_name_ascii = "uwvm2test_lazy_strategy_code_size_small_validate",
                         .split = lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                                      .cu_policy = lazy::lazy_compile_unit_split_policy_t::code_size,
                                                      .cu_code_size = 1uz},
                         .validation_mode = lazy_validation_mode_t::validate_on_lazy_compile,
                         .worker_count = 1uz,
                         .priority = 0u,
                         .use_direct_compile = false,
                         .compile_function_index = k_fn_structured,
                         .expect_primary_request = false},
            compile_case{.module_name_ascii = "uwvm2test_lazy_strategy_code_size_large_assume",
                         .split = lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                                      .cu_policy = lazy::lazy_compile_unit_split_policy_t::code_size,
                                                      .cu_code_size = 4096uz},
                         .validation_mode = lazy_validation_mode_t::assume_full_code_verified,
                         .worker_count = 0uz,
                         .priority = 1u,
                         .use_direct_compile = false,
                         .compile_function_index = k_fn_call_indirect},
        };

        for(auto const& cc: cases)
        {
            ::std::fprintf(stderr, "uwvm2test lazy strategy case: %s\n", cc.module_name_ascii);
            try
            {
                UWVM2TEST_REQUIRE(run_compile_case<opt>(cc) == 0);
            }
            catch(::fast_io::error const&)
            {
                return strict::fail(__LINE__, cc.module_name_ascii);
            }
            catch(...)
            {
                return strict::fail(__LINE__, "uncaught exception");
            }
        }
        UWVM2TEST_REQUIRE(test_invalid_request_guards<opt>() == 0);
        return 0;
    }
}  // namespace

int main()
{
#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
    return 0;
#else
    return test_lazy_strategy_matrix();
#endif
}
