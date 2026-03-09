#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_i64(::std::int32_t a, ::std::int64_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32_f64(::std::int32_t a, double b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] ::std::size_t bytecode_count_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
    {
        if(fptr == nullptr) { return 0uz; }
        ::std::array<::std::byte, sizeof(Fptr)> needle{};
        ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(Fptr));
        if(bytes.size() < needle.size()) { return 0uz; }

        ::std::size_t count{};
        for(::std::size_t i{}; i + needle.size() <= bytes.size(); ++i)
        {
            if(::std::memcmp(bytes.data() + i, needle.data(), needle.size()) == 0) { ++count; }
        }
        return count;
    }

    [[nodiscard]] byte_vec build_memory_offset_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: local.get addr ; i32.load offset=12  => i32_load_localget_off with non-zero memarg offset
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 112);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x12345678u));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get addr ; i64.load offset=16  => i64_load_localget_off with non-zero memarg offset
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 216);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x1122334455667788ull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get addr ; i64.load offset=24 ; local.set tmp ; local.get tmp
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 324);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x0123456789abcdefull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 24u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get base ; i32.const 20 ; i32.add ; i32.load offset=8 => i32_load_local_plus_imm with non-zero memarg offset
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 428);
            op(c, wasm_op::i32_const); i32(c, 55);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 8u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get addr ; local.get value ; i32.store offset=12 => i32_store_localget_off with non-zero memarg offset
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 12u);

            op(c, wasm_op::i32_const); i32(c, 512);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get addr ; local.get value ; i64.store offset=16 => i64_store_localget_off with non-zero memarg offset
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 16u);

            op(c, wasm_op::i32_const); i32(c, 616);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local.get base ; i32.const 24 ; i32.add ; local.get value ; i32.store offset=8 => i32_store_local_plus_imm with non-zero memarg offset
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 8u);

            op(c, wasm_op::i32_const); i32(c, 732);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        // f7: local.get base ; i32.const 28 ; i32.add ; f32.load offset=4
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 804);
            op(c, wasm_op::f32_const); f32(c, 3.75f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 28);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 4u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: local.get base ; i32.const 32 ; i32.add ; local.get value ; f64.store offset=8
        {
            func_type ty{{k_val_i32, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 32);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 8u);

            op(c, wasm_op::i32_const); i32(c, 840);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
#endif

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 9uz);
#else
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 7uz);
#endif
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                            pack_i32(100),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x12345678u);

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(1),
                                                                            pack_i32(200),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x1122334455667788ull);

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(2),
                                                                            pack_i32(300),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x0123456789abcdefull);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(400),
                                              nullptr,
                                              nullptr)
                                       .results) == 55);

        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(4),
                                                                            pack_i32x2(500, static_cast<::std::int32_t>(0x55667788u)),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x55667788u);

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(5),
                                                                            pack_i32_i64(600, static_cast<::std::int64_t>(0x123456789abcdef0ull)),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x123456789abcdef0ull);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i32x2(700, 77),
                                              nullptr,
                                              nullptr)
                                       .results) == 77);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i32(772),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.75f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i32_f64(800, 9.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 9.5);
#endif
        return 0;
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_i32_load_localget = optable::translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_load_localget = optable::translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_load_set_local = optable::translate::get_uwvmint_i64_load_localget_set_local_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i32_load_local_plus_imm = optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i32_store_localget = optable::translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_store_localget = optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i32_store_local_plus_imm = optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple);

        auto const exp_i32_load_plain = optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_load_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i32_store_plain = optable::translate::get_uwvmint_i32_store_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_local_set_i64 = optable::translate::get_uwvmint_local_set_i64_fptr_from_tuple<Opt>(curr, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i64_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i64_load_set_local));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i64_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_local_set_i64));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i32_store_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_i64_store_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_i64_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_i32_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_i32_store_plain));

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const exp_f32_load_local_plus_imm = optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_f64_store_local_plus_imm = optable::translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_f64_store_plain = optable::translate::get_uwvmint_f64_store_fptr_from_tuple<Opt>(curr, mem, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_store_plain));
# endif
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_local_get_i64 = optable::translate::get_uwvmint_local_get_i64_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_local_set_i64 = optable::translate::get_uwvmint_local_set_i64_fptr_from_tuple<Opt>(curr, tuple);

        auto const exp_i32_load_localget = optable::translate::get_uwvmint_i32_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_i32_load_local_plus_imm =
            optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_i32_store_local_plus_imm =
            optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);

        auto const exp_i32_load_plain = optable::translate::get_uwvmint_i32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_i64_load_plain = optable::translate::get_uwvmint_i64_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_i32_store_plain = optable::translate::get_uwvmint_i32_store_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;
        auto const& bc5 = cm.local_funcs.index_unchecked(5).op.operands;
        auto const& bc6 = cm.local_funcs.index_unchecked(6).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_load_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_local_get_i32) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_load_plain));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_set_i64));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i64));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc2, exp_local_get_i32) == 1uz);
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc2, exp_local_get_i64) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_i32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_i32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_i32_store_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc4, exp_local_get_i32) == 2uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc5, exp_i64_store_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc5, exp_local_get_i32) == 1uz);
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc5, exp_local_get_i64) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc6, exp_i32_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc6, exp_i32_store_plain));

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const exp_f32_load_local_plus_imm =
            optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_f64_store_local_plus_imm =
            optable::translate::get_uwvmint_f64_store_local_plus_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);
        auto const exp_f64_store_plain = optable::translate::get_uwvmint_f64_store_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_store_plain));
# endif
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt,
                                            ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem) == 0);
        }
        else
#endif
        {
            UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
        }

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_offset_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_offset_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_offset_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_offset_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
