#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)

int main() { return 0; }

#else

#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
    using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
    using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
    using wasm_externref = ::uwvm2::object::global::wasm_externref_t;

    constexpr ::std::uint8_t k_val_v128{0x7bu};
    constexpr ::std::uint8_t k_val_externref{0x6fu};

    template <typename ValueType, ::std::size_t Index>
    struct mutable_host_global
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view global_name{
            []() constexpr noexcept -> ::uwvm2::utils::container::u8string_view
            {
                if constexpr(Index == 0uz) { return u8"g_i32"; }
                else if constexpr(Index == 1uz) { return u8"g_i64"; }
                else if constexpr(Index == 2uz) { return u8"g_f32"; }
                else if constexpr(Index == 3uz) { return u8"g_f64"; }
                else if constexpr(Index == 4uz) { return u8"g_v128"; }
                else if constexpr(Index == 5uz) { return u8"g_funcref"; }
                else { return u8"g_externref"; }
            }()};
        inline static constexpr bool is_mutable{true};
        using value_type = ValueType;

        value_type value{};

        friend value_type global_get(mutable_host_global& global) noexcept { return global.value; }
        friend void global_set(mutable_host_global& global, value_type value) noexcept { global.value = value; }
    };

    using host_i32_global = mutable_host_global<wasm_i32, 0uz>;
    using host_i64_global = mutable_host_global<wasm_i64, 1uz>;
    using host_f32_global = mutable_host_global<wasm_f32, 2uz>;
    using host_f64_global = mutable_host_global<wasm_f64, 3uz>;
    using host_v128_global = mutable_host_global<wasm_v128, 4uz>;
    using host_funcref_global = mutable_host_global<wasm_funcref, 5uz>;
    using host_externref_global = mutable_host_global<wasm_externref, 6uz>;

    struct host_globals_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"host_globals"};
        using local_global_tuple = ::uwvm2::utils::container::tuple<host_i32_global,
                                                                    host_i64_global,
                                                                    host_f32_global,
                                                                    host_f64_global,
                                                                    host_v128_global,
                                                                    host_funcref_global,
                                                                    host_externref_global>;
        local_global_tuple local_global{};
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_module<host_globals_module>);

    [[nodiscard]] byte_vec build_i32_alias_module()
    {
        module_builder mb{};
        mb.add_import_global("host_globals", "g_i32", k_val_i32, true);
        mb.add_export_global(0u, "g_i32");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_consumer_module()
    {
        module_builder mb{};

        // The first import exercises Wasm import forwarding into a host leaf. The remaining
        // imports exercise direct local-imported leaves for every supported global carrier.
        mb.add_import_global("global_alias", "g_i32", k_val_i32, true);
        mb.add_import_global("host_globals", "g_i64", k_val_i64, true);
        mb.add_import_global("host_globals", "g_f32", k_val_f32, true);
        mb.add_import_global("host_globals", "g_f64", k_val_f64, true);
        mb.add_import_global("host_globals", "g_v128", k_val_v128, true);
        mb.add_import_global("host_globals", "g_funcref", k_ref_funcref, true);
        mb.add_import_global("host_globals", "g_externref", k_val_externref, true);

        auto const append_roundtrip_func{[&](::std::uint8_t value_type, ::std::uint32_t global_index)
                                         {
                                             func_type ty{{value_type}, {value_type}};
                                             func_body fb{};
                                             append_u8(fb.code, u8(wasm_op::local_get));
                                             append_u32_leb(fb.code, 0u);
                                             append_u8(fb.code, u8(wasm_op::global_set));
                                             append_u32_leb(fb.code, global_index);
                                             append_u8(fb.code, u8(wasm_op::global_get));
                                             append_u32_leb(fb.code, global_index);
                                             append_u8(fb.code, u8(wasm_op::end));
                                             (void)mb.add_func(::std::move(ty), ::std::move(fb));
                                         }};

        append_roundtrip_func(k_val_i32, 0u);
        append_roundtrip_func(k_val_i64, 1u);
        append_roundtrip_func(k_val_f32, 2u);
        append_roundtrip_func(k_val_f64, 3u);
        append_roundtrip_func(k_val_v128, 4u);
        append_roundtrip_func(k_ref_funcref, 5u);
        append_roundtrip_func(k_val_externref, 6u);

        // Matches the i32 global fusion pattern. Host-backed globals must deliberately stay on
        // the typed bridge path because the fused helper requires a direct storage pointer.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            append_u8(fb.code, u8(wasm_op::global_get));
            append_u32_leb(fb.code, 0u);
            append_u8(fb.code, u8(wasm_op::i32_const));
            append_i32_leb(fb.code, 1);
            append_u8(fb.code, u8(wasm_op::i32_add));
            append_u8(fb.code, u8(wasm_op::global_set));
            append_u32_leb(fb.code, 0u);
            append_u8(fb.code, u8(wasm_op::global_get));
            append_u32_leb(fb.code, 0u);
            append_u8(fb.code, u8(wasm_op::end));
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ValueType>
    [[nodiscard]] byte_vec pack_value(ValueType const& value)
    {
        byte_vec packed(sizeof(value));
        ::std::memcpy(packed.data(), ::std::addressof(value), sizeof(value));
        return packed;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename GlobalT>
    [[nodiscard]] bool contains_local_imported_bridge_pair(compiled_local_func_t const& func) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        bool found_get{};
        bool found_set{};
        auto const check_curr{[&](optable::uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
                              {
                                  auto const get_fptr =
                                      optable::translate::get_uwvmint_local_imported_global_get_typed_fptr_from_tuple<Opt, GlobalT>(curr, tuple);
                                  auto const set_fptr =
                                      optable::translate::get_uwvmint_local_imported_global_set_typed_fptr_from_tuple<Opt, GlobalT>(curr, tuple);
                                  found_get = found_get || bytecode_contains_fptr(func.op.operands, get_fptr);
                                  found_set = found_set || bytecode_contains_fptr(func.op.operands, set_fptr);
                              }};

        if constexpr(Opt.is_tail_call && optable::variable_details::stacktop_enabled_for<Opt, GlobalT>())
        {
            constexpr auto begin{optable::variable_details::range_begin<Opt, GlobalT>()};
            constexpr auto end{optable::variable_details::range_end<Opt, GlobalT>()};
            for(::std::size_t pos{begin}; pos != end; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                if constexpr(::std::same_as<GlobalT, wasm_i32>) { curr.i32_stack_top_curr_pos = pos; }
                else if constexpr(::std::same_as<GlobalT, wasm_i64>) { curr.i64_stack_top_curr_pos = pos; }
                else if constexpr(::std::same_as<GlobalT, wasm_f32>) { curr.f32_stack_top_curr_pos = pos; }
                else if constexpr(::std::same_as<GlobalT, wasm_f64>) { curr.f64_stack_top_curr_pos = pos; }
                else if constexpr(::std::same_as<GlobalT, wasm_v128>) { curr.v128_stack_top_curr_pos = pos; }
                check_curr(curr);
            }
        }
        else
        {
            check_curr({});
        }

        return found_get && found_set;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt, wasm_feature_parameter_t const& features) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm{compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features))};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 8uz);

        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_i32>(cm.local_funcs.index_unchecked(0uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_i64>(cm.local_funcs.index_unchecked(1uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_f32>(cm.local_funcs.index_unchecked(2uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_f64>(cm.local_funcs.index_unchecked(3uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_v128>(cm.local_funcs.index_unchecked(4uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_funcref>(cm.local_funcs.index_unchecked(5uz))));
        UWVM2TEST_REQUIRE((contains_local_imported_bridge_pair<Opt, wasm_externref>(cm.local_funcs.index_unchecked(6uz))));

        wasm_v128 v128{};
        auto* const v128_bytes{reinterpret_cast<unsigned char*>(::std::addressof(v128))};
        for(::std::size_t i{}; i != sizeof(v128); ++i) { v128_bytes[i] = static_cast<unsigned char>(i * 13uz + 7uz); }

        wasm_funcref funcref{};
        funcref.ref.storage.func_idx = 0u;
        funcref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;

        static int extern_object{42};
        wasm_externref externref{};
        externref.ref.storage.ptr = ::std::addressof(extern_object);
        externref.ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_extern;

        byte_vec const packed_values[]{
            pack_value(wasm_i32{0x1234567}),
            pack_value(wasm_i64{0x123456789abcdefll}),
            pack_value(wasm_f32{123.25f}),
            pack_value(wasm_f64{-9876.5}),
            pack_value(v128),
            pack_value(funcref),
            pack_value(externref),
        };

        using runner = interpreter_runner<Opt>;
        for(::std::size_t i{}; i != 7uz; ++i)
        {
            auto const result{runner::run(cm.local_funcs.index_unchecked(i),
                                          rt.local_defined_function_vec_storage.index_unchecked(i),
                                          packed_values[i],
                                          nullptr,
                                          nullptr)};
            UWVM2TEST_REQUIRE(result.results.size() == packed_values[i].size());
            bool roundtrip_matches{};
            if(i == 5uz)
            {
                wasm_funcref actual{};
                ::std::memcpy(::std::addressof(actual), result.results.data(), sizeof(actual));
                roundtrip_matches = actual.ref.kind == funcref.ref.kind && actual.ref.storage.func_idx == funcref.ref.storage.func_idx;
            }
            else if(i == 6uz)
            {
                wasm_externref actual{};
                ::std::memcpy(::std::addressof(actual), result.results.data(), sizeof(actual));
                roundtrip_matches = actual.ref.kind == externref.ref.kind && actual.ref.storage.ptr == externref.ref.storage.ptr;
            }
            else
            {
                roundtrip_matches = ::std::memcmp(result.results.data(), packed_values[i].data(), packed_values[i].size()) == 0;
            }
            if(!roundtrip_matches)
            {
                ::std::fprintf(stderr, "local-imported global roundtrip mismatch at type index %zu\n", i);
                return fail(__LINE__, "local-imported global roundtrip result");
            }
        }

        auto const increment1{runner::run(cm.local_funcs.index_unchecked(7uz),
                                          rt.local_defined_function_vec_storage.index_unchecked(7uz),
                                          {},
                                          nullptr,
                                          nullptr)};
        auto const increment2{runner::run(cm.local_funcs.index_unchecked(7uz),
                                          rt.local_defined_function_vec_storage.index_unchecked(7uz),
                                          {},
                                          nullptr,
                                          nullptr)};
        UWVM2TEST_REQUIRE(load_i32(increment1.results) == wasm_i32{0x1234568});
        UWVM2TEST_REQUIRE(load_i32(increment2.results) == wasm_i32{0x1234569});
        return 0;
    }

    [[nodiscard]] int test_local_imported_global_bridge() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto const alias_wasm{build_i32_alias_module()};
        auto const consumer_wasm{build_consumer_module()};
        auto const features{make_wasm1p1_feature_parameter()};
        ::uwvm2::uwvm::wasm::type::local_imported_t host_module{host_globals_module{}};

        auto prep{prepare_runtime_from_wasm(consumer_wasm,
                                            u8"local_imported_global_consumer",
                                            {{.wasm_bytes = ::std::addressof(alias_wasm), .module_name = u8"global_alias"}},
                                            features,
                                            {host_module})};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        auto const& rt{*prep.mod};
        UWVM2TEST_REQUIRE(rt.imported_global_vec_storage.size() == 7uz);

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, features) == 0);
        }
        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, features) == 0);
        }
        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, features) == 0);
        }
        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt, features) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_local_imported_global_bridge();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

#endif
