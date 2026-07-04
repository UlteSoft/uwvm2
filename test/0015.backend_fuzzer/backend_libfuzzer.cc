#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <fast_io.h>

#include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/translate.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/storage.h>
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#include <uwvm2/uwvm/runtime/storage/impl.h>
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <uwvm2/uwvm/wasm/type/impl.h>

namespace
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace rtmode = ::uwvm2::uwvm::runtime::runtime_mode;
    namespace int_compiler = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm;
    namespace int_optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

#if !defined(UWVM_DISABLE_INT) && (defined(UWVM_USE_DEFAULT_INT) || defined(UWVM_USE_UWVM_INT))
# define UWVM_BACKEND_FUZZER_HAS_UWVM_INT 1
#else
# define UWVM_BACKEND_FUZZER_HAS_UWVM_INT 0
#endif

#if !defined(UWVM_DISABLE_JIT) && (defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT))
# define UWVM_BACKEND_FUZZER_HAS_LLVM_JIT 1
#else
# define UWVM_BACKEND_FUZZER_HAS_LLVM_JIT 0
#endif

#if UWVM_BACKEND_FUZZER_HAS_UWVM_INT && UWVM_BACKEND_FUZZER_HAS_LLVM_JIT
# define UWVM_BACKEND_FUZZER_HAS_TIERED 1
#else
# define UWVM_BACKEND_FUZZER_HAS_TIERED 0
#endif

    using value_type_t = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using wasm_byte_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;
    using module_t = storage::wasm_module_storage_t;
    using function_type_t = storage::wasm_binfmt1_final_function_type_t;
    using code_t = storage::wasm_binfmt1_final_wasm_code_t;
    using import_type_t = storage::wasm_binfmt1_final_import_type_t;
    using table_type_t = storage::wasm_binfmt1_final_table_type_t;
    using memory_type_t = storage::wasm_binfmt1_final_memory_type_t;
    using global_type_t = storage::wasm_binfmt1_final_global_type_t;
    using local_global_type_t = storage::wasm_binfmt1_final_local_global_type_t;
    using local_entry_t = typename decltype(::std::declval<code_t&>().locals)::value_type;
    using byte_vec = ::std::vector<::std::byte>;

    constexpr char8_t k_main_module_name_storage[] = u8"backend-libfuzzer-main";
    constexpr ::uwvm2::utils::container::u8string_view k_main_module_name{
        k_main_module_name_storage,
        sizeof(k_main_module_name_storage) / sizeof(char8_t) - 1uz};

    constexpr ::std::size_t k_i32_local_count{8uz};
    constexpr ::std::size_t k_i64_local_count{2uz};
    constexpr ::std::size_t k_max_input_bytes{4096uz};

    constexpr ::std::uint32_t k_imported_function_count{1u};
    constexpr ::std::uint32_t k_local_main_index{0u};
    constexpr ::std::uint32_t k_local_i32_add_index{1u};
    constexpr ::std::uint32_t k_local_i32_mix_index{2u};
    constexpr ::std::uint32_t k_local_i32_recursive_index{3u};
    constexpr ::std::uint32_t k_local_i64_mix_index{4u};
    constexpr ::std::uint32_t k_local_f64_neg_index{5u};
    constexpr ::std::size_t k_local_function_count{6uz};
    constexpr ::std::size_t k_type_count{4uz};

    constexpr ::std::uint32_t k_type_void_void{0u};
    constexpr ::std::uint32_t k_type_i32_i32{1u};
    constexpr ::std::uint32_t k_type_i64_i64{2u};
    constexpr ::std::uint32_t k_type_f64_f64{3u};

    [[nodiscard]] constexpr ::std::uint32_t local_function_index_to_function_index(::std::uint32_t local_index) noexcept
    { return k_imported_function_count + local_index; }

    struct local_decl_model
    {
        ::std::uint32_t count{};
        value_type_t type{};
    };

    struct generated_function
    {
        ::std::uint32_t type_index{};
        ::std::vector<::std::uint8_t> body{};
        ::std::uint32_t expr_offset{};
        ::std::array<local_decl_model, 4uz> locals{};
        ::std::size_t local_decl_count{};
        ::std::uint32_t all_local_count{};
    };

    struct helper_model
    {
        ::std::uint32_t i32_add_const{};
        ::std::uint32_t i32_mix_xor{};
        ::std::uint32_t i32_mix_add{};
        ::std::uint32_t i32_mix_rot{};
        ::std::uint64_t i64_add_const{};
        ::std::uint64_t i64_xor_const{};
    };

    struct fuzz_case
    {
        ::std::array<generated_function, k_local_function_count> functions{};
        helper_model helpers{};
        ::std::uint32_t global_initial{};
        ::std::size_t entry_function_index{local_function_index_to_function_index(k_local_main_index)};
        bool has_memory{};
        bool has_table{};
        bool has_global{true};
        bool has_imported_function{true};
        bool has_runtime_call_bridge{};
    };

    void append_u8(::std::vector<::std::uint8_t>& out, ::std::uint8_t v);
    void append_uleb(::std::vector<::std::uint8_t>& out, ::std::uint64_t v);

    void set_function_body(generated_function& fn,
                           ::std::uint32_t type_index,
                           ::std::vector<::std::uint8_t>&& expr,
                           ::std::initializer_list<local_decl_model> locals)
    {
        fn.type_index = type_index;
        fn.body.clear();
        fn.local_decl_count = 0uz;
        fn.all_local_count = 0u;
        for(auto const local: locals)
        {
            if(local.count == 0u) { continue; }
            fn.locals[fn.local_decl_count++] = local;
            fn.all_local_count += local.count;
        }

        append_uleb(fn.body, fn.local_decl_count);
        for(::std::size_t i{}; i != fn.local_decl_count; ++i)
        {
            auto const local{fn.locals[i]};
            append_uleb(fn.body, local.count);
            append_u8(fn.body, static_cast<::std::uint8_t>(local.type));
        }
        fn.expr_offset = static_cast<::std::uint32_t>(fn.body.size());
        fn.body.insert(fn.body.end(), expr.begin(), expr.end());
    }

    [[nodiscard]] ::std::uint32_t u32(::std::uint64_t v) noexcept
    { return static_cast<::std::uint32_t>(v); }

    [[nodiscard]] ::std::int32_t i32(::std::uint32_t v) noexcept
    {
        return v & 0x80000000u ? static_cast<::std::int32_t>(static_cast<::std::uint64_t>(v) - 0x100000000ull)
                               : static_cast<::std::int32_t>(v);
    }

    [[nodiscard]] ::std::uint32_t rotl32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        return r == 0u ? x : static_cast<::std::uint32_t>((x << r) | (x >> (32u - r)));
    }

    [[nodiscard]] ::std::uint32_t rotr32(::std::uint32_t x, ::std::uint32_t r) noexcept
    {
        r &= 31u;
        return r == 0u ? x : static_cast<::std::uint32_t>((x >> r) | (x << (32u - r)));
    }

    [[nodiscard]] ::std::uint32_t shr_s32(::std::uint32_t x, ::std::uint32_t r) noexcept
    { return static_cast<::std::uint32_t>(i32(x) >> (r & 31u)); }

    void append_u8(::std::vector<::std::uint8_t>& out, ::std::uint8_t v)
    { out.push_back(v); }

    void append_uleb(::std::vector<::std::uint8_t>& out, ::std::uint64_t v)
    {
        for(;;)
        {
            auto b{static_cast<::std::uint8_t>(v & 0x7Full)};
            v >>= 7u;
            if(v != 0u) { b = static_cast<::std::uint8_t>(b | 0x80u); }
            out.push_back(b);
            if(v == 0u) { return; }
        }
    }

    void append_sleb(::std::vector<::std::uint8_t>& out, ::std::int64_t v)
    {
        bool more{true};
        while(more)
        {
            auto b{static_cast<::std::uint8_t>(static_cast<::std::uint64_t>(v) & 0x7Full)};
            v >>= 7;
            bool const sign{(b & 0x40u) != 0u};
            if((v == 0 && !sign) || (v == -1 && sign)) { more = false; }
            else { b = static_cast<::std::uint8_t>(b | 0x80u); }
            out.push_back(b);
        }
    }

    void append_le32(::std::vector<::std::uint8_t>& out, ::std::uint32_t v)
    {
        for(unsigned i{}; i != 4u; ++i) { append_u8(out, static_cast<::std::uint8_t>((v >> (i * 8u)) & 0xffu)); }
    }

    void append_le64(::std::vector<::std::uint8_t>& out, ::std::uint64_t v)
    {
        append_le32(out, static_cast<::std::uint32_t>(v));
        append_le32(out, static_cast<::std::uint32_t>(v >> 32u));
    }

    void emit_i32_const(::std::vector<::std::uint8_t>& out, ::std::uint32_t v)
    {
        append_u8(out, 0x41u);
        append_sleb(out, static_cast<::std::int64_t>(i32(v)));
    }

    void emit_i64_const(::std::vector<::std::uint8_t>& out, ::std::uint64_t v)
    {
        append_u8(out, 0x42u);
        append_sleb(out, static_cast<::std::int64_t>(v));
    }

    void emit_f32_const_bits(::std::vector<::std::uint8_t>& out, ::std::uint32_t bits)
    {
        append_u8(out, 0x43u);
        append_le32(out, bits);
    }

    void emit_f64_const_bits(::std::vector<::std::uint8_t>& out, ::std::uint64_t bits)
    {
        append_u8(out, 0x44u);
        append_le64(out, bits);
    }

    void emit_local_get(::std::vector<::std::uint8_t>& out, ::std::uint32_t i)
    {
        append_u8(out, 0x20u);
        append_uleb(out, i);
    }

    void emit_local_set(::std::vector<::std::uint8_t>& out, ::std::uint32_t i)
    {
        append_u8(out, 0x21u);
        append_uleb(out, i);
    }

    void emit_local_tee(::std::vector<::std::uint8_t>& out, ::std::uint32_t i)
    {
        append_u8(out, 0x22u);
        append_uleb(out, i);
    }

    void emit_call(::std::vector<::std::uint8_t>& out, ::std::uint32_t function_index)
    {
        append_u8(out, 0x10u);
        append_uleb(out, function_index);
    }

    void emit_call_indirect(::std::vector<::std::uint8_t>& out, ::std::uint32_t type_index, ::std::uint32_t table_index = 0u)
    {
        append_u8(out, 0x11u);
        append_uleb(out, type_index);
        append_uleb(out, table_index);
    }

    void emit_global_get(::std::vector<::std::uint8_t>& out, ::std::uint32_t i)
    {
        append_u8(out, 0x23u);
        append_uleb(out, i);
    }

    void emit_global_set(::std::vector<::std::uint8_t>& out, ::std::uint32_t i)
    {
        append_u8(out, 0x24u);
        append_uleb(out, i);
    }

    void emit_memarg(::std::vector<::std::uint8_t>& out, ::std::uint32_t align, ::std::uint32_t offset = 0u)
    {
        append_uleb(out, align);
        append_uleb(out, offset);
    }

    void emit_memory_size(::std::vector<::std::uint8_t>& out)
    {
        append_u8(out, 0x3fu);
        append_u8(out, 0x00u);
    }

    void emit_memory_grow(::std::vector<::std::uint8_t>& out)
    {
        append_u8(out, 0x40u);
        append_u8(out, 0x00u);
    }

    void emit_check_i32(::std::vector<::std::uint8_t>& out, ::std::uint32_t expected)
    {
        emit_i32_const(out, expected);
        append_u8(out, 0x47u);
        append_u8(out, 0x04u);
        append_u8(out, 0x40u);
        append_u8(out, 0x00u);
        append_u8(out, 0x0bu);
    }

    void emit_check_i64(::std::vector<::std::uint8_t>& out, ::std::uint64_t expected)
    {
        emit_i64_const(out, expected);
        append_u8(out, 0x52u);
        append_u8(out, 0x04u);
        append_u8(out, 0x40u);
        append_u8(out, 0x00u);
        append_u8(out, 0x0bu);
    }

    void emit_check_f32_bits(::std::vector<::std::uint8_t>& out, ::std::uint32_t expected_bits)
    {
        append_u8(out, 0xbcu);
        emit_check_i32(out, expected_bits);
    }

    void emit_check_f64_bits(::std::vector<::std::uint8_t>& out, ::std::uint64_t expected_bits)
    {
        append_u8(out, 0xbdu);
        emit_check_i64(out, expected_bits);
    }

    struct input_reader
    {
        ::std::uint8_t const* data{};
        ::std::size_t size{};
        ::std::size_t pos{};

        [[nodiscard]] ::std::uint8_t byte(::std::uint8_t fallback = 0u) noexcept
        {
            if(pos < size) { return data[pos++]; }
            fallback = static_cast<::std::uint8_t>(fallback * 33u + static_cast<::std::uint8_t>(pos));
            ++pos;
            return fallback;
        }

        [[nodiscard]] ::std::uint32_t next_u32() noexcept
        {
            ::std::uint32_t v{};
            for(unsigned i{}; i != 4u; ++i) { v |= static_cast<::std::uint32_t>(byte(static_cast<::std::uint8_t>(17u + i))) << (i * 8u); }
            return v;
        }

        [[nodiscard]] ::std::uint64_t next_u64() noexcept
        {
            ::std::uint64_t lo{next_u32()};
            ::std::uint64_t hi{next_u32()};
            return lo | (hi << 32u);
        }

        [[nodiscard]] ::std::size_t pick(::std::size_t n) noexcept
        { return n == 0uz ? 0uz : static_cast<::std::size_t>(byte()) % n; }
    };

    [[nodiscard]] ::std::uint32_t helper_i32_add(helper_model const& h, ::std::uint32_t x) noexcept
    { return x + h.i32_add_const; }

    [[nodiscard]] ::std::uint32_t helper_i32_mix(helper_model const& h, ::std::uint32_t x) noexcept
    { return rotl32(x ^ h.i32_mix_xor, h.i32_mix_rot) + h.i32_mix_add; }

    [[nodiscard]] ::std::uint64_t helper_i64_mix(helper_model const& h, ::std::uint64_t x) noexcept
    { return (x + h.i64_add_const) ^ h.i64_xor_const; }

    void build_helper_functions(fuzz_case& out)
    {
        {
            ::std::vector<::std::uint8_t> expr{};
            emit_local_get(expr, 0u);
            emit_i32_const(expr, out.helpers.i32_add_const);
            append_u8(expr, 0x6au);
            append_u8(expr, 0x0bu);
            set_function_body(out.functions[k_local_i32_add_index], k_type_i32_i32, ::std::move(expr), {});
        }

        {
            ::std::vector<::std::uint8_t> expr{};
            emit_local_get(expr, 0u);
            emit_i32_const(expr, out.helpers.i32_mix_xor);
            append_u8(expr, 0x73u);
            emit_i32_const(expr, out.helpers.i32_mix_rot);
            append_u8(expr, 0x77u);
            emit_i32_const(expr, out.helpers.i32_mix_add);
            append_u8(expr, 0x6au);
            append_u8(expr, 0x0bu);
            set_function_body(out.functions[k_local_i32_mix_index], k_type_i32_i32, ::std::move(expr), {});
        }

        {
            ::std::vector<::std::uint8_t> expr{};
            emit_local_get(expr, 0u);
            append_u8(expr, 0x45u);
            append_u8(expr, 0x04u);
            append_u8(expr, static_cast<::std::uint8_t>(value_type_t::i32));
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x05u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6bu);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_recursive_index));
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            set_function_body(out.functions[k_local_i32_recursive_index], k_type_i32_i32, ::std::move(expr), {});
        }

        {
            ::std::vector<::std::uint8_t> expr{};
            emit_local_get(expr, 0u);
            emit_i64_const(expr, out.helpers.i64_add_const);
            append_u8(expr, 0x7cu);
            emit_i64_const(expr, out.helpers.i64_xor_const);
            append_u8(expr, 0x85u);
            append_u8(expr, 0x0bu);
            set_function_body(out.functions[k_local_i64_mix_index], k_type_i64_i64, ::std::move(expr), {});
        }

        {
            ::std::vector<::std::uint8_t> expr{};
            emit_local_get(expr, 0u);
            append_u8(expr, 0x9au);
            append_u8(expr, 0x0bu);
            set_function_body(out.functions[k_local_f64_neg_index], k_type_f64_f64, ::std::move(expr), {});
        }
    }

    struct code_projector
    {
        input_reader& in;
        fuzz_case& out;
        bool enable_runtime_calls{};
        ::std::vector<::std::uint8_t> expr{};
        ::std::array<::std::uint32_t, k_i32_local_count> i32_locals{};
        ::std::array<::std::uint64_t, k_i64_local_count> i64_locals{};
        ::std::vector<::std::uint32_t> i32_stack{};
        ::std::uint32_t global_i32{};
        bool grew_memory{};
        bool enable_tiered_osr_stress{};
        bool enable_tiered_switch_stress{};

        void init_locals()
        {
            for(::std::uint32_t i{}; i != k_i32_local_count; ++i)
            {
                auto const v{in.next_u32()};
                i32_locals[i] = v;
                emit_i32_const(expr, v);
                emit_local_set(expr, i);
            }
            for(::std::uint32_t i{}; i != k_i64_local_count; ++i)
            {
                auto const v{in.next_u64()};
                i64_locals[i] = v;
                emit_i64_const(expr, v);
                emit_local_set(expr, static_cast<::std::uint32_t>(k_i32_local_count) + i);
            }
        }

        void push_i32_const()
        {
            auto const v{in.next_u32()};
            emit_i32_const(expr, v);
            i32_stack.push_back(v);
        }

        void push_i32_local()
        {
            auto const idx{static_cast<::std::uint32_t>(in.pick(k_i32_local_count))};
            emit_local_get(expr, idx);
            i32_stack.push_back(i32_locals[idx]);
        }

        void unary_i32(::std::uint8_t op)
        {
            auto const v{i32_stack.back()};
            i32_stack.pop_back();
            ::std::uint32_t r{};
            switch(op)
            {
                case 0x45u: r = v == 0u ? 1u : 0u; break;
                case 0x67u: r = v == 0u ? 32u : static_cast<::std::uint32_t>(__builtin_clz(v)); break;
                case 0x68u: r = v == 0u ? 32u : static_cast<::std::uint32_t>(__builtin_ctz(v)); break;
                case 0x69u: r = static_cast<::std::uint32_t>(__builtin_popcount(v)); break;
                default: ::std::abort();
            }
            append_u8(expr, op);
            i32_stack.push_back(r);
        }

        void binary_i32(::std::uint8_t op)
        {
            auto const rhs{i32_stack.back()};
            i32_stack.pop_back();
            auto const lhs{i32_stack.back()};
            i32_stack.pop_back();
            ::std::uint32_t r{};
            switch(op)
            {
                case 0x6Au: r = lhs + rhs; break;
                case 0x6Bu: r = lhs - rhs; break;
                case 0x6Cu: r = lhs * rhs; break;
                case 0x71u: r = lhs & rhs; break;
                case 0x72u: r = lhs | rhs; break;
                case 0x73u: r = lhs ^ rhs; break;
                case 0x74u: r = lhs << (rhs & 31u); break;
                case 0x75u: r = shr_s32(lhs, rhs); break;
                case 0x76u: r = lhs >> (rhs & 31u); break;
                case 0x77u: r = rotl32(lhs, rhs); break;
                case 0x78u: r = rotr32(lhs, rhs); break;
                case 0x46u: r = lhs == rhs ? 1u : 0u; break;
                case 0x47u: r = lhs != rhs ? 1u : 0u; break;
                case 0x48u: r = i32(lhs) < i32(rhs) ? 1u : 0u; break;
                case 0x49u: r = lhs < rhs ? 1u : 0u; break;
                case 0x4Eu: r = i32(lhs) >= i32(rhs) ? 1u : 0u; break;
                case 0x4Fu: r = lhs >= rhs ? 1u : 0u; break;
                default: ::std::abort();
            }
            append_u8(expr, op);
            i32_stack.push_back(r);
        }

        void local_store(bool tee)
        {
            auto const idx{static_cast<::std::uint32_t>(in.pick(k_i32_local_count))};
            auto const v{i32_stack.back()};
            i32_stack.pop_back();
            i32_locals[idx] = v;
            if(tee)
            {
                emit_local_tee(expr, idx);
                i32_stack.push_back(v);
            }
            else
            {
                emit_local_set(expr, idx);
            }
        }

        void select_i32()
        {
            auto const cond{i32_stack.back()};
            i32_stack.pop_back();
            auto const v2{i32_stack.back()};
            i32_stack.pop_back();
            auto const v1{i32_stack.back()};
            i32_stack.pop_back();
            append_u8(expr, 0x1bu);
            i32_stack.push_back(cond != 0u ? v1 : v2);
        }

        void drop_i32()
        {
            i32_stack.pop_back();
            append_u8(expr, 0x1au);
        }

        void memory_segment()
        {
            out.has_memory = true;
            auto const offset{static_cast<::std::uint32_t>((in.next_u32() % 4096u) & ~3u)};
            auto const value{in.next_u32()};
            emit_i32_const(expr, offset);
            emit_i32_const(expr, value);
            append_u8(expr, 0x36u);
            emit_memarg(expr, 2u);
            emit_i32_const(expr, offset);
            append_u8(expr, 0x28u);
            emit_memarg(expr, 2u);
            emit_check_i32(expr, value);
        }

        void memory_grow_segment()
        {
            if(grew_memory) { return; }
            grew_memory = true;
            out.has_memory = true;

            emit_memory_size(expr);
            emit_check_i32(expr, 1u);

            emit_i32_const(expr, 1u);
            emit_memory_grow(expr);
            emit_check_i32(expr, 1u);

            emit_memory_size(expr);
            emit_check_i32(expr, 2u);

            auto const edge_value{in.next_u32()};
            emit_i32_const(expr, 65536u - 4u);
            emit_i32_const(expr, edge_value);
            append_u8(expr, 0x36u);
            emit_memarg(expr, 2u);
            emit_i32_const(expr, 65536u - 4u);
            append_u8(expr, 0x28u);
            emit_memarg(expr, 2u);
            emit_check_i32(expr, edge_value);

            auto const cross_value{in.next_u32()};
            emit_i32_const(expr, 65536u - 2u);
            emit_i32_const(expr, cross_value);
            append_u8(expr, 0x36u);
            emit_memarg(expr, 0u);
            emit_i32_const(expr, 65536u - 2u);
            append_u8(expr, 0x28u);
            emit_memarg(expr, 0u);
            emit_check_i32(expr, cross_value);

            auto const page2_value{in.next_u32()};
            emit_i32_const(expr, (2u * 65536u) - 4u);
            emit_i32_const(expr, page2_value);
            append_u8(expr, 0x36u);
            emit_memarg(expr, 2u);
            emit_i32_const(expr, (2u * 65536u) - 4u);
            append_u8(expr, 0x28u);
            emit_memarg(expr, 2u);
            emit_check_i32(expr, page2_value);

            emit_i32_const(expr, 10u);
            emit_memory_grow(expr);
            emit_check_i32(expr, 0xffffffffu);
        }

        void global_segment()
        {
            out.has_global = true;
            emit_global_get(expr, 0u);
            emit_check_i32(expr, global_i32);

            auto const next{in.next_u32()};
            emit_i32_const(expr, next);
            emit_global_set(expr, 0u);
            global_i32 = next;

            emit_global_get(expr, 0u);
            emit_check_i32(expr, global_i32);
        }

        void float_segment()
        {
            auto const f32_payload{(in.next_u32() & 0x003fffffu) | 0x00400000u};
            auto const f32_nan_bits{0x7f800000u | f32_payload};
            emit_f32_const_bits(expr, f32_nan_bits);
            emit_check_f32_bits(expr, f32_nan_bits);

            emit_f64_const_bits(expr, 0x8000000000000000ull);
            emit_check_f64_bits(expr, 0x8000000000000000ull);

            emit_f64_const_bits(expr, 0x402a000000000000ull);
            emit_f64_const_bits(expr, 0x4014000000000000ull);
            append_u8(expr, 0xa0u);
            append_u8(expr, 0xaau);
            emit_check_i32(expr, 18u);
        }

        void direct_call_segment()
        {
            if(!enable_runtime_calls) { return; }
            out.has_runtime_call_bridge = true;

            auto const x{in.next_u32()};
            emit_i32_const(expr, x);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_add_index));
            emit_check_i32(expr, helper_i32_add(out.helpers, x));

            auto const y{in.next_u32()};
            emit_i32_const(expr, y);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_mix_index));
            emit_check_i32(expr, helper_i32_mix(out.helpers, y));

            auto const z{static_cast<::std::uint32_t>(in.byte() & 7u)};
            emit_i32_const(expr, z);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_recursive_index));
            emit_check_i32(expr, z + 1u);

            auto const w{in.next_u64()};
            emit_i64_const(expr, w);
            emit_call(expr, local_function_index_to_function_index(k_local_i64_mix_index));
            emit_check_i64(expr, helper_i64_mix(out.helpers, w));

            auto const nan_payload{(in.next_u64() & 0x0007ffffffffffffull) | 1ull};
            auto const nan_bits{0x7ff8000000000000ull | nan_payload};
            emit_f64_const_bits(expr, nan_bits);
            emit_call(expr, local_function_index_to_function_index(k_local_f64_neg_index));
            emit_check_f64_bits(expr, nan_bits ^ 0x8000000000000000ull);

            auto const imported_x{in.next_u32()};
            emit_i32_const(expr, imported_x);
            emit_call(expr, 0u);
            emit_check_i32(expr, helper_i32_add(out.helpers, imported_x));
        }

        void indirect_call_segment()
        {
            if(!enable_runtime_calls) { return; }
            out.has_table = true;
            out.has_runtime_call_bridge = true;

            auto const selector{static_cast<::std::uint32_t>(in.byte() & 1u)};
            auto const x{in.next_u32()};
            emit_i32_const(expr, x);
            emit_i32_const(expr, selector);
            emit_call_indirect(expr, k_type_i32_i32);
            emit_check_i32(expr, selector == 0u ? helper_i32_add(out.helpers, x) : helper_i32_mix(out.helpers, x));

            auto const y{x ^ in.next_u32()};
            emit_i32_const(expr, y);
            emit_i32_const(expr, selector);
            emit_call_indirect(expr, k_type_i32_i32);
            emit_check_i32(expr, selector == 0u ? helper_i32_add(out.helpers, y) : helper_i32_mix(out.helpers, y));
        }

        void hot_direct_call_segment()
        {
            if(!enable_runtime_calls) { return; }
            out.has_runtime_call_bridge = true;

            auto const use_mix{(in.byte() & 1u) != 0u};
            auto const bound{static_cast<::std::uint32_t>(2304u + static_cast<::std::uint32_t>(in.byte() & 7u) * 128u)};
            ::std::uint32_t acc{};
            for(::std::uint32_t i{}; i != bound; ++i)
            {
                acc = u32(acc + (use_mix ? helper_i32_mix(out.helpers, i) : helper_i32_add(out.helpers, i)));
            }

            emit_i32_const(expr, 0u);
            emit_local_set(expr, 0u);
            emit_i32_const(expr, 0u);
            emit_local_set(expr, 1u);
            append_u8(expr, 0x02u);
            append_u8(expr, 0x40u);
            append_u8(expr, 0x03u);
            append_u8(expr, 0x40u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, bound);
            append_u8(expr, 0x4fu);
            append_u8(expr, 0x0du);
            append_uleb(expr, 1u);
            emit_local_get(expr, 1u);
            emit_local_get(expr, 0u);
            emit_call(expr,
                      local_function_index_to_function_index(use_mix ? k_local_i32_mix_index : k_local_i32_add_index));
            append_u8(expr, 0x6au);
            emit_local_set(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 0u);
            append_u8(expr, 0x0cu);
            append_uleb(expr, 0u);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            emit_local_get(expr, 1u);
            emit_check_i32(expr, acc);

            i32_locals[0] = bound;
            i32_locals[1] = acc;
        }

        void hot_indirect_call_segment()
        {
            if(!enable_runtime_calls) { return; }
            out.has_table = true;
            out.has_runtime_call_bridge = true;

            auto const bound{static_cast<::std::uint32_t>(2304u + static_cast<::std::uint32_t>(in.byte() & 3u) * 128u)};
            ::std::uint32_t acc{};
            for(::std::uint32_t i{}; i != bound; ++i)
            {
                acc = u32(acc + ((i & 1u) == 0u ? helper_i32_add(out.helpers, i) : helper_i32_mix(out.helpers, i)));
            }

            emit_i32_const(expr, 0u);
            emit_local_set(expr, 0u);
            emit_i32_const(expr, 0u);
            emit_local_set(expr, 1u);
            append_u8(expr, 0x02u);
            append_u8(expr, 0x40u);
            append_u8(expr, 0x03u);
            append_u8(expr, 0x40u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, bound);
            append_u8(expr, 0x4fu);
            append_u8(expr, 0x0du);
            append_uleb(expr, 1u);
            emit_local_get(expr, 1u);
            emit_local_get(expr, 0u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x71u);
            emit_call_indirect(expr, k_type_i32_i32);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 0u);
            append_u8(expr, 0x0cu);
            append_uleb(expr, 0u);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            emit_local_get(expr, 1u);
            emit_check_i32(expr, acc);

            i32_locals[0] = bound;
            i32_locals[1] = acc;
        }

        void call_boundary_matrix_segment()
        {
            if(!enable_runtime_calls) { return; }
            out.has_table = true;
            out.has_runtime_call_bridge = true;

            auto const direct_x{in.next_u32()};
            auto const direct_x_expected{helper_i32_add(out.helpers, direct_x)};
            emit_i32_const(expr, direct_x);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_add_index));
            emit_local_set(expr, 2u);
            i32_locals[2] = direct_x_expected;
            emit_local_get(expr, 2u);
            emit_check_i32(expr, direct_x_expected);

            auto const direct_y{in.next_u32()};
            emit_i32_const(expr, direct_y);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_mix_index));
            append_u8(expr, 0x1au);

            auto const direct_z{static_cast<::std::uint32_t>(in.byte() & 7u)};
            auto const direct_z_expected{direct_z + 1u};
            emit_i32_const(expr, direct_z);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_recursive_index));
            emit_local_set(expr, 3u);
            i32_locals[3] = direct_z_expected;
            emit_local_get(expr, 3u);
            emit_check_i32(expr, direct_z_expected);

            auto const imported_x{in.next_u32()};
            auto const imported_x_expected{helper_i32_add(out.helpers, imported_x)};
            emit_i32_const(expr, imported_x);
            emit_call(expr, 0u);
            emit_local_set(expr, 4u);
            i32_locals[4] = imported_x_expected;
            emit_local_get(expr, 4u);
            emit_check_i32(expr, imported_x_expected);

            auto const direct_w{in.next_u64()};
            auto const direct_w_expected{helper_i64_mix(out.helpers, direct_w)};
            emit_i64_const(expr, direct_w);
            emit_call(expr, local_function_index_to_function_index(k_local_i64_mix_index));
            emit_local_set(expr, 8u);
            i64_locals[0] = direct_w_expected;
            emit_local_get(expr, 8u);
            emit_check_i64(expr, direct_w_expected);

            auto const indirect_x{in.next_u32()};
            auto const indirect_x_expected{helper_i32_add(out.helpers, indirect_x)};
            emit_i32_const(expr, indirect_x);
            emit_i32_const(expr, 0u);
            emit_call_indirect(expr, k_type_i32_i32);
            emit_local_set(expr, 5u);
            i32_locals[5] = indirect_x_expected;
            emit_local_get(expr, 5u);
            emit_check_i32(expr, indirect_x_expected);

            auto const indirect_y{in.next_u32()};
            emit_i32_const(expr, indirect_y);
            emit_i32_const(expr, 1u);
            emit_call_indirect(expr, k_type_i32_i32);
            append_u8(expr, 0x1au);
        }

        void tiered_switch_pressure_segment()
        {
            if(!enable_tiered_switch_stress) { return; }
            if(!enable_runtime_calls) { return; }

            auto const bound{70000u};
            ::std::uint32_t acc{};
            for(::std::uint32_t i{}; i != bound; ++i) { acc = u32(acc + helper_i32_add(out.helpers, i)); }

            emit_i32_const(expr, 0u);
            emit_local_set(expr, 0u);
            emit_i32_const(expr, 0u);
            emit_local_set(expr, 1u);
            append_u8(expr, 0x02u);
            append_u8(expr, 0x40u);
            append_u8(expr, 0x03u);
            append_u8(expr, 0x40u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, bound);
            append_u8(expr, 0x4fu);
            append_u8(expr, 0x0du);
            append_uleb(expr, 1u);
            emit_local_get(expr, 1u);
            emit_local_get(expr, 0u);
            emit_call(expr, local_function_index_to_function_index(k_local_i32_add_index));
            append_u8(expr, 0x6au);
            emit_local_set(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 0u);
            append_u8(expr, 0x0cu);
            append_uleb(expr, 0u);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            emit_local_get(expr, 1u);
            emit_check_i32(expr, acc);

            i32_locals[0] = bound;
            i32_locals[1] = acc;
        }

        void tiered_osr_loop_segment()
        {
            if(!enable_tiered_osr_stress) { return; }

            for(::std::size_t i{}; i != 1600uz; ++i) { append_u8(expr, 0x01u); }

            constexpr ::std::uint32_t bound{36000u};
            auto const salt{in.next_u32() & 0xffffu};
            ::std::uint32_t acc{};
            for(::std::uint32_t i{}; i != bound; ++i) { acc = u32(acc + (i ^ salt)); }

            emit_i32_const(expr, 0u);
            emit_local_set(expr, 0u);
            emit_i32_const(expr, 0u);
            emit_local_set(expr, 1u);
            append_u8(expr, 0x02u);
            append_u8(expr, 0x40u);
            append_u8(expr, 0x03u);
            append_u8(expr, 0x40u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, bound);
            append_u8(expr, 0x4fu);
            append_u8(expr, 0x0du);
            append_uleb(expr, 1u);
            emit_local_get(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, salt);
            append_u8(expr, 0x73u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 0u);
            append_u8(expr, 0x0cu);
            append_uleb(expr, 0u);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            emit_local_get(expr, 1u);
            emit_check_i32(expr, acc);

            i32_locals[0] = bound;
            i32_locals[1] = acc;
        }

        void i64_segment()
        {
            auto const a{in.next_u64()};
            auto const b{in.next_u64()};
            auto const shift{static_cast<::std::uint64_t>(in.byte() & 63u)};
            auto const expected{static_cast<::std::uint64_t>((a + b) ^ (a << shift))};
            emit_i64_const(expr, a);
            emit_i64_const(expr, b);
            append_u8(expr, 0x7cu);
            emit_i64_const(expr, a);
            emit_i64_const(expr, shift);
            append_u8(expr, 0x86u);
            append_u8(expr, 0x85u);
            emit_check_i64(expr, expected);
        }

        void loop_segment()
        {
            auto const bound{static_cast<::std::uint32_t>(4u + (in.byte() & 31u))};
            auto const salt{in.next_u32() & 0xffffu};
            ::std::uint32_t acc{};
            for(::std::uint32_t i{}; i != bound; ++i) { acc = u32(acc + (i ^ salt)); }

            emit_i32_const(expr, 0u);
            emit_local_set(expr, 0u);
            emit_i32_const(expr, 0u);
            emit_local_set(expr, 1u);
            append_u8(expr, 0x02u);
            append_u8(expr, 0x40u);
            append_u8(expr, 0x03u);
            append_u8(expr, 0x40u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, bound);
            append_u8(expr, 0x4fu);
            append_u8(expr, 0x0du);
            append_uleb(expr, 1u);
            emit_local_get(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, salt);
            append_u8(expr, 0x73u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 1u);
            emit_local_get(expr, 0u);
            emit_i32_const(expr, 1u);
            append_u8(expr, 0x6au);
            emit_local_set(expr, 0u);
            append_u8(expr, 0x0cu);
            append_uleb(expr, 0u);
            append_u8(expr, 0x0bu);
            append_u8(expr, 0x0bu);
            emit_local_get(expr, 1u);
            emit_check_i32(expr, acc);

            i32_locals[0] = bound;
            i32_locals[1] = acc;
        }

        void finish()
        {
            global_segment();
            float_segment();
            memory_grow_segment();
            direct_call_segment();
            indirect_call_segment();
            call_boundary_matrix_segment();
            tiered_switch_pressure_segment();
            hot_direct_call_segment();
            if((in.byte() & 1u) != 0u) { hot_indirect_call_segment(); }
            tiered_osr_loop_segment();

            auto const step_count{static_cast<::std::size_t>(16u + (in.byte() & 0x7fu))};
            for(::std::size_t i{}; i != step_count; ++i)
            {
                auto const depth{i32_stack.size()};
                auto const action{in.byte() % 100u};
                if(depth == 0uz || (action < 28u && depth < 16uz))
                {
                    if((in.byte() & 1u) != 0u) { push_i32_local(); }
                    else { push_i32_const(); }
                }
                else if(action < 38u)
                {
                    append_u8(expr, 0x01u);
                }
                else if(action < 48u && depth >= 1uz)
                {
                    local_store((in.byte() & 1u) != 0u);
                }
                else if(action < 58u && depth >= 1uz)
                {
                    static constexpr ::std::array<::std::uint8_t, 4uz> ops{0x45u, 0x67u, 0x68u, 0x69u};
                    unary_i32(ops[in.pick(ops.size())]);
                }
                else if(action < 66u && depth >= 1uz)
                {
                    drop_i32();
                }
                else if(action < 74u && depth >= 3uz)
                {
                    select_i32();
                }
                else if(action < 80u)
                {
                    memory_segment();
                }
                else if(action < 83u)
                {
                    i64_segment();
                }
                else if(action < 86u)
                {
                    loop_segment();
                }
                else if(action < 89u)
                {
                    memory_grow_segment();
                }
                else if(action < 92u)
                {
                    global_segment();
                }
                else if(action < 95u)
                {
                    float_segment();
                }
                else if(action < 97u)
                {
                    direct_call_segment();
                }
                else if(action < 99u)
                {
                    indirect_call_segment();
                }
                else if(depth >= 2uz)
                {
                    static constexpr ::std::array<::std::uint8_t, 16uz> ops{
                        0x6au, 0x6bu, 0x6cu, 0x71u, 0x72u, 0x73u, 0x74u, 0x75u,
                        0x76u, 0x77u, 0x78u, 0x46u, 0x47u, 0x48u, 0x49u, 0x4eu};
                    binary_i32(ops[in.pick(ops.size())]);
                }
            }

            if(i32_stack.empty()) { push_i32_local(); }
            while(i32_stack.size() > 1uz) { binary_i32(0x6au); }
            auto const final_i32{i32_stack.back()};
            emit_local_tee(expr, 0u);
            i32_locals[0] = final_i32;
            emit_check_i32(expr, final_i32);
            emit_local_get(expr, 0u);
            emit_global_set(expr, 0u);
            global_i32 = final_i32;
            append_u8(expr, 0x0bu);

            set_function_body(out.functions[k_local_main_index],
                              k_type_void_void,
                              ::std::move(expr),
                              {{static_cast<::std::uint32_t>(k_i32_local_count), value_type_t::i32},
                               {static_cast<::std::uint32_t>(k_i64_local_count), value_type_t::i64}});
        }
    };

    [[nodiscard]] fuzz_case project_input(::std::uint8_t const* data,
                                          ::std::size_t size,
                                          bool enable_runtime_calls = true,
                                          bool enable_tiered_osr_stress = false,
                                          bool enable_tiered_switch_stress = false)
    {
        size = ::std::min(size, k_max_input_bytes);
        input_reader in{data, size, 0uz};
        fuzz_case out{};
        out.helpers.i32_add_const = in.next_u32();
        out.helpers.i32_mix_xor = in.next_u32();
        out.helpers.i32_mix_add = in.next_u32();
        out.helpers.i32_mix_rot = in.byte() & 31u;
        out.helpers.i64_add_const = in.next_u64();
        out.helpers.i64_xor_const = in.next_u64();
        out.global_initial = in.next_u32();
        build_helper_functions(out);

        code_projector p{in, out, enable_runtime_calls};
        p.enable_tiered_osr_stress = enable_tiered_osr_stress;
        p.enable_tiered_switch_stress = enable_tiered_switch_stress;
        p.global_i32 = out.global_initial;
        p.init_locals();
        p.finish();
        return out;
    }

    [[nodiscard]] ::std::size_t abi_bytes(value_type_t t) noexcept
    {
        switch(t)
        {
            case value_type_t::i32:
            case value_type_t::f32:
                return 4uz;
            case value_type_t::i64:
            case value_type_t::f64:
                return 8uz;
            default:
                return 0uz;
        }
    }

    [[nodiscard]] ::std::size_t abi_total_bytes(value_type_t const* begin, value_type_t const* end) noexcept
    {
        ::std::size_t total{};
        for(auto it{begin}; it != end; ++it)
        {
            auto const add{abi_bytes(*it)};
            if(add == 0uz || add > (::std::numeric_limits<::std::size_t>::max() - total)) { return 0uz; }
            total += add;
        }
        return total;
    }

    struct direct_module_owner
    {
        ::std::array<value_type_t, 1uz> empty_type_storage{value_type_t::i32};
        ::std::array<value_type_t, 1uz> i32_type_storage{value_type_t::i32};
        ::std::array<value_type_t, 1uz> i64_type_storage{value_type_t::i64};
        ::std::array<value_type_t, 1uz> f64_type_storage{value_type_t::f64};
        ::std::array<function_type_t, k_type_count> types{};
        ::std::array<import_type_t, 1uz> imports{};
        ::std::array<code_t, k_local_function_count> codes{};
        ::std::array<::std::vector<wasm_byte_t>, k_local_function_count> code_bytes{};
        ::std::array<table_type_t, 1uz> tables{};
        ::std::array<memory_type_t, 1uz> memories{};
        ::std::array<global_type_t, 1uz> globals{};
        ::std::array<local_global_type_t, 1uz> local_globals{};

        void build(fuzz_case const& c, module_t& module)
        {
            types[k_type_void_void].parameter.begin = empty_type_storage.data();
            types[k_type_void_void].parameter.end = empty_type_storage.data();
            types[k_type_void_void].result.begin = empty_type_storage.data();
            types[k_type_void_void].result.end = empty_type_storage.data();

            types[k_type_i32_i32].parameter.begin = i32_type_storage.data();
            types[k_type_i32_i32].parameter.end = i32_type_storage.data() + 1uz;
            types[k_type_i32_i32].result.begin = i32_type_storage.data();
            types[k_type_i32_i32].result.end = i32_type_storage.data() + 1uz;

            types[k_type_i64_i64].parameter.begin = i64_type_storage.data();
            types[k_type_i64_i64].parameter.end = i64_type_storage.data() + 1uz;
            types[k_type_i64_i64].result.begin = i64_type_storage.data();
            types[k_type_i64_i64].result.end = i64_type_storage.data() + 1uz;

            types[k_type_f64_f64].parameter.begin = f64_type_storage.data();
            types[k_type_f64_f64].parameter.end = f64_type_storage.data() + 1uz;
            types[k_type_f64_f64].result.begin = f64_type_storage.data();
            types[k_type_f64_f64].result.end = f64_type_storage.data() + 1uz;

            module.type_section_storage.type_section_begin = types.data();
            module.type_section_storage.type_section_end = types.data() + types.size();

            for(::std::size_t i{}; i != k_local_function_count; ++i)
            {
                auto const& src{c.functions[i]};
                auto& bytes{code_bytes[i]};
                bytes.clear();
                bytes.reserve(src.body.size());
                for(auto const b: src.body) { bytes.push_back(static_cast<wasm_byte_t>(b)); }

                auto& code{codes[i]};
                auto const* begin{bytes.data()};
                code.body.code_begin = begin;
                code.body.expr_begin = begin + src.expr_offset;
                code.body.code_end = begin + bytes.size();
                code.locals.clear();
                code.locals.reserve(src.local_decl_count);
                for(::std::size_t j{}; j != src.local_decl_count; ++j)
                {
                    local_entry_t entry{};
                    entry.count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(src.locals[j].count);
                    entry.type = src.locals[j].type;
                    code.locals.push_back_unchecked(entry);
                }
                code.all_local_count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(src.all_local_count);
            }

            module.local_defined_function_vec_storage.reserve(k_local_function_count);
            for(::std::size_t i{}; i != k_local_function_count; ++i)
            {
                storage::local_defined_function_storage_t fn{};
                fn.function_type_ptr = ::std::addressof(types[c.functions[i].type_index]);
                fn.wasm_code_ptr = ::std::addressof(codes[i]);
                module.local_defined_function_vec_storage.push_back_unchecked(fn);
            }

            module.local_defined_code_vec_storage.reserve(k_local_function_count);
            for(::std::size_t i{}; i != k_local_function_count; ++i)
            {
                storage::local_defined_code_storage_t rec{};
                rec.code_type_ptr = ::std::addressof(codes[i]);
                rec.func_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(i));
                module.local_defined_code_vec_storage.push_back_unchecked(rec);
            }

            if(c.has_imported_function)
            {
                auto& import_ty{imports[0]};
                import_ty.imports.type = ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func;
                import_ty.imports.storage.function = ::std::addressof(types[k_type_i32_i32]);

                module.imported_function_vec_storage.reserve(1uz);
                storage::imported_function_storage_t imported{};
                imported.target.defined_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(k_local_i32_add_index));
                imported.import_type_ptr = ::std::addressof(import_ty);
                imported.link_kind = storage::imported_function_link_kind::defined;
                module.imported_function_vec_storage.push_back_unchecked(imported);
            }

            if(c.has_table)
            {
                auto& table_ty{tables[0]};
                table_ty.limits.min = 2u;
                table_ty.limits.present_max = true;
                table_ty.limits.max = 2u;

                module.local_defined_table_vec_storage.reserve(1uz);
                storage::local_defined_table_storage_t table{};
                table.table_type_ptr = ::std::addressof(table_ty);
                table.owner_module_rt_ptr = ::std::addressof(module);
                table.elems.resize(2uz);
                auto& elem0{table.elems.index_unchecked(0uz)};
                elem0.type = storage::local_defined_table_elem_storage_type_t::func_ref_defined;
                elem0.storage.defined_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(k_local_i32_add_index));
                auto& elem1{table.elems.index_unchecked(1uz)};
                elem1.type = storage::local_defined_table_elem_storage_type_t::func_ref_defined;
                elem1.storage.defined_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(k_local_i32_mix_index));
                module.local_defined_table_vec_storage.push_back_unchecked(::std::move(table));
            }

#if UWVM_BACKEND_FUZZER_HAS_LLVM_JIT
            module.llvm_jit_call_indirect_table_views.resize(module.imported_table_vec_storage.size() + module.local_defined_table_vec_storage.size());
#endif

            if(c.has_memory)
            {
                auto& mem_ty{memories[0]};
                mem_ty.limits.min = 1u;
                mem_ty.limits.present_max = true;
                mem_ty.limits.max = 3u;
                module.local_defined_memory_vec_storage.emplace_back();
                auto& mem_rec{module.local_defined_memory_vec_storage.back()};
                mem_rec.memory_type_ptr = ::std::addressof(mem_ty);
                mem_rec.effective_limits.min = 1u;
                mem_rec.effective_limits.present_max = true;
                mem_rec.effective_limits.max = 3u;
                mem_rec.memory.init_by_page_count(1u);
            }

            if(c.has_global)
            {
                auto& global_ty{globals[0]};
                global_ty.type = value_type_t::i32;
                global_ty.is_mutable = true;
                local_globals[0].global = global_ty;

                module.local_defined_global_vec_storage.reserve(1uz);
                storage::local_defined_global_storage_t global{};
                global.global_type_ptr = ::std::addressof(local_globals[0].global);
                global.local_global_type_ptr = ::std::addressof(local_globals[0]);
                global.owner_module_rt_ptr = ::std::addressof(module);
                global.init_state = storage::wasm_global_init_state::initialized;
                global.global.kind = ::uwvm2::object::global::global_type::wasm_i32;
                global.global.is_mutable = true;
                global.global.storage.i32 = i32(c.global_initial);
                module.local_defined_global_vec_storage.push_back_unchecked(global);
            }
        }
    };

    void configure_quiet_runtime() noexcept
    {
        ::uwvm2::uwvm::io::show_verbose = false;
        ::uwvm2::uwvm::io::show_depend_warning = false;
        ::uwvm2::uwvm::io::enable_runtime_log = false;
        rtmode::runtime_compile_threads_existed = true;
        rtmode::global_runtime_compile_threads = 0;
        rtmode::global_runtime_compile_threads_resolved = 0;
        rtmode::runtime_scheduling_policy_existed = true;
        rtmode::global_runtime_scheduling_policy = rtmode::runtime_scheduling_policy_t::function_count;
        rtmode::global_runtime_scheduling_size = 1uz;
#if UWVM_BACKEND_FUZZER_HAS_TIERED
        rtmode::runtime_tiered_disable_uwvm_int_lazy_interpreter = false;
        rtmode::runtime_tiered_disable_llvm_full_jit = false;
#endif
#if UWVM_BACKEND_FUZZER_HAS_LLVM_JIT
        rtmode::global_runtime_llvm_jit_cache_path_mode = rtmode::runtime_llvm_jit_cache_path_mode_t::disabled;
#endif
    }

    void configure_mode(::std::string_view mode)
    {
        configure_quiet_runtime();
        if(mode == "uwvm-int-full")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::full_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::uwvm_interpreter_only;
            return;
        }
        if(mode == "uwvm-int-lazy")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::lazy_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::uwvm_interpreter_only;
            return;
        }
        if(mode == "llvm-jit-full")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::full_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::llvm_jit_only;
            return;
        }
        if(mode == "llvm-jit-lazy")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::lazy_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::llvm_jit_only;
            return;
        }
        if(mode == "tiered")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::lazy_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
            return;
        }
        if(mode == "tiered-no-t0")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::lazy_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
#if UWVM_BACKEND_FUZZER_HAS_TIERED
            rtmode::runtime_tiered_disable_uwvm_int_lazy_interpreter = true;
#endif
            return;
        }
        if(mode == "tiered-no-t2")
        {
            rtmode::global_runtime_mode = rtmode::runtime_mode_t::lazy_compile;
            rtmode::global_runtime_compiler = rtmode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
#if UWVM_BACKEND_FUZZER_HAS_TIERED
            rtmode::runtime_tiered_disable_llvm_full_jit = true;
#endif
            return;
        }
        ::std::cerr << "backend_libfuzzer: unknown runtime mode: " << mode << '\n';
        ::std::abort();
    }

    [[nodiscard]] ::std::uint32_t run_runtime_mode(fuzz_case const& c, ::std::string_view mode)
    {
        configure_mode(mode);
#if UWVM_BACKEND_FUZZER_HAS_LLVM_JIT
        ::uwvm2::runtime::lib::llvm_jit_reset_runtime_state_host_api();
#endif
        direct_module_owner owner{};
        storage::wasm_module_runtime_storage.clear();
        auto [it, inserted]{storage::wasm_module_runtime_storage.try_emplace(k_main_module_name)};
        static_cast<void>(inserted);
        owner.build(c, it->second);
        if(mode == "uwvm-int-full" || mode == "llvm-jit-full")
        {
            ::uwvm2::runtime::lib::full_compile_run_config cfg{};
            cfg.entry_function_index = c.entry_function_index;
            ::uwvm2::runtime::lib::full_compile_and_run_main_module(k_main_module_name, cfg);
        }
        else
        {
            ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
            cfg.entry_function_index = c.entry_function_index;
            cfg.assume_full_code_verified = true;
            ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(k_main_module_name, cfg);
        }

        if(it->second.local_defined_global_vec_storage.size() == 0uz) { return 0u; }
        auto const& global{it->second.local_defined_global_vec_storage.index_unchecked(0uz).global};
        return static_cast<::std::uint32_t>(global.storage.i32);
    }

    template <::std::size_t ScalarSlots>
    [[nodiscard]] consteval int_optable::uwvm_interpreter_translate_option_t make_tailcall_scalar4_merged_opt() noexcept
    {
        static_assert(ScalarSlots > 0uz);
        constexpr ::std::size_t begin{3uz};
        constexpr ::std::size_t end{begin + ScalarSlots};
        return int_optable::uwvm_interpreter_translate_option_t{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = begin,
            .i32_stack_top_end_pos = end,
            .i64_stack_top_begin_pos = begin,
            .i64_stack_top_end_pos = end,
            .f32_stack_top_begin_pos = begin,
            .f32_stack_top_end_pos = end,
            .f64_stack_top_begin_pos = begin,
            .f64_stack_top_end_pos = end,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX,
        };
    }

    template <::std::size_t I32Slots, ::std::size_t I64Slots, ::std::size_t F32Slots, ::std::size_t F64Slots>
    [[nodiscard]] consteval int_optable::uwvm_interpreter_translate_option_t make_tailcall_fully_split_opt() noexcept
    {
        static_assert(I32Slots > 0uz);
        static_assert(I64Slots > 0uz);
        static_assert(F32Slots > 0uz);
        static_assert(F64Slots > 0uz);

        constexpr ::std::size_t i32_begin{3uz};
        constexpr ::std::size_t i32_end{i32_begin + I32Slots};
        constexpr ::std::size_t i64_begin{i32_end};
        constexpr ::std::size_t i64_end{i64_begin + I64Slots};
        constexpr ::std::size_t f32_begin{i64_end};
        constexpr ::std::size_t f32_end{f32_begin + F32Slots};
        constexpr ::std::size_t f64_begin{f32_end};
        constexpr ::std::size_t f64_end{f64_begin + F64Slots};

        return int_optable::uwvm_interpreter_translate_option_t{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = i32_begin,
            .i32_stack_top_end_pos = i32_end,
            .i64_stack_top_begin_pos = i64_begin,
            .i64_stack_top_end_pos = i64_end,
            .f32_stack_top_begin_pos = f32_begin,
            .f32_stack_top_end_pos = f32_end,
            .f64_stack_top_begin_pos = f64_begin,
            .f64_stack_top_end_pos = f64_end,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX,
        };
    }

    template <int_optable::uwvm_interpreter_translate_option_t CompileOption>
    struct interpreter_runner
    {
        static constexpr ::std::size_t tuple_size{int_compiler::details::interpreter_tuple_size<CompileOption>()};

        template <::std::size_t I>
        using arg_t = int_compiler::details::interpreter_tuple_arg_t<I, CompileOption>;

        template <::std::size_t I>
        [[nodiscard]] static constexpr arg_t<I> init_arg(::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
        {
            if constexpr(I == 0uz) { return ip; }
            else if constexpr(I == 1uz) { return sp; }
            else if constexpr(I == 2uz) { return local_base; }
            else { return arg_t<I>{}; }
        }

        template <::std::size_t... Is>
        [[nodiscard]] static auto make_args(::std::index_sequence<Is...>, ::std::byte const* ip, ::std::byte* sp, ::std::byte* local_base) noexcept
        { return ::std::tuple<arg_t<Is>...>(init_arg<Is>(ip, sp, local_base)...); }

        using opfunc_ptr_t = int_compiler::details::interpreter_expected_opfunc_ptr_t<CompileOption>;

        static void run(int_optable::local_func_storage_t const& fn, storage::local_defined_function_storage_t const& rt_fn)
        {
            auto const* const ft{rt_fn.function_type_ptr};
            if(ft == nullptr) { ::fast_io::fast_terminate(); }
            auto const param_bytes{abi_total_bytes(ft->parameter.begin, ft->parameter.end)};
            auto const result_bytes{abi_total_bytes(ft->result.begin, ft->result.end)};
            if(param_bytes != 0uz || result_bytes != 0uz) { ::fast_io::fast_terminate(); }

            constexpr ::std::size_t k_align{16uz};
            auto align_up = [](byte_vec& buf) noexcept -> ::std::byte*
            {
                auto const p{reinterpret_cast<::std::uintptr_t>(buf.data())};
                auto const a{(p + (k_align - 1uz)) & ~(k_align - 1uz)};
                return reinterpret_cast<::std::byte*>(a);
            };

            byte_vec local_buf(fn.local_bytes_max + k_align);
            ::std::memset(local_buf.data(), 0, local_buf.size());
            ::std::byte* local_base{align_up(local_buf)};

            byte_vec stack_buf((fn.operand_stack_byte_max == 0uz ? 64uz : fn.operand_stack_byte_max) + k_align);
            ::std::memset(stack_buf.data(), 0, stack_buf.size());
            ::std::byte* operand_base{align_up(stack_buf)};

            auto args{make_args(::std::make_index_sequence<tuple_size>{}, fn.op.operands.data(), operand_base, local_base)};
            if constexpr(CompileOption.is_tail_call)
            {
                opfunc_ptr_t fptr{};
                ::std::memcpy(::std::addressof(fptr), ::std::get<0>(args), sizeof(fptr));
                ::std::apply([&](auto... a) { fptr(a...); }, args);
            }
            else
            {
                while(::std::get<0>(args) != nullptr)
                {
                    opfunc_ptr_t fptr{};
                    ::std::memcpy(::std::addressof(fptr), ::std::get<0>(args), sizeof(fptr));
                    ::std::apply([&](auto&... a) { fptr(a...); }, args);
                }
            }
        }
    };

    void manual_memory_oob_trap(::uwvm2::object::memory::error::memory_error_t const&) noexcept
    { ::fast_io::fast_terminate(); }

    void install_manual_traps() noexcept
    {
        int_optable::unreachable_func = ::fast_io::fast_terminate;
        int_optable::trap_invalid_conversion_to_integer_func = ::fast_io::fast_terminate;
        int_optable::trap_integer_divide_by_zero_func = ::fast_io::fast_terminate;
        int_optable::trap_integer_overflow_func = ::fast_io::fast_terminate;
        int_optable::trap_memory_out_of_bounds_func = manual_memory_oob_trap;
    }

    template <int_optable::uwvm_interpreter_translate_option_t CompileOption>
    void run_ring_once(fuzz_case const& c)
    {
        static_assert(!CompileOption.is_tail_call || int_compiler::details::interpreter_tuple_has_no_holes<CompileOption>());
        direct_module_owner owner{};
        module_t module{};
        owner.build(c, module);
        ::uwvm2::validation::error::code_validation_error_impl err{};
        int_optable::compile_option opt{};
        opt.curr_wasm_id = 0uz;
        auto compiled{int_compiler::compile_all_from_uwvm_single_func<CompileOption>(module, opt, err)};
        if(compiled.local_funcs.size() == 0uz) { ::fast_io::fast_terminate(); }
        interpreter_runner<CompileOption>::run(compiled.local_funcs.index_unchecked(0uz),
                                               module.local_defined_function_vec_storage.index_unchecked(0uz));
    }

    void run_ring_matrix(fuzz_case const& c)
    {
        configure_quiet_runtime();
        install_manual_traps();
        constexpr int_optable::uwvm_interpreter_translate_option_t byref_opt{.is_tail_call = false};
        constexpr int_optable::uwvm_interpreter_translate_option_t tail_min_opt{.is_tail_call = true};
        constexpr auto tail_scalar1_opt{make_tailcall_scalar4_merged_opt<1uz>()};
        constexpr auto tail_split_small_opt{make_tailcall_fully_split_opt<2uz, 2uz, 2uz, 2uz>()};
        constexpr auto tail_sysv_opt{make_tailcall_fully_split_opt<3uz, 3uz, 8uz, 8uz>()};
        constexpr auto tail_aapcs64_opt{make_tailcall_fully_split_opt<5uz, 5uz, 8uz, 8uz>()};
        run_ring_once<byref_opt>(c);
        run_ring_once<tail_min_opt>(c);
        run_ring_once<tail_scalar1_opt>(c);
        run_ring_once<tail_split_small_opt>(c);
        run_ring_once<tail_sysv_opt>(c);
        run_ring_once<tail_aapcs64_opt>(c);
    }

    ::std::vector<::std::string> g_modes{
        "uwvm-int-ring-matrix",
        "uwvm-int-lazy",
        "uwvm-int-full",
        "llvm-jit-lazy",
        "llvm-jit-full",
        "tiered",
        "tiered-no-t0",
        "tiered-no-t2",
    };
    bool g_trace{};
    bool g_stress_tiered_osr{};
    bool g_stress_tiered_switch{};

    void set_modes_from_env()
    {
        char const* env{::std::getenv("UWVM_BACKEND_LIBFUZZER_MODES")};
        if(env == nullptr || *env == '\0') { return; }
        ::std::vector<::std::string> modes{};
        ::std::string cur{};
        for(char const* p{env}; *p != '\0'; ++p)
        {
            if(*p == ',' || *p == ' ' || *p == '\t' || *p == '\n')
            {
                if(!cur.empty())
                {
                    modes.push_back(cur);
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(*p);
            }
        }
        if(!cur.empty()) { modes.push_back(cur); }
        if(!modes.empty()) { g_modes = ::std::move(modes); }
    }

    [[nodiscard]] bool env_enabled(char const* name) noexcept
    {
        char const* env{::std::getenv(name)};
        return env != nullptr && *env != '\0' && ::std::strcmp(env, "0") != 0 && ::std::strcmp(env, "false") != 0 && ::std::strcmp(env, "no") != 0;
    }
}  // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    set_modes_from_env();
    g_trace = env_enabled("UWVM_BACKEND_LIBFUZZER_TRACE");
    g_stress_tiered_osr = env_enabled("UWVM_BACKEND_LIBFUZZER_STRESS_TIERED_OSR");
    g_stress_tiered_switch = env_enabled("UWVM_BACKEND_LIBFUZZER_STRESS_TIERED_SWITCH");
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    if(size == 0uz) { return 0; }
    if(g_trace) { ::std::cerr << "backend-libfuzzer: input-size=" << size << '\n'; }
    auto c{project_input(data, size, true, g_stress_tiered_osr, g_stress_tiered_switch)};
    fuzz_case ring_case{};
    bool ring_case_ready{};
    bool have_runtime_observation{};
    ::std::uint32_t runtime_observation{};
    for(auto const& mode: g_modes)
    {
        if(g_trace) { ::std::cerr << "backend-libfuzzer: mode=" << mode << '\n'; }
        if(mode == "uwvm-int-ring-matrix")
        {
            if(!ring_case_ready)
            {
                ring_case = project_input(data, size, false);
                ring_case_ready = true;
            }
            run_ring_matrix(ring_case);
        }
        else
        {
            auto const observation{run_runtime_mode(c, mode)};
            if(!have_runtime_observation)
            {
                runtime_observation = observation;
                have_runtime_observation = true;
            }
            else if(observation != runtime_observation)
            {
                ::std::cerr << "backend-libfuzzer: runtime observation mismatch: baseline=0x" << ::std::hex << runtime_observation
                            << " mode=" << mode << " observation=0x" << observation << ::std::dec << '\n';
                ::std::abort();
            }
        }
    }
    return 0;
}
