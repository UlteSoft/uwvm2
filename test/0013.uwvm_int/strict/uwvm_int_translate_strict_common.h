#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

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

namespace uwvm2test::uwvm_int_strict
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
            return ::uwvm2test::uwvm_int_strict::fail(__LINE__, #cond); \
        }                                \
    } while(false)

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

    inline void append_i64_leb(byte_vec& out, ::std::int64_t v)
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
    constexpr ::std::uint8_t k_val_i64 = 0x7eu;
    constexpr ::std::uint8_t k_val_f32 = 0x7du;
    constexpr ::std::uint8_t k_val_f64 = 0x7cu;
    constexpr ::std::uint8_t k_ref_funcref = 0x70u;

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

    struct element_segment
    {
        ::std::uint32_t table_index{};
        byte_vec offset_expr{};  // init expr bytes, ends with 0x0b
        ::std::vector<::std::uint32_t> func_indices{};
    };

    struct module_builder
    {
        ::std::vector<func_type> types{};
        ::std::vector<::std::uint32_t> function_type_indices{};
        ::std::vector<func_body> function_bodies{};
        ::std::vector<export_entry> exports{};

        bool has_table{};
        ::std::uint32_t table_min{};
        ::std::uint32_t table_max{};
        bool table_has_max{};

        bool has_memory{};
        ::std::uint32_t memory_min{};
        ::std::uint32_t memory_max{};
        bool memory_has_max{};
        bool export_memory{};

        ::std::vector<global_entry> globals{};
        ::std::vector<element_segment> elements{};

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

            // table section (4)
            if(has_table)
            {
                byte_vec sec{};
                append_u32_leb(sec, 1u);
                append_u8(sec, k_ref_funcref);
                ::std::uint8_t flags = table_has_max ? 0x01u : 0x00u;
                append_u8(sec, flags);
                append_u32_leb(sec, table_min);
                if(table_has_max) { append_u32_leb(sec, table_max); }
                emit_section(4u, sec);
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

            // element section (9) - MVP encoding: active segments only
            if(!elements.empty())
            {
                byte_vec sec{};
                append_u32_leb(sec, static_cast<::std::uint32_t>(elements.size()));
                for(auto const& seg : elements)
                {
                    append_u32_leb(sec, seg.table_index);
                    append_bytes(sec, seg.offset_expr);
                    append_u32_leb(sec, static_cast<::std::uint32_t>(seg.func_indices.size()));
                    for(auto const idx : seg.func_indices) { append_u32_leb(sec, idx); }
                }
                emit_section(9u, sec);
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
            ::fast_io::fast_terminate();
        }
        if(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
        {
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
        if(it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) { ::fast_io::fast_terminate(); }

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
            if(ft == nullptr) { ::fast_io::fast_terminate(); }

            auto const param_bytes = abi_total_bytes(ft->parameter.begin, ft->parameter.end);
            auto const result_bytes = abi_total_bytes(ft->result.begin, ft->result.end);
            if(param_bytes != packed_params.size()) { ::fast_io::fast_terminate(); }

            // Align locals/stack to 16 bytes (match runtime's typical alignment).
            constexpr ::std::size_t k_align = 16uz;
            auto align_up = [](byte_vec& buf) noexcept -> ::std::byte*
            {
                auto const p = reinterpret_cast<::std::uintptr_t>(buf.data());
                auto const a = (p + (k_align - 1uz)) & ~(k_align - 1uz);
                return reinterpret_cast<::std::byte*>(a);
            };

            byte_vec local_buf(fn.local_bytes_max + k_align);
            // Fill with non-zero pattern, then zero-initialize only the Wasm-visible locals region.
            // This catches bugs where `local_bytes_zeroinit_end` is computed too small or internal-temp locals are read before write.
            ::std::memset(local_buf.data(), 0xCD, local_buf.size());
            ::std::byte* local_base = align_up(local_buf);
            if(fn.local_bytes_zeroinit_end > fn.local_bytes_max) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(fn.local_bytes_zeroinit_end != 0uz) { ::std::memset(local_base, 0, fn.local_bytes_zeroinit_end); }
            if(param_bytes != 0uz) { ::std::memcpy(local_base, packed_params.data(), param_bytes); }

            byte_vec stack_buf((fn.operand_stack_byte_max == 0uz ? 64uz : fn.operand_stack_byte_max) + k_align);
            ::std::memset(stack_buf.data(), 0xCC, stack_buf.size());
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

    [[nodiscard]] inline ::std::int64_t load_i64(byte_vec const& b, ::std::size_t off = 0uz) noexcept
    {
        ::std::int64_t v{};
        ::std::memcpy(::std::addressof(v), b.data() + off, sizeof(v));
        return v;
    }

    [[nodiscard]] inline float load_f32(byte_vec const& b, ::std::size_t off = 0uz) noexcept
    {
        float v{};
        ::std::memcpy(::std::addressof(v), b.data() + off, sizeof(v));
        return v;
    }

    [[nodiscard]] inline double load_f64(byte_vec const& b, ::std::size_t off = 0uz) noexcept
    {
        double v{};
        ::std::memcpy(::std::addressof(v), b.data() + off, sizeof(v));
        return v;
    }

    [[nodiscard]] inline byte_vec pack_i32(::std::int32_t v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_i64(::std::int64_t v)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(v), 8);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f32(float v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f64(double v)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(v), 8);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_no_params()
    {
        return {};
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] inline bool bytecode_contains_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
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
}  // namespace uwvm2test::uwvm_int_strict
