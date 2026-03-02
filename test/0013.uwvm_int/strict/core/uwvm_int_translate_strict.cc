#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <tuple>
#include <bit>
#include <cstdio>
#include <limits>
#include <utility>
#
#ifndef UWVM_MODULE
# include <fast_io.h>
#
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/mvp.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/wasm1.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/storage.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/loader/load_and_check_modules.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm_op = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    static_assert(sizeof(wasm_op) == 1);

    using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    namespace compiler = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
    namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

    using runtime_module_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using runtime_local_func_t = ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t;

    using compiled_module_t = optable::uwvm_interpreter_full_function_symbol_t;
    using compiled_local_func_t = optable::local_func_storage_t;

    [[nodiscard]] constexpr ::std::uint8_t u8(wasm_op op) noexcept
    {
        return static_cast<::std::uint8_t>(op);
    }

    [[nodiscard]] inline int fail(int code, char const* msg) noexcept
    {
        ::std::fprintf(stderr, "uwvm2test FAIL(%d): %s\n", code, msg);
        return code;
    }

#define UWVM2TEST_REQUIRE(cond)           \
    do                                   \
    {                                    \
        if(!(cond)) [[unlikely]]         \
        {                                \
            return fail(__LINE__, #cond); \
        }                                \
    } while(false)

    inline char const* g_trap_ctx_mode{"init"};
    inline ::std::size_t g_trap_ctx_func{SIZE_MAX};

    inline void set_trap_ctx(char const* mode, ::std::size_t func_index) noexcept
    {
        g_trap_ctx_mode = mode;
        g_trap_ctx_func = func_index;
    }

    using byte_vec = ::std::vector<::std::byte>;

    inline void append_u8(byte_vec& out, ::std::uint8_t v)
    {
        out.push_back(static_cast<::std::byte>(v));
    }

    inline void append_bytes(byte_vec& out, byte_vec const& bytes)
    {
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    inline void append_u32_leb(byte_vec& out, ::std::uint32_t v)
    {
        for(;;)
        {
            ::std::uint8_t byte = static_cast<::std::uint8_t>(v & 0x7fu);
            v >>= 7u;
            if(v != 0u) { byte |= 0x80u; }
            append_u8(out, byte);
            if(v == 0u) { break; }
        }
    }

    inline void append_i32_leb(byte_vec& out, ::std::int32_t v)
    {
        bool more = true;
        while(more)
        {
            ::std::uint8_t byte = static_cast<::std::uint8_t>(v & 0x7f);
            v >>= 7;
            bool const sign_bit = (byte & 0x40u) != 0u;
            if((v == 0 && !sign_bit) || (v == -1 && sign_bit))
            {
                more = false;
            }
            else
            {
                byte |= 0x80u;
            }
            append_u8(out, byte);
        }
    }

    inline void append_f32_ieee(byte_vec& out, float v)
    {
        ::std::uint32_t bits = ::std::bit_cast<::std::uint32_t>(v);
        append_u8(out, static_cast<::std::uint8_t>(bits & 0xffu));
        append_u8(out, static_cast<::std::uint8_t>((bits >> 8) & 0xffu));
        append_u8(out, static_cast<::std::uint8_t>((bits >> 16) & 0xffu));
        append_u8(out, static_cast<::std::uint8_t>((bits >> 24) & 0xffu));
    }

    inline void append_f64_ieee(byte_vec& out, double v)
    {
        ::std::uint64_t bits = ::std::bit_cast<::std::uint64_t>(v);
        for(int i = 0; i < 8; ++i)
        {
            append_u8(out, static_cast<::std::uint8_t>((bits >> (8 * i)) & 0xffu));
        }
    }

    constexpr ::std::uint8_t k_block_empty = 0x40u;
    constexpr ::std::uint8_t k_val_i32 = 0x7fu;

    struct func_type
    {
        ::std::vector<::std::uint8_t> params{};
        ::std::vector<::std::uint8_t> results{};
    };

    struct func_body
    {
        ::std::vector<::std::pair<::std::uint32_t, ::std::uint8_t>> locals{};
        byte_vec code{};  // includes final 0x0b for function end
    };

    struct export_entry
    {
        byte_vec name_utf8{};
        ::std::uint8_t kind{};      // 0=func, 2=mem
        ::std::uint32_t index{};    // funcidx/memidx
    };

    struct global_entry
    {
        ::std::uint8_t valtype{};
        bool mut{};
        byte_vec init_expr{};  // ends with 0x0b
    };

    struct module_builder
    {
        ::std::vector<func_type> types{};
        ::std::vector<::std::uint32_t> function_type_indices{};
        ::std::vector<func_body> function_bodies{};
        ::std::vector<export_entry> exports{};

        bool has_memory{};
        ::std::uint32_t memory_min{};
        ::std::uint32_t memory_max{};
        bool memory_has_max{};
        bool export_memory{};

        ::std::vector<global_entry> globals{};

        ::std::uint32_t add_func(func_type ty, func_body body)
        {
            auto const type_index = static_cast<::std::uint32_t>(types.size());
            types.push_back(::std::move(ty));

            auto const func_index = static_cast<::std::uint32_t>(function_bodies.size());
            function_type_indices.push_back(type_index);
            function_bodies.push_back(::std::move(body));
            return func_index;
        }

        void add_export_func(::std::uint32_t func_index, char const* name_ascii)
        {
            export_entry ex{};
            auto const len = ::std::strlen(name_ascii);
            append_u32_leb(ex.name_utf8, static_cast<::std::uint32_t>(len));
            for(::std::size_t i = 0; i < len; ++i) { append_u8(ex.name_utf8, static_cast<::std::uint8_t>(name_ascii[i])); }
            ex.kind = 0u;
            ex.index = func_index;
            exports.push_back(::std::move(ex));
        }

        void add_export_memory(char const* name_ascii)
        {
            export_entry ex{};
            auto const len = ::std::strlen(name_ascii);
            append_u32_leb(ex.name_utf8, static_cast<::std::uint32_t>(len));
            for(::std::size_t i = 0; i < len; ++i) { append_u8(ex.name_utf8, static_cast<::std::uint8_t>(name_ascii[i])); }
            ex.kind = 2u;
            ex.index = 0u;
            exports.push_back(::std::move(ex));
        }

        byte_vec build() const
        {
            byte_vec out{};

            // wasm header
            append_u8(out, 0x00u);
            append_u8(out, 0x61u);
            append_u8(out, 0x73u);
            append_u8(out, 0x6du);
            append_u8(out, 0x01u);
            append_u8(out, 0x00u);
            append_u8(out, 0x00u);
            append_u8(out, 0x00u);

            auto emit_section = [&](::std::uint8_t id, byte_vec const& sec)
            {
                append_u8(out, id);
                append_u32_leb(out, static_cast<::std::uint32_t>(sec.size()));
                append_bytes(out, sec);
            };

            // type section (1)
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(types.size()));
                for(auto const& ty : types)
                {
                    append_u8(sec, 0x60u);
                    append_u32_leb(sec, static_cast<::std::uint32_t>(ty.params.size()));
                    for(auto const p : ty.params) { append_u8(sec, p); }
                    append_u32_leb(sec, static_cast<::std::uint32_t>(ty.results.size()));
                    for(auto const r : ty.results) { append_u8(sec, r); }
                }
                emit_section(1u, sec);
            }

            // function section (3)
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(function_type_indices.size()));
                for(auto const idx : function_type_indices) { append_u32_leb(sec, idx); }
                emit_section(3u, sec);
            }

            // memory section (5)
            if(has_memory)
            {
                byte_vec sec{};
                append_u32_leb(sec, 1u);
                ::std::uint8_t flags = memory_has_max ? 0x01u : 0x00u;
                append_u8(sec, flags);
                append_u32_leb(sec, memory_min);
                if(memory_has_max) { append_u32_leb(sec, memory_max); }
                emit_section(5u, sec);
            }

            // global section (6)
            if(!globals.empty())
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(globals.size()));
                for(auto const& g : globals)
                {
                    append_u8(sec, g.valtype);
                    append_u8(sec, g.mut ? 0x01u : 0x00u);
                    append_bytes(sec, g.init_expr);
                }
                emit_section(6u, sec);
            }

            // export section (7)
            if(!exports.empty())
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(exports.size()));
                for(auto const& ex : exports)
                {
                    append_bytes(sec, ex.name_utf8);
                    append_u8(sec, ex.kind);
                    append_u32_leb(sec, ex.index);
                }
                emit_section(7u, sec);
            }

            // code section (10)
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(function_bodies.size()));

                for(auto const& fn : function_bodies)
                {
                    byte_vec body{};
                    append_u32_leb(body, static_cast<::std::uint32_t>(fn.locals.size()));
                    for(auto const& loc : fn.locals)
                    {
                        append_u32_leb(body, loc.first);
                        append_u8(body, loc.second);
                    }
                    append_bytes(body, fn.code);

                    byte_vec sized{};
                    append_u32_leb(sized, static_cast<::std::uint32_t>(body.size()));
                    append_bytes(sized, body);
                    append_bytes(sec, sized);
                }

                emit_section(10u, sec);
            }

            return out;
        }
    };

    [[nodiscard]] constexpr ::std::size_t abi_bytes(wasm_value_type t) noexcept
    {
        switch(t)
        {
            case wasm_value_type::i32:
            case wasm_value_type::f32:
                return 4uz;
            case wasm_value_type::i64:
            case wasm_value_type::f64:
                return 8uz;
            default:
                return 0uz;
        }
    }

    [[nodiscard]] inline ::std::size_t abi_total_bytes(wasm_value_type const* begin, wasm_value_type const* end) noexcept
    {
        ::std::size_t total{};
        for(auto it = begin; it != end; ++it)
        {
            auto const add = abi_bytes(*it);
            if(add == 0uz) { return 0uz; }
            if(add > (::std::numeric_limits<::std::size_t>::max() - total)) { return 0uz; }
            total += add;
        }
        return total;
    }

    struct prepared_runtime
    {
        runtime_module_t const* mod{};
    };

    [[nodiscard]] inline prepared_runtime prepare_runtime_from_wasm(byte_vec const& wasm_bytes, ::uwvm2::utils::container::u8string_view module_name)
    {
        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;

        // Reset global storages (match fuzz harness).
        ::uwvm2::uwvm::wasm::storage::all_module.clear();
        ::uwvm2::uwvm::wasm::storage::all_module_export.clear();
        ::uwvm2::uwvm::wasm::storage::preloaded_wasm.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        ::uwvm2::uwvm::wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        ::uwvm2::uwvm::wasm::storage::weak_symbol.clear();
#endif
        ::uwvm2::uwvm::wasm::storage::preload_local_imported.clear();

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        auto const* begin = wasm_bytes.data();
        auto const* end = wasm_bytes.data() + wasm_bytes.size();

        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t module_storage{};
        module_storage = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin,
                                                                            end,
                                                                            parse_err,
                                                                            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t{});

        ::uwvm2::uwvm::wasm::storage::execute_wasm = ::uwvm2::uwvm::wasm::type::wasm_file_t{1u};
        ::uwvm2::uwvm::wasm::storage::execute_wasm.file_name = u8"uwvm2test.wasm";
        ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = module_name;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.binfmt_ver = 1u;
        ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(module_storage);

        if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            ::std::fprintf(stderr, "uwvm2test PREP FAIL: construct_all_module_and_check_duplicate_module\n");
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        }
        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
            ::std::fprintf(stderr, "uwvm2test PREP FAIL: check_import_exist_and_detect_cycles\n");
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        }

        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.clear();
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.reserve(1uz);
        ::uwvm2::uwvm::runtime::initializer::details::import_alias_sanity_checked = false;

        runtime_module_t rt{};
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = module_name;
        ::uwvm2::uwvm::runtime::initializer::details::initialize_from_wasm_file(::uwvm2::uwvm::wasm::storage::execute_wasm, rt);
        ::uwvm2::uwvm::runtime::initializer::details::current_initializing_module_name = {};
        ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.try_emplace(module_name, ::std::move(rt));

        auto it = ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(module_name);
        if(it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end())
        {
            ::std::fprintf(stderr, "uwvm2test PREP FAIL: runtime storage missing module\n");
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        }

        return prepared_runtime{ .mod = ::std::addressof(it->second) };
    }

    template <optable::uwvm_interpreter_translate_option_t CompileOption>
    struct interpreter_runner
    {
        static constexpr ::std::size_t tuple_size = compiler::details::interpreter_tuple_size<CompileOption>();

        template <::std::size_t I>
        using arg_t = compiler::details::interpreter_tuple_arg_t<I, CompileOption>;

        template <::std::size_t... Is>
        static ::std::tuple<arg_t<Is>...> make_args(::std::index_sequence<Is...>, ::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
        {
            return ::std::tuple<arg_t<Is>...>(init_arg<Is>(ip, sp, local_base)...);
        }

        template <::std::size_t I>
        static constexpr arg_t<I> init_arg(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
        {
            if constexpr(I == 0uz) { return ip; }
            else if constexpr(I == 1uz) { return sp; }
            else if constexpr(I == 2uz) { return local_base; }
            else { return arg_t<I>{}; }
        }

        struct run_result
        {
            byte_vec results{};
            bool hit_expected0{};
            bool hit_expected1{};
        };

        using opfunc_ptr_t = compiler::details::interpreter_expected_opfunc_ptr_t<CompileOption>;

        static run_result run(compiled_local_func_t const& fn,
                              runtime_local_func_t const& rt_fn,
                              byte_vec const& packed_params,
                              opfunc_ptr_t expected0,
                              opfunc_ptr_t expected1)
        {
            auto const* const ft = rt_fn.function_type_ptr;
            if(ft == nullptr)
            {
                ::std::fprintf(stderr, "uwvm2test RUN FAIL: function_type_ptr is null\n");
                ::std::fflush(stderr);
                ::fast_io::fast_terminate();
            }

            auto const param_bytes = abi_total_bytes(ft->parameter.begin, ft->parameter.end);
            auto const result_bytes = abi_total_bytes(ft->result.begin, ft->result.end);
            if(param_bytes != packed_params.size())
            {
                ::std::fprintf(stderr,
                               "uwvm2test RUN FAIL: param_bytes(%zu) != packed_params.size(%zu)\n",
                               param_bytes,
                               packed_params.size());
                ::std::fflush(stderr);
                ::fast_io::fast_terminate();
            }

            // Align locals/stack to 16 bytes (match runtime's typical alignment).
            constexpr ::std::size_t k_align = 16uz;
            auto align_up = [](byte_vec& buf) noexcept -> ::std::byte*
            {
                auto const p = reinterpret_cast<::std::uintptr_t>(buf.data());
                auto const a = (p + (k_align - 1uz)) & ~(k_align - 1uz);
                return reinterpret_cast<::std::byte*>(a);
            };

            byte_vec local_buf(fn.local_bytes_max + k_align);
            ::std::memset(local_buf.data(), 0, local_buf.size());
            ::std::byte* local_base = align_up(local_buf);
            if(param_bytes != 0uz) { ::std::memcpy(local_base, packed_params.data(), param_bytes); }

            byte_vec stack_buf((fn.operand_stack_byte_max == 0uz ? 64uz : fn.operand_stack_byte_max) + k_align);
            ::std::memset(stack_buf.data(), 0, stack_buf.size());
            ::std::byte* operand_base = align_up(stack_buf);

            auto args = make_args(::std::make_index_sequence<tuple_size>{}, fn.op.operands.data(), operand_base, local_base);

            bool hit0{};
            bool hit1{};

            if constexpr(CompileOption.is_tail_call)
            {
                opfunc_ptr_t fptr{};
                ::std::memcpy(::std::addressof(fptr), ::std::get<0>(args), sizeof(fptr));
                // tailcall chain executes inside, no per-op instrumentation.
                (void)hit0;
                (void)hit1;
                ::std::apply([&](auto... a) { fptr(a...); }, args);
            }
            else
            {
                while(::std::get<0>(args) != nullptr)
                {
                    opfunc_ptr_t fptr{};
                    ::std::memcpy(::std::addressof(fptr), ::std::get<0>(args), sizeof(fptr));
                    if(expected0 != nullptr && fptr == expected0) { hit0 = true; }
                    if(expected1 != nullptr && fptr == expected1) { hit1 = true; }
                    ::std::apply([&](auto&... a) { fptr(a...); }, args);
                }
            }

            run_result rr{};
            rr.hit_expected0 = hit0;
            rr.hit_expected1 = hit1;
            rr.results.resize(result_bytes);
            if(result_bytes != 0uz) { ::std::memcpy(rr.results.data(), operand_base, result_bytes); }
            return rr;
        }
    };

    [[nodiscard]] inline ::std::int32_t load_i32(byte_vec const& b, ::std::size_t off = 0uz) noexcept
    {
        ::std::int32_t v{};
        ::std::memcpy(::std::addressof(v), b.data() + off, sizeof(v));
        return v;
    }

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, unsigned r) noexcept
    {
        r &= 31u;
        return (x << r) | (x >> ((32u - r) & 31u));
    }

    [[nodiscard]] constexpr ::std::uint32_t rotr32(::std::uint32_t x, unsigned r) noexcept
    {
        r &= 31u;
        return (x >> r) | (x << ((32u - r) & 31u));
    }

    [[nodiscard]] byte_vec pack_i32(::std::int32_t v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_no_params()
    {
        return {};
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool bytecode_contains_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
    {
        if(fptr == nullptr) { return false; }
        ::std::array<::std::byte, sizeof(Fptr)> needle{};
        ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(Fptr));
        if(bytes.size() < needle.size()) { return false; }
        for(::std::size_t i{}; i + needle.size() <= bytes.size(); ++i)
        {
            if(::std::memcmp(bytes.data() + i, needle.data(), needle.size()) == 0) { return true; }
        }
        return false;
    }

    [[nodiscard]] byte_vec build_strict_suite_module()
    {
        module_builder mb{};

        // Enable memory + one mutable i32 global to cover global/memory ops.
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 2u;
        mb.export_memory = true;

        mb.globals.push_back(global_entry{
            .valtype = k_val_i32,
            .mut = true,
            .init_expr = [&]
            {
                byte_vec e{};
                append_u8(e, u8(wasm_op::i32_const));
                append_i32_leb(e, 0);
                append_u8(e, u8(wasm_op::end));
                return e;
            }()});

        // Helpers to build bodies.
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: regress_localtee_br_if (param i32) (result i32) (local i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: i32
            auto& c = fb.code;

            // local1 = -1 (so i32.add decrements)
            op(c, wasm_op::i32_const); i32(c, -1);
            op(c, wasm_op::local_set); u32(c, 1u);

            // block
            op(c, wasm_op::block); append_u8(c, k_block_empty);
            // loop
            op(c, wasm_op::loop); append_u8(c, k_block_empty);

            // local.get 0 ; nop ; local.get 1 ; i32.add ; local.tee 0 ; br_if 0
            // nop acts as a combine barrier so delay_local sees rhs local.get (and the translator must avoid *_local_tee
            // mega-op because local.tee is followed by br_if and may be fused into br_if_local_tee_nz).
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::br_if); u32(c, 0u);

            // end loop, end block
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            // return local0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: br_if unwind + internal temp local usage (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block); append_u8(c, k_val_i32);  // (block (result i32)
            op(c, wasm_op::i32_const); i32(c, 10);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::br_if); u32(c, 0u);
            // fallthrough: drop extra stack values, then push canonical result
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::end);  // end block
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: br_table select (param i32) (result i32) (local i32 out)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            // out = 30
            op(c, wasm_op::i32_const); i32(c, 30);
            op(c, wasm_op::local_set); u32(c, 1u);

            // block outer
            op(c, wasm_op::block); append_u8(c, k_block_empty);
            // block middle
            op(c, wasm_op::block); append_u8(c, k_block_empty);
            // block inner
            op(c, wasm_op::block); append_u8(c, k_block_empty);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::br_table);
            // vec length=2, labels: 0,1, default: 2
            u32(c, 2u);
            u32(c, 0u);
            u32(c, 1u);
            u32(c, 2u);

            op(c, wasm_op::end);  // end inner

            // case 0:
            op(c, wasm_op::i32_const); i32(c, 10);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::br); u32(c, 1u);  // break to outer end

            op(c, wasm_op::end);  // end middle

            // case 1:
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::br); u32(c, 0u);  // break outer

            op(c, wasm_op::end);  // end outer

            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: if (result i32) with unreachable then-branch (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::if_); append_u8(c, k_val_i32);
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i32_const); i32(c, 111);
            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 222);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: memory mix (param i32) (result i32) -> (x&0xff) + (x&0xffff) + x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // store8 [0] = x
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            // load8_u [0]
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);

            // store16 [2] = x
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            // load16_u [2]
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);

            // store [4] = x
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            // load [4]
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: memory signext (param i32) (result i32) -> load8_s + load16_s
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // store8 [0] = x ; load8_s [0]
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load8_s); u32(c, 0u); u32(c, 0u);

            // store16 [2] = x ; load16_s [2]
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: memory.size + memory.grow sanity (result i32) -> sum(s0, g0, s1, g1, s2)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::memory_size); u32(c, 0u);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: global add (param i32) (result i32) -> g = g + x ; return g
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::global_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::global_set); u32(c, 0u);
            op(c, wasm_op::global_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: i32 numeric mix (param i32) (result i32) (local i32 acc)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // acc
            auto& c = fb.code;

            // acc = a
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + clz(a)
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_clz);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc xor (ctz(a) + popcnt(a))
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_ctz);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_popcnt);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + (a * 3)
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + ((a & 0x55) | (a ^ 0xaa))
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0x55);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0xaa);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_or);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + (rotl(a,5) ^ rotr(a,7))
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_rotr);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + (a/7) + (a%7)
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_div_u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_rem_u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            // acc = acc + select(0x1111, 0x2222, a<0)
            op(c, wasm_op::i32_const); i32(c, 0x1111);
            op(c, wasm_op::i32_const); i32(c, 0x2222);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_lt_s);
            op(c, wasm_op::select);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);

            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: float ops (result i32) -> 14
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const); append_f32_ieee(c, 1.5f);
            op(c, wasm_op::f32_const); append_f32_ieee(c, 2.25f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::f64_const); append_f64_ieee(c, 6.5);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: delay_local positive-case (param i32) (result i32) (local i32)
        // Pattern: local.get 0 ; local.get 1 ; i32.add ; local.tee 0 ; drop
        // (must NOT be followed by br_if so the delay_local *_local_tee mega-op can be used safely)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: i32
            auto& c = fb.code;

            // local1 = -1 (so i32.add decrements)
            op(c, wasm_op::i32_const); i32(c, -1);
            op(c, wasm_op::local_set); u32(c, 1u);

            // local.get 0 ; nop ; local.get 1 ; i32.add ; local.tee 0 ; drop
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::drop);

            // return local0 (a-1)
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11..: trivial call-inline patterns (for compile-time classification)
        auto add_trivial = [&](func_type ty, byte_vec code) {
            func_body fb{};
            fb.code = ::std::move(code);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // nop_void: end
        add_trivial(func_type{{}, {}}, byte_vec{static_cast<::std::byte>(u8(wasm_op::end))});

        // const_i32: i32.const IMM ; end
        {
            byte_vec c{};
            op(c, wasm_op::i32_const); i32(c, 123);
            op(c, wasm_op::end);
            add_trivial(func_type{{}, {k_val_i32}}, ::std::move(c));
        }

        // param0_i32: local.get 0 ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // add_const_i32: local.get 0 ; i32.const IMM ; i32.add ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // mul_add_const_i32: local.get 0 ; i32.const MUL ; i32.mul ; i32.const ADD ; i32.add ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // xor_i32: local.get 0 ; local.get 1 ; i32.xor ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32, k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // xor_add_const_i32 (Pattern C'): local.get 1 ; i32.const IMM ; i32.xor ; local.get 0 ; i32.add ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 0x1234);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32, k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // sub_or_const_i32: local.get 0 ; local.get 1 ; i32.const IMM ; i32.or ; i32.sub ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 0x55aa);
            op(c, wasm_op::i32_or);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32, k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // sum8_xor_const_i32: see matcher pattern E
        {
            byte_vec c{};
            // (0+1)
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            // (2+3) + prev
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            // (4+5)
            op(c, wasm_op::local_get); u32(c, 4u);
            op(c, wasm_op::local_get); u32(c, 5u);
            op(c, wasm_op::i32_add);
            // (6+7) + prev + prev
            op(c, wasm_op::local_get); u32(c, 6u);
            op(c, wasm_op::local_get); u32(c, 7u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            // xor const
            op(c, wasm_op::i32_const); i32(c, 0x7777);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}},
                        ::std::move(c));
        }

        // xor_const_i32: local.get 0 ; i32.const IMM ; i32.xor ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0x7f7f);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // mul_const_i32: local.get 0 ; i32.const IMM ; i32.mul ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // rotr_add_const_i32: local.get 0 ; i32.const ROT ; i32.rotr ; i32.const ADD ; i32.add ; end
        {
            byte_vec c{};
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 13);
            op(c, wasm_op::i32_rotr);
            op(c, wasm_op::i32_const); i32(c, 0x3333);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        // xorshift32_i32: canonical pattern (see matcher)
        {
            byte_vec c{};
            // local.get 0
            op(c, wasm_op::local_get); u32(c, 0u);
            // local.get 0 ; i32.const 13 ; i32.shl ; i32.xor ; local.set 0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 13);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set); u32(c, 0u);
            // local.get 0 ; local.get 0 ; i32.const 17 ; i32.shr_u ; i32.xor ; local.set 0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 17);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set); u32(c, 0u);
            // local.get 0 ; local.get 0 ; i32.const 5 ; i32.shl ; i32.xor ; local.set 0
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set); u32(c, 0u);
            // local.get 0 ; end
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);
            add_trivial(func_type{{k_val_i32}, {k_val_i32}}, ::std::move(c));
        }

        if(mb.export_memory) { mb.add_export_memory("mem"); }

        return mb.build();
    }

    [[nodiscard]] int test_trivial_inline_matcher_direct() noexcept
    {
        using code_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t;
        using kind_t = optable::trivial_defined_call_kind;

        auto make_code = [&](byte_vec const& expr) -> code_t
        {
            code_t c{};
            c.body.code_begin = reinterpret_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*>(expr.data());
            c.body.expr_begin = reinterpret_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*>(expr.data());
            c.body.code_end = reinterpret_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*>(expr.data() + expr.size());
            return c;
        };

        // nop_void via (i32.const IMM ; drop ; end)
        {
            byte_vec e{};
            append_u8(e, u8(wasm_op::i32_const)); append_i32_leb(e, 1);
            append_u8(e, u8(wasm_op::drop));
            append_u8(e, u8(wasm_op::end));
            auto c = make_code(e);
            auto const m = compiler::details::match_trivial_call_inline_body(::std::addressof(c));
            UWVM2TEST_REQUIRE(m.kind == kind_t::nop_void);
        }

        // nop_void via (local.get 0 ; drop ; end)
        {
            byte_vec e{};
            append_u8(e, u8(wasm_op::local_get)); append_u32_leb(e, 0u);
            append_u8(e, u8(wasm_op::drop));
            append_u8(e, u8(wasm_op::end));
            auto c = make_code(e);
            auto const m = compiler::details::match_trivial_call_inline_body(::std::addressof(c));
            UWVM2TEST_REQUIRE(m.kind == kind_t::nop_void);
        }

        // none: (nop ; end)
        {
            byte_vec e{};
            append_u8(e, u8(wasm_op::nop));
            append_u8(e, u8(wasm_op::end));
            auto c = make_code(e);
            auto const m = compiler::details::match_trivial_call_inline_body(::std::addressof(c));
            UWVM2TEST_REQUIRE(m.kind == kind_t::none);
        }

        return 0;
    }

    [[nodiscard]] int test_compile_and_execute() noexcept
    {
        // Install optable hooks (unexpected traps/calls should terminate the test process).
        optable::unreachable_func = +[]() noexcept
        {
            ::std::fprintf(stderr, "uwvm2test TRAP: unreachable mode=%s func=%zu\n", g_trap_ctx_mode, g_trap_ctx_func);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };
        optable::trap_invalid_conversion_to_integer_func = +[]() noexcept
        {
            ::std::fprintf(stderr, "uwvm2test TRAP: invalid_conversion mode=%s func=%zu\n", g_trap_ctx_mode, g_trap_ctx_func);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };
        optable::trap_integer_divide_by_zero_func = +[]() noexcept
        {
            ::std::fprintf(stderr, "uwvm2test TRAP: div_by_zero mode=%s func=%zu\n", g_trap_ctx_mode, g_trap_ctx_func);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };
        optable::trap_integer_overflow_func = +[]() noexcept
        {
            ::std::fprintf(stderr, "uwvm2test TRAP: int_overflow mode=%s func=%zu\n", g_trap_ctx_mode, g_trap_ctx_func);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };

        optable::call_func = +[](::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte**)
        {
            ::std::fprintf(stderr,
                           "uwvm2test TRAP: call mode=%s func=%zu wasm_module_id=%zu call_func_index=%zu\n",
                           g_trap_ctx_mode,
                           g_trap_ctx_func,
                           wasm_module_id,
                           func_index);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };
        optable::call_indirect_func = +[](::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte**)
        {
            ::std::fprintf(stderr,
                           "uwvm2test TRAP: call_indirect mode=%s func=%zu wasm_module_id=%zu type_index=%zu table_index=%zu\n",
                           g_trap_ctx_mode,
                           g_trap_ctx_func,
                           wasm_module_id,
                           type_index,
                           table_index);
            ::std::fflush(stderr);
            ::fast_io::fast_terminate();
        };

        auto wasm = build_strict_suite_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_mod");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        runtime_module_t const& rt = *prep.mod;

        // Compile + execute in three modes to cover:
        // - byref interpreter (non-tailcall)
        // - tailcall interpreter
        // - tailcall with i32 stacktop caching (exercise stacktop-aware translation paths)

        // Mode A: byref (no stacktop caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            set_trap_ctx("ModeA.compile", SIZE_MAX);
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            // Expected combined ops (only asserted when the feature is enabled at compile time).
            using Runner = interpreter_runner<opt>;
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto exp_br_if_local_tee =
                optable::translate::get_uwvmint_br_if_local_tee_nz_fptr_from_tuple<opt>(curr, tuple);
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
            constexpr auto exp_delay_local_add_tee =
                optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                    opt,
                    optable::numeric_details::int_binop::add>(curr, tuple);
#endif

            // f0 regress: for n in {1,2,3,100} -> 0
            {
                bool hit_br_if_local_tee{};
                bool hit_delay_local{};

                set_trap_ctx("ModeA.run", 0uz);
                for(int v : {1, 2, 3, 100})
                {
                    auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                          rt.local_defined_function_vec_storage.index_unchecked(0),
                                          pack_i32(v),
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
                                          exp_br_if_local_tee,
#else
                                          nullptr,
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
                                          exp_delay_local_add_tee
#else
                                          nullptr
#endif
                    );
                    UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
                    hit_br_if_local_tee = hit_br_if_local_tee || rr.hit_expected0;
                    hit_delay_local = hit_delay_local || rr.hit_expected1;
                }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
                UWVM2TEST_REQUIRE(hit_br_if_local_tee);
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
                // The translator must *avoid* the delay_local *_local_tee mega-op when local.tee is immediately
                // followed by br_if, because local.tee + br_if may be fused into br_if_local_tee_nz.
                UWVM2TEST_REQUIRE(!hit_delay_local);
#endif
            }

            // f10 delay_local positive case: x -> x-1, and should use *_local_tee mega-op when enabled.
            {
                set_trap_ctx("ModeA.run", 10uz);
                auto rr = Runner::run(cm.local_funcs.index_unchecked(10),
                                      rt.local_defined_function_vec_storage.index_unchecked(10),
                                      pack_i32(10),
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
                                      exp_delay_local_add_tee,
#else
                                      nullptr,
#endif
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 9);
            }

            // f1: br_if unwind returns 20 regardless of param
            {
                set_trap_ctx("ModeA.run", 1uz);
                auto rr0 = Runner::run(cm.local_funcs.index_unchecked(1),
                                       rt.local_defined_function_vec_storage.index_unchecked(1),
                                       pack_i32(0),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr0.results) == 20);

                auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                       rt.local_defined_function_vec_storage.index_unchecked(1),
                                       pack_i32(1),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr1.results) == 20);
            }

            // f2: br_table mapping
            {
                auto run = [&](int x) -> int
                {
                    set_trap_ctx("ModeA.run", 2uz);
                    auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                          rt.local_defined_function_vec_storage.index_unchecked(2),
                                          pack_i32(x),
                                          nullptr,
                                          nullptr);
                    return load_i32(rr.results);
                };
                UWVM2TEST_REQUIRE(run(0) == 10);
                UWVM2TEST_REQUIRE(run(1) == 20);
                UWVM2TEST_REQUIRE(run(2) == 30);
                UWVM2TEST_REQUIRE(run(100) == 30);
            }

            // f3: if_unreachable_else with param=0 -> 222 (avoid param!=0 which traps).
            {
                set_trap_ctx("ModeA.run", 3uz);
                auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                      rt.local_defined_function_vec_storage.index_unchecked(3),
                                      pack_i32(0),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 222);
            }

            // f4: memory mix
            {
                set_trap_ctx("ModeA.run", 4uz);
                ::std::int32_t const x = static_cast<::std::int32_t>(0x12345678u);
                auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                      rt.local_defined_function_vec_storage.index_unchecked(4),
                                      pack_i32(x),
                                      nullptr,
                                      nullptr);
                ::std::uint32_t ux = static_cast<::std::uint32_t>(x);
                ::std::uint32_t expected = (ux & 0xffu) + (ux & 0xffffu) + ux;
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expected);
            }

            // f5: memory signext: param=0x8080 -> (-128)+(-32640) = -32768
            {
                set_trap_ctx("ModeA.run", 5uz);
                ::std::int32_t const x = 0x00008080;
                auto rr = Runner::run(cm.local_funcs.index_unchecked(5),
                                      rt.local_defined_function_vec_storage.index_unchecked(5),
                                      pack_i32(x),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == static_cast<::std::int32_t>(0xffff8000u));
            }

            // f7: global add is stateful; run only in byref mode
            {
                set_trap_ctx("ModeA.run", 7uz);
                auto rr1 = Runner::run(cm.local_funcs.index_unchecked(7),
                                       rt.local_defined_function_vec_storage.index_unchecked(7),
                                       pack_i32(5),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr1.results) == 5);
                auto rr2 = Runner::run(cm.local_funcs.index_unchecked(7),
                                       rt.local_defined_function_vec_storage.index_unchecked(7),
                                       pack_i32(7),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr2.results) == 12);
            }

            // f8: numeric mix correctness (check a few cases)
            {
                auto expect = [&](::std::uint32_t a) -> ::std::uint32_t
                {
                    ::std::uint32_t acc = a;
                    auto clz = (a == 0u) ? 32u : static_cast<::std::uint32_t>(__builtin_clz(a));
                    auto ctz = (a == 0u) ? 32u : static_cast<::std::uint32_t>(__builtin_ctz(a));
                    auto pop = static_cast<::std::uint32_t>(__builtin_popcount(a));
                    acc = acc + clz;
                    acc = acc ^ (ctz + pop);
                    acc = acc + (a * 3u);
                    acc = acc + ((a & 0x55u) | (a ^ 0xaau));
                    acc = acc + (rotl32(a, 5u) ^ rotr32(a, 7u));
                    acc = acc + (a / 7u) + (a % 7u);
                    ::std::uint32_t sel = (static_cast<::std::int32_t>(a) < 0) ? 0x1111u : 0x2222u;
                    acc = acc + sel;
                    return acc;
                };

                for(::std::uint32_t a : {0u, 1u, 0x12345678u, 0x80000000u})
                {
                    set_trap_ctx("ModeA.run", 8uz);
                    auto rr = Runner::run(cm.local_funcs.index_unchecked(8),
                                          rt.local_defined_function_vec_storage.index_unchecked(8),
                                          pack_i32(static_cast<::std::int32_t>(a)),
                                          nullptr,
                                          nullptr);
                    UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expect(a));
                }
            }

            // f9: float ops -> 14
            {
                set_trap_ctx("ModeA.run", 9uz);
                auto rr = Runner::run(cm.local_funcs.index_unchecked(9),
                                      rt.local_defined_function_vec_storage.index_unchecked(9),
                                      pack_no_params(),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 14);
            }

            // f6: memory.size/grow is stateful; run once here and expect 5
            {
                set_trap_ctx("ModeA.run", 6uz);
                auto rr = Runner::run(cm.local_funcs.index_unchecked(6),
                                      rt.local_defined_function_vec_storage.index_unchecked(6),
                                      pack_no_params(),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 5);
            }

            // Verify compile-time trivial kind classification for the appended trivial functions.
            {
                auto const& info = cm.local_defined_call_info;
                UWVM2TEST_REQUIRE(info.index_unchecked(11).trivial_kind == optable::trivial_defined_call_kind::nop_void);
                UWVM2TEST_REQUIRE(info.index_unchecked(12).trivial_kind == optable::trivial_defined_call_kind::const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(12).trivial_imm == 123);
                UWVM2TEST_REQUIRE(info.index_unchecked(13).trivial_kind == optable::trivial_defined_call_kind::param0_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(14).trivial_kind == optable::trivial_defined_call_kind::add_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(14).trivial_imm == 7);
                UWVM2TEST_REQUIRE(info.index_unchecked(15).trivial_kind == optable::trivial_defined_call_kind::mul_add_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(15).trivial_imm == 3);
                UWVM2TEST_REQUIRE(info.index_unchecked(15).trivial_imm2 == 9);
                UWVM2TEST_REQUIRE(info.index_unchecked(16).trivial_kind == optable::trivial_defined_call_kind::xor_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(17).trivial_kind == optable::trivial_defined_call_kind::xor_add_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(17).trivial_imm == 0x1234);
                UWVM2TEST_REQUIRE(info.index_unchecked(18).trivial_kind == optable::trivial_defined_call_kind::sub_or_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(18).trivial_imm == 0x55aa);
                UWVM2TEST_REQUIRE(info.index_unchecked(19).trivial_kind == optable::trivial_defined_call_kind::sum8_xor_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(19).trivial_imm == 0x7777);
                UWVM2TEST_REQUIRE(info.index_unchecked(20).trivial_kind == optable::trivial_defined_call_kind::xor_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(20).trivial_imm == 0x7f7f);
                UWVM2TEST_REQUIRE(info.index_unchecked(21).trivial_kind == optable::trivial_defined_call_kind::mul_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(21).trivial_imm == 11);
                UWVM2TEST_REQUIRE(info.index_unchecked(22).trivial_kind == optable::trivial_defined_call_kind::rotr_add_const_i32);
                UWVM2TEST_REQUIRE(info.index_unchecked(22).trivial_imm == 0x3333);
                UWVM2TEST_REQUIRE(info.index_unchecked(22).trivial_imm2 == 13);
                UWVM2TEST_REQUIRE(info.index_unchecked(23).trivial_kind == optable::trivial_defined_call_kind::xorshift32_i32);
            }
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            set_trap_ctx("ModeB.compile", SIZE_MAX);
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto exp_br_if_local_tee =
                optable::translate::get_uwvmint_br_if_local_tee_nz_fptr_from_tuple<opt>(curr, tuple);
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
            constexpr auto exp_delay_local_add_tee =
                optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                    opt,
                    optable::numeric_details::int_binop::add>(curr, tuple);
#endif

            // Translate-time strictness: make sure key mega-ops are actually emitted in tailcall mode.
            // f0 regress should fuse local.tee+br_if, and must avoid delay_local *_local_tee.
            // f10 should use delay_local *_local_tee (local.tee not followed by br_if).
            {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_br_if_local_tee));
#endif
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && (defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT) || defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY))
                UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_delay_local_add_tee));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_delay_local_add_tee));
#endif
            }

            // Only run non-stateful functions here (memory/global already modified in Mode A).
            set_trap_ctx("ModeB.run", 0uz);
            for(int v : {1, 2, 3, 10})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(v),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
            }

            set_trap_ctx("ModeB.run", 1uz);
            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_i32(0),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 20);

            set_trap_ctx("ModeB.run", 2uz);
            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_i32(100),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == 30);

            set_trap_ctx("ModeB.run", 8uz);
            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(8),
                                   rt.local_defined_function_vec_storage.index_unchecked(8),
                                   pack_i32(static_cast<::std::int32_t>(0x12345678u)),
                                   nullptr,
                                   nullptr);
            (void)rr3;

            set_trap_ctx("ModeB.run", 9uz);
            auto rr4 = Runner::run(cm.local_funcs.index_unchecked(9),
                                   rt.local_defined_function_vec_storage.index_unchecked(9),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr4.results) == 14);

            set_trap_ctx("ModeB.run", 10uz);
            auto rr10 = Runner::run(cm.local_funcs.index_unchecked(10),
                                    rt.local_defined_function_vec_storage.index_unchecked(10),
                                    pack_i32(10),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr10.results) == 9);
        }

        // Mode C: tailcall + stacktop caching (small rings, scalar-only)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                // Wasm1 requires i32/i64/f32/f64 all enabled together for mixed-type stack modeling.
                // Use 2-slot integer ring (merged i32/i64) and 2-slot float ring (merged f32/f64).
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            set_trap_ctx("ModeC.compile", SIZE_MAX);
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            // Exercise control-flow + arithmetic with caching enabled.
            set_trap_ctx("ModeC.run", 0uz);
            for(int v : {1, 2, 3, 100})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(v),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
            }

            set_trap_ctx("ModeC.run", 1uz);
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 20);

            set_trap_ctx("ModeC.run", 2uz);
            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_i32(1),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == 20);
        }

        return 0;
    }
}  // namespace

int main()
{
    if(int e = test_trivial_inline_matcher_direct(); e) { return e; }
    if(int e = test_compile_and_execute(); e) { return e; }
    return 0;
}
