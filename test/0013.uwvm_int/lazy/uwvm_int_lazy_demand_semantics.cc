#include "uwvm_int_lazy_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    enum : ::std::size_t
    {
        k_fn_leaf = 0uz,
        k_fn_direct = 1uz,
        k_fn_indirect = 2uz,
        k_fn_probe = 3uz,
        k_fn_count = 4uz,
    };

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_demand_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;
        mb.table_has_max = true;
        mb.table_max = 1u;

        auto op = [](byte_vec& c, wasm_op o) { strict::append_u8(c, u8(o)); };
        auto u32 = [](byte_vec& c, ::std::uint32_t v) { strict::append_u32_leb(c, v); };
        auto i32 = [](byte_vec& c, ::std::int32_t v) { strict::append_i32_leb(c, v); };

        func_type i32_to_i32{{k_val_i32}, {k_val_i32}};
        func_type i32_i32_to_i32{{k_val_i32, k_val_i32}, {k_val_i32}};
        func_type void_ty{{}, {}};

        {
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::i32_add);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(i32_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::call);
            u32(fb.code, k_fn_leaf);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(i32_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::call_indirect);
            u32(fb.code, 0u);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(i32_i32_to_i32, ::std::move(fb));
        }

        {
            func_body fb{};
            op(fb.code, wasm_op::nop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(void_ty, ::std::move(fb));
        }

        strict::element_segment seg{};
        op(seg.offset_expr, wasm_op::i32_const);
        i32(seg.offset_expr, 0);
        op(seg.offset_expr, wasm_op::end);
        seg.func_indices.push_back(k_fn_leaf);
        mb.elements.push_back(::std::move(seg));

        return mb.build();
    }

    [[nodiscard]] bool function_is_compiled(lazy_module_t const& storage, ::std::size_t local_index) noexcept
    {
        return storage.functions.index_unchecked(local_index).materialization_state.state.load(::std::memory_order_acquire) ==
               lazy_compile_state_t::compiled;
    }

    [[nodiscard]] ::std::size_t compiled_function_count(lazy_module_t const& storage) noexcept
    {
        ::std::size_t total{};
        for(auto const& fn: storage.functions)
        {
            if(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled) { ++total; }
        }
        return total;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_local_via_scheduler(prepared_runtime const& prep,
                                                  lazy_module_t& storage,
                                                  lazy_compile_options_t& options,
                                                  ::uwvm2::utils::container::u8string_view module_name,
                                                  ::std::size_t local_index,
                                                  ::std::size_t worker_count,
                                                  unsigned priority,
                                                  bool queue_duplicate,
                                                  ::uwvm2::utils::thread::lazy_compile_scheduler_stats_snapshot& stats_out)
    {
        auto const& fn{storage.functions.index_unchecked(local_index)};
        UWVM2TEST_REQUIRE(fn.primary_cu_index != SIZE_MAX);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::lazy_compile_request_context ctx{.curr_module = prep.mod,
                                               .lazy_storage = ::std::addressof(storage),
                                               .options = options,
                                               .compile_unit_index = fn.primary_cu_index,
                                               .err = ::std::addressof(err),
                                               .module_name = module_name};

        auto request{lazy::make_lazy_compile_request<Opt>(ctx, priority)};
        UWVM2TEST_REQUIRE(request.unit != nullptr);
        UWVM2TEST_REQUIRE(request.compile != nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = worker_count});

        if(queue_duplicate)
        {
            UWVM2TEST_REQUIRE(scheduler.try_request(request));
            UWVM2TEST_REQUIRE(scheduler.try_request(request));
            UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        }
        else
        {
            UWVM2TEST_REQUIRE(scheduler.ensure_ready(request));
        }

        auto const stats{scheduler.snapshot_stats()};
        scheduler.stop();

        UWVM2TEST_REQUIRE(err.err_code == errc::ok);
        UWVM2TEST_REQUIRE(function_is_compiled(storage, local_index));
        stats_out = stats;
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int test_direct_call_demand_compile()
    {
        auto wasm{build_demand_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_demand_direct")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == k_fn_count);

        auto options{make_lazy_options(u8"uwvm2test_lazy_demand_direct", lazy_validation_mode_t::validate_on_lazy_compile)};
        UWVM2TEST_REQUIRE(options.validator_module_storage != nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler_stats_snapshot stats{};
        UWVM2TEST_REQUIRE(compile_local_via_scheduler<Opt>(prep,
                                                           storage,
                                                           options,
                                                           u8"uwvm2test_lazy_demand_direct",
                                                           k_fn_direct,
                                                           1uz,
                                                           0u,
                                                           true,
                                                           stats) == 0);
        UWVM2TEST_REQUIRE(stats.enqueued_requests == 1uz);
        UWVM2TEST_REQUIRE(stats.duplicate_requests >= 1uz);
        UWVM2TEST_REQUIRE(stats.inline_compiles == 0uz);
        UWVM2TEST_REQUIRE(stats.helper_compiles == 0uz);
        UWVM2TEST_REQUIRE(stats.worker_compiles == 1uz);

        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 1uz);

        runner_lazy_call_bridge_scope<Opt> bridge_scope{prep, storage, options};
        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_direct),
                            prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_direct),
                            pack_i32(41),
                            nullptr,
                            nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);

        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 2uz);

        auto rr_second{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_direct),
                                   prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_direct),
                                   pack_i32(9),
                                   nullptr,
                                   nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr_second.results) == 10);
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 2uz);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int test_indirect_call_demand_compile()
    {
        auto wasm{build_demand_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_lazy_demand_indirect")};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        auto storage{initialize_lazy_storage(*prep.mod, small_code_size_split_config())};
        UWVM2TEST_REQUIRE(storage.functions.size() == k_fn_count);

        auto options{make_lazy_options(u8"uwvm2test_lazy_demand_indirect", lazy_validation_mode_t::assume_full_code_verified)};
        UWVM2TEST_REQUIRE(options.validator_module_storage == nullptr);

        ::uwvm2::utils::thread::lazy_compile_scheduler_stats_snapshot stats{};
        UWVM2TEST_REQUIRE(compile_local_via_scheduler<Opt>(prep,
                                                           storage,
                                                           options,
                                                           u8"uwvm2test_lazy_demand_indirect",
                                                           k_fn_indirect,
                                                           0uz,
                                                           1u,
                                                           false,
                                                           stats) == 0);
        UWVM2TEST_REQUIRE(stats.enqueued_requests == 0uz);
        UWVM2TEST_REQUIRE(stats.duplicate_requests == 0uz);
        UWVM2TEST_REQUIRE(stats.inline_compiles == 1uz);
        UWVM2TEST_REQUIRE(stats.worker_compiles == 0uz);
        UWVM2TEST_REQUIRE(stats.helper_compiles == 0uz);

        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 1uz);

        runner_lazy_call_bridge_scope<Opt> bridge_scope{prep, storage, options};
        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_indirect),
                            prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_indirect),
                            pack_i32x2(41, 0),
                            nullptr,
                            nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);

        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 2uz);

        auto rr_second{Runner::run(storage.compiled.local_funcs.index_unchecked(k_fn_indirect),
                                   prep.mod->local_defined_function_vec_storage.index_unchecked(k_fn_indirect),
                                   pack_i32x2(8, 0),
                                   nullptr,
                                   nullptr)};
        UWVM2TEST_REQUIRE(load_i32(rr_second.results) == 9);
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_leaf));
        UWVM2TEST_REQUIRE(function_is_compiled(storage, k_fn_indirect));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_direct));
        UWVM2TEST_REQUIRE(!function_is_compiled(storage, k_fn_probe));
        UWVM2TEST_REQUIRE(compiled_function_count(storage) == 2uz);
        return 0;
    }

    [[nodiscard]] int test_lazy_demand_semantics()
    {
        configure_unexpected_traps();

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
        UWVM2TEST_REQUIRE(test_direct_call_demand_compile<opt>() == 0);
        UWVM2TEST_REQUIRE(test_indirect_call_demand_compile<opt>() == 0);
        return 0;
    }
}  // namespace

int main()
{
    return test_lazy_demand_semantics();
}
