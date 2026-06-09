#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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

#include "backend_fuzzer_cases.inc"

namespace fuzzer = ::uwvm2_backend_fuzzer_generated;

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
    using memory_type_t = storage::wasm_binfmt1_final_memory_type_t;
    using local_entry_t = typename decltype(::std::declval<code_t&>().locals)::value_type;
    using byte_vec = ::std::vector<::std::byte>;

    constexpr char8_t k_main_module_name_storage[] = u8"backend-fuzzer-main";
    constexpr ::uwvm2::utils::container::u8string_view k_main_module_name{
        k_main_module_name_storage,
        sizeof(k_main_module_name_storage) / sizeof(char8_t) - 1uz};

    [[nodiscard]] value_type_t to_value_type(::std::uint8_t v) noexcept
    { return static_cast<value_type_t>(v); }

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

    [[noreturn]] void die(char const* msg)
    {
        ::std::cerr << "backend_fuzzer_runner: " << msg << '\n';
        ::std::exit(2);
    }

    [[nodiscard]] ::std::size_t parse_size_arg(char const* s, char const* what)
    {
        char* end{};
        unsigned long long const v{::std::strtoull(s, ::std::addressof(end), 0)};
        if(s == end || end == nullptr || *end != '\0') { die(what); }
        if(v > static_cast<unsigned long long>((::std::numeric_limits<::std::size_t>::max)())) { die(what); }
        return static_cast<::std::size_t>(v);
    }

    struct direct_module_owner
    {
        ::std::vector<value_type_t> value_types{};
        ::std::vector<function_type_t> types{};
        ::std::vector<code_t> codes{};
        ::std::vector<::std::vector<wasm_byte_t>> code_bytes{};
        ::std::vector<memory_type_t> memories{};

        void build(::std::size_t case_index, module_t& module)
        {
            if(case_index >= fuzzer::k_cases.size()) { die("case index out of range"); }

            auto const& c{fuzzer::k_cases[case_index]};

            value_types.clear();
            value_types.reserve(fuzzer::k_value_types.size() + 1uz);
            value_types.push_back(value_type_t::i32);
            for(auto const t: fuzzer::k_value_types) { value_types.push_back(to_value_type(t)); }
            auto const* value_base{value_types.data() + 1};

            types.clear();
            types.resize(c.type_count);
            for(::std::size_t i{}; i != c.type_count; ++i)
            {
                auto const& td{fuzzer::k_types[c.type_begin + i]};
                auto& ty{types[i]};
                ty.parameter.begin = value_base + td.params_begin;
                ty.parameter.end = ty.parameter.begin + td.params_count;
                ty.result.begin = value_base + td.results_begin;
                ty.result.end = ty.result.begin + td.results_count;
            }

            codes.clear();
            code_bytes.clear();
            codes.resize(c.func_count);
            code_bytes.resize(c.func_count);

            for(::std::size_t i{}; i != c.func_count; ++i)
            {
                auto const& fd{fuzzer::k_funcs[c.func_begin + i]};
                auto& owned_bytes{code_bytes[i]};
                owned_bytes.reserve(fd.code_size);
                for(::std::size_t j{}; j != fd.code_size; ++j)
                {
                    owned_bytes.push_back(static_cast<wasm_byte_t>(fuzzer::k_code_bytes[fd.code_begin + j]));
                }

                auto& code{codes[i]};
                auto const* begin{owned_bytes.data()};
                code.body.code_begin = begin;
                code.body.expr_begin = begin + fd.expr_offset;
                code.body.code_end = begin + fd.code_size;

                code.locals.reserve(fd.locals_count);
                ::std::uint_least32_t local_total{};
                for(::std::size_t j{}; j != fd.locals_count; ++j)
                {
                    auto const base{static_cast<::std::size_t>((fd.locals_begin + j) * 2u)};
                    local_entry_t entry{};
                    entry.count = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(fuzzer::k_local_decls[base]);
                    entry.type = to_value_type(static_cast<::std::uint8_t>(fuzzer::k_local_decls[base + 1uz]));
                    code.locals.push_back_unchecked(entry);
                    local_total += entry.count;
                }
                code.all_local_count = local_total;
            }

            module.type_section_storage.type_section_begin = types.data();
            module.type_section_storage.type_section_end = types.data() + types.size();

            module.local_defined_function_vec_storage.reserve(c.func_count);
            for(::std::size_t i{}; i != c.func_count; ++i)
            {
                auto const& fd{fuzzer::k_funcs[c.func_begin + i]};
                if(fd.type_index < c.type_begin || fd.type_index >= c.type_begin + c.type_count) { die("function type index outside case type slice"); }

                storage::local_defined_function_storage_t fn{};
                fn.function_type_ptr = ::std::addressof(types[fd.type_index - c.type_begin]);
                fn.wasm_code_ptr = ::std::addressof(codes[i]);
                module.local_defined_function_vec_storage.push_back_unchecked(fn);
            }

            module.local_defined_code_vec_storage.reserve(c.func_count);
            for(::std::size_t i{}; i != c.func_count; ++i)
            {
                storage::local_defined_code_storage_t rec{};
                rec.code_type_ptr = ::std::addressof(codes[i]);
                rec.func_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(i));
                module.local_defined_code_vec_storage.push_back_unchecked(rec);
            }

            memories.clear();
            if(c.has_memory)
            {
                memories.resize(1uz);
                auto& ty{memories[0]};
                ty.limits.min = c.memory_min;
                ty.limits.present_max = c.memory_has_max;
                ty.limits.max = c.memory_has_max ? c.memory_max : decltype(ty.limits)::default_max;

                module.local_defined_memory_vec_storage.emplace_back();
                auto& rec{module.local_defined_memory_vec_storage.back()};
                rec.memory_type_ptr = ::std::addressof(memories[0]);
                rec.effective_limits.min = c.memory_min;
                rec.effective_limits.present_max = c.memory_has_max;
                rec.effective_limits.max = c.memory_has_max ? c.memory_max : decltype(rec.effective_limits)::default_max;
                rec.memory.init_by_page_count(rec.effective_limits.min);
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

        die("unknown runtime mode");
    }

    int run_runtime_mode(::std::size_t case_index, ::std::string_view mode)
    {
        configure_mode(mode);

#if UWVM_BACKEND_FUZZER_HAS_LLVM_JIT
        ::uwvm2::runtime::lib::llvm_jit_reset_runtime_state_host_api();
#endif

        direct_module_owner owner{};
        storage::wasm_module_runtime_storage.clear();
        auto [it, inserted]{storage::wasm_module_runtime_storage.try_emplace(k_main_module_name)};
        static_cast<void>(inserted);
        owner.build(case_index, it->second);

        auto const& c{fuzzer::k_cases[case_index]};
        if(mode == "uwvm-int-full" || mode == "llvm-jit-full")
        {
            ::uwvm2::runtime::lib::full_compile_run_config cfg{};
            cfg.entry_function_index = c.entry_func_index;
            ::uwvm2::runtime::lib::full_compile_and_run_main_module(k_main_module_name, cfg);
        }
        else
        {
            ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
            cfg.entry_function_index = c.entry_func_index;
            cfg.assume_full_code_verified = true;
            ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(k_main_module_name, cfg);
        }
        return 0;
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
    void run_ring_once(::std::size_t case_index)
    {
        static_assert(!CompileOption.is_tail_call || int_compiler::details::interpreter_tuple_has_no_holes<CompileOption>());

        direct_module_owner owner{};
        module_t module{};
        owner.build(case_index, module);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        int_optable::compile_option opt{};
        opt.curr_wasm_id = 0uz;
        auto compiled{int_compiler::compile_all_from_uwvm_single_func<CompileOption>(module, opt, err)};

        auto const& c{fuzzer::k_cases[case_index]};
        auto const import_count{module.imported_function_vec_storage.size()};
        if(c.entry_func_index < import_count) { ::fast_io::fast_terminate(); }
        auto const local_index{static_cast<::std::size_t>(c.entry_func_index - import_count)};
        if(local_index >= compiled.local_funcs.size()) { ::fast_io::fast_terminate(); }

        interpreter_runner<CompileOption>::run(compiled.local_funcs.index_unchecked(local_index),
                                               module.local_defined_function_vec_storage.index_unchecked(local_index));
    }

    int run_ring_matrix(::std::size_t case_index)
    {
        configure_quiet_runtime();
        install_manual_traps();

        constexpr int_optable::uwvm_interpreter_translate_option_t byref_opt{.is_tail_call = false};
        constexpr int_optable::uwvm_interpreter_translate_option_t tail_min_opt{.is_tail_call = true};
        constexpr auto tail_scalar1_opt{make_tailcall_scalar4_merged_opt<1uz>()};
        constexpr auto tail_split_small_opt{make_tailcall_fully_split_opt<2uz, 2uz, 2uz, 2uz>()};
        constexpr auto tail_sysv_opt{make_tailcall_fully_split_opt<3uz, 3uz, 8uz, 8uz>()};
        constexpr auto tail_aapcs64_opt{make_tailcall_fully_split_opt<5uz, 5uz, 8uz, 8uz>()};

        run_ring_once<byref_opt>(case_index);
        run_ring_once<tail_min_opt>(case_index);
        run_ring_once<tail_scalar1_opt>(case_index);
        run_ring_once<tail_split_small_opt>(case_index);
        run_ring_once<tail_sysv_opt>(case_index);
        run_ring_once<tail_aapcs64_opt>(case_index);
        return 0;
    }

    void list_modes()
    {
        ::std::cout << "uwvm-int-ring-matrix\n"
                    << "uwvm-int-lazy\n"
                    << "uwvm-int-full\n"
                    << "llvm-jit-lazy\n"
                    << "llvm-jit-full\n"
                    << "tiered\n"
                    << "tiered-no-t0\n"
                    << "tiered-no-t2\n";
    }

    void list_cases()
    {
        for(::std::size_t i{}; i != fuzzer::k_cases.size(); ++i)
        {
            auto const& c{fuzzer::k_cases[i]};
            ::std::cout << i << '\t' << c.name << '\t' << (c.expect_trap ? "trap" : "ok") << '\n';
        }
    }
}  // namespace

int main(int argc, char** argv)
{
    ::std::size_t case_index{};
    ::std::string_view mode{"uwvm-int-ring-matrix"};
    bool have_case{};

    for(int i{1}; i < argc;)
    {
        ::std::string_view arg{argv[i]};
        if(arg == "--case")
        {
            if(i + 1 >= argc) { die("--case requires an argument"); }
            case_index = parse_size_arg(argv[i + 1], "invalid --case value");
            have_case = true;
            i += 2;
            continue;
        }
        if(arg == "--mode")
        {
            if(i + 1 >= argc) { die("--mode requires an argument"); }
            mode = argv[i + 1];
            i += 2;
            continue;
        }
        if(arg == "--list-modes")
        {
            list_modes();
            return 0;
        }
        if(arg == "--list-cases")
        {
            list_cases();
            return 0;
        }
        if(arg == "-h" || arg == "--help")
        {
            ::std::cout << "usage: backend_fuzzer_runner --case <index> --mode <mode>\n";
            list_modes();
            return 0;
        }
        die("unknown argument");
    }

    if(!have_case) { die("--case is required"); }
    if(case_index >= fuzzer::k_cases.size()) { die("case index out of range"); }

    if(mode == "uwvm-int-ring-matrix") { return run_ring_matrix(case_index); }
    return run_runtime_mode(case_index, mode);
}
