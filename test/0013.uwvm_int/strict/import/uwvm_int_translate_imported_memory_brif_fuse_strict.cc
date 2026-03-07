#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
    using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;

    [[nodiscard]] native_memory_t const* resolve_imported_memory(imported_memory_storage_t const& im) noexcept
    {
        using link_kind = imported_memory_storage_t::imported_memory_link_kind;

        switch(im.link_kind)
        {
            case link_kind::defined:
                if(im.target.defined_ptr == nullptr) { return nullptr; }
                return ::std::addressof(im.target.defined_ptr->memory);
            case link_kind::imported:
                if(im.target.imported_ptr == nullptr) { return nullptr; }
                return resolve_imported_memory(*im.target.imported_ptr);
            case link_kind::local_imported:
                // Not used in this test.
                return nullptr;
            case link_kind::unresolved:
            default:
                return nullptr;
        }
    }

    [[nodiscard]] byte_vec pack_i32_f32(::std::int32_t a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_provider_memory_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;
        mb.add_export_memory("mem");
        return mb.build();
    }

    [[nodiscard]] byte_vec build_imported_memory_brif_fuse_module()
    {
        module_builder mb{};

        // Import memory0 from "prov" as "mem".
        mb.add_import_memory("prov", "mem", 1u, 1u, true);

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: (param i32 addr, f32 rhs) -> i32
        // store 1.25f at mem[addr], then: if (f32.load(addr) < rhs) return 111 else 222
        {
            func_type ty{{k_val_i32, k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // mem[addr] = 1.25f
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 1.25f);
            op(c, wasm_op::f32_store);
            u32(c, 2u);  // align=4
            u32(c, 0u);  // offset=0

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            // f32.load(addr) < rhs
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_load);
            u32(c, 2u);
            u32(c, 0u);

            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_lt);

            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        // Ensure imported memory is actually resolved (regardless of combine mode).
        UWVM2TEST_REQUIRE(rt.imported_memory_vec_storage.size() == 1uz);
        auto const* const mem_p = resolve_imported_memory(rt.imported_memory_vec_storage.index_unchecked(0));
        UWVM2TEST_REQUIRE(mem_p != nullptr);

        using Runner = interpreter_runner<Opt>;

        // rhs=2.0 => 1.25 < 2.0, take branch => 111
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32_f32(0, 2.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        // rhs=1.0 => 1.25 < 1.0 false => 222
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32_f32(0, 1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const exp_mem_f32_lt_rhs =
                optable::translate::get_uwvmint_br_if_f32_load_localget_off_lt_localget_rhs_fptr_from_tuple<Opt>(curr, *mem_p, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_mem_f32_lt_rhs));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_imported_memory_brif_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm_provider = build_provider_memory_module();
        auto wasm_consumer = build_imported_memory_brif_fuse_module();

        auto prep = prepare_runtime_from_wasm(wasm_consumer,
                                              u8"cons",
                                              {
                                                  preloaded_wasm_module{.wasm_bytes = ::std::addressof(wasm_provider), .module_name = u8"prov"},
                                              });
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(rt.imported_memory_vec_storage.size() == 1uz);

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // tailcall (needed for memory mega-fusions)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_imported_memory_brif_fuse();
}
