#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#ifndef UWVM_MODULE
# include <fast_io.h>
#
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/mvp.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/translate.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/storage.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/runtime/initializer/init.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/impl.h>
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

    enum class strict_matrix_level : ::std::uint8_t
    {
        default_,
        expanded,
        coverage,
    };

    [[nodiscard]] inline bool env_token_list_contains(char const* env_name, char const* token) noexcept
    {
        auto const* env = ::std::getenv(env_name);
        if(env == nullptr || *env == '\0') { return false; }

        auto const token_len = ::std::strlen(token);
        auto const match = [&](char const* first, char const* last) noexcept
        {
            while(first != last && (*first == ' ' || *first == '\t')) { ++first; }
            while(first != last && (last[-1] == ' ' || last[-1] == '\t')) { --last; }

            auto const len = static_cast<::std::size_t>(last - first);
            return (len == 3uz && ::std::memcmp(first, "all", 3uz) == 0) || (len == token_len && ::std::memcmp(first, token, token_len) == 0);
        };

        char const* first = env;
        for(char const* p = env;; ++p)
        {
            if(*p != '\0' && *p != ',' && *p != ';' && *p != '|') { continue; }
            if(match(first, p)) { return true; }
            if(*p == '\0') { break; }
            first = p + 1;
        }
        return false;
    }

    [[nodiscard]] inline strict_matrix_level current_strict_matrix_level() noexcept
    {
        if(auto const* env = ::std::getenv("UWVM2TEST_MATRIX_LEVEL"); env != nullptr && *env != '\0')
        {
            if(::std::strcmp(env, "coverage") == 0) { return strict_matrix_level::coverage; }
            if(::std::strcmp(env, "expanded") == 0 || ::std::strcmp(env, "wide") == 0 || ::std::strcmp(env, "full") == 0)
            {
                return strict_matrix_level::expanded;
            }
            return strict_matrix_level::default_;
        }

        if(::std::getenv("LLVM_PROFILE_FILE") != nullptr) { return strict_matrix_level::coverage; }
        return strict_matrix_level::default_;
    }

    [[nodiscard]] inline bool strict_matrix_level_at_least(strict_matrix_level level) noexcept
    {
        return static_cast<::std::uint8_t>(current_strict_matrix_level()) >= static_cast<::std::uint8_t>(level);
    }

    template <typename T>
    [[nodiscard]] inline T pick_for_strict_matrix_level(T base, T expanded, T coverage) noexcept
    {
        switch(current_strict_matrix_level())
        {
            case strict_matrix_level::coverage: return coverage;
            case strict_matrix_level::expanded: return expanded;
            default: return base;
        }
    }

    template <::std::size_t IntSlots, ::std::size_t FloatSlots, bool ShareV128 = false>
    [[nodiscard]] consteval optable::uwvm_interpreter_translate_option_t make_tailcall_hardfloat_abi_opt() noexcept
    {
        static_assert(IntSlots > 0uz);
        static_assert(FloatSlots > 0uz);

        constexpr ::std::size_t int_begin{3uz};
        constexpr ::std::size_t int_end{int_begin + IntSlots};
        constexpr ::std::size_t float_begin{int_end};
        constexpr ::std::size_t float_end{float_begin + FloatSlots};

        return optable::uwvm_interpreter_translate_option_t{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = int_begin,
            .i32_stack_top_end_pos = int_end,
            .i64_stack_top_begin_pos = int_begin,
            .i64_stack_top_end_pos = int_end,
            .f32_stack_top_begin_pos = float_begin,
            .f32_stack_top_end_pos = float_end,
            .f64_stack_top_begin_pos = float_begin,
            .f64_stack_top_end_pos = float_end,
            .v128_stack_top_begin_pos = ShareV128 ? float_begin : SIZE_MAX,
            .v128_stack_top_end_pos = ShareV128 ? float_end : SIZE_MAX,
        };
    }

    template <::std::size_t ScalarSlots>
    [[nodiscard]] consteval optable::uwvm_interpreter_translate_option_t make_tailcall_scalar4_merged_opt() noexcept
    {
        static_assert(ScalarSlots > 0uz);
        constexpr ::std::size_t begin{3uz};
        constexpr ::std::size_t end{begin + ScalarSlots};

        return optable::uwvm_interpreter_translate_option_t{
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
    [[nodiscard]] consteval optable::uwvm_interpreter_translate_option_t make_tailcall_fully_split_opt() noexcept
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

        return optable::uwvm_interpreter_translate_option_t{
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

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_byref_opt{.is_tail_call = false};
    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_min_opt{.is_tail_call = true};
    inline constexpr auto k_test_tail_sysv_opt{make_tailcall_fully_split_opt<3uz, 3uz, 8uz, 8uz>()};
    inline constexpr auto k_test_tail_aapcs64_opt{make_tailcall_fully_split_opt<5uz, 5uz, 8uz, 8uz>()};
    inline constexpr auto k_test_tail_sysv_v128_opt{make_tailcall_hardfloat_abi_opt<3uz, 8uz, true>()};
    inline constexpr auto k_test_tail_aapcs64_v128_opt{make_tailcall_hardfloat_abi_opt<5uz, 8uz, true>()};

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_initial_stacktop_currpos() noexcept
    {
        constexpr auto begin_or_disabled = [](::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        {
            return begin_pos == SIZE_MAX || begin_pos == end_pos ? SIZE_MAX : begin_pos;
        };

        return optable::uwvm_interpreter_stacktop_currpos_t{
            .i32_stack_top_curr_pos = begin_or_disabled(Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos),
            .i64_stack_top_curr_pos = begin_or_disabled(Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos),
            .f32_stack_top_curr_pos = begin_or_disabled(Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos),
            .f64_stack_top_curr_pos = begin_or_disabled(Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos),
            .v128_stack_top_curr_pos = begin_or_disabled(Opt.v128_stack_top_begin_pos, Opt.v128_stack_top_end_pos),
        };
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_entry_stacktop_currpos() noexcept
    {
        constexpr bool i32_enabled{Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos};
        constexpr bool i64_enabled{Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos};
        constexpr bool f32_enabled{Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos};
        constexpr bool f64_enabled{Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos};
        constexpr bool v128_enabled{Opt.v128_stack_top_begin_pos != SIZE_MAX && Opt.v128_stack_top_begin_pos != Opt.v128_stack_top_end_pos};

        if constexpr(i32_enabled || i64_enabled || f32_enabled || f64_enabled || v128_enabled)
        {
            return make_initial_stacktop_currpos<Opt>();
        }
        else
        {
            return {};
        }
    }

    [[nodiscard]] inline bool abi_mode_enabled(char const* token) noexcept
    {
        auto const* env = ::std::getenv("UWVM2TEST_ABI_MODES");
        if(env == nullptr || *env == '\0')
        {
            return ::std::strcmp(token, "byref") == 0 || ::std::strcmp(token, "tail-min") == 0 || ::std::strcmp(token, "tail-sysv") == 0 ||
                   ::std::strcmp(token, "tail-aapcs64") == 0;
        }
        return env_token_list_contains("UWVM2TEST_ABI_MODES", token);
    }

    [[nodiscard]] inline bool legacy_layouts_enabled() noexcept
    {
        return strict_matrix_level_at_least(strict_matrix_level::expanded) || abi_mode_enabled("legacy-layouts");
    }

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
        ::std::uint8_t kind{};      // 0=func, 1=table, 2=mem, 3=global
        ::std::uint32_t index{};    // funcidx/tableidx/memidx/globalidx
    };

    struct global_entry
    {
        ::std::uint8_t valtype{};
        bool mut{};
        byte_vec init_expr{};  // ends with 0x0b
    };

    struct import_func_entry
    {
        byte_vec module_utf8{};
        byte_vec name_utf8{};
        ::std::uint32_t type_index{};
    };

    struct import_table_entry
    {
        byte_vec module_utf8{};
        byte_vec name_utf8{};
        ::std::uint8_t elem_type{k_ref_funcref};
        ::std::uint32_t min{};
        ::std::uint32_t max{};
        bool has_max{};
    };

    struct import_memory_entry
    {
        byte_vec module_utf8{};
        byte_vec name_utf8{};
        ::std::uint32_t min{};
        ::std::uint32_t max{};
        bool has_max{};
    };

    struct import_global_entry
    {
        byte_vec module_utf8{};
        byte_vec name_utf8{};
        ::std::uint8_t valtype{};
        bool mut{};
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
        ::std::vector<import_func_entry> import_funcs{};
        ::std::vector<import_table_entry> import_tables{};
        ::std::vector<import_memory_entry> import_memories{};
        ::std::vector<import_global_entry> import_globals{};

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

        static void encode_name_utf8(byte_vec& out, char const* ascii)
        {
            auto const len = ::std::strlen(ascii);
            append_u32_leb(out, static_cast<::std::uint32_t>(len));
            for(::std::size_t i = 0; i < len; ++i) { append_u8(out, static_cast<::std::uint8_t>(ascii[i])); }
        }

        void add_import_func(char const* module_ascii, char const* name_ascii, ::std::uint32_t type_index)
        {
            import_func_entry im{};
            encode_name_utf8(im.module_utf8, module_ascii);
            encode_name_utf8(im.name_utf8, name_ascii);
            im.type_index = type_index;
            import_funcs.push_back(::std::move(im));
        }

        void add_import_table(char const* module_ascii,
                              char const* name_ascii,
                              ::std::uint32_t min,
                              ::std::uint32_t max,
                              bool has_max,
                              ::std::uint8_t elem_type = k_ref_funcref)
        {
            import_table_entry im{};
            encode_name_utf8(im.module_utf8, module_ascii);
            encode_name_utf8(im.name_utf8, name_ascii);
            im.elem_type = elem_type;
            im.min = min;
            im.max = max;
            im.has_max = has_max;
            import_tables.push_back(::std::move(im));
        }

        void add_import_memory(char const* module_ascii, char const* name_ascii, ::std::uint32_t min, ::std::uint32_t max, bool has_max)
        {
            import_memory_entry im{};
            encode_name_utf8(im.module_utf8, module_ascii);
            encode_name_utf8(im.name_utf8, name_ascii);
            im.min = min;
            im.max = max;
            im.has_max = has_max;
            import_memories.push_back(::std::move(im));
        }

        void add_import_global(char const* module_ascii, char const* name_ascii, ::std::uint8_t valtype, bool mut)
        {
            import_global_entry im{};
            encode_name_utf8(im.module_utf8, module_ascii);
            encode_name_utf8(im.name_utf8, name_ascii);
            im.valtype = valtype;
            im.mut = mut;
            import_globals.push_back(::std::move(im));
        }

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
            encode_name_utf8(ex.name_utf8, name_ascii);
            ex.kind = 0u;
            ex.index = func_index;
            exports.push_back(::std::move(ex));
        }

        void add_export_table(::std::uint32_t table_index, char const* name_ascii)
        {
            export_entry ex{};
            encode_name_utf8(ex.name_utf8, name_ascii);
            ex.kind = 1u;
            ex.index = table_index;
            exports.push_back(::std::move(ex));
        }

        void add_export_memory(char const* name_ascii)
        {
            export_entry ex{};
            encode_name_utf8(ex.name_utf8, name_ascii);
            ex.kind = 2u;
            ex.index = 0u;
            exports.push_back(::std::move(ex));
        }

        void add_export_global(::std::uint32_t global_index, char const* name_ascii)
        {
            export_entry ex{};
            encode_name_utf8(ex.name_utf8, name_ascii);
            ex.kind = 3u;
            ex.index = global_index;
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

            // import section (2)
            if(!import_funcs.empty() || !import_tables.empty() || !import_memories.empty() || !import_globals.empty())
            {
                byte_vec sec{};
                auto const total = static_cast<::std::uint32_t>(import_funcs.size() + import_tables.size() + import_memories.size() + import_globals.size());
                append_u32_leb(sec, total);

                for(auto const& im : import_funcs)
                {
                    append_bytes(sec, im.module_utf8);
                    append_bytes(sec, im.name_utf8);
                    append_u8(sec, 0x00u);  // kind: func
                    append_u32_leb(sec, im.type_index);
                }

                for(auto const& im : import_tables)
                {
                    append_bytes(sec, im.module_utf8);
                    append_bytes(sec, im.name_utf8);
                    append_u8(sec, 0x01u);  // kind: table
                    append_u8(sec, im.elem_type);
                    ::std::uint8_t flags = im.has_max ? 0x01u : 0x00u;
                    append_u8(sec, flags);
                    append_u32_leb(sec, im.min);
                    if(im.has_max) { append_u32_leb(sec, im.max); }
                }

                for(auto const& im : import_memories)
                {
                    append_bytes(sec, im.module_utf8);
                    append_bytes(sec, im.name_utf8);
                    append_u8(sec, 0x02u);  // kind: mem
                    ::std::uint8_t flags = im.has_max ? 0x01u : 0x00u;
                    append_u8(sec, flags);
                    append_u32_leb(sec, im.min);
                    if(im.has_max) { append_u32_leb(sec, im.max); }
                }

                for(auto const& im : import_globals)
                {
                    append_bytes(sec, im.module_utf8);
                    append_bytes(sec, im.name_utf8);
                    append_u8(sec, 0x03u);  // kind: global
                    append_u8(sec, im.valtype);
                    append_u8(sec, im.mut ? 0x01u : 0x00u);
                }

                emit_section(2u, sec);
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

    struct preloaded_wasm_module
    {
        byte_vec const* wasm_bytes{};
        ::uwvm2::utils::container::u8string_view module_name{};
    };

    [[nodiscard]] inline prepared_runtime prepare_runtime_from_wasm(
        byte_vec const& wasm_bytes,
        ::uwvm2::utils::container::u8string_view module_name,
        ::std::initializer_list<preloaded_wasm_module> preloaded = {})
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
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1)
        // Allow tests to load WASI-Preview1 importing modules (e.g. reference workloads) without depending on the CLI loader.
        ::uwvm2::uwvm::wasm::storage::preload_local_imported.emplace_back(
            ::uwvm2::uwvm::imported::wasi::wasip1::local_imported::wasip1_local_imported_module);
#endif

        if(preloaded.size() != 0uz)
        {
            ::uwvm2::uwvm::wasm::storage::preloaded_wasm.reserve(preloaded.size());
            for(auto const& pl : preloaded)
            {
                if(pl.wasm_bytes == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                ::uwvm2::parser::wasm::base::error_impl pre_err{};
                auto const* pre_begin = pl.wasm_bytes->data();
                auto const* pre_end = pl.wasm_bytes->data() + pl.wasm_bytes->size();

                ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t pre_storage{};
                pre_storage = ::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(
                    pre_begin,
                    pre_end,
                    pre_err,
                    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t{});

                ::uwvm2::uwvm::wasm::type::wasm_file_t wf{1u};
                wf.file_name = u8"uwvm2test_preloaded.wasm";
                wf.module_name = pl.module_name;
                wf.binfmt_ver = 1u;
                wf.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(pre_storage);
                ::uwvm2::uwvm::wasm::storage::preloaded_wasm.emplace_back(::std::move(wf));
            }
        }

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

        // Fully initialize runtime storage (preloaded modules + execute module) so imported refs (func/table/mem/global)
        // are properly linked. This matches the normal uwvm initialization pipeline.
        ::uwvm2::uwvm::runtime::initializer::initialize_runtime();

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
