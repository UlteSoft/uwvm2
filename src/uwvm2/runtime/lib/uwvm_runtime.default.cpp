/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// std
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

// platform
#if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
# include <malloc.h>
#elif !UWVM_HAS_BUILTIN(__builtin_alloca)
# include <alloca.h>
#endif
# if defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)
#  include <llvm/Analysis/TargetTransformInfo.h>
#  include <llvm/ADT/StringMap.h>
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/MCJIT.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/SourceMgr.h>
#  include <llvm/Support/TargetSelect.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/Transforms/InstCombine/InstCombine.h>
#  include <llvm/Transforms/Scalar.h>
#  include <llvm/Transforms/Scalar/GVN.h>
#  include <llvm/Transforms/Utils.h>
#  include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# endif

#ifndef UWVM_MODULE
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif

namespace uwvm2::runtime::lib
{
    namespace
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using runtime_imported_func_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
        using runtime_local_func_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t;
        using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
        using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;
        using runtime_llvm_jit_raw_call_target_t = ::uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t;
        using runtime_llvm_jit_call_indirect_table_view_t = ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_view_t;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        using capi_function_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        using local_imported_t = ::uwvm2::uwvm::wasm::type::local_imported_t;
        using preload_module_memory_attribute_t = ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t;
        using preload_memory_descriptor_t = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t;

        using compiled_module_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
        using compiled_local_func_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;
#if defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)
        using llvm_jit_compiled_module_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::full_function_symbol_t;
#endif

        constexpr ::std::size_t local_slot_size{sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_i64_f32_f64_u)};
        static_assert(local_slot_size == 8uz);

        struct compiled_defined_func_info
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
            runtime_local_func_storage_t const* runtime_func{};
            compiled_local_func_t const* compiled_func{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
        };

        struct compiled_module_record
        {
            ::uwvm2::utils::container::u8string_view module_name{};
            runtime_module_storage_t const* runtime_module{};
            compiled_module_t compiled{};
#if defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)
            llvm_jit_compiled_module_t llvm_jit_compiled{};
            ::std::unique_ptr<::llvm::LLVMContext> llvm_jit_context_holder{};
            ::std::unique_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_local_entry_addresses{};
            bool llvm_jit_ready{};
#endif

            // Canonical type-index table for fast call_indirect signature checks.
            // Maps each type index to its canonical representative (deduplicated by {params, results}).
            ::uwvm2::utils::container::vector<::std::size_t> type_canon_index{};
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<runtime_llvm_jit_raw_call_target_t>> llvm_jit_call_indirect_targets{};

            compiled_module_record() = default;
            compiled_module_record(compiled_module_record const&) = delete;
            compiled_module_record& operator= (compiled_module_record const&) = delete;
            compiled_module_record(compiled_module_record&&) = default;
            compiled_module_record& operator= (compiled_module_record&&) = default;
            ~compiled_module_record() noexcept;
        };

        struct defined_func_ptr_range
        {
            ::std::uintptr_t begin{};
            ::std::uintptr_t end{};
            ::std::size_t module_id{};
        };

        struct runtime_global_state
        {
            ::uwvm2::utils::container::vector<compiled_module_record> modules{};
            ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view, ::std::size_t> module_name_to_id{};

            // Full-compile: keep the hot local-call path O(1) by indexing local funcs with vectors (not hash maps).
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<compiled_defined_func_info>> defined_func_cache{};

            // For indirect calls / import-alias resolution that only has `local_defined_function_storage_t*`,
            // map pointer-address to {module_id, local_index} via a sorted range table.
            ::uwvm2::utils::container::vector<defined_func_ptr_range> defined_func_ptr_ranges{};

            ::std::atomic_bool bridges_initialized{false};
            ::std::atomic_bool compiled_all{false};
        };

        inline runtime_global_state g_runtime{};  // [global]

        inline bool& runtime_process_exiting_flag() noexcept
        {
            static bool flag{};
            return flag;
        }

        inline void mark_runtime_process_exiting() noexcept { runtime_process_exiting_flag() = true; }

        inline void ensure_runtime_process_exit_handler_registered() noexcept
        {
            static bool registered{(::std::atexit(mark_runtime_process_exiting), true)};
            static_cast<void>(registered);
        }

        inline compiled_module_record::~compiled_module_record() noexcept
        {
#if defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)
            if(runtime_process_exiting_flag())
            {
                llvm_jit_compiled.llvm_jit_module.llvm_module.release();
                llvm_jit_compiled.llvm_jit_module.llvm_context_holder.release();
                llvm_jit_engine.release();
                llvm_jit_context_holder.release();
            }
#endif
        }

        struct cached_import_target;

        struct call_stack_frame
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
        };

        struct call_stack_tls_state
        {
            inline static constexpr ::std::size_t kCallStackMaxDepth{4096uz};
            inline static constexpr ::std::size_t kCallIndirectCacheEntries{8uz};

            using thread_local_allocator = ::fast_io::native_thread_local_allocator;
            ::uwvm2::utils::container::vector<call_stack_frame, thread_local_allocator> frames{};

            struct call_indirect_cache_entry
            {
                runtime_table_storage_t const* table{};
                void const* elems_data{};
                ::std::uint_least32_t selector{};
                ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected_ft_ptr{};

                ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t elem_type{};
                void const* target_ptr{};

                ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const* defined_info{};
                cached_import_target const* imported_tgt{};
            };

            static_assert((kCallIndirectCacheEntries & (kCallIndirectCacheEntries - 1uz)) == 0uz, "cache size must be power-of-two.");
            ::uwvm2::utils::container::array<call_indirect_cache_entry, kCallIndirectCacheEntries> call_indirect_cache{};

            inline call_stack_tls_state() noexcept { frames.reserve(kCallStackMaxDepth); }

            inline void push(call_stack_frame fr) noexcept
            {
                if(frames.size() < frames.capacity()) [[likely]] { frames.push_back_unchecked(fr); }
                else
                {
                    frames.push_back(fr);
                }
            }

            inline void pop() noexcept
            {
                if(!frames.empty()) [[likely]] { frames.pop_back_unchecked(); }
            }
        };

        struct preload_call_context_t
        {
            inline static constexpr ::std::size_t invalid_module_id{::std::numeric_limits<::std::size_t>::max()};

            ::std::size_t module_id{invalid_module_id};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};
        };

#if defined(UWVM_USE_THREAD_LOCAL)
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local call_stack_tls_state g_call_stack{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local preload_call_context_t g_preload_call_context{};  // [global] [thread_local]

        [[nodiscard]] UWVM_ALWAYS_INLINE inline call_stack_tls_state& get_call_stack() noexcept { return g_call_stack; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline preload_call_context_t& get_preload_call_context() noexcept { return g_preload_call_context; }

        inline void erase_current_thread_state() noexcept {}
#else
        using os_thread_id_t =
# if defined(__SINGLE_THREAD__)
            ::std::size_t;
# else
            decltype(::fast_io::this_thread::get_id());
# endif

        struct runtime_thread_state
        {
            call_stack_tls_state call_stack{};
            preload_call_context_t preload_call_context{};
        };

        [[nodiscard]] UWVM_ALWAYS_INLINE inline os_thread_id_t current_thread_id() noexcept
        {
# if defined(__SINGLE_THREAD__)
            return 0uz;
# else
            return ::fast_io::this_thread::get_id();
# endif
        }

        inline ::uwvm2::utils::container::concurrent_node_map<os_thread_id_t, runtime_thread_state> g_thread_states{};  // [global]

        /// @warning `g_thread_states` entries MUST be cleaned up on thread exit.
        ///          Currently only the main thread is used; we clear `g_thread_states` at the end of
        ///          `full_compile_and_run_main_module()` (by erasing the current thread state).
        ///          When implementing `wasi-thread`, every created thread must erase its own state on exit to avoid
        ///          unbounded growth and possible thread-id reuse issues.

        struct thread_states_reserve_guard
        {
            inline thread_states_reserve_guard() noexcept { g_thread_states.reserve(256uz); }
        };

        inline thread_states_reserve_guard g_thread_states_reserve_guard{};  // [global]

        [[nodiscard]] inline runtime_thread_state& get_thread_state() noexcept
        {
            auto const id{current_thread_id()};
            runtime_thread_state* st{};

            g_thread_states.try_emplace_and_visit(
                id,
                [&](auto& kv) noexcept { st = ::std::addressof(kv.second); },
                [&](auto& kv) noexcept { st = ::std::addressof(kv.second); });

            if(st == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            return *st;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline call_stack_tls_state& get_call_stack() noexcept { return get_thread_state().call_stack; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline preload_call_context_t& get_preload_call_context() noexcept { return get_thread_state().preload_call_context; }

        inline void erase_current_thread_state() noexcept { g_thread_states.erase(current_thread_id()); }
#endif

        struct preload_call_context_guard
        {
            preload_call_context_t* ctx{};
            preload_call_context_t saved{};

            inline explicit preload_call_context_guard(preload_module_memory_attribute_t const* attribute) noexcept :
                ctx{::std::addressof(get_preload_call_context())}, saved{*ctx}
            {
                auto& call_stack{get_call_stack()};
                if(!call_stack.frames.empty()) [[likely]] { ctx->module_id = call_stack.frames.back().module_id; }
                else
                {
                    ctx->module_id = preload_call_context_t::invalid_module_id;
                }
                ctx->preload_module_memory_attribute = attribute;
            }

            inline preload_call_context_guard(preload_call_context_guard const&) noexcept = delete;
            inline preload_call_context_guard& operator= (preload_call_context_guard const&) noexcept = delete;

            inline ~preload_call_context_guard()
            {
                if(this->ctx != nullptr) [[likely]] { *this->ctx = this->saved; }
            }
        };

        [[nodiscard]] inline compiled_defined_func_info const* find_defined_func_info(runtime_local_func_storage_t const* f) noexcept
        {
            if(f == nullptr) [[unlikely]] { return nullptr; }
            if(g_runtime.defined_func_ptr_ranges.empty()) [[unlikely]] { return nullptr; }

            ::std::uintptr_t const addr{reinterpret_cast<::std::uintptr_t>(f)};
            auto const it{::std::upper_bound(g_runtime.defined_func_ptr_ranges.begin(),
                                             g_runtime.defined_func_ptr_ranges.end(),
                                             addr,
                                             [](auto const a, defined_func_ptr_range const& r) constexpr noexcept { return a < r.begin; })};
            if(it == g_runtime.defined_func_ptr_ranges.begin()) [[unlikely]] { return nullptr; }

            auto const& r{*(it - 1u)};
            if(addr < r.begin || addr >= r.end) [[unlikely]] { return nullptr; }

            constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
            static_assert(elem_size != 0uz);

            ::std::uintptr_t const off_bytes{addr - r.begin};
            if((off_bytes % elem_size) != 0u) [[unlikely]] { return nullptr; }
            auto const local_idx{static_cast<::std::size_t>(off_bytes / elem_size)};

            if(r.module_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { return nullptr; }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(r.module_id)};
            if(local_idx >= mod_cache.size()) [[unlikely]] { return nullptr; }

            auto const* const info{::std::addressof(mod_cache.index_unchecked(local_idx))};
            if(info->runtime_func != f) [[unlikely]] { return nullptr; }
            return info;
        }

        struct call_stack_guard
        {
            call_stack_tls_state* tls{};

            inline constexpr explicit call_stack_guard(call_stack_tls_state& s, ::std::size_t module_id, ::std::size_t function_index) noexcept : tls{&s}
            { tls->push(call_stack_frame{module_id, function_index}); }

            call_stack_guard(call_stack_guard const&) = delete;
            call_stack_guard& operator= (call_stack_guard const&) = delete;

            inline constexpr ~call_stack_guard()
            {
                if(tls) [[likely]] { tls->pop(); }
            }
        };

        enum class trap_kind : unsigned
        {
            // opfunc
            unreachable,
            invalid_conversion_to_integer,
            integer_divide_by_zero,
            integer_overflow,
            // call_indirect (wasm1.0 MVP)
            call_indirect_table_out_of_bounds,
            call_indirect_null_element,
            call_indirect_type_mismatch,
            // uncatched int error (wasm 3.0, exception)
            uncatched_int_tag

        };

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view trap_kind_name(trap_kind k) noexcept
        {
            switch(k)
            {
                case trap_kind::unreachable:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"catch unreachable"};
                }
                case trap_kind::invalid_conversion_to_integer:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"invalid conversion to integer"};
                }
                case trap_kind::integer_divide_by_zero:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer divide by zero"};
                }
                case trap_kind::integer_overflow:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer overflow"};
                }
                case trap_kind::call_indirect_table_out_of_bounds:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: table index out of bounds"};
                }
                case trap_kind::call_indirect_null_element:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: uninitialized element"};
                }
                case trap_kind::call_indirect_type_mismatch:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: signature mismatch"};
                }
                case trap_kind::uncatched_int_tag:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"tag: uncatched wasm exception"};
                }
                [[unlikely]] default:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"unknown trap"};
                }
            }
        }

        [[nodiscard]] inline ::uwvm2::utils::container::u8string_view resolve_module_display_name(::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return module_name; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return module_name; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return module_name; }

            auto const n{wf->wasm_custom_name.module_name};
            if(n.empty()) { return module_name; }
            return n;
        }

        [[nodiscard]] inline ::uwvm2::utils::container::u8string_view resolve_func_display_name(::uwvm2::utils::container::u8string_view module_name,
                                                                                                ::std::size_t function_index) noexcept
        {
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return {}; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return {}; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return {}; }

            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            constexpr auto wasm_u32_max{static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())};
            if(function_index > wasm_u32_max) { return {}; }

            auto const key{static_cast<wasm_u32>(function_index)};
            auto const fn_it{wf->wasm_custom_name.function_name.find(key)};
            if(fn_it == wf->wasm_custom_name.function_name.end()) { return {}; }
            return fn_it->second;
        }

        inline constexpr void dump_call_stack_for_trap() noexcept
        {
            // No copies will be made here.
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Call stack:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            auto const& frames{get_call_stack().frames};
            auto const n{frames.size()};
            for(::std::size_t i{}; i != n; ++i)
            {
                auto const& fr{frames.index_unchecked(n - 1uz - i)};
                if(fr.module_id >= g_runtime.modules.size()) { continue; }

                auto const& mod_rec{g_runtime.modules.index_unchecked(fr.module_id)};
                auto const mod_name{resolve_module_display_name(mod_rec.module_name)};
                auto const fn_name{resolve_func_display_name(mod_rec.module_name, fr.function_index)};

                ::fast_io::io::perr(u8log_output_ul,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"#",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    i,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" module=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    mod_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" func_idx=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    fr.function_index);

                if(!fn_name.empty())
                {
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8" func_name=\"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        fn_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\"");
                }

                ::fast_io::io::perrln(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            ::fast_io::io::perrln(u8log_output_ul);
        }

        [[noreturn]] inline constexpr void trap_fatal(trap_kind k) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Runtime crash (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                trap_kind_name(k),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8")\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            dump_call_stack_for_trap();

            ::fast_io::fast_terminate();
        }

#ifdef UWVM_CPP_EXCEPTIONS
        [[noreturn]] inline void print_and_terminate_compile_validation_error(::uwvm2::utils::container::u8string_view module_name,
                                                                              ::uwvm2::validation::error::code_validation_error_impl const& v_err) noexcept
        {
            // Try to print detailed validator diagnostics (same format as `uwvm/runtime/validator/validate.h`).
            auto const fallback_and_terminate{
                [&]() noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Validation error during compilation (module=\"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\").\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }};

            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { fallback_and_terminate(); }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) [[unlikely]] { fallback_and_terminate(); }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { fallback_and_terminate(); }

            auto const file_name{wf->file_name};
            auto const& module_storage{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

            auto const module_begin{module_storage.module_span.module_begin};
            auto const module_end{module_storage.module_span.module_end};
            if(module_begin == nullptr || module_end == nullptr) [[unlikely]] { fallback_and_terminate(); }

            ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{module_begin, v_err.err_curr, module_end};

            ::uwvm2::validation::error::error_output_t errout{};
            errout.module_begin = module_begin;
            errout.err = v_err;
            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
# endif

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                // 1
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validation error in WebAssembly Code (module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", file=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                file_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\").\n",
                                // 2
                                errout,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\n"
                                // 3
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validator Memory Indication: ",
                                memory_printer,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");

            ::fast_io::fast_terminate();
        }
#endif

        enum class valtype_kind : unsigned
        {
            wasm_enum,
            raw_u8
        };

        struct valtype_vec_view
        {
            valtype_kind kind{};
            void const* data{};
            ::std::size_t size{};

            inline constexpr ::std::uint_least8_t at(::std::size_t i) const noexcept
            {
                if(i >= size) [[unlikely]] { return 0u; }

                if(kind == valtype_kind::raw_u8)
                {
                    auto const p{static_cast<::std::uint_least8_t const*>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return p[i];
                }
                else
                {
                    using wasm_value_type_const_ptr_t_may_alias UWVM_GNU_MAY_ALIAS = wasm_value_type const*;

                    auto const p{static_cast<wasm_value_type_const_ptr_t_may_alias>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return static_cast<::std::uint_least8_t>(p[i]);
                }
            }
        };

        struct func_sig_view
        {
            valtype_vec_view params{};
            valtype_vec_view results{};
        };

        // Precomputed import dispatch table for O(1) imported calls.
        // This is built once before execution (after uwvm runtime initialization + compilation).
        struct cached_import_target
        {
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};
            call_stack_frame frame{};
            func_sig_view sig{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};

            union
            {
                struct
                {
                    runtime_local_func_storage_t const* runtime_func{};
                    compiled_local_func_t const* compiled_func{};
                } defined;

                runtime_imported_func_storage_t::local_imported_target_t local_imported;

                capi_function_t const* capi_ptr;
            } u{};
        };

        inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<cached_import_target>> g_import_call_cache{};  // [global]

        [[nodiscard]] inline constexpr ::std::size_t valtype_size(::std::uint_least8_t code) noexcept
        {
            switch(static_cast<wasm_value_type>(code))
            {
                case wasm_value_type::i32:
                {
                    return 4uz;
                }
                case wasm_value_type::i64:
                {
                    return 8uz;
                }
                case wasm_value_type::f32:
                {
                    return 4uz;
                }
                case wasm_value_type::f64:
                {
                    return 8uz;
                }
                default:
                {
                    if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) { return 16uz; }
                    return 0uz;
                }
            }
        }

        [[nodiscard]] inline constexpr bool func_sig_equal(func_sig_view const& a, func_sig_view const& b) noexcept
        {
            if(a.params.size != b.params.size || a.results.size != b.results.size) { return false; }
            for(::std::size_t i{}; i != a.params.size; ++i)
            {
                if(a.params.at(i) != b.params.at(i)) { return false; }
            }
            for(::std::size_t i{}; i != a.results.size; ++i)
            {
                if(a.results.at(i) != b.results.at(i)) { return false; }
            }
            return true;
        }

        [[nodiscard]] inline constexpr ::std::size_t total_abi_bytes(valtype_vec_view const& v) noexcept
        {
            ::std::size_t total{};
            for(::std::size_t i{}; i != v.size; ++i)
            {
                auto const sz{valtype_size(v.at(i))};
                if(sz == 0uz) [[unlikely]] { return 0uz; }
                if(total > (::std::numeric_limits<::std::size_t>::max() - sz)) [[unlikely]] { return 0uz; }
                total += sz;
            }
            return total;
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_defined(runtime_local_func_storage_t const* f) noexcept
        {
            auto const ft{f->function_type_ptr};
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        [[nodiscard]] inline func_sig_view func_sig_from_local_imported(local_imported_t const* m, ::std::size_t idx) noexcept
        {
            auto const info{m->get_function_information_from_index(idx)};
            if(!info.successed) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& ft{info.function_type};
            return {
                {valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                {valtype_kind::wasm_enum, ft.result.begin,    static_cast<::std::size_t>(ft.result.end - ft.result.begin)      }
            };
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_capi(capi_function_t const* f) noexcept
        {
            return {
                {valtype_kind::raw_u8, f->para_type_vec_begin, f->para_type_vec_size},
                {valtype_kind::raw_u8, f->res_type_vec_begin,  f->res_type_vec_size }
            };
        }

        [[nodiscard]] inline preload_module_memory_attribute_t const* find_preload_module_memory_attribute(capi_function_t const* f) noexcept
        { return ::uwvm2::uwvm::wasm::storage::find_loaded_preload_module_memory_attribute(f); }

        struct resolved_func
        {
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};

            union
            {
                runtime_local_func_storage_t const* defined_ptr;
                runtime_imported_func_storage_t::local_imported_target_t local_imported;
                capi_function_t const* capi_ptr;
            } u{};
        };

        // Import resolution is performed by uwvm runtime initializer.
        // This runtime only consumes the initialized link_kind/target fields and never performs on-demand linking.
        [[nodiscard]] inline runtime_imported_func_storage_t const* resolve_import_leaf_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto curr{f};
            for(::std::size_t steps{};; ++steps)
            {
                // The initializer guarantees import-alias chains are finite and acyclic. This guard is for internal bugs.
                if(steps > 8192uz) [[unlikely]] { return nullptr; }
                if(curr == nullptr) [[unlikely]] { return nullptr; }

                switch(curr->link_kind)
                {
                    case link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        continue;
                    }
                    case link_kind::defined: [[fallthrough]];
                    case link_kind::local_imported:
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    case link_kind::dl:
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    case link_kind::weak_symbol:
#endif
                    {
                        return curr;
                    }
                    case link_kind::unresolved:
                    {
                        return nullptr;
                    }
                    default:
                    {
                        return nullptr;
                    }
                }
            }
        }

        [[nodiscard]] inline resolved_func resolve_func_from_import_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto const leaf{resolve_import_leaf_assuming_initialized(f)};
            if(leaf == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            switch(leaf->link_kind)
            {
                case link_kind::defined:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::defined;
                    r.u.defined_ptr = leaf->target.defined_ptr;
                    return r;
                }
                case link_kind::local_imported:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::local_imported;
                    r.u.local_imported = leaf->target.local_imported;
                    return r;
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case link_kind::dl:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::dl;
                    r.u.capi_ptr = leaf->target.dl_ptr;
                    return r;
                }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case link_kind::weak_symbol:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::weak_symbol;
                    r.u.capi_ptr = leaf->target.weak_symbol_ptr;
                    return r;
                }
#endif
                case link_kind::imported: [[fallthrough]];
                default:
                {
                    [[unlikely]] ::fast_io::fast_terminate();
                }
            }
        }

        using opfunc_byref_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<::std::byte const*, ::std::byte*, ::std::byte*>;

        using byte_allocator = ::fast_io::native_thread_local_allocator;

        struct heap_buf_guard
        {
            void* ptr{};

            inline constexpr heap_buf_guard() noexcept = default;

            heap_buf_guard(heap_buf_guard const&) = delete;
            heap_buf_guard& operator= (heap_buf_guard const&) = delete;

            inline constexpr ~heap_buf_guard()
            {
                if(ptr) { byte_allocator::deallocate(ptr); }
            }
        };

#if defined(UWVM_USE_THREAD_LOCAL)
        struct thread_local_bump_allocator
        {
            ::std::byte* base{};
            ::std::size_t cap{};
            ::std::size_t sp{};

            thread_local_bump_allocator(thread_local_bump_allocator const&) = delete;
            thread_local_bump_allocator& operator= (thread_local_bump_allocator const&) = delete;

            inline constexpr thread_local_bump_allocator() noexcept = default;

            inline ~thread_local_bump_allocator()
            {
                if(base) { byte_allocator::deallocate(base); }
            }

            [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t mark() const noexcept { return sp; }

            UWVM_ALWAYS_INLINE inline constexpr void release(::std::size_t m) noexcept { sp = m; }

            [[nodiscard]] UWVM_ALWAYS_INLINE static inline constexpr ::std::size_t align_up(::std::size_t v, ::std::size_t a) noexcept
            { return (v + (a - 1uz)) & ~(a - 1uz); }

            UWVM_ALWAYS_INLINE inline void ensure_capacity(::std::size_t need) noexcept
            {
                if(need <= cap) [[likely]] { return; }

                auto new_cap{cap ? cap : 4096uz};
                while(new_cap < need) { new_cap <<= 1; }

                void* const new_mem{byte_allocator::allocate(new_cap)};
                if(new_mem == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(base) [[likely]]
                {
                    ::std::memcpy(new_mem, base, sp);
                    byte_allocator::deallocate(base);
                }

                base = static_cast<::std::byte*>(new_mem);
                cap = new_cap;
            }

            [[nodiscard]] UWVM_ALWAYS_INLINE inline ::std::byte* allocate_bytes(::std::size_t n, ::std::size_t align = 16uz) noexcept
            {
                if(n == 0uz) [[unlikely]] { n = 1uz; }
                sp = align_up(sp, align);
                if(sp > (::std::numeric_limits<::std::size_t>::max() - n)) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const need{sp + n};
                ensure_capacity(need);
                auto* const p{base + sp};
                sp = need;
                return p;
            }
        };

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local thread_local_bump_allocator g_call_scratch{};  // [global] [thread_local]
#endif

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        inline ::uwvm2::object::memory::linear::native_memory_t const* resolve_memory0_ptr(runtime_module_storage_t const& rt) noexcept
        {
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const import_n{rt.imported_memory_vec_storage.size()};
            if(import_n == 0uz)
            {
                if(rt.local_defined_memory_vec_storage.empty()) { return nullptr; }
                return ::std::addressof(rt.local_defined_memory_vec_storage.index_unchecked(0uz).memory);
            }

            constexpr ::std::size_t kMaxChain{4096uz};
            auto const* curr{::std::addressof(rt.imported_memory_vec_storage.index_unchecked(0uz))};
            for(::std::size_t steps{}; steps != kMaxChain; ++steps)
            {
                if(curr == nullptr) { return nullptr; }

                switch(curr->link_kind)
                {
                    case memory_link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        break;
                    }
                    case memory_link_kind::defined:
                    {
                        auto const* const def{curr->target.defined_ptr};
                        if(def == nullptr) { return nullptr; }
                        return ::std::addressof(def->memory);
                    }
                    case memory_link_kind::local_imported: [[fallthrough]];
                    case memory_link_kind::unresolved: [[fallthrough]];
                    default:
                    {
                        return nullptr;
                    }
                }
            }
            return nullptr;
        }
#endif

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        inline void bind_default_wasip1_memory(runtime_module_storage_t const& rt) noexcept
        {
            // Best-effort binding: WASI functions will trap/return errors if a caller without memory[0] invokes them.
            // Always overwrite the pointer to avoid using a stale memory from a previous run.
            auto const* const mem0{resolve_memory0_ptr(rt)};
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.wasip1_memory =
                const_cast<::uwvm2::object::memory::linear::native_memory_t*>(mem0);
        }
#endif

        [[nodiscard]] inline runtime_module_storage_t const* get_active_preload_runtime_module() noexcept
        {
            auto const& ctx{get_preload_call_context()};
            if(ctx.module_id == preload_call_context_t::invalid_module_id) [[unlikely]] { return nullptr; }
            if(ctx.module_id >= g_runtime.modules.size()) [[unlikely]] { return nullptr; }
            return g_runtime.modules.index_unchecked(ctx.module_id).runtime_module;
        }

        [[nodiscard]] inline preload_module_memory_attribute_t const* get_active_preload_memory_attribute() noexcept
        { return get_preload_call_context().preload_module_memory_attribute; }

        [[nodiscard]] inline bool preload_memory_index_is_selected(preload_module_memory_attribute_t const* attribute, ::std::size_t memory_index) noexcept
        {
            if(attribute == nullptr) [[unlikely]] { return false; }
            if(attribute->apply_to_all_memories) [[likely]] { return true; }

            return attribute->memory_index_set.find(memory_index) != attribute->memory_index_set.cend();
        }

        struct preload_memory_rights_t
        {
            bool allow_access{};
            bool prefer_mmap{};
        };

        [[nodiscard]] inline constexpr preload_memory_rights_t requested_preload_memory_rights(preload_module_memory_attribute_t const* attribute,
                                                                                               ::std::size_t memory_index) noexcept
        {
            using access_mode = ::uwvm2::uwvm::wasm::type::preload_module_memory_access_mode_t;

            if(!preload_memory_index_is_selected(attribute, memory_index)) [[unlikely]] { return {}; }

            switch(attribute->memory_access_mode)
            {
                case access_mode::copy:
                {
                    return {true, false};
                }
                case access_mode::mmap:
                {
                    return {true, true};
                }
                case access_mode::none: [[fallthrough]];
                default:
                {
                    return {};
                }
            }
        }

        struct resolved_preload_memory_t
        {
            enum class target_kind : unsigned
            {
                native_defined,
                local_imported
            };

            target_kind kind{target_kind::native_defined};
            native_memory_t const* native_memory{};
            local_imported_t* local_imported{};
            ::std::size_t local_imported_index{};
            ::std::byte* memory_begin{};
            ::std::uint_least64_t page_count{};
            ::std::uint_least64_t page_size_bytes{};
            ::std::uint_least64_t byte_length{};
            unsigned backend_kind{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_native_defined};
            unsigned mmap_delivery_state{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none};
            ::std::uint_least64_t partial_protection_limit_bytes{};
            void const* dynamic_length_atomic_object{};
        };

        struct preload_memory_delivery_t
        {
            unsigned delivery_state{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none};
        };

        [[nodiscard]] inline constexpr bool
            compute_preload_byte_length(::std::uint_least64_t page_count, ::std::uint_least64_t page_size_bytes, ::std::uint_least64_t& byte_length) noexcept
        {
            if(page_size_bytes != 0u && page_count > (::std::numeric_limits<::std::uint_least64_t>::max() / page_size_bytes)) [[unlikely]] { return false; }
            byte_length = page_count * page_size_bytes;
            return true;
        }

#if defined(UWVM_SUPPORT_MMAP)
        [[nodiscard]] inline constexpr ::std::uint_least64_t preload_partial_protection_limit_bytes(native_memory_t const& memory) noexcept
        {

            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                if(memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64)
                {
                    return ::uwvm2::object::memory::linear::max_partial_protection_wasm64_length;
                }
                return ::uwvm2::object::memory::linear::max_partial_protection_wasm32_length;
            }
            else
            {
                static_cast<void>(memory);
                return ::uwvm2::object::memory::linear::max_partial_protection_wasm32_length;
            }
        }
#endif

        [[nodiscard]] inline bool resolve_defined_preload_memory(native_memory_t const& memory, resolved_preload_memory_t& resolved) noexcept
        {
            resolved.kind = resolved_preload_memory_t::target_kind::native_defined;
            resolved.native_memory = ::std::addressof(memory);
            resolved.local_imported = nullptr;
            resolved.local_imported_index = 0uz;
            resolved.page_size_bytes = static_cast<::std::uint_least64_t>(1u) << memory.custom_page_size_log2;
            resolved.backend_kind = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_native_defined;
            resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none;
            resolved.partial_protection_limit_bytes = 0u;
            resolved.dynamic_length_atomic_object = nullptr;

            ::std::size_t snapshot_byte_length{};
            if(!::uwvm2::object::memory::linear::with_memory_access_snapshot(memory,
                                                                             [&](::std::byte* memory_begin, ::std::size_t byte_length) noexcept
                                                                             {
                                                                                 resolved.memory_begin = memory_begin;
                                                                                 snapshot_byte_length = byte_length;
                                                                                 return true;
                                                                             })) [[unlikely]]
            {
                return false;
            }

            resolved.byte_length = static_cast<::std::uint_least64_t>(snapshot_byte_length);
            resolved.page_count = static_cast<::std::uint_least64_t>(snapshot_byte_length >> memory.custom_page_size_log2);
            if(resolved.memory_begin == nullptr && resolved.byte_length != 0u) [[unlikely]] { return false; }

#if defined(UWVM_SUPPORT_MMAP)
            if constexpr(native_memory_t::can_mmap)
            {
                if(memory.require_dynamic_determination_memory_size())
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_dynamic_bounds;
                    resolved.dynamic_length_atomic_object = memory.memory_length_p;
                }
                else if(memory.is_full_page_protection())
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_full_protection;
                }
                else
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_partial_protection;
                    resolved.partial_protection_limit_bytes = preload_partial_protection_limit_bytes(memory);
                }
            }
#endif

            return true;
        }

        [[nodiscard]] inline bool resolve_local_imported_preload_memory(local_imported_t* local_imported,
                                                                        ::std::size_t local_imported_index,
                                                                        resolved_preload_memory_t& resolved) noexcept
        {
            if(local_imported == nullptr) [[unlikely]] { return false; }

            ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
            if(!local_imported->memory_access_snapshot_from_index(local_imported_index, snapshot)) [[unlikely]] { return false; }

            resolved.kind = resolved_preload_memory_t::target_kind::local_imported;
            resolved.native_memory = nullptr;
            resolved.local_imported = local_imported;
            resolved.local_imported_index = local_imported_index;
            resolved.memory_begin = snapshot.memory_begin;
            resolved.page_count = snapshot.page_count;
            resolved.page_size_bytes = local_imported->memory_page_size_from_index(local_imported_index);
            resolved.backend_kind = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_local_imported;
            resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none;
            resolved.partial_protection_limit_bytes = 0u;
            resolved.dynamic_length_atomic_object = nullptr;

            if(!compute_preload_byte_length(resolved.page_count, resolved.page_size_bytes, resolved.byte_length)) [[unlikely]] { return false; }
            if(resolved.memory_begin == nullptr && resolved.byte_length != 0u) [[unlikely]] { return false; }

            return true;
        }

        [[nodiscard]] inline bool
            resolve_preload_memory(runtime_module_storage_t const& rt, ::std::size_t memory_index, resolved_preload_memory_t& resolved) noexcept
        {
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const imported_count{rt.imported_memory_vec_storage.size()};
            if(memory_index < imported_count)
            {
                constexpr ::std::size_t kMaxChain{4096uz};
                auto const* curr{::std::addressof(rt.imported_memory_vec_storage.index_unchecked(memory_index))};

                for(::std::size_t steps{}; steps != kMaxChain; ++steps)
                {
                    if(curr == nullptr) [[unlikely]] { return false; }

                    switch(curr->link_kind)
                    {
                        case memory_link_kind::imported:
                        {
                            curr = curr->target.imported_ptr;
                            break;
                        }
                        case memory_link_kind::defined:
                        {
                            auto const* const defined{curr->target.defined_ptr};
                            if(defined == nullptr) [[unlikely]] { return false; }
                            return resolve_defined_preload_memory(defined->memory, resolved);
                        }
                        case memory_link_kind::local_imported:
                        {
                            return resolve_local_imported_preload_memory(curr->target.local_imported.module_ptr, curr->target.local_imported.index, resolved);
                        }
                        case memory_link_kind::unresolved: [[fallthrough]];
                        default:
                        {
                            return false;
                        }
                    }
                }

                return false;
            }

            auto const local_index{memory_index - imported_count};
            if(local_index >= rt.local_defined_memory_vec_storage.size()) [[unlikely]] { return false; }
            return resolve_defined_preload_memory(rt.local_defined_memory_vec_storage.index_unchecked(local_index).memory, resolved);
        }

        [[nodiscard]] inline constexpr preload_memory_delivery_t determine_preload_memory_delivery(preload_memory_rights_t rights,
                                                                                                   resolved_preload_memory_t const& resolved) noexcept
        {
            if(!rights.allow_access) [[unlikely]] { return {}; }

            if(rights.prefer_mmap && resolved.mmap_delivery_state != ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none)
            {
                return {resolved.mmap_delivery_state};
            }

            return {::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_copy};
        }

        [[nodiscard]] inline constexpr bool
            validate_preload_copy_range(::std::uint_least64_t byte_length, ::std::uint_least64_t offset, ::std::size_t size, ::std::size_t& offset_out) noexcept
        {
            if(byte_length > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return false; }
            if(offset > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return false; }

            auto const host_byte_length{static_cast<::std::size_t>(byte_length)};
            offset_out = static_cast<::std::size_t>(offset);

            if(offset_out > host_byte_length) [[unlikely]] { return false; }
            if(size > (host_byte_length - offset_out)) [[unlikely]] { return false; }

            return true;
        }

        template <typename Fn>
        [[nodiscard]] inline bool with_native_preload_copy_access(native_memory_t const& memory, Fn&& fn) noexcept
        { return ::uwvm2::object::memory::linear::with_memory_access_snapshot(memory, ::std::forward<Fn>(fn)); }

        [[nodiscard]] inline constexpr ::std::size_t active_preload_total_memory_count(runtime_module_storage_t const& rt) noexcept
        { return rt.imported_memory_vec_storage.size() + rt.local_defined_memory_vec_storage.size(); }

        [[nodiscard]] inline bool try_build_preload_memory_descriptor(::std::size_t memory_index,
                                                                      preload_memory_descriptor_t* out,
                                                                      resolved_preload_memory_t* resolved_out,
                                                                      preload_memory_delivery_t* delivery_out) noexcept
        {
            auto const* const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!resolve_preload_memory(*rt, memory_index, resolved)) [[unlikely]] { return false; }

            auto const rights{requested_preload_memory_rights(get_active_preload_memory_attribute(), memory_index)};
            auto const delivery{determine_preload_memory_delivery(rights, resolved)};
            if(delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none) [[unlikely]] { return false; }

            if(out != nullptr)
            {
                *out = preload_memory_descriptor_t{
                    .memory_index = memory_index,
                    .delivery_state = delivery.delivery_state,
                    .backend_kind = resolved.backend_kind,
                    .reserved0 = 0u,
                    .reserved1 = 0u,
                    .page_count = resolved.page_count,
                    .page_size_bytes = resolved.page_size_bytes,
                    .byte_length = resolved.byte_length,
                    .partial_protection_limit_bytes = resolved.partial_protection_limit_bytes,
                    .mmap_view_begin =
                        delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_copy ? nullptr : resolved.memory_begin,
                    .dynamic_length_atomic_object = delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_dynamic_bounds
                                                        ? resolved.dynamic_length_atomic_object
                                                        : nullptr};
            }

            if(resolved_out != nullptr) [[likely]] { *resolved_out = resolved; }
            if(delivery_out != nullptr) [[likely]] { *delivery_out = delivery; }
            return true;
        }

        [[nodiscard]] inline ::std::size_t preload_memory_descriptor_count_impl() noexcept
        {
            auto const* const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return 0uz; }

            auto const total{active_preload_total_memory_count(*rt)};
            ::std::size_t count{};
            for(::std::size_t i{}; i != total; ++i) { count += static_cast<::std::size_t>(try_build_preload_memory_descriptor(i, nullptr, nullptr, nullptr)); }
            return count;
        }

        [[nodiscard]] inline bool preload_memory_descriptor_at_impl(::std::size_t descriptor_index, preload_memory_descriptor_t* out) noexcept
        {
            if(out == nullptr) [[unlikely]] { return false; }

            auto const* const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return false; }

            auto const total{active_preload_total_memory_count(*rt)};
            ::std::size_t current_descriptor_index{};
            preload_memory_descriptor_t descriptor{};
            for(::std::size_t memory_index{}; memory_index != total; ++memory_index)
            {
                if(!try_build_preload_memory_descriptor(memory_index, ::std::addressof(descriptor), nullptr, nullptr)) { continue; }
                if(current_descriptor_index == descriptor_index) [[likely]]
                {
                    *out = descriptor;
                    return true;
                }
                ++current_descriptor_index;
            }

            return false;
        }

        [[nodiscard]] inline bool
            preload_memory_read_impl(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept
        {
            if(size != 0uz && destination == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!try_build_preload_memory_descriptor(memory_index, nullptr, ::std::addressof(resolved), nullptr)) [[unlikely]] { return false; }
            if(size == 0uz) [[unlikely]] { return true; }

            switch(resolved.kind)
            {
                case resolved_preload_memory_t::target_kind::native_defined:
                {
                    auto const* const memory{resolved.native_memory};
                    if(memory == nullptr) [[unlikely]] { return false; }
                    return with_native_preload_copy_access(
                        *memory,
                        [&](::std::byte* memory_begin, ::std::size_t byte_length) noexcept
                        {
                            if(memory_begin == nullptr) [[unlikely]] { return false; }

                            ::std::size_t host_offset{};
                            if(!validate_preload_copy_range(static_cast<::std::uint_least64_t>(byte_length), offset, size, host_offset)) [[unlikely]]
                            {
                                return false;
                            }

                            ::std::memcpy(destination, memory_begin + host_offset, size);
                            return true;
                        });
                }
                case resolved_preload_memory_t::target_kind::local_imported:
                {
                    auto* const local_imported{resolved.local_imported};
                    if(local_imported == nullptr) [[unlikely]] { return false; }
                    return local_imported->memory_read_from_index(resolved.local_imported_index, offset, destination, size);
                }
                default:
                {
                    return false;
                }
            }
        }

        [[nodiscard]] inline bool
            preload_memory_write_impl(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept
        {
            if(size != 0uz && source == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!try_build_preload_memory_descriptor(memory_index, nullptr, ::std::addressof(resolved), nullptr)) [[unlikely]] { return false; }
            if(size == 0uz) [[unlikely]] { return true; }

            switch(resolved.kind)
            {
                case resolved_preload_memory_t::target_kind::native_defined:
                {
                    auto const* const memory{resolved.native_memory};
                    if(memory == nullptr) [[unlikely]] { return false; }
                    return with_native_preload_copy_access(
                        *memory,
                        [&](::std::byte* memory_begin, ::std::size_t byte_length) noexcept
                        {
                            if(memory_begin == nullptr) [[unlikely]] { return false; }

                            ::std::size_t host_offset{};
                            if(!validate_preload_copy_range(static_cast<::std::uint_least64_t>(byte_length), offset, size, host_offset)) [[unlikely]]
                            {
                                return false;
                            }

                            ::std::memcpy(memory_begin + host_offset, source, size);
                            return true;
                        });
                }
                case resolved_preload_memory_t::target_kind::local_imported:
                {
                    auto* const local_imported{resolved.local_imported};
                    if(local_imported == nullptr) [[unlikely]] { return false; }
                    return local_imported->memory_write_to_index(resolved.local_imported_index, offset, source, size);
                }
                default:
                {
                    return false;
                }
            }
        }

        // IMPORTANT:
        // Do NOT wrap `alloca` in a helper function that returns the pointer: `alloca` is stack-frame bound,
        // so allocating in a callee and returning the pointer is a dangling pointer unless the compiler inlines it.
        // Use this macro so the `alloca` happens in the caller's stack frame.
#if UWVM_HAS_BUILTIN(__builtin_alloca)
# define UWVM_ALLOCA_BYTES(uwvm_n) __builtin_alloca(uwvm_n)
#elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
# define UWVM_ALLOCA_BYTES(uwvm_n) _alloca(uwvm_n)
#else
# define UWVM_ALLOCA_BYTES(uwvm_n) alloca(uwvm_n)
#endif

#define UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(dst_ptr, bytes_expr, guard_obj)                                                                          \
    {                                                                                                                                                          \
        ::std::size_t uwvm_n{bytes_expr};                                                                                                                      \
        if(uwvm_n == 0uz) [[unlikely]] { uwvm_n = 1uz; }                                                                                                       \
        if(uwvm_n < 1024uz)                                                                                                                                    \
        {                                                                                                                                                      \
            void* const uwvm_p{UWVM_ALLOCA_BYTES(uwvm_n)};                                                                                                     \
            ::std::memset(uwvm_p, 0, uwvm_n);                                                                                                                  \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            void* const uwvm_p{byte_allocator::allocate_zero(uwvm_n)};                                                                                         \
            guard_obj.ptr = uwvm_p;                                                                                                                            \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
    }

#define UWVM_STACK_OR_HEAP_ALLOC_BYTES_NONNULL(dst_ptr, bytes_expr, guard_obj)                                                                                 \
    {                                                                                                                                                          \
        ::std::size_t uwvm_n{bytes_expr};                                                                                                                      \
        if(uwvm_n == 0uz) [[unlikely]] { uwvm_n = 1uz; }                                                                                                       \
        if(uwvm_n < 1024uz)                                                                                                                                    \
        {                                                                                                                                                      \
            void* const uwvm_p{UWVM_ALLOCA_BYTES(uwvm_n)};                                                                                                     \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            void* const uwvm_p{byte_allocator::allocate(uwvm_n)};                                                                                              \
            guard_obj.ptr = uwvm_p;                                                                                                                            \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
    }

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        using uwvmint_interp_arg_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_arg_t<I, CompileOption>;

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        UWVM_ALWAYS_INLINE inline constexpr uwvmint_interp_arg_t<I, CompileOption>
            uwvmint_init_interp_arg(::std::byte const* ip, ::std::byte* stack_top, ::std::byte* local_base) noexcept
        {
            if constexpr(I == 0uz) { return ip; }
            else if constexpr(I == 1uz) { return stack_top; }
            else if constexpr(I == 2uz) { return local_base; }
            else
            {
                return uwvmint_interp_arg_t<I, CompileOption>{};
            }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
        UWVM_ALWAYS_INLINE inline void execute_compiled_defined_tailcall_impl(::std::index_sequence<Is...>,
                                                                              ::std::byte const* ip,
                                                                              ::std::byte* stack_top,
                                                                              ::std::byte* local_base) noexcept
        {
            using opfunc_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<uwvmint_interp_arg_t<Is, CompileOption>...>;
            opfunc_t fn;  // no init
            ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
            fn(uwvmint_init_interp_arg<Is, CompileOption>(ip, stack_top, local_base)...);
        }

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t res{};

#if !(defined(__pdp11) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
            res.is_tail_call = true;
#endif

#if defined(__ARM_PCS_AAPCS64) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            // aarch64: AAPCS64 (x0-x7 integer args, v0-v7 fp/simd args)
            // 3 fixed args: (ip, operand_stack_top, local_base) => occupy x0-x2
            // Use remaining integer args (x3-x7) for i32/i64 stack-top caching, and fp/simd args (v0-v7) for f32/f64/v128.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
#elif defined(__arm__) || defined(_M_ARM)
            // ARM32: AAPCS/EABI (r0-r3 integer args; hard-float variants may also use VFP regs).
            // After the 3 fixed interpreter args, there is at most 1 remaining core argument register (r3).
            // A full scalar+fp stack-top cache would largely spill to memory on most ABIs, negating the benefit.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                     \
    (!defined(_WIN32) || (defined(__GNUC__) || defined(__clang__)))
            // x86_64: sysv abi
            // x86_64: sysv abi in ms abi
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 14uz;
#elif defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
    !(defined(__GNUC__) || defined(__clang__))
            // x86_64: Windows x64 (MS ABI) (rcx/rdx/r8/r9, xmm0-xmm3)
            // This ABI provides only 4 register argument slots total. After the 3 fixed interpreter args, only 1 slot remains (r9/xmm3).
            // Empirically, enabling a 1-slot scalar4-merged stack-top cache tends to regress overall performance
            // (register pressure + spills), so keep stack-top caching disabled by default here.
# if 0
            /// @deprecated MS ABI "1-slot" stack-top cache experiment.
            ///             Often regresses performance due to spills/register shuffling. Kept for reference.
            ///             Keep v128 caching off by default.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 4uz;
# endif
#elif defined(__i386__) || defined(_M_IX86)
            // i386: (usually) only 2 register argument slots under fastcall (ecx/edx), and we already need 3 fixed args.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
            // powerpc64: SysV ELF (r3-r10 integer args, VSX for fp/simd)
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
            // powerpc32: AIX/SysV/EABI variants differ in i64/f64 passing (often reg-pairs) and hard-float rules.
            // Keep stack-top caching disabled by default for correctness across ABIs.
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
# if defined(__riscv_float_abi_soft) || defined(__riscv_float_abi_single)
            // riscv64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring in the integer register file.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# else
            // riscv64: psABI (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing is not consistently vector-reg based across toolchains/ABIs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
# endif
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
            // riscv32: i64/f64 are passed in register pairs and the effective register slots are tight.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
#elif defined(__loongarch__) && defined(__loongarch64)
# if defined(__loongarch_soft_float) || defined(__loongarch_single_float)
            // loongarch64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# else
            // loongarch64: LP64D (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing may be lowered to GPR pairs/stack depending on ABI + vector extension.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
# endif
#elif defined(__loongarch__)
            // loongarch32: i64/f64 passing uses pairs / stack depending on ABI; keep caching disabled by default.
#elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
            // MIPS ABIs are slot-based: fp args are only register-passed while they remain within the ABI's argument slots.
            // We conservatively target the 8-slot N32/N64 ABIs; O32 has only 4 slots and cannot satisfy Wasm1's minimum ring sizes without heavy spilling.
# if defined(__mips_n32) || defined(__mips_n64)
#  if defined(__mips_soft_float)
            // N32/N64 soft-float: use a scalar4-merged ring in the integer slots (fits in the 8 arg slots).
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // N32/N64 hard-float: keep total args within 8 slots so fp values still use FPRs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  endif
# endif
#elif defined(__s390x__)
            // s390x: Linux ABI (r2-r6 integer args, f0/f2/... fp args). Keep v128 caching off by default:
            // 16-byte vectors can be passed indirectly by pointer.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#elif defined(__s390__) || defined(__SYSC_ZARCH__)
            // s390 (31-bit) / z/Architecture (non-s390x toolchains): i64/f64 passing is ABI-sensitive (often reg pairs).
            // Leave stack-top caching disabled by default.
#elif defined(__sparc__)
            // SPARC: multiple ABIs (v8/v9, 32/64) with different fp arg rules. Leave caching disabled by default.
#elif defined(__IA64__) || defined(_M_IA64) || defined(__ia64__) || defined(__itanium__)
            // IA-64: Itanium ABI is rare today; keep caching disabled by default to avoid ABI mismatches.
#elif defined(__alpha__)
            // Alpha: uncommon; leave caching disabled by default.
#elif defined(__m68k__) || defined(__mc68000__)
            // m68k: uncommon; leave caching disabled by default.
#elif defined(__HPPA__)
            // HPPA: uncommon; leave caching disabled by default.
#elif defined(__e2k__)
            // E2K: uncommon; leave caching disabled by default.
#elif defined(__XTENSA__)
            // Xtensa: embedded; leave caching disabled by default.
#elif defined(__BFIN__)
            // Blackfin: embedded; leave caching disabled by default.
#elif defined(__convex__)
            // Convex: historical; leave caching disabled by default.
#elif defined(__370__) || defined(__THW_370__)
            // System/370: historical; leave caching disabled by default.
#elif defined(__pdp10) || defined(__pdp7) || defined(__pdp11)
            // PDP family: historical; leave caching disabled by default.
#elif defined(__THW_RS6000) || defined(_IBMR2) || defined(_POWER) || defined(_ARCH_PWR) || defined(_ARCH_PWR2)
            // RS/6000: historical; leave caching disabled by default.
#elif defined(__CUDA_ARCH__)
            // PTX (CUDA device code): stack-top caching is not applicable here.
#elif defined(__sh__)
            // SuperH: embedded; leave caching disabled by default.
#elif defined(__AVR__)
            // AVR: embedded; leave caching disabled by default.
#elif defined(__wasm__)
            // UWVM itself may be built as wasm32-wasi; stack-top caching via native ABI registers is not applicable here.
#endif

            return res;
        }

        UWVM_ALWAYS_INLINE inline void copy_bytes_small(::std::byte* dst, ::std::byte const* src, ::std::size_t n) noexcept
        {
            if(n != 0uz) [[likely]] { ::std::memcpy(dst, src, n); }
        }

        UWVM_ALWAYS_INLINE inline void zero_bytes_small(::std::byte* dst, ::std::size_t n) noexcept
        {
            if(n != 0uz) [[likely]] { ::std::memset(dst, 0, n); }
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline ::std::byte* align_ptr_up(::std::byte* p, ::std::size_t align) noexcept
        {
            auto const v{reinterpret_cast<::std::uintptr_t>(p)};
            auto const a{align};
            return reinterpret_cast<::std::byte*>((v + (a - 1uz)) & ~(a - 1uz));
        }

        [[nodiscard]] inline ::uwvm2::utils::container::vector<::std::size_t> build_type_canon_index(runtime_module_storage_t const& module) noexcept
        {
            using ft_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t;

            auto const type_begin{module.type_section_storage.type_section_begin};
            auto const type_end{module.type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { return {}; }

            auto const total{static_cast<::std::size_t>(type_end - type_begin)};
            ::uwvm2::utils::container::vector<::std::size_t> canon{};
            canon.resize(total);

            auto const sig_equal{[](ft_t const& a, ft_t const& b) constexpr noexcept -> bool
                                 {
                                     auto const a_params{static_cast<::std::size_t>(a.parameter.end - a.parameter.begin)};
                                     auto const b_params{static_cast<::std::size_t>(b.parameter.end - b.parameter.begin)};
                                     if(a_params != b_params) { return false; }

                                     auto const a_res{static_cast<::std::size_t>(a.result.end - a.result.begin)};
                                     auto const b_res{static_cast<::std::size_t>(b.result.end - b.result.begin)};
                                     if(a_res != b_res) { return false; }

                                     for(::std::size_t i{}; i != a_params; ++i)
                                     {
                                         if(a.parameter.begin[i] != b.parameter.begin[i]) { return false; }
                                     }

                                     for(::std::size_t i{}; i != a_res; ++i)
                                     {
                                         if(a.result.begin[i] != b.result.begin[i]) { return false; }
                                     }

                                     return true;
                                 }};

            for(::std::size_t i{}; i != total; ++i)
            {
                canon.index_unchecked(i) = i;
                for(::std::size_t j{}; j != i; ++j)
                {
                    if(sig_equal(type_begin[i], type_begin[j]))
                    {
                        canon.index_unchecked(i) = canon.index_unchecked(j);
                        break;
                    }
                }
            }

            return canon;
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t invalid_llvm_jit_encoded_type_id() noexcept
        {
            return (::std::numeric_limits<::std::uint_least32_t>::max)();
        }

        [[nodiscard]] inline ::std::uint_least32_t find_canonical_type_id_for_sig(compiled_module_record const& rec, func_sig_view sig) noexcept
        {
            auto const* const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return invalid_llvm_jit_encoded_type_id(); }

            auto const type_begin{runtime_module->type_section_storage.type_section_begin};
            auto const type_end{runtime_module->type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr || type_begin > type_end) [[unlikely]]
            {
                return invalid_llvm_jit_encoded_type_id();
            }

            auto const total{static_cast<::std::size_t>(type_end - type_begin)};
            auto const canon_ok{rec.type_canon_index.size() == total};

            for(::std::size_t i{}; i != total; ++i)
            {
                auto const& ft{type_begin[i]};
                auto const ft_sig{func_sig_view{
                    {valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                    {valtype_kind::wasm_enum, ft.result.begin, static_cast<::std::size_t>(ft.result.end - ft.result.begin)}}};
                if(!func_sig_equal(sig, ft_sig)) { continue; }

                auto const canonical_index{canon_ok ? rec.type_canon_index.index_unchecked(i) : i};
                if(canonical_index > static_cast<::std::size_t>((::std::numeric_limits<::std::uint_least32_t>::max)())) [[unlikely]]
                {
                    return invalid_llvm_jit_encoded_type_id();
                }
                return static_cast<::std::uint_least32_t>(canonical_index);
            }

            return invalid_llvm_jit_encoded_type_id();
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline bool
            try_execute_trivial_defined_call(::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const& info,
                                             ::std::byte** stack_top_ptr) noexcept
        {
            using trivial_kind_t = ::uwvm2::runtime::compiler::uwvm_int::optable::trivial_defined_call_kind;

            if(info.trivial_kind == trivial_kind_t::none) { return false; }

            auto* const stack_top{*stack_top_ptr};
            auto* const args_begin{stack_top - info.param_bytes};

            auto const load_u32{[](::std::byte const* p) noexcept -> ::std::uint_least32_t
                                {
                                    ::std::uint_least32_t v{};  // no init
                                    ::std::memcpy(::std::addressof(v), p, sizeof(v));
                                    return v;
                                }};
            auto const store_u32{[](::std::byte* p, ::std::uint_least32_t v) noexcept { ::std::memcpy(p, ::std::addressof(v), sizeof(v)); }};

            ::std::uint_least32_t const imm_u32{::std::bit_cast<::std::uint_least32_t>(info.trivial_imm)};
            ::std::uint_least32_t const imm2_u32{::std::bit_cast<::std::uint_least32_t>(info.trivial_imm2)};

            switch(info.trivial_kind)
            {
                case trivial_kind_t::nop_void:
                {
                    if(info.result_bytes != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    *stack_top_ptr = args_begin;
                    return true;
                }
                case trivial_kind_t::const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, imm_u32);
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::param0_i32:
                {
                    // Keep the first i32 argument and drop the rest.
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes < 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::add_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a + imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a ^ imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::mul_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a * imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::rotr_add_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    ::std::uint_least32_t const shift{static_cast<::std::uint_least32_t>(imm2_u32) & 31u};
                    ::std::uint_least32_t const r{::std::rotr(static_cast<::std::uint_least32_t>(a), shift)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(r + imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xorshift32_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t x{load_u32(args_begin)};
                    x ^= static_cast<::std::uint_least32_t>(x << 13u);
                    x ^= static_cast<::std::uint_least32_t>(x >> 17u);
                    x ^= static_cast<::std::uint_least32_t>(x << 5u);
                    store_u32(args_begin, x);
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::mul_add_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>((a * imm_u32) + imm2_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    ::std::uint_least32_t const b{load_u32(args_begin + 4uz)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a ^ b));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_add_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    ::std::uint_least32_t const b{load_u32(args_begin + 4uz)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a + (b ^ imm_u32)));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::sub_or_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const a{load_u32(args_begin)};
                    ::std::uint_least32_t const b{load_u32(args_begin + 4uz)};
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(a - (b | imm_u32)));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::sum8_xor_const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(info.param_bytes != 32uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t acc{};
                    for(::std::size_t i{}; i != 8uz; ++i) { acc += load_u32(args_begin + i * 4uz); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(acc ^ imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                [[unlikely]] default:
                {
                    break;
                }
            }

            return false;
        }

        inline void execute_compiled_defined(call_stack_tls_state& call_stack,
                                             [[maybe_unused]] runtime_local_func_storage_t const* runtime_func,
                                             compiled_local_func_t const* compiled_func,
                                             ::std::size_t param_bytes,
                                             ::std::size_t result_bytes,
                                             ::std::byte** caller_stack_top_ptr) noexcept
        {
            auto const local_bytes_raw{compiled_func->local_bytes_max};
            auto const stack_cap_raw{compiled_func->operand_stack_byte_max};
            constexpr ::std::size_t kInternalTempLocalBytes{8uz};
            ::std::size_t wasm_locals_bytes{local_bytes_raw};
            if(local_bytes_raw >= kInternalTempLocalBytes) [[likely]] { wasm_locals_bytes = local_bytes_raw - kInternalTempLocalBytes; }
            if(param_bytes > wasm_locals_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const zeroinit_end_raw{compiled_func->local_bytes_zeroinit_end};
            if(zeroinit_end_raw < param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(zeroinit_end_raw > wasm_locals_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const zero_n{zeroinit_end_raw - param_bytes};
            constexpr ::std::size_t kFrameAlign{16uz};
            constexpr ::std::size_t kFrameAlignPad{kFrameAlign - 1uz};
            bool const align_wasm_locals_start{zero_n >= 64uz};

            // Frame layout:
            // - locals region (with optional padding for aligning the Wasm-locals start after params)
            // - operand stack region (16-byte aligned)
            ::std::size_t local_alloc_n{local_bytes_raw};
            if(local_alloc_n == 0uz) [[unlikely]] { local_alloc_n = 1uz; }
            if(align_wasm_locals_start)
            {
                if(local_alloc_n > (::std::numeric_limits<::std::size_t>::max() - kFrameAlignPad)) [[unlikely]] { ::fast_io::fast_terminate(); }
                local_alloc_n += kFrameAlignPad;
            }

            ::std::size_t frame_alloc_n{local_alloc_n};
            if(stack_cap_raw != 0uz) [[likely]]
            {
                if(frame_alloc_n > (::std::numeric_limits<::std::size_t>::max() - (kFrameAlignPad + stack_cap_raw))) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                frame_alloc_n += kFrameAlignPad + stack_cap_raw;
            }

            constexpr ::std::size_t kAllocaMaxBytesPerFrame{4096uz};
            constexpr ::std::size_t kAllocaMaxCallDepth{128uz};
#if defined(UWVM_USE_THREAD_LOCAL)
            bool const use_scratch{frame_alloc_n > kAllocaMaxBytesPerFrame || call_stack.frames.size() > kAllocaMaxCallDepth};
            ::std::size_t scratch_mark{};
            if(use_scratch) { scratch_mark = g_call_scratch.mark(); }
#else
            bool const use_heap{frame_alloc_n > kAllocaMaxBytesPerFrame || call_stack.frames.size() > kAllocaMaxCallDepth};

            auto const heap_alloc_aligned{[](::std::size_t n, ::std::size_t align, heap_buf_guard& g) noexcept -> ::std::byte*
                                          {
                                              if(n == 0uz) [[unlikely]] { n = 1uz; }
                                              if(n > (::std::numeric_limits<::std::size_t>::max() - (align - 1uz))) [[unlikely]]
                                              {
                                                  ::fast_io::fast_terminate();
                                              }
                                              auto const raw_n{n + (align - 1uz)};
                                              void* const raw{byte_allocator::allocate(raw_n)};
                                              if(raw == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                                              g.ptr = raw;
                                              return align_ptr_up(static_cast<::std::byte*>(raw), align);
                                          }};
#endif

            auto caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - param_bytes};
            // Pop params from the caller stack first (so nested calls can't see them).
            *caller_stack_top_ptr = caller_args_begin;

            ::std::byte* frame_alloc{};
#if defined(UWVM_USE_THREAD_LOCAL)
            if(use_scratch) { frame_alloc = g_call_scratch.allocate_bytes(frame_alloc_n, kFrameAlign); }
            else
#else
            heap_buf_guard frame_heap_guard{};
            if(use_heap) { frame_alloc = heap_alloc_aligned(frame_alloc_n, kFrameAlign, frame_heap_guard); }
            else
#endif
            {
                frame_alloc = static_cast<::std::byte*>(UWVM_ALLOCA_BYTES(frame_alloc_n));
            }

            // Allocate locals as a packed byte buffer (i32/f32=4, i64/f64=8, plus the internal temp local).
            ::std::byte* const local_alloc{frame_alloc};
            ::std::byte* local_base{};
            if(align_wasm_locals_start)
            {
                // Align the start of the Wasm locals region (after params). This can improve bulk-zero performance on some libc `memset`s.
                local_base = align_ptr_up(local_alloc + param_bytes, kFrameAlign) - param_bytes;
            }
            else
            {
                local_base = local_alloc;
            }

            if(param_bytes > local_bytes_raw) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#else
                ::fast_io::fast_terminate();
#endif
            }

            if(param_bytes != 0uz) { copy_bytes_small(local_base, caller_args_begin, param_bytes); }

            // Wasm-visible locals (excluding params) are zero-initialized. The internal temp local (last 8 bytes) is not Wasm-visible and does not
            // require initialization; avoiding a tiny `memset` per call saves a lot of overhead on call-heavy benchmarks.
            if(zero_n != 0uz) { zero_bytes_small(local_base + param_bytes, zero_n); }

            // Allocate operand stack with the exact max byte size computed by the compiler (byte-packed: i32/f32=4, i64/f64=8).
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(stack_cap_raw == 0uz && compiled_func->operand_stack_max != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
#endif
            if(stack_cap_raw < result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::std::byte* operand_base{};
            ::std::byte operand_dummy{};
            if(stack_cap_raw == 0uz) [[unlikely]] { operand_base = ::std::addressof(operand_dummy); }
            else
            {
                operand_base = align_ptr_up(frame_alloc + local_alloc_n, kFrameAlign);
            }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            // Operand stack is fully defined by writes along interpreter execution; it does not require zero-initialization.
            // Keep it zeroed only in detailed debug-check builds to catch any accidental reads of uninitialized operands.
            ::std::memset(operand_base, 0, (stack_cap_raw == 0uz ? 1uz : stack_cap_raw));
#endif

            ::std::byte const* ip{compiled_func->op.operands.data()};
            ::std::byte* stack_top{operand_base};

            constexpr auto curr_target_tranopt{get_curr_target_tranopt()};

            if constexpr(curr_target_tranopt.is_tail_call)
            {
                constexpr ::std::size_t tuple_size{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_size<curr_target_tranopt>()};
                execute_compiled_defined_tailcall_impl<curr_target_tranopt>(::std::make_index_sequence<tuple_size>{}, ip, stack_top, local_base);
            }
            else
            {
                while(ip != nullptr) [[likely]]
                {
                    opfunc_byref_t fn;  // no init
                    ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
                    fn(ip, stack_top, local_base);
                }

                auto const actual_result_bytes{static_cast<::std::size_t>(stack_top - operand_base)};
                if(actual_result_bytes != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            }

            // Append results back to caller stack.
            copy_bytes_small(*caller_stack_top_ptr, operand_base, result_bytes);
            *caller_stack_top_ptr += result_bytes;

#if defined(UWVM_USE_THREAD_LOCAL)
            if(use_scratch) { g_call_scratch.release(scratch_mark); }
#endif
        }

        inline void invoke_local_imported(runtime_imported_func_storage_t::local_imported_target_t const& tgt,
                                          ::std::size_t para_bytes,
                                          ::std::size_t res_bytes,
                                          ::std::byte** caller_stack_top_ptr) noexcept
        {
            local_imported_t const* m{tgt.module_ptr};
            if(m == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            m->call_func_index(tgt.index, resbuf, parbuf);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

        inline void invoke_capi(capi_function_t const* f,
                                preload_module_memory_attribute_t const* preload_module_memory_attribute,
                                ::std::size_t para_bytes,
                                ::std::size_t res_bytes,
                                ::std::byte** caller_stack_top_ptr) noexcept
        {
            if(f == nullptr || f->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            preload_call_context_guard preload_guard{preload_module_memory_attribute};
            f->func_ptr(resbuf, parbuf);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

        inline void invoke_compiled_defined_raw_buffers(call_stack_tls_state& call_stack,
                                                        call_stack_frame frame,
                                                        runtime_local_func_storage_t const* runtime_func,
                                                        compiled_local_func_t const* compiled_func,
                                                        ::std::size_t param_bytes,
                                                        ::std::size_t result_bytes,
                                                        ::std::byte* result_buffer,
                                                        ::std::byte const* param_buffer) noexcept
        {
            if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr) || runtime_func == nullptr || compiled_func == nullptr)
                [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            auto const stack_bytes{param_bytes < result_bytes ? result_bytes : param_bytes};
            heap_buf_guard host_stack_guard{};
            ::std::byte* host_stack_base{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);

            if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, param_buffer, param_bytes); }

            ::std::byte* stack_top_ptr{host_stack_base + param_bytes};
            call_stack_guard g{call_stack, frame.module_id, frame.function_index};
            execute_compiled_defined(call_stack, runtime_func, compiled_func, param_bytes, result_bytes, ::std::addressof(stack_top_ptr));

            if(result_bytes != 0uz) { ::std::memcpy(result_buffer, host_stack_base, result_bytes); }
        }

        inline void llvm_jit_raw_call_defined_entry(::std::uintptr_t context_address,
                                                    ::std::uintptr_t result_buffer_address,
                                                    ::std::size_t result_bytes,
                                                    ::std::uintptr_t param_buffer_address,
                                                    ::std::size_t param_bytes) noexcept
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                auto const* const info{
                    reinterpret_cast<::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const*>(context_address)};
                auto* const result_buffer{reinterpret_cast<::std::byte*>(result_buffer_address)};
                auto const* const param_buffer{reinterpret_cast<::std::byte const*>(param_buffer_address)};

                if(info == nullptr || param_bytes != info->param_bytes || result_bytes != info->result_bytes) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                auto& call_stack{get_call_stack()};
                invoke_compiled_defined_raw_buffers(call_stack,
                                                   call_stack_frame{info->module_id, info->function_index},
                                                   static_cast<runtime_local_func_storage_t const*>(info->runtime_func),
                                                   info->compiled_func,
                                                   info->param_bytes,
                                                   info->result_bytes,
                                                   result_buffer,
                                                   param_buffer);
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                trap_fatal(trap_kind::uncatched_int_tag);
            }
#endif
        }

        inline void llvm_jit_raw_call_cached_import_entry(::std::uintptr_t context_address,
                                                          ::std::uintptr_t result_buffer_address,
                                                          ::std::size_t result_bytes,
                                                          ::std::uintptr_t param_buffer_address,
                                                          ::std::size_t param_bytes) noexcept
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                auto const* const tgt{reinterpret_cast<cached_import_target const*>(context_address)};
                auto* const result_buffer{reinterpret_cast<::std::byte*>(result_buffer_address)};
                auto const* const param_buffer{reinterpret_cast<::std::byte const*>(param_buffer_address)};

                if(tgt == nullptr || param_bytes != tgt->param_bytes || result_bytes != tgt->result_bytes ||
                   (result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr))
                    [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                auto& call_stack{get_call_stack()};
                call_stack_guard g{call_stack, tgt->frame.module_id, tgt->frame.function_index};

                switch(tgt->k)
                {
                    case cached_import_target::kind::defined:
                    {
                        invoke_compiled_defined_raw_buffers(call_stack,
                                                            tgt->frame,
                                                            tgt->u.defined.runtime_func,
                                                            tgt->u.defined.compiled_func,
                                                            tgt->param_bytes,
                                                            tgt->result_bytes,
                                                            result_buffer,
                                                            param_buffer);
                        return;
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        auto const* const local_imported_module{tgt->u.local_imported.module_ptr};
                        if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        local_imported_module->call_func_index(tgt->u.local_imported.index, result_buffer, const_cast<::std::byte*>(param_buffer));
                        return;
                    }
                    case cached_import_target::kind::dl:
                    case cached_import_target::kind::weak_symbol:
                    {
                        auto const* const capi_ptr{tgt->u.capi_ptr};
                        if(capi_ptr == nullptr || capi_ptr->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        preload_call_context_guard preload_guard{tgt->preload_module_memory_attribute};
                        capi_ptr->func_ptr(result_buffer, const_cast<::std::byte*>(param_buffer));
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                trap_fatal(trap_kind::uncatched_int_tag);
            }
#endif
        }

        [[maybe_unused]] inline void invoke_resolved(resolved_func const& rf, ::std::byte** caller_stack_top_ptr) noexcept
        {
            switch(rf.k)
            {
                case resolved_func::kind::defined:
                {
                    auto const* const info{find_defined_func_info(rf.u.defined_ptr)};
                    if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto& call_stack{get_call_stack()};
                    execute_compiled_defined(call_stack, info->runtime_func, info->compiled_func, info->param_bytes, info->result_bytes, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::local_imported:
                {
                    local_imported_t const* m{rf.u.local_imported.module_ptr};
                    auto const sig{func_sig_from_local_imported(m, rf.u.local_imported.index)};
                    if(sig.params.data == nullptr && sig.params.size != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_local_imported(rf.u.local_imported, para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::dl:
                case resolved_func::kind::weak_symbol:
                {
                    capi_function_t const* f{rf.u.capi_ptr};
                    auto const sig{func_sig_from_capi(f)};

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_capi(f, find_preload_module_memory_attribute(f), para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        [[nodiscard]] inline constexpr runtime_table_storage_t const* resolve_table(runtime_module_storage_t const& module, ::std::size_t table_index) noexcept
        {
            auto const import_n{module.imported_table_vec_storage.size()};
            if(table_index < import_n)
            {
                auto t{::std::addressof(module.imported_table_vec_storage.index_unchecked(table_index))};
                for(;;)
                {
                    if(t == nullptr) [[unlikely]] { return nullptr; }
                    using lk = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    switch(t->link_kind)
                    {
                        case lk::defined:
                        {
                            return t->target.defined_ptr;
                        }
                        case lk::imported:
                        {
                            t = t->target.imported_ptr;
                            continue;
                        }
                        case lk::unresolved: [[fallthrough]];
                        default:
                        {
                            return nullptr;
                        }
                    }
                }
            }

            auto const local_index{table_index - import_n};
            if(local_index >= module.local_defined_table_vec_storage.size()) [[unlikely]] { return nullptr; }
            return ::std::addressof(module.local_defined_table_vec_storage.index_unchecked(local_index));
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr func_sig_view
            expected_sig_from_type_index(runtime_module_storage_t const& module, ::std::size_t type_index, bool& ok) noexcept
        {
            ok = false;
            auto const begin{module.type_section_storage.type_section_begin};
            auto const end{module.type_section_storage.type_section_end};
            if(begin == nullptr || end == nullptr) [[unlikely]] { return {}; }
            auto const total{static_cast<::std::size_t>(end - begin)};
            if(type_index >= total) [[unlikely]] { return {}; }

            auto const ft{begin + type_index};
            ok = true;
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        [[nodiscard]] inline ::std::size_t find_runtime_module_id_from_storage_ptr(runtime_module_storage_t const* runtime_module_ptr) noexcept
        {
            if(runtime_module_ptr == nullptr) [[unlikely]] { return ::std::numeric_limits<::std::size_t>::max(); }

            for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
            {
                if(g_runtime.modules.index_unchecked(module_id).runtime_module == runtime_module_ptr) { return module_id; }
            }

            return ::std::numeric_limits<::std::size_t>::max();
        }

        inline void populate_llvm_jit_call_indirect_table_views() noexcept
        {
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

            for(::std::size_t caller_module_id{}; caller_module_id != g_runtime.modules.size(); ++caller_module_id)
            {
                auto& caller_rec{g_runtime.modules.index_unchecked(caller_module_id)};
                auto* const caller_runtime_module{const_cast<runtime_module_storage_t*>(caller_rec.runtime_module)};
                if(caller_runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const all_table_count{
                    caller_runtime_module->imported_table_vec_storage.size() + caller_runtime_module->local_defined_table_vec_storage.size()};
                if(caller_runtime_module->llvm_jit_call_indirect_table_views.size() != all_table_count) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                caller_rec.llvm_jit_call_indirect_targets.clear();
                caller_rec.llvm_jit_call_indirect_targets.resize(all_table_count);

                for(::std::size_t table_index{}; table_index != all_table_count; ++table_index)
                {
                    auto& table_view{caller_runtime_module->llvm_jit_call_indirect_table_views.index_unchecked(table_index)};
                    table_view = {};

                    auto const* const resolved_table{resolve_table(*caller_runtime_module, table_index)};
                    if(resolved_table == nullptr) { continue; }

                    auto const* const provider_runtime_module{resolved_table->owner_module_rt_ptr};
                    if(provider_runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const provider_module_id{find_runtime_module_id_from_storage_ptr(provider_runtime_module)};
                    if(provider_module_id == (::std::numeric_limits<::std::size_t>::max)()) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto& target_vec{caller_rec.llvm_jit_call_indirect_targets.index_unchecked(table_index)};
                    target_vec.clear();
                    target_vec.resize(resolved_table->elems.size());

                    auto const* const provider_import_begin{provider_runtime_module->imported_function_vec_storage.data()};
                    auto const provider_import_count{provider_runtime_module->imported_function_vec_storage.size()};
                    auto const* const provider_import_cache{
                        provider_module_id < g_import_call_cache.size() ? ::std::addressof(g_import_call_cache.index_unchecked(provider_module_id)) : nullptr};
                    if(provider_import_cache == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    for(::std::size_t elem_index{}; elem_index != resolved_table->elems.size(); ++elem_index)
                    {
                        auto& target{target_vec.index_unchecked(elem_index)};
                        target = {};

                        auto const& elem{resolved_table->elems.index_unchecked(elem_index)};
                        switch(elem.type)
                        {
                            case table_elem_type::func_ref_defined:
                            {
                                auto const* const defined_func_ptr{elem.storage.defined_ptr};
                                if(defined_func_ptr == nullptr) { break; }

                                auto const* const defined_info{find_defined_func_info(defined_func_ptr)};
                                if(defined_info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                                target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_defined_entry);
                                target.context_address = reinterpret_cast<::std::uintptr_t>(defined_info);
                                target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, func_sig_from_defined(defined_info->runtime_func));
                                break;
                            }
                            case table_elem_type::func_ref_imported:
                            {
                                auto const* const imported_func_ptr{elem.storage.imported_ptr};
                                if(imported_func_ptr == nullptr) { break; }
                                if(provider_import_begin == nullptr || imported_func_ptr < provider_import_begin ||
                                   imported_func_ptr >= provider_import_begin + provider_import_count)
                                    [[unlikely]]
                                {
                                    ::fast_io::fast_terminate();
                                }

                                auto const import_index{static_cast<::std::size_t>(imported_func_ptr - provider_import_begin)};
                                if(import_index >= provider_import_cache->size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                                auto const& cached_target{provider_import_cache->index_unchecked(import_index)};
                                target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_cached_import_entry);
                                target.context_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(cached_target));
                                target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, cached_target.sig);
                                break;
                            }
                            [[unlikely]] default:
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                    }

                    table_view.data_address = reinterpret_cast<::std::uintptr_t>(target_vec.data());
                    table_view.size = target_vec.size();
                }
            }
        }

        inline void ensure_bridges_initialized() noexcept;
        inline void compile_all_modules_if_needed() noexcept;

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            describe_runtime_mode(::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t mode) noexcept
        {
            switch(mode)
            {
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile:
                    return u8"lazy_compile";
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile_with_full_code_verification:
                    return u8"lazy_compile_with_full_code_verification";
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile:
                    return u8"full_compile";
                [[unlikely]] default:
                    return u8"<unknown-runtime-mode>";
            }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            describe_runtime_compiler(::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only:
                    return u8"uwvm_interpreter_only";
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter:
                    return u8"debug_interpreter";
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                    return u8"uwvm_interpreter_llvm_jit_tiered";
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                    return u8"llvm_jit_only";
#endif
                [[unlikely]] default:
                    return u8"<unknown-runtime-compiler>";
            }
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline bool runtime_compiler_requests_llvm_jit_translation() noexcept
        {
            switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
            {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                    return true;
# endif
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                    return true;
                [[unlikely]] default:
                    return false;
            }
        }

        [[nodiscard]] inline bool runtime_compiler_requires_llvm_jit_execution() noexcept
        {
            return ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                   ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only;
        }

        [[nodiscard]] inline ::std::string
            get_runtime_llvm_jit_wasm_function_name(runtime_module_storage_t const& runtime_module, ::std::size_t func_index)
        {
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            return llvm_jit_translate_details::get_llvm_wasm_function_name(
                runtime_module, static_cast<llvm_jit_translate_details::validation_module_traits_t::wasm_u32>(func_index));
        }

        inline bool ensure_llvm_jit_native_target_initialized() noexcept
        {
            static bool initialized{};
            static bool success{};

            if(initialized) { return success; }

            initialized = true;
            success = !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() &&
                      !::llvm::InitializeNativeTargetAsmParser();
            return success;
        }

        [[nodiscard]] inline auto get_llvm_jit_host_target_attributes()
        {
            ::llvm::SmallVector<::std::string, 16> mattrs{};
            auto host_features{::llvm::sys::getHostCPUFeatures()};
            if(!host_features.empty())
            {
                for(auto const& [feature_name, feature_enabled] : host_features)
                {
                    auto prefix{feature_enabled ? '+' : '-'};
                    mattrs.push_back(::std::string(1u, prefix) + feature_name.str());
                }
            }
            return mattrs;
        }

        [[nodiscard]] inline bool optimize_runtime_llvm_jit_module(::llvm::Module& module, ::llvm::TargetMachine& target_machine) noexcept
        {
            ::std::string verify_error{};
            ::llvm::raw_string_ostream verify_stream(verify_error);
            if(::llvm::verifyModule(module, ::std::addressof(verify_stream))) [[unlikely]] { return false; }

            ::llvm::legacy::FunctionPassManager function_pass_manager(::std::addressof(module));
            function_pass_manager.add(::llvm::createTargetTransformInfoWrapperPass(target_machine.getTargetIRAnalysis()));
            function_pass_manager.add(::llvm::createPromoteMemoryToRegisterPass());
            function_pass_manager.add(::llvm::createInstructionCombiningPass());
            function_pass_manager.add(::llvm::createReassociatePass());
            function_pass_manager.add(::llvm::createGVNPass());
            function_pass_manager.add(::llvm::createCFGSimplificationPass());
            function_pass_manager.add(::llvm::createLICMPass());
            function_pass_manager.add(::llvm::createInstSimplifyLegacyPass());
            function_pass_manager.add(::llvm::createDeadCodeEliminationPass());

            function_pass_manager.doInitialization();
            for(auto& function : module)
            {
                if(function.isDeclaration()) { continue; }
                function_pass_manager.run(function);
            }
            function_pass_manager.doFinalization();

            ::std::string optimized_verify_error{};
            ::llvm::raw_string_ostream optimized_verify_stream(optimized_verify_error);
            return !::llvm::verifyModule(module, ::std::addressof(optimized_verify_stream));
        }

        [[nodiscard]] inline bool try_materialize_runtime_module_llvm_jit(compiled_module_record& rec) noexcept
        {
            constexpr auto llvm_jit_materialize_verbose_info{
                []<typename... Args>(Args&&... args)
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};
            constexpr auto llvm_jit_materialize_error{
                []<typename... Args>(Args&&... args)
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                        u8"\n");
                }};

            rec.llvm_jit_ready = false;
            rec.llvm_jit_local_entry_addresses.clear();
            rec.llvm_jit_engine.reset();
            rec.llvm_jit_context_holder.reset();

            auto const* const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const local_func_count{runtime_module->local_defined_function_vec_storage.size()};
            if(local_func_count == 0uz)
            {
                rec.llvm_jit_ready = true;
                return true;
            }

            auto& llvm_jit_module_storage{rec.llvm_jit_compiled.llvm_jit_module};
            if(!llvm_jit_module_storage.emitted || llvm_jit_module_storage.llvm_context_holder == nullptr || llvm_jit_module_storage.llvm_module == nullptr)
                [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT materialization missing emitted module state for module=\"",
                                               rec.module_name,
                                               u8"\".");
                }
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT native target initialization failed for module=\"",
                                               rec.module_name,
                                               u8"\".");
                }
                return false;
            }

            auto llvm_context_holder{::std::move(llvm_jit_module_storage.llvm_context_holder)};
            auto merged_module{::std::move(llvm_jit_module_storage.llvm_module)};
            if(llvm_context_holder == nullptr || merged_module == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT materialization lost module/context ownership for module=\"",
                                               rec.module_name,
                                               u8"\".");
                }
                return false;
            }

            auto const target_triple{::llvm::sys::getDefaultTargetTriple()};
            auto const host_cpu_name{::llvm::sys::getHostCPUName().str()};
            auto const host_target_attributes{get_llvm_jit_host_target_attributes()};

            ::std::string engine_error{};
            ::llvm::EngineBuilder target_builder{};
            target_builder.setErrorStr(::std::addressof(engine_error))
                .setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(::llvm::CodeGenOptLevel::Aggressive)
                .setMCPU(host_cpu_name)
                .setMAttrs(host_target_attributes);

            ::std::unique_ptr<::llvm::TargetMachine> target_machine{
                target_builder.selectTarget(::llvm::Triple(target_triple), "", host_cpu_name, host_target_attributes)};
            if(target_machine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT target selection failed for module=\"",
                                               rec.module_name,
                                               u8"\": ",
                                               ::fast_io::mnp::code_cvt(engine_error));
                }
                return false;
            }

            merged_module->setTargetTriple(::llvm::Triple(target_triple));
            merged_module->setDataLayout(target_machine->createDataLayout());
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                llvm_jit_materialize_verbose_info(u8"LLVM JIT materialization optimizing module \"",
                                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                  rec.module_name,
                                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                  u8"\". ");
            }
            if(!optimize_runtime_llvm_jit_module(*merged_module, *target_machine)) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT module optimization/verification failed for module=\"",
                                               rec.module_name,
                                               u8"\".");
                }
                return false;
            }

            auto* raw_engine{::llvm::EngineBuilder(::std::move(merged_module))
                                 .setEngineKind(::llvm::EngineKind::JIT)
                                 .setErrorStr(::std::addressof(engine_error))
                                 .setOptLevel(::llvm::CodeGenOptLevel::Aggressive)
                                 .setMCPU(host_cpu_name)
                                 .setMAttrs(host_target_attributes)
                                 .setMCJITMemoryManager(::std::make_unique<::llvm::SectionMemoryManager>())
                                 .create(target_machine.release())};
            if(raw_engine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT engine creation failed for module=\"",
                                               rec.module_name,
                                               u8"\": ",
                                               ::fast_io::mnp::code_cvt(engine_error));
                }
                return false;
            }

            ::std::unique_ptr<::llvm::ExecutionEngine> llvm_jit_engine{raw_engine};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                llvm_jit_materialize_verbose_info(u8"LLVM JIT materialization finalizing object for module \"",
                                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                  rec.module_name,
                                                  ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                  u8"\". ");
            }
            llvm_jit_engine->finalizeObject();

            auto const import_func_count{runtime_module->imported_function_vec_storage.size()};
            rec.llvm_jit_local_entry_addresses.resize(local_func_count);
            for(::std::size_t local_index{}; local_index != local_func_count; ++local_index)
            {
                auto const function_name{get_runtime_llvm_jit_wasm_function_name(*runtime_module, import_func_count + local_index)};
                auto const function_address{llvm_jit_engine->getFunctionAddress(function_name)};
                if(function_address == 0u) [[unlikely]]
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        auto* found_function{llvm_jit_engine->FindFunctionNamed(function_name)};
                        ::std::string function_type_text{};
                        if(found_function != nullptr)
                        {
                            ::llvm::raw_string_ostream function_type_stream(function_type_text);
                            found_function->getFunctionType()->print(function_type_stream);
                        }
                        llvm_jit_materialize_error(u8"LLVM JIT could not resolve function address for module=\"",
                                                   rec.module_name,
                                                   u8"\", function=\"",
                                                   ::fast_io::mnp::code_cvt(function_name),
                                                   u8"\", found=",
                                                   found_function != nullptr,
                                                   u8", declaration=",
                                                   found_function != nullptr && found_function->isDeclaration(),
                                                   u8", linkage=",
                                                   found_function != nullptr
                                                       ? static_cast<unsigned>(found_function->getLinkage())
                                                       : static_cast<unsigned>(::llvm::GlobalValue::ExternalLinkage),
                                                   u8", type=",
                                                   ::fast_io::mnp::code_cvt(function_type_text),
                                                   u8".");
                    }
                    return false;
                }
                rec.llvm_jit_local_entry_addresses.index_unchecked(local_index) = static_cast<::std::uintptr_t>(function_address);
            }

            rec.llvm_jit_context_holder = ::std::move(llvm_context_holder);
            rec.llvm_jit_engine = ::std::move(llvm_jit_engine);
            rec.llvm_jit_ready = true;
            return true;
        }

        [[nodiscard]] inline bool try_invoke_runtime_llvm_jit_defined_entry(::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const* const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) { return false; }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_local_entry_addresses.size() || !rec.llvm_jit_ready) { return false; }

            auto const function_address{rec.llvm_jit_local_entry_addresses.index_unchecked(local_index)};
            if(function_address == 0u) [[unlikely]] { return false; }

            using entry_fn_t = void (*)();
            auto& call_stack{get_call_stack()};
            call_stack_guard g{call_stack, module_id, function_index};
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            entry_fn();
            return true;
        }
#else
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_llvm_jit_translation() noexcept { return false; }
        [[nodiscard, maybe_unused]] inline constexpr bool runtime_compiler_requires_llvm_jit_execution() noexcept { return false; }
#endif

        // ==========
        // Bridges
        // ==========

        inline void unreachable_trap() noexcept { trap_fatal(trap_kind::unreachable); }

        inline void trap_invalid_conversion_to_integer() noexcept { trap_fatal(trap_kind::invalid_conversion_to_integer); }

        inline void trap_integer_divide_by_zero() noexcept { trap_fatal(trap_kind::integer_divide_by_zero); }

        inline void trap_integer_overflow() noexcept { trap_fatal(trap_kind::integer_overflow); }

        inline void call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_runtime.compiled_all.load(::std::memory_order_acquire)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
            if(wasm_module_id == SIZE_MAX) [[likely]]
            {
                using call_info_t = ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info;
                auto const* const info{reinterpret_cast<call_info_t const*>(func_index)};
                if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const* const rf{static_cast<runtime_local_func_storage_t const*>(info->runtime_func)};
                auto const* const cf{info->compiled_func};
                if(rf == nullptr || cf == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(try_execute_trivial_defined_call(*info, stack_top_ptr)) { return; }

                auto& call_stack{get_call_stack()};
                call_stack_guard g{call_stack, info->module_id, info->function_index};
                execute_compiled_defined(call_stack, rf, cf, info->param_bytes, info->result_bytes, stack_top_ptr);
                return;
            }

            auto& call_stack{get_call_stack()};

            if(wasm_module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_runtime.modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            auto const import_n{module.imported_function_vec_storage.size()};
            auto const local_n{module.local_defined_function_vec_storage.size()};
            if(func_index >= import_n + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(func_index < import_n)
            {
                if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                if(func_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const& tgt{cache.index_unchecked(func_index)};
                call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};

                switch(tgt.k)
                {
                    case cached_import_target::kind::defined:
                    {
                        execute_compiled_defined(call_stack,
                                                 tgt.u.defined.runtime_func,
                                                 tgt.u.defined.compiled_func,
                                                 tgt.param_bytes,
                                                 tgt.result_bytes,
                                                 stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::dl:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::weak_symbol:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }

            auto const local_index{func_index - import_n};
            auto const lf{::std::addressof(module.local_defined_function_vec_storage.index_unchecked(local_index))};
            call_stack_guard g{call_stack, wasm_module_id, func_index};
            if(wasm_module_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(wasm_module_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& info{mod_cache.index_unchecked(local_index)};
            if(info.runtime_func != lf) [[unlikely]] { ::fast_io::fast_terminate(); }
            execute_compiled_defined(call_stack, info.runtime_func, info.compiled_func, info.param_bytes, info.result_bytes, stack_top_ptr);
        }

        inline void
            call_indirect_bridge(::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_runtime.compiled_all.load(::std::memory_order_acquire)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
            if(wasm_module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_runtime.modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};
            auto& call_stack{get_call_stack()};

            // Pop selector index (i32).
            wasm_i32 selector_i32;  // no init
            *stack_top_ptr -= sizeof(selector_i32);
            ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
            auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

            auto const table{resolve_table(module, table_index)};
            if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(selector_u32 >= table->elems.size()) [[unlikely]] { trap_fatal(trap_kind::call_indirect_table_out_of_bounds); }

            auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};

            auto const type_begin{module.type_section_storage.type_section_begin};
            auto const type_end{module.type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const type_total{static_cast<::std::size_t>(type_end - type_begin)};
            if(type_index >= type_total) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const* const expected_ft_ptr{type_begin + type_index};
            if(expected_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Inline cache: the common hot case is a stable (table, selector, expected-type) triple inside a loop.
            // Cache the resolved call target to avoid repeating signature checks, pointer arithmetic and cache lookups.
            void const* const elems_data{table->elems.data()};
            auto const cache_index{static_cast<::std::size_t>(selector_u32) & (call_stack_tls_state::kCallIndirectCacheEntries - 1uz)};
            {
                auto& ic{call_stack.call_indirect_cache[cache_index]};
                if(ic.table == table && ic.elems_data == elems_data && ic.selector == selector_u32 && ic.expected_ft_ptr == expected_ft_ptr &&
                   ic.elem_type == elem.type && ic.target_ptr != nullptr)
                {
                    if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined)
                    {
                        auto const def_ptr{elem.storage.defined_ptr};
                        if(def_ptr != nullptr && ic.target_ptr == static_cast<void const*>(def_ptr) && ic.defined_info != nullptr)
                        {
                            auto const& info{*ic.defined_info};
                            if(try_execute_trivial_defined_call(info, stack_top_ptr)) { return; }
                            call_stack_guard g{call_stack, info.module_id, info.function_index};
                            auto const* const rf{static_cast<runtime_local_func_storage_t const*>(info.runtime_func)};
                            if(rf == nullptr || info.compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                            execute_compiled_defined(call_stack, rf, info.compiled_func, info.param_bytes, info.result_bytes, stack_top_ptr);
                            return;
                        }
                    }
                    else if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported)
                    {
                        auto const imp_ptr{elem.storage.imported_ptr};
                        if(imp_ptr != nullptr && ic.target_ptr == static_cast<void const*>(imp_ptr) && ic.imported_tgt != nullptr)
                        {
                            auto const& tgt{*ic.imported_tgt};
                            call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};
                            switch(tgt.k)
                            {
                                case cached_import_target::kind::defined:
                                {
                                    execute_compiled_defined(call_stack,
                                                             tgt.u.defined.runtime_func,
                                                             tgt.u.defined.compiled_func,
                                                             tgt.param_bytes,
                                                             tgt.result_bytes,
                                                             stack_top_ptr);
                                    return;
                                }
                                case cached_import_target::kind::local_imported:
                                {
                                    invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                                    return;
                                }
                                case cached_import_target::kind::dl:
                                case cached_import_target::kind::weak_symbol:
                                {
                                    invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                                    return;
                                }
                                [[unlikely]] default:
                                {
                                    ::fast_io::fast_terminate();
                                }
                            }
                        }
                    }
                }
            }

            // Fast signature check (miss path): compare canonical type indices (deduplicated by signature) to avoid scanning long param lists.
            auto const& canon{module_rec.type_canon_index};
            bool const canon_ok{canon.size() == type_total};
            auto const expected_canon{canon_ok ? canon.index_unchecked(type_index) : type_index};
            auto const type_begin_addr{reinterpret_cast<::std::uintptr_t>(type_begin)};
            auto const type_end_addr{reinterpret_cast<::std::uintptr_t>(type_end)};
            constexpr ::std::size_t kFTSize{sizeof(*type_begin)};

            auto const sig_from_ft{[](::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* ft) constexpr noexcept -> func_sig_view
                                   {
                                       return {
                                           {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                                           {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
                                       };
                                   }};

            switch(elem.type)
            {
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined:
                {
                    auto const def_ptr{elem.storage.defined_ptr};
                    if(def_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    auto const actual_ft_ptr{def_ptr->function_type_ptr};
                    if(actual_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    if(actual_ft_ptr != expected_ft_ptr)
                    {
                        bool match{};
                        if(canon_ok)
                        {
                            auto const actual_addr{reinterpret_cast<::std::uintptr_t>(actual_ft_ptr)};
                            if(actual_addr >= type_begin_addr && actual_addr < type_end_addr)
                            {
                                auto const diff{static_cast<::std::size_t>(actual_addr - type_begin_addr)};
                                if(diff % kFTSize == 0uz)
                                {
                                    auto const actual_index{diff / kFTSize};
                                    match = (canon.index_unchecked(actual_index) == expected_canon);
                                }
                            }
                        }

                        if(!match)
                        {
                            auto const expected_sig{sig_from_ft(expected_ft_ptr)};
                            if(!func_sig_equal(expected_sig, sig_from_ft(actual_ft_ptr))) [[unlikely]] { trap_fatal(trap_kind::call_indirect_type_mismatch); }
                        }
                    }

                    auto const base{module.local_defined_function_vec_storage.data()};
                    auto const local_n{module.local_defined_function_vec_storage.size()};
                    if(base == nullptr || def_ptr < base || def_ptr >= base + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const local_idx{static_cast<::std::size_t>(def_ptr - base)};

                    if(local_idx >= module_rec.compiled.local_defined_call_info.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& info{module_rec.compiled.local_defined_call_info.index_unchecked(local_idx)};
                    if(info.runtime_func != def_ptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(def_ptr);
                        ic.defined_info = ::std::addressof(info);
                        ic.imported_tgt = nullptr;
                    }

                    if(try_execute_trivial_defined_call(info, stack_top_ptr)) { return; }

                    call_stack_guard g{call_stack, info.module_id, info.function_index};
                    auto const* const rf{static_cast<runtime_local_func_storage_t const*>(info.runtime_func)};
                    if(rf == nullptr || info.compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_compiled_defined(call_stack, rf, info.compiled_func, info.param_bytes, info.result_bytes, stack_top_ptr);
                    return;
                }
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported:
                {
                    auto const imp_ptr{elem.storage.imported_ptr};
                    if(imp_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    auto const import_type_ptr{imp_ptr->import_type_ptr};
                    if(import_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const actual_ft_ptr{import_type_ptr->imports.storage.function};
                    if(actual_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    if(actual_ft_ptr != expected_ft_ptr)
                    {
                        bool match{};
                        if(canon_ok)
                        {
                            auto const actual_addr{reinterpret_cast<::std::uintptr_t>(actual_ft_ptr)};
                            if(actual_addr >= type_begin_addr && actual_addr < type_end_addr)
                            {
                                auto const diff{static_cast<::std::size_t>(actual_addr - type_begin_addr)};
                                if(diff % kFTSize == 0uz)
                                {
                                    auto const actual_index{diff / kFTSize};
                                    match = (canon.index_unchecked(actual_index) == expected_canon);
                                }
                            }
                        }

                        if(!match)
                        {
                            auto const expected_sig{sig_from_ft(expected_ft_ptr)};
                            if(!func_sig_equal(expected_sig, sig_from_ft(actual_ft_ptr))) [[unlikely]] { trap_fatal(trap_kind::call_indirect_type_mismatch); }
                        }
                    }

                    auto const base{module.imported_function_vec_storage.data()};
                    auto const imp_n{module.imported_function_vec_storage.size()};
                    if(base == nullptr || imp_ptr < base || imp_ptr >= base + imp_n) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const idx{static_cast<::std::size_t>(imp_ptr - base)};

                    if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                    if(idx >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& tgt{cache.index_unchecked(idx)};

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(imp_ptr);
                        ic.defined_info = nullptr;
                        ic.imported_tgt = ::std::addressof(tgt);
                    }

                    call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};
                    switch(tgt.k)
                    {
                        case cached_import_target::kind::defined:
                        {
                            execute_compiled_defined(call_stack,
                                                     tgt.u.defined.runtime_func,
                                                     tgt.u.defined.compiled_func,
                                                     tgt.param_bytes,
                                                     tgt.result_bytes,
                                                     stack_top_ptr);
                            return;
                        }
                        case cached_import_target::kind::local_imported:
                        {
                            invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                            return;
                        }
                        case cached_import_target::kind::dl:
                        case cached_import_target::kind::weak_symbol:
                        {
                            invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                            return;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline void ensure_bridges_initialized() noexcept
        {
            if(g_runtime.bridges_initialized.load(::std::memory_order_acquire)) { return; }

            static ::std::atomic_flag init_lock = ATOMIC_FLAG_INIT;
            while(init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.bridges_initialized.load(::std::memory_order_acquire)) { return; }
                ::fast_io::this_thread::yield();
            }

            if(!g_runtime.bridges_initialized.load(::std::memory_order_relaxed))
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func = unreachable_trap;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func = trap_invalid_conversion_to_integer;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func = trap_integer_divide_by_zero;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func = trap_integer_overflow;

                ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = call_bridge;
                ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = call_indirect_bridge;

                g_runtime.bridges_initialized.store(true, ::std::memory_order_release);
            }

            init_lock.clear(::std::memory_order_release);
        }

        inline void compile_all_modules_if_needed() noexcept
        {
            ensure_runtime_process_exit_handler_registered();
            ensure_bridges_initialized();
            if(g_runtime.compiled_all.load(::std::memory_order_acquire)) { return; }

            static ::std::atomic_flag compile_lock = ATOMIC_FLAG_INIT;
            while(compile_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.compiled_all.load(::std::memory_order_acquire)) { return; }
                ::fast_io::this_thread::yield();
            }

            if(g_runtime.compiled_all.load(::std::memory_order_relaxed))
            {
                compile_lock.clear(::std::memory_order_release);
                return;
            }

            ::fast_io::unix_timestamp start_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
#endif
            }

            [[maybe_unused]] constexpr auto runtime_compile_threads_warn{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8" (runtime-compile-threads)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};

            [[maybe_unused]] constexpr auto runtime_compile_threads_warn_to_fatal{
                []() constexpr noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(runtime-compile-threads)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }};

            constexpr auto runtime_compile_threads_verbose_info{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};

            using runtime_compile_threads_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t;

            auto effective_extra_compile_threads{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved};
            auto const runtime_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};
            auto const default_runtime_compile_threads_policy_active{!::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed ||
                                                                     runtime_compile_threads_policy == runtime_compile_threads_policy_t::default_policy};
            auto const aggressive_runtime_compile_threads_policy_active{::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed &&
                                                                        runtime_compile_threads_policy == runtime_compile_threads_policy_t::aggressive};
            auto const adaptive_runtime_compile_threads_policy_active{default_runtime_compile_threads_policy_active ||
                                                                      aggressive_runtime_compile_threads_policy_active};
            ::uwvm2::utils::container::u8string_view const adaptive_runtime_compile_threads_policy_name{
                aggressive_runtime_compile_threads_policy_active ? ::uwvm2::utils::container::u8string_view{u8"aggressive"}
                                                                 : ::uwvm2::utils::container::u8string_view{u8"default"}};
            auto const adaptive_target_task_groups_per_adjusted_compile_thread{
                aggressive_runtime_compile_threads_policy_active
                    ? ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::aggressive_target_task_groups_per_adjusted_compile_thread
                    : ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::default_target_task_groups_per_adjusted_compile_thread};
#ifndef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            if(effective_extra_compile_threads != 0uz)
            {
                if(::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                {
                    runtime_compile_threads_warn(
                        u8"Runtime compile threads resolved to ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        effective_extra_compile_threads,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8", but this platform does not provide fast_io::native_thread. Falling back to main-thread-only uwvm-int full translation.");

                    if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                }

                effective_extra_compile_threads = 0uz;
            }
#endif

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                if(effective_extra_compile_threads == 0uz)
                {
                    runtime_compile_threads_verbose_info(u8"UWVM Interpreter full translation will run on the main thread only. ");
                }
                else if(adaptive_runtime_compile_threads_policy_active)
                {
                    runtime_compile_threads_verbose_info(u8"UWVM Interpreter full translation will use up to ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads + 1uz,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" compile threads (main=1, extra-up-to=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
                else
                {
                    runtime_compile_threads_verbose_info(u8"UWVM Interpreter full translation will use ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads + 1uz,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" compile threads (main=1, extra=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
            }

            using runtime_scheduling_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t;
            using compile_task_split_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t;

            auto const runtime_scheduling_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_policy};
            auto const runtime_scheduling_size{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size};
            auto const runtime_scheduling_policy_existed{::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_existed};

            auto const compile_task_split_conf{
                runtime_scheduling_policy == runtime_scheduling_policy_t::function_count
                    ? ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config{.policy =
                                                                                                                 compile_task_split_policy_t::function_count,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 !runtime_scheduling_policy_existed}
                    : ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config{.policy = compile_task_split_policy_t::code_size,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 !runtime_scheduling_policy_existed}
            };

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                if(effective_extra_compile_threads == 0uz)
                {
                    if(::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_existed)
                    {
                        runtime_compile_threads_verbose_info(
                            u8"Runtime scheduling policy is configured but inactive because no extra runtime compile threads are enabled. ");
                    }
                }
            }

            // Assign module ids.
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
            g_import_call_cache.clear();

            auto const& rt_map{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage};
            g_runtime.modules.reserve(rt_map.size());
            g_runtime.module_name_to_id.reserve(rt_map.size());

            ::std::size_t id{};
            for(auto const& kv: rt_map)
            {
                g_runtime.module_name_to_id.emplace(kv.first, id);
                compiled_module_record rec{};
                rec.module_name = kv.first;
                rec.runtime_module = ::std::addressof(kv.second);
                g_runtime.modules.push_back(::std::move(rec));
                ++id;
            }

            g_runtime.defined_func_cache.resize(g_runtime.modules.size());

            constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t kTranslateOpt{get_curr_target_tranopt()};
            auto const compile_llvm_jit_translation{runtime_compiler_requests_llvm_jit_translation()};

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::uwvm2::utils::container::u8string_view const llvm_jit_translation_state{
                    compile_llvm_jit_translation ? ::uwvm2::utils::container::u8string_view{u8"enabled"}
                                                 : ::uwvm2::utils::container::u8string_view{u8"disabled"}};
                runtime_compile_threads_verbose_info(u8"Resolved runtime configuration: mode=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     describe_runtime_mode(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode),
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8", compiler=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     describe_runtime_compiler(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler),
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8", llvm-jit-translation=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color,
                                                                          compile_llvm_jit_translation ? UWVM_COLOR_U8_LT_GREEN : UWVM_COLOR_U8_YELLOW),
                                                     llvm_jit_translation_state,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8". ");

                if(compile_llvm_jit_translation)
                {
                    runtime_compile_threads_verbose_info(
                        u8"LLVM JIT translation artifacts will also be generated during full translation. ");
                }
            }

            // Compile modules and build function map.
            for(auto& rec: g_runtime.modules)
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option opt{};
                auto const it{g_runtime.module_name_to_id.find(rec.module_name)};
                if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                opt.curr_wasm_id = it->second;

                auto const thread_resolution_compile_task_split_conf{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_compile_task_split_config(*rec.runtime_module,
                                                                                                                             compile_task_split_conf,
                                                                                                                             effective_extra_compile_threads)};
                auto const runtime_scheduling_policy_adjusted_for_thread_resolution{
                    !runtime_scheduling_policy_existed && compile_task_split_conf.policy == compile_task_split_policy_t::code_size &&
                    thread_resolution_compile_task_split_conf.split_size != runtime_scheduling_size};
                auto effective_module_extra_compile_threads{effective_extra_compile_threads};
                if(adaptive_runtime_compile_threads_policy_active)
                {
                    effective_module_extra_compile_threads =
                        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_adaptive_extra_compile_threads(
                            *rec.runtime_module,
                            thread_resolution_compile_task_split_conf,
                            effective_extra_compile_threads,
                            adaptive_target_task_groups_per_adjusted_compile_thread,
                            runtime_scheduling_policy_adjusted_for_thread_resolution);
                }
                auto const effective_compile_task_split_conf{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_compile_task_split_config(
                        *rec.runtime_module,
                        compile_task_split_conf,
                        effective_module_extra_compile_threads)};
                auto const default_runtime_scheduling_policy_adjusted{!runtime_scheduling_policy_existed &&
                                                                      compile_task_split_conf.policy == compile_task_split_policy_t::code_size &&
                                                                      effective_compile_task_split_conf.split_size != runtime_scheduling_size};

                if(::uwvm2::uwvm::io::show_verbose && effective_extra_compile_threads != 0uz) [[unlikely]]
                {
                    if(effective_module_extra_compile_threads == 0uz)
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation will run on the main thread only. ");
                    }
                    else
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation will use ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads + 1uz,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" compile threads (main=1, extra=",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"). ");
                    }

                    if(effective_compile_task_split_conf.policy == compile_task_split_policy_t::function_count)
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation scheduling policy uses ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             u8"func_count",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" with ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" functions per task. ");
                    }
                    else
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation scheduling policy uses ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             u8"code_size",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" with ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" wasm code-body bytes per task. ");
                    }

                    if(adaptive_runtime_compile_threads_policy_active && effective_module_extra_compile_threads != effective_extra_compile_threads)
                    {
                        runtime_compile_threads_verbose_info(::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             adaptive_runtime_compile_threads_policy_name,
                                                             u8" runtime compile thread policy adjusted module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" extra compile threads from ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to avoid overscheduling a small module. ");
                    }

                    if(default_runtime_scheduling_policy_adjusted)
                    {
                        runtime_compile_threads_verbose_info(u8"Default runtime scheduling policy adjusted module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" code_size from ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             runtime_scheduling_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to reduce scheduling overhead for a small module. ");
                    }
                }

                ::uwvm2::validation::error::code_validation_error_impl err{};

#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        runtime_compile_threads_verbose_info(u8"Begin uwvm-int full translation for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\". ");
                    }
                    rec.compiled = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm<kTranslateOpt>(
                        *rec.runtime_module,
                        opt,
                        err,
                        effective_module_extra_compile_threads,
                        effective_compile_task_split_conf);

                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        runtime_compile_threads_verbose_info(u8"uwvm-int full translation finished for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\". ");
                    }

#if defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)
                    if(compile_llvm_jit_translation)
                    {
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            runtime_compile_threads_verbose_info(u8"Begin LLVM JIT translation for module \"",
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                                 rec.module_name,
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                                 u8"\". ");
                        }
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option llvm_jit_opt{};
                        llvm_jit_opt.curr_wasm_id = opt.curr_wasm_id;
                        rec.llvm_jit_compiled = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(
                            *rec.runtime_module,
                            llvm_jit_opt,
                            err,
                            effective_module_extra_compile_threads,
                            { .policy = static_cast<::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t>(
                                  static_cast<unsigned>(effective_compile_task_split_conf.policy)),
                              .split_size = effective_compile_task_split_conf.split_size,
                              .adjust_for_default_policy = effective_compile_task_split_conf.adjust_for_default_policy });
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            runtime_compile_threads_verbose_info(u8"LLVM JIT translation finished for module \"",
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                                 rec.module_name,
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                                 u8"\". ");
                        }
                    }
#endif
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    print_and_terminate_compile_validation_error(rec.module_name, err);
                }
#endif

                rec.type_canon_index = build_type_canon_index(*rec.runtime_module);

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                if(local_n != rec.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
#if defined(UWVM_RUNTIME_LLVM_JIT)
                if(compile_llvm_jit_translation && local_n != rec.llvm_jit_compiled.local_funcs.size())
                    [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                if(compile_llvm_jit_translation)
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        runtime_compile_threads_verbose_info(u8"Begin LLVM JIT materialization for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\". ");
                    }
                    if(!try_materialize_runtime_module_llvm_jit(rec)) [[unlikely]]
                    {
                        if(runtime_compiler_requires_llvm_jit_execution())
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"LLVM JIT materialization failed for module=\"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                rec.module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            u8"[warn]  ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"LLVM JIT materialization failed for module=\"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            rec.module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\"; falling back to interpreter execution for this module.\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    }
                    else if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        runtime_compile_threads_verbose_info(u8"LLVM JIT materialization finished for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\". ");
                    }
                }
#endif

                auto& mod_cache{g_runtime.defined_func_cache.index_unchecked(opt.curr_wasm_id)};
                mod_cache.clear();
                mod_cache.resize(local_n);

                if(local_n != 0uz)
                {
                    auto const base_ptr{rec.runtime_module->local_defined_function_vec_storage.data()};
                    if(base_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    ::std::uintptr_t const begin{reinterpret_cast<::std::uintptr_t>(base_ptr)};
                    constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
                    static_assert(elem_size != 0uz);
                    if(local_n > (::std::numeric_limits<::std::uintptr_t>::max() / elem_size)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uintptr_t const bytes{static_cast<::std::uintptr_t>(local_n * elem_size)};
                    if(begin > ::std::numeric_limits<::std::uintptr_t>::max() - bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                    g_runtime.defined_func_ptr_ranges.push_back(defined_func_ptr_range{begin, begin + bytes, opt.curr_wasm_id});
                }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    auto const runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
                    auto const compiled_func{::std::addressof(rec.compiled.local_funcs.index_unchecked(i))};

                    auto const sig{func_sig_from_defined(runtime_func)};
                    auto const param_bytes{total_abi_bytes(sig.params)};
                    auto const result_bytes{total_abi_bytes(sig.results)};
                    if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    mod_cache.index_unchecked(i) = compiled_defined_func_info{opt.curr_wasm_id,
                                                                              rec.runtime_module->imported_function_vec_storage.size() + i,
                                                                              runtime_func,
                                                                              compiled_func,
                                                                              param_bytes,
                                                                              result_bytes};
                }
            }

            if(!g_runtime.defined_func_ptr_ranges.empty())
            {
                ::std::sort(g_runtime.defined_func_ptr_ranges.begin(),
                            g_runtime.defined_func_ptr_ranges.end(),
                            [](defined_func_ptr_range const& a, defined_func_ptr_range const& b) constexpr noexcept { return a.begin < b.begin; });
            }

            // Build an O(1) dispatch table for imported calls, flattening any import-alias chains ahead of time.
            g_import_call_cache.resize(g_runtime.modules.size());
            for(::std::size_t mid{}; mid != g_runtime.modules.size(); ++mid)
            {
                auto const& rec{g_runtime.modules.index_unchecked(mid)};
                auto const rt{rec.runtime_module};
                if(rt == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_n{rt->imported_function_vec_storage.size()};
                auto& cache{g_import_call_cache.index_unchecked(mid)};
                cache.clear();
                cache.resize(import_n);

                for(::std::size_t i{}; i != import_n; ++i)
                {
                    auto const imp{::std::addressof(rt->imported_function_vec_storage.index_unchecked(i))};
                    auto const rf{resolve_func_from_import_assuming_initialized(imp)};

                    cached_import_target tgt{};
                    // Default to the import slot frame; for resolved wasm functions we overwrite with the final module/function index.
                    tgt.frame.module_id = mid;
                    tgt.frame.function_index = i;

                    switch(rf.k)
                    {
                        case resolved_func::kind::defined:
                        {
                            auto const* const info{find_defined_func_info(rf.u.defined_ptr)};
                            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = info->module_id;
                            tgt.frame.function_index = info->function_index;
                            tgt.sig = func_sig_from_defined(info->runtime_func);
                            tgt.param_bytes = info->param_bytes;
                            tgt.result_bytes = info->result_bytes;
                            tgt.u.defined.runtime_func = info->runtime_func;
                            tgt.u.defined.compiled_func = info->compiled_func;
                            break;
                        }
                        case resolved_func::kind::local_imported:
                        {
                            tgt.k = cached_import_target::kind::local_imported;
                            tgt.u.local_imported = rf.u.local_imported;
                            tgt.sig = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::dl:
                        {
                            tgt.k = cached_import_target::kind::dl;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::weak_symbol:
                        {
                            tgt.k = cached_import_target::kind::weak_symbol;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    cache.index_unchecked(i) = tgt;
                }
            }

            populate_llvm_jit_call_indirect_table_views();

            // finished
            ::fast_io::unix_timestamp end_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
#endif

                // verbose
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"UWVM Interperter full translation done. (time=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    end_time - start_time,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"s). ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            g_runtime.compiled_all.store(true, ::std::memory_order_release);
            compile_lock.clear(::std::memory_order_release);
        }

    }  // namespace

    void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config cfg) noexcept
    {
        compile_all_modules_if_needed();

        auto const it{g_runtime.module_name_to_id.find(main_module_name)};
        if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const main_id{it->second};

        auto const main_module{g_runtime.modules.index_unchecked(main_id).runtime_module};
        if(main_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        // Bind WASI Preview1 env to the main module's memory[0] before any guest-to-host call is made.
        bind_default_wasip1_memory(*main_module);
#endif

        auto const import_n{main_module->imported_function_vec_storage.size()};
        auto const total_n{import_n + main_module->local_defined_function_vec_storage.size()};
        if(cfg.entry_function_index >= total_n) [[unlikely]] { ::fast_io::fast_terminate(); }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        ::std::size_t llvm_jit_entry_module_id{main_id};
        ::std::size_t llvm_jit_entry_function_index{cfg.entry_function_index};
#endif

        // Allocate the exact host-call stack space required by the entry function signature.
        // Layout: [params...] then call; the callee pops params and pushes results.
        ::std::size_t param_bytes{};
        ::std::size_t result_bytes{};
        if(cfg.entry_function_index < import_n)
        {
            if(main_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& cache{g_import_call_cache.index_unchecked(main_id)};
            if(cfg.entry_function_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& tgt{cache.index_unchecked(cfg.entry_function_index)};

            // For VM entry, only allow imported functions that ultimately resolve to a wasm-defined function.
            if(tgt.k != cached_import_target::kind::defined) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function is imported but resolves to a non-wasm implementation (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            // We don't pass host arguments; require `() -> ()`.
            if(tgt.sig.params.size != 0uz || tgt.sig.results.size != 0uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function signature is not () -> () (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = tgt.param_bytes;
            result_bytes = tgt.result_bytes;
#if defined(UWVM_RUNTIME_LLVM_JIT)
            llvm_jit_entry_module_id = tgt.frame.module_id;
            llvm_jit_entry_function_index = tgt.frame.function_index;
#endif
        }
        else
        {
            auto const local_index{cfg.entry_function_index - import_n};
            if(main_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(main_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& entry_info{mod_cache.index_unchecked(local_index)};
            auto const* const expected_rt{::std::addressof(main_module->local_defined_function_vec_storage.index_unchecked(local_index))};
            if(entry_info.runtime_func != expected_rt) [[unlikely]] { ::fast_io::fast_terminate(); }

            // We don't pass host arguments; require `() -> ()`.
            if(entry_info.param_bytes != 0uz || entry_info.result_bytes != 0uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function signature is not () -> () (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = entry_info.param_bytes;
            result_bytes = entry_info.result_bytes;
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        if(runtime_compiler_requests_llvm_jit_translation())
        {
            if(try_invoke_runtime_llvm_jit_defined_entry(llvm_jit_entry_module_id, llvm_jit_entry_function_index))
            {
                erase_current_thread_state();
                return;
            }

            if(runtime_compiler_requires_llvm_jit_execution()) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"LLVM JIT entry execution is unavailable for module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\", func_idx=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    cfg.entry_function_index,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT entry execution is unavailable for module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                main_module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", func_idx=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                cfg.entry_function_index,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"; falling back to interpreter execution.\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }
#endif

        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - result_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const stack_bytes{param_bytes + result_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);
        ::std::byte* stack_top_ptr{host_stack_base + param_bytes};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
#endif

        // Currently only main-thread execution exists. Clean up current thread state on exit to avoid state growth and
        // possible thread-id reuse issues. Do NOT `clear()` here: main-thread exit does not imply other threads exit.
        erase_current_thread_state();
    }

    template <typename TrailerWriter, typename InvokeBridge>
    inline void llvm_jit_invoke_raw_host_bridge_common(void const* runtime_module_ptr,
                                                       void* result_buffer,
                                                       ::std::size_t result_bytes,
                                                       void const* param_buffer,
                                                       ::std::size_t param_bytes,
                                                       ::std::size_t trailer_bytes,
                                                       TrailerWriter&& trailer_writer,
                                                       InvokeBridge&& invoke_bridge) noexcept
    {
        compile_all_modules_if_needed();
        ensure_bridges_initialized();

        if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr)) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto const* const runtime_module_storage_ptr{static_cast<runtime_module_storage_t const*>(runtime_module_ptr)};
        auto const wasm_module_id{find_runtime_module_id_from_storage_ptr(runtime_module_storage_ptr)};
        if(wasm_module_id == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - trailer_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const input_bytes{param_bytes + trailer_bytes};
        auto const stack_bytes{input_bytes < result_bytes ? result_bytes : input_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);

        if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, param_buffer, param_bytes); }
        if(trailer_bytes != 0uz) { trailer_writer(host_stack_base + param_bytes); }

        ::std::byte* stack_top_ptr{host_stack_base + input_bytes};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            invoke_bridge(wasm_module_id, ::std::addressof(stack_top_ptr));
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
#endif

        if(result_bytes != 0uz) { ::std::memcpy(result_buffer, host_stack_base, result_bytes); }
    }

    void llvm_jit_call_raw_host_api(void const* runtime_module_ptr,
                                    ::std::uint_least32_t func_index,
                                    void* result_buffer,
                                    ::std::size_t result_bytes,
                                    void const* param_buffer,
                                    ::std::size_t param_bytes) noexcept
    {
        llvm_jit_invoke_raw_host_bridge_common(runtime_module_ptr,
                                              result_buffer,
                                              result_bytes,
                                              param_buffer,
                                              param_bytes,
                                              0uz,
                                              [](::std::byte*) noexcept {},
                                              [&](::std::size_t wasm_module_id, ::std::byte** stack_top_ptr)
                                              { call_bridge(wasm_module_id, func_index, stack_top_ptr); });
    }

    [[nodiscard]] ::std::size_t preload_memory_descriptor_count_host_api() noexcept { return preload_memory_descriptor_count_impl(); }

    [[nodiscard]] bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index, preload_memory_descriptor_t* out) noexcept
    { return preload_memory_descriptor_at_impl(descriptor_index, out); }

    [[nodiscard]] bool preload_memory_read_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept
    { return preload_memory_read_impl(memory_index, offset, destination, size); }

    [[nodiscard]] bool preload_memory_write_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept
    { return preload_memory_write_impl(memory_index, offset, source, size); }

}  // namespace uwvm2::runtime::lib

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
