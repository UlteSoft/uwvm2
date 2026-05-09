#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    using errc = ::uwvm2::validation::error::code_validation_error_code;
    using lazy_compile_request_t = ::uwvm2::utils::thread::lazy_compile_request;

    [[nodiscard]] byte_vec build_add_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::local_get);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 5);
        op(fb.code, wasm_op::i32_add);
        op(fb.code, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_bad_local_module()
    {
        module_builder mb{};

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::local_get);
        u32(fb.code, 1u);
        op(fb.code, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int verify_compiled_and_run(prepared_runtime const& prep,
                                              lazy_module_t const& storage,
                                              lazy_compile_request_t const& request,
                                              ::uwvm2::validation::error::code_validation_error_impl const& err)
    {
        UWVM2TEST_REQUIRE(request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled);
        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(!storage.compiled.local_funcs.index_unchecked(0).op.operands.empty());

        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(0),
                            prep.mod->local_defined_function_vec_storage.index_unchecked(0),
                            pack_i32(37),
                            nullptr,
                            nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_inline_high_priority(byte_vec const& wasm,
                                                           ::uwvm2::utils::container::u8string_view module_name,
                                                           lazy_validation_mode_t validation_mode)
    {
        auto prep{prepare_runtime_from_wasm(wasm, module_name)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);
        auto const& fn{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 1u)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);
        UWVM2TEST_REQUIRE(request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state_t::uncompiled);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 1uz});
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        scheduler.stop();

        return verify_compiled_and_run<Opt>(prep, storage, request, err);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_queued(byte_vec const& wasm,
                                             ::uwvm2::utils::container::u8string_view module_name,
                                             lazy_validation_mode_t validation_mode)
    {
        auto prep{prepare_runtime_from_wasm(wasm, module_name)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);
        auto const& fn{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 0u)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 1uz});
        UWVM2TEST_REQUIRE(scheduler.try_request(request));
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        scheduler.stop();

        return verify_compiled_and_run<Opt>(prep, storage, request, err);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run_inline_no_workers(byte_vec const& wasm,
                                                        ::uwvm2::utils::container::u8string_view module_name,
                                                        lazy_validation_mode_t validation_mode)
    {
        auto prep{prepare_runtime_from_wasm(wasm, module_name)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);
        auto const& fn{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 0u)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 0uz});
        UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        scheduler.stop();

        return verify_compiled_and_run<Opt>(prep, storage, request, err);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_via_request_or_compile_inline(byte_vec const& wasm,
                                                            ::uwvm2::utils::container::u8string_view module_name,
                                                            lazy_validation_mode_t validation_mode)
    {
        auto prep{prepare_runtime_from_wasm(wasm, module_name)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == 1uz);
        auto const& fn{storage.functions.index_unchecked(0)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        auto options{make_lazy_options(module_name, validation_mode)};
        if(validation_mode == lazy_validation_mode_t::validate_on_lazy_compile) { UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr); }
        else { UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 0u)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 0uz});
        scheduler.request_or_compile_inline(request);
        scheduler.stop();

        return verify_compiled_and_run<Opt>(prep, storage, request, err);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_invalid_expect_failure()
    {
        auto wasm{build_bad_local_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_invalid_local")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        auto const& fn{storage.functions.index_unchecked(0)};

        auto options{make_lazy_options(u8"uwvm2test_lazy_invalid_local", lazy_validation_mode_t::validate_on_lazy_compile)};
        UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = u8"uwvm2test_lazy_invalid_local"};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, 1u)};
        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = 0uz});
        UWVM2TEST_REQUIRE(!scheduler.ensure_ready(request));
        scheduler.stop();

        UWVM2TEST_REQUIRE(request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state_t::failed);
        UWVM2TEST_REQUIRE(err.err_code == errc::illegal_local_index);

        return 0;
    }

    [[nodiscard]] int test_lazy_scheduler()
    {
        configure_unexpected_traps();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
        auto wasm{build_add_module()};
        UWVM2TEST_REQUIRE(
            compile_and_run_inline_high_priority<opt>(wasm, u8"uwvm2test_lazy_scheduler_validate", lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(
            compile_and_run_inline_high_priority<opt>(wasm, u8"uwvm2test_lazy_scheduler_assume", lazy_validation_mode_t::assume_full_code_verified) == 0);
        UWVM2TEST_REQUIRE(
            compile_and_run_queued<opt>(wasm, u8"uwvm2test_lazy_scheduler_queued", lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(
            compile_and_run_inline_no_workers<opt>(wasm, u8"uwvm2test_lazy_scheduler_inline", lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(compile_via_request_or_compile_inline<opt>(
                              wasm,
                              u8"uwvm2test_lazy_scheduler_request_or_compile_inline",
                              lazy_validation_mode_t::validate_on_lazy_compile) == 0);
        UWVM2TEST_REQUIRE(compile_invalid_expect_failure<opt>() == 0);

        return 0;
    }
}  // namespace

int main()
{
#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
    return 0;
#else
    return test_lazy_scheduler();
#endif
}
