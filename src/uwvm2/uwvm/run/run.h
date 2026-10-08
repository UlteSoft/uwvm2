/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        run.h
 * @brief       Top-level UWVM execution driver and runtime-entry preparation helpers.
 * @details     This header owns the final handoff from the command-line/loader layer to the executable
 *              runtime backend.  It performs the remaining orchestration steps that must happen after
 *              all command-line options have been parsed and after the initial module paths have been
 *              recorded:
 *
 *              - load the executable, local, and weak-symbol WebAssembly modules;
 *              - build the process-wide module registry and reject duplicate module names;
 *              - serve non-executing modes such as section-detail printing and parser validation;
 *              - validate cross-module imports and initialize runtime storage;
 *              - resolve the entry function, including the explicit `--wasm-set-start-func` override;
 *              - resolve the runtime compile-thread policy into the value consumed by runtime compilers;
 *              - dispatch to lazy compilation, lazy compilation with eager validation, or full compilation.
 *
 *              The helper functions in this file intentionally keep the interface between the UWVM
 *              front-end and the runtime library narrow: the runtime receives an import-inclusive wasm
 *              function index plus optional raw ABI buffers for entry parameters and results.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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

#pragma once
#include <uwvm2/runtime/lib/uwvm_runtime_posix_abi.h>

#ifndef UWVM_MODULE
// std
# include <bit>
# include <array>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <cstdlib>
# include <cstdio>
# include <limits>
# include <memory>
# include <span>
# include <type_traits>
# include <utility>
# include <vector>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <thread>
# endif
// platform
# include <uwvm2/runtime/lib/uwvm_runtime.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/control/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/impl.h>
# include <uwvm2/uwvm/debugger/impl.h>
# if defined(__unix__) || defined(__APPLE__)
#  include <unistd.h>
# endif
# if defined(__linux__)
#  include <sys/socket.h>
#  include <sys/syscall.h>
# endif
# include "retval.h"
# include "loader.h"
# include "owned_source.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
#if defined(UWVM_RUNTIME_HAS_BACKEND)
    /**
     * @brief   Resolve the default entry function for the main module.
     * @details The returned value is the import-inclusive WebAssembly function index used by the runtime
     *          storage layer.  Resolution is deliberately conservative because the default entry path
     *          does not provide host-side arguments or inspect host-import ABIs.
     *
     *          Resolution order:
     *          1. the WebAssembly start section, when present;
     *          2. an exported function named `_start` from `all_module_export`;
     *          3. an exported function named `main` from `all_module_export`;
     *          4. the parsed export section directly, again checking `_start` before `main`.
     *
     *          The parsed-export fallback protects the runtime from stale or absent export-map storage.
     *          Every candidate must resolve to a `() -> ()` wasm function.  Imported candidates are accepted
     *          only when the import chain ultimately points at a wasm-defined function; host-defined import
     *          leaves are rejected because the default runtime-entry ABI here is wasm-function based.
     *
     * @param   main_module_name Name key of the executable module in the global wasm/runtime registries.
     * @return  Import-inclusive wasm function index for the default entry function.
     * @warning This function does not return on failure.  It emits a fatal diagnostic and terminates when no
     *          valid default entry function can be found.
     */
    inline constexpr ::std::size_t resolve_default_first_entry_function_index(::uwvm2::utils::container::u8string_view main_module_name) noexcept
    {
        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
        using start_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t;
        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
        using imported_function_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
        using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

        // We currently do not provide host arguments to the default entry function; therefore, the entry must be `() -> ()`.
        // The wasm 1.0 start section already has that signature requirement by specification, and the exported fallbacks
        // intentionally use the same rule so all default-entry paths share one runtime ABI.
        // Default-entry resolution runs after runtime initialization, which has already rejected import-alias cycles.
        // Use the initialized runtime storage size as the walk bound so deeply re-exported but valid entry functions are
        // still discoverable while corrupted alias chains cannot loop forever.
        auto const import_link_walk_bound{[]() constexpr noexcept -> ::std::size_t
                                          {
                                              ::std::size_t bound{};
                                              for(auto const& module_entry: ::uwvm2::uwvm::runtime::storage::active_runtime_registry())
                                              {
                                                  auto const imported_function_count{module_entry.second.imported_function_vec_storage.size()};
                                                  if(imported_function_count > ::std::numeric_limits<::std::size_t>::max() - bound)
                                                  {
                                                      return ::std::numeric_limits<::std::size_t>::max();
                                                  }
                                                  bound += imported_function_count;
                                              }
                                              return bound;
                                          }()};

        auto const resolve_import_leaf{[import_link_walk_bound](imported_function_storage_t const* imp) constexpr noexcept -> imported_function_storage_t const*
                                       {
                                           for(::std::size_t steps{};; ++steps)
                                           {
                                               // Exceeding the storage-derived bound means the chain is no longer the acyclic structure the initializer proved.
                                               if(steps > import_link_walk_bound || imp == nullptr) [[unlikely]] { return nullptr; }
                                               if(imp->link_kind != func_link_kind::imported) { return imp; }
                                               imp = imp->target.imported_ptr;
                                           }
                                           return nullptr;
                                       }};

        // Validate an import-inclusive function index against runtime type storage.  For imported functions, inspect
        // the resolved wasm-defined leaf because the import entry itself may only be a forwarding slot.
        auto const is_void_to_void_wasm_func_index{[&](::std::size_t func_index) constexpr noexcept -> bool
                                                   {
                                                       auto const rt_it{::uwvm2::uwvm::runtime::storage::active_runtime_registry().find(main_module_name)};
                                                       if(rt_it == ::uwvm2::uwvm::runtime::storage::active_runtime_registry().end()) [[unlikely]]
                                                       {
                                                           return false;
                                                       }

                                                       auto const& rt{rt_it->second};
                                                       auto const import_n{rt.imported_function_vec_storage.size()};
                                                       auto const local_n{rt.local_defined_function_vec_storage.size()};
                                                       if(func_index >= import_n + local_n) [[unlikely]] { return false; }

                                                       auto const is_void_to_void_ft{
                                                           [](auto const& ft) constexpr noexcept -> bool
                                                           { return ft.parameter.begin == ft.parameter.end && ft.result.begin == ft.result.end; }};

                                                       if(func_index < import_n)
                                                       {
                                                           auto const imp{::std::addressof(rt.imported_function_vec_storage.index_unchecked(func_index))};
                                                           auto const leaf{resolve_import_leaf(imp)};
                                                           if(leaf == nullptr) [[unlikely]] { return false; }

                                                           // Allow imported entry only when it ultimately resolves to a wasm-defined function.
                                                           if(leaf->link_kind != func_link_kind::defined) { return false; }
                                                           auto const f{leaf->target.defined_ptr};
                                                           if(f == nullptr || f->function_type_ptr == nullptr) [[unlikely]] { return false; }
                                                           return is_void_to_void_ft(*f->function_type_ptr);
                                                       }

                                                       auto const local_idx{func_index - import_n};
                                                       auto const f{::std::addressof(rt.local_defined_function_vec_storage.index_unchecked(local_idx))};
                                                       if(f == nullptr || f->function_type_ptr == nullptr) [[unlikely]] { return false; }
                                                       return is_void_to_void_ft(*f->function_type_ptr);
                                                   }};

        // Prefer the start section when present.  The parser stores the optional section as a span, so presence must
        // be tested with the parser's non-null `sec_begin` sentinel rather than by subtracting span pointers.
        auto const all_module_it{::uwvm2::uwvm::wasm::storage::all_module.find(main_module_name)};
        if(all_module_it != ::uwvm2::uwvm::wasm::storage::all_module.end())
        {
            auto const& am{all_module_it->second};
            if(am.type == module_type_t::exec_wasm || am.type == module_type_t::preloaded_wasm)
            {
                auto const wf{am.module_storage_ptr.wf};
                if(wf != nullptr && wf->binfmt_ver == 1u)
                {
                    auto const& mod{wf->wasm_module_storage.wasm_binfmt_ver1_storage};
                    auto const& startsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_t>(mod.sections)};

                    // Note: do not subtract pointers here; the default (absent) span is {nullptr, nullptr} and pointer
                    // subtraction would be UB. `sec_begin != nullptr` is the parser's "section present" flag.
                    if(startsec.sec_span.sec_begin != nullptr)
                    {
                        if(auto const rt_it{::uwvm2::uwvm::runtime::storage::active_runtime_registry().find(main_module_name)};
                           rt_it != ::uwvm2::uwvm::runtime::storage::active_runtime_registry().end())
                        {
                            auto const import_n{rt_it->second.imported_function_vec_storage.size()};
                            auto const idx{static_cast<::std::size_t>(startsec.start_idx)};
                            auto const total_n{import_n + rt_it->second.local_defined_function_vec_storage.size()};
                            // The parser validated the start signature. Unlike conventional CLI exports,
                            // a standard start section may also designate an imported host function.
                            if(idx < total_n) { return idx; }
                        }
                    }
                }

                static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            }
        }

        // Otherwise, fall back to conventional exported entrypoints from the export map.  `_start` follows WASI and
        // common standalone-wasm conventions; `main` is retained as a compatibility fallback.
        ::std::size_t idx{};
        auto const mit{::uwvm2::uwvm::wasm::storage::all_module_export.find(main_module_name)};
        if(mit != ::uwvm2::uwvm::wasm::storage::all_module_export.end())
        {
            auto const try_export{[&](::uwvm2::utils::container::u8string_view export_name) constexpr noexcept -> bool
                                  {
                                      auto const eit{mit->second.find(export_name)};
                                      if(eit == mit->second.end()) { return false; }

                                      auto const& ex{eit->second};
                                      if(ex.type != module_type_t::exec_wasm && ex.type != module_type_t::preloaded_wasm) { return false; }

                                      auto const exp{ex.storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                                      if(exp == nullptr || exp->type != external_types::func) { return false; }

                                      auto const resolved{static_cast<::std::size_t>(exp->storage.func_idx)};
                                      auto const rt_it{::uwvm2::uwvm::runtime::storage::active_runtime_registry().find(main_module_name)};
                                      if(rt_it == ::uwvm2::uwvm::runtime::storage::active_runtime_registry().end()) [[unlikely]] { return false; }
                                      auto const import_n{rt_it->second.imported_function_vec_storage.size()};
                                      auto const total_n{import_n + rt_it->second.local_defined_function_vec_storage.size()};
                                      if(resolved >= total_n) { return false; }
                                      if(!is_void_to_void_wasm_func_index(resolved)) { return false; }

                                      idx = resolved;
                                      return true;
                                  }};

            if(try_export(::uwvm2::utils::container::u8string_view{u8"_start"})) { return idx; }
            if(try_export(::uwvm2::utils::container::u8string_view{u8"main"})) { return idx; }
        }

        // Fallback: if `all_module_export` is missing/stale, resolve from the parsed export section directly.
        // This avoids relying on a separately-constructed export map for entrypoint resolution.
        if(all_module_it != ::uwvm2::uwvm::wasm::storage::all_module.end())
        {
            auto const& am{all_module_it->second};
            if(am.type == module_type_t::exec_wasm || am.type == module_type_t::preloaded_wasm)
            {
                auto const wf{am.module_storage_ptr.wf};
                if(wf != nullptr && wf->binfmt_ver == 1u)
                {
                    auto const& mod{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

                    auto get_exportsec_from_feature_tuple{[&mod]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(
                                                              ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept -> decltype(auto)
                                                          {
                                                              return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                                                  ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>(
                                                                  mod.sections);
                                                          }};

                    auto const& exportsec{get_exportsec_from_feature_tuple(::uwvm2::uwvm::wasm::feature::all_features)};
                    if(exportsec.sec_span.sec_begin != nullptr)
                    {
                        auto const try_export_from_section{[&](::uwvm2::utils::container::u8string_view export_name) constexpr noexcept -> bool
                                                           {
                                                               for(auto const& e: exportsec.exports)
                                                               {
                                                                   if(e.export_name != export_name) { continue; }
                                                                   if(e.exports.type != external_types::func) { return false; }

                                                                   auto const resolved{static_cast<::std::size_t>(e.exports.storage.func_idx)};
                                                                   if(!is_void_to_void_wasm_func_index(resolved)) { return false; }
                                                                   idx = resolved;
                                                                   return true;
                                                               }
                                                               return false;
                                                           }};

                        if(try_export_from_section(::uwvm2::utils::container::u8string_view{u8"_start"})) { return idx; }
                        if(try_export_from_section(::uwvm2::utils::container::u8string_view{u8"main"})) { return idx; }
                    }
                }

                static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            }
        }

        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"Cannot resolve entry function for module=\"",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            main_module_name,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"\": expected start section or exported function \"_start\"/\"main\" with signature () -> ().\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    /**
     * @brief Runtime-entry call descriptor passed from the UWVM front-end to the runtime library.
     * @details The function index is always import-inclusive.  `param_buffer` and `result_buffer` are raw,
     *          tightly-packed host-native object-representation byte buffers containing only scalar wasm entry values
     *          that this layer currently supports (`i32`, `i64`, `f32`, and `f64`).  These are not WebAssembly
     *          linear-memory encodings, so no little-endian normalization is performed here.  Empty vectors represent
     *          an absent ABI buffer and are converted to null pointers before calling runtime library APIs.
     */
    struct runtime_entry_invocation
    {
        /// Import-inclusive wasm function index selected as the entry point.
        ::std::size_t function_index{};

        /// Packed entry-parameter bytes.  The runtime library interprets this according to the selected function type.
        ::uwvm2::utils::container::vector<::std::byte> param_buffer{};

        /// Packed result storage bytes.  The runtime writes entry results here when the selected entry has results.
        ::uwvm2::utils::container::vector<::std::byte> result_buffer{};
    };

    /**
     * @brief Normalize a wasm value-type enum value into a compact integer code.
     * @details Parser feature sets use related enum types for different wasm versions.  Converting through an integer
     *          keeps later helpers independent of the concrete enum type while preserving the binary value-type code.
     */
    [[nodiscard]] inline constexpr ::std::uint_least8_t wasm_entry_type_code(auto type) noexcept { return static_cast<::std::uint_least8_t>(type); }

    /**
     * @brief   Return the scalar ABI byte width for a supported entry value type.
     * @param   code WebAssembly value-type code.
     * @return  4 for `i32`/`f32`, 8 for `i64`/`f64`, or 0 for unsupported/non-scalar entry types.
     */
    [[nodiscard]] inline constexpr ::std::size_t wasm_entry_scalar_abi_size(::std::uint_least8_t code) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(code))
        {
            case wasm_value_type::i32: return 4uz;
            case wasm_value_type::i64: return 8uz;
            case wasm_value_type::f32: return 4uz;
            case wasm_value_type::f64: return 8uz;
            default: return 0uz;
        }
    }

    /**
     * @brief   Convert a wasm value-type code to a short diagnostic name.
     * @details Names are used only for user-facing diagnostics and verbose logs.  Unsupported reference/vector types
     *          are still named when their value-type codes are known so fatal messages can identify the rejected type.
     * @param   code WebAssembly value-type code.
     * @return  Static UTF-8 string view naming the value type.
     */
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view wasm_entry_type_name(::std::uint_least8_t code) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(code))
        {
            case wasm_value_type::i32: return {u8"i32"};
            case wasm_value_type::i64: return {u8"i64"};
            case wasm_value_type::f32: return {u8"f32"};
            case wasm_value_type::f64: return {u8"f64"};
            default:
            {
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) { return {u8"v128"}; }
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)) { return {u8"funcref"}; }
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref)) { return {u8"externref"}; }
                return {u8"unknown"};
            }
        }
    }

    /**
     * @brief   Run a fast_io scanner and require it to consume the whole input range.
     * @details Entry argument parsing must reject partial literals such as `123abc`.  This helper centralizes the
     *          "parse succeeded and stopped exactly at `last`" check for integer and floating-point scanners.
     */
    template <typename ScanManipulator>
    [[nodiscard]] inline constexpr bool wasm_entry_scan_exact(char8_t const* first, char8_t const* last, ScanManipulator scan_manipulator) noexcept
    {
        auto const [next, err]{::fast_io::parse_by_scan(first, last, scan_manipulator)};
        return err == ::fast_io::parse_code::ok && next == last;
    }

    /**
     * @brief   Parse an integer entry argument into the exact wasm integer storage type.
     * @details Accepted forms are signed decimal when `allow_signed_decimal` is true, plus unsigned decimal, hexadecimal,
     *          binary, and octal forms.  Unsigned forms are bit-cast into the signed wasm storage type, allowing users to
     *          express raw two's-complement bit patterns without relying on implementation-defined signed overflow.
     *
     * @tparam  allow_signed_decimal Whether to first accept a signed decimal literal.
     * @tparam  Out                  Destination integer type used by the wasm runtime ABI.
     * @tparam  Unsigned             Unsigned integer type with the same width as `Out`.
     * @param   str                  User-provided UTF-8 argument token.
     * @param   out                  Receives the parsed value on success.
     * @return  true when the whole token was parsed successfully, false otherwise.
     */
    template <bool allow_signed_decimal, typename Out, typename Unsigned>
    [[nodiscard]] inline constexpr bool parse_wasm_entry_integer(::uwvm2::utils::container::u8string_view str, Out & out) noexcept
    {
        static_assert(sizeof(Out) == sizeof(Unsigned));
        auto const first{str.cbegin()};
        auto const last{str.cend()};
        if(first == last) [[unlikely]] { return false; }

        // Scan in the order users normally write entry arguments: signed decimal first, then unsigned decimal, followed
        // by explicit-base unsigned forms (`0x`, `0b`, `0o`).  Bare leading-zero octal spellings such as `0777` are
        // intentionally rejected; callers must use `0o777` when they want octal.  There is no need to pre-check prefixes
        // here because fast_io scanners fail quickly on non-matching input, and `wasm_entry_scan_exact` rejects partial
        // matches after each attempted scan.
        //
        // fast_io scanners write to the target only on a successful scan.  Value-initialize the temporary nonetheless:
        // this keeps the successful short-circuit path visibly free of indeterminate reads for conservative compiler
        // data-flow analysis and provides a deterministic defensive value if a future scanner contract regresses.

        if constexpr(allow_signed_decimal)
        {
            Out signed_value{};
            if(wasm_entry_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(signed_value)))
            {
                out = signed_value;
                return true;
            }
        }

        Unsigned unsigned_value{};
        if(wasm_entry_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::hex0x_get<true, false>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::bin0b_get<true, false>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::oct0o_get<true, false>(unsigned_value)))
        {
            out = ::std::bit_cast<Out>(unsigned_value);
            return true;
        }

        return false;
    }

    /**
     * @brief Emit a fatal `--wasm-set-start-func` diagnostic and terminate.
     * @details This helper keeps all explicit-entry fatal paths formatted consistently and guarantees that callers do
     *          not continue after an invalid user-specified entry function or entry argument.
     */
    template <typename... Args>
    [[noreturn]] inline constexpr void wasm_set_start_func_fatal(Args && ... args) noexcept
    {
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            ::std::forward<Args>(args)...,
                            u8"\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    /**
     * @brief Parse an entry argument as a wasm `i32` storage value.
     */
    [[nodiscard]] inline constexpr bool parse_wasm_entry_i32(::uwvm2::utils::container::u8string_view str,
                                                             ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 & out) noexcept
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        static_assert(sizeof(wasm_i32) == 4uz && sizeof(wasm_u32) == 4uz);
        return parse_wasm_entry_integer<true, wasm_i32, wasm_u32>(str, out);
    }

    /**
     * @brief Parse an entry argument as a wasm `i64` storage value.
     */
    [[nodiscard]] inline constexpr bool parse_wasm_entry_i64(::uwvm2::utils::container::u8string_view str,
                                                             ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 & out) noexcept
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        static_assert(sizeof(wasm_i64) == 8uz && sizeof(wasm_u64) == 8uz);
        return parse_wasm_entry_integer<true, wasm_i64, wasm_u64>(str, out);
    }

    /**
     * @brief   Parse a floating-point literal from a half-open UTF-8 range.
     * @details Decimal and hexfloat forms are scanned by fast_io.  In every case, partial consumption is rejected.
     */
    template <typename T>
    [[nodiscard]] inline constexpr bool parse_wasm_entry_float_range(char8_t const* first, char8_t const* last, T& out) noexcept
    {
        if(first == last) { return false; }

        // Scan ordinary floating-point syntax.
        auto const [next, err]{::fast_io::parse_by_scan(first, last, out)};
        if(err == ::fast_io::parse_code::ok && next == last) { return true; }

        // Scan binary floating-point syntax, which must use the 0x-prefixed hexfloat form.
        auto const [hex_next, hex_err]{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::hexfloat0x_get<true>(out))};
        if(hex_err == ::fast_io::parse_code::ok && hex_next == last) { return true; }

        return false;
    }

    /**
     * @brief   Classify an input literal for arity-mismatch diagnostics.
     * @details The classification is intentionally descriptive rather than exact.  For example, a literal that fits
     *          `i32` also fits `i64`, so it is reported as `i32/i64 literal`; similarly, many `f32` literals can be
     *          represented as `f64`.
     */
    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view wasm_entry_input_literal_type(::uwvm2::utils::container::u8string_view str) noexcept
    {
        // These are parser sinks used only to classify the token; the parsed values are never read.

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32_value;  // no init necessary
        if(parse_wasm_entry_i32(str, i32_value)) { return {u8"i32/i64 literal"}; }

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64_value;  // no init necessary
        if(parse_wasm_entry_i64(str, i64_value)) { return {u8"i64 literal"}; }

        float f32_value;  // no init necessary
        if(parse_wasm_entry_float_range(str.cbegin(), str.cend(), f32_value)) { return {u8"f32/f64 literal"}; }

        double f64_value;  // no init necessary
        if(parse_wasm_entry_float_range(str.cbegin(), str.cend(), f64_value)) { return {u8"f64 literal"}; }

        return {u8"unknown literal"};
    }

    /**
     * @brief Print a wasm function type span as `(type, type, ...)`.
     * @details Used in diagnostics where a full function signature is more helpful than only a raw arity count.
     */
    template <typename Output, typename ValueTypePtr>
    inline constexpr void print_wasm_entry_type_span(Output & output, ValueTypePtr begin, ValueTypePtr end) noexcept
    {
        ::fast_io::io::perr(output, u8"(");
        bool first{true};
        for(auto curr{begin}; curr != end; ++curr)
        {
            if(!first) { ::fast_io::io::perr(output, u8", "); }
            first = false;
            ::fast_io::io::perr(output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                wasm_entry_type_name(wasm_entry_type_code(*curr)),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
        }
        ::fast_io::io::perr(output, u8")");
    }

    /**
     * @brief Print the standard UWVM informational prefix to an existing output stream.
     * @details This helper is used while a multi-line diagnostic already holds the output-stream lock, so it accepts
     *          the caller's stream reference rather than reacquiring the global logger.
     */
    template <typename Output>
    inline constexpr void print_wasm_entry_info_prefix(Output & output) noexcept
    {
        ::fast_io::io::perr(output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
    }

    /**
     * @brief Emit a detailed fatal diagnostic for explicit-entry arity mismatch.
     * @details The diagnostic prints the selected function signature and a best-effort classification of every supplied
     *          token.  The entire multi-line record is emitted while holding the logger lock to avoid interleaving with
     *          other verbose/runtime output.
     */
    template <typename FunctionType, typename Tokens>
    [[noreturn]] inline constexpr void wasm_set_start_func_arity_mismatch_fatal(::std::uint32_t local_function_index,
                                                                                FunctionType const& ft,
                                                                                Tokens const& argument_tokens) noexcept
    {
        auto const param_count{static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)};
        auto const argument_token_count{argument_tokens.size()};

        // Emit this multi-part diagnostic under one stream lock, then write through the unlocked stream reference.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        ::fast_io::io::perr(u8log_output_ul,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"--wasm-set-start-func argument count mismatch for local function ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            local_function_index,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8": expected ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            param_count,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8" argument(s), got ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            argument_token_count,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8".\n");
        print_wasm_entry_info_prefix(u8log_output_ul);
        ::fast_io::io::perr(u8log_output_ul, u8"function type: ");
        print_wasm_entry_type_span(u8log_output_ul, ft.parameter.begin, ft.parameter.end);
        ::fast_io::io::perr(u8log_output_ul, u8" -> ");
        print_wasm_entry_type_span(u8log_output_ul, ft.result.begin, ft.result.end);
        ::fast_io::io::perr(u8log_output_ul, u8"\n");
        print_wasm_entry_info_prefix(u8log_output_ul);
        ::fast_io::io::perr(u8log_output_ul, u8"input types:   (");
        for(::std::size_t i{}; i != argument_token_count; ++i)
        {
            if(i != 0uz) { ::fast_io::io::perr(u8log_output_ul, u8", "); }
            // The loop bound is exactly `argument_token_count`, so this unchecked lookup is range-proven here.
            auto const arg{argument_tokens.index_unchecked(i)};
            ::fast_io::io::perr(u8log_output_ul,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                wasm_entry_input_literal_type(arg),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8" ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                u8"\"",
                                arg,
                                u8"\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
        }
        ::fast_io::io::perr(u8log_output_ul, u8")\n\n", ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    /**
     * @brief   Append a trivially-copyable wasm ABI value into an entry buffer.
     * @details Values are copied byte-for-byte into a compact host-native ABI buffer.  The buffer layout intentionally
     *          avoids alignment assumptions because it crosses the front-end/runtime-library boundary as `std::byte*`
     *          plus byte length.  Bounds failures indicate an internal size-calculation bug and terminate immediately.
     */
    template <typename T>
    inline constexpr void write_wasm_entry_value(::uwvm2::utils::container::vector<::std::byte> & buffer, ::std::size_t& offset, T const& value) noexcept
    {
        static_assert(::std::is_trivially_copyable_v<T>);
        if(offset > buffer.size() || sizeof(T) > buffer.size() - offset) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::memcpy(buffer.data() + offset, ::std::addressof(value), sizeof(T));
        offset += sizeof(T);
    }

    /**
     * @brief   Read a trivially-copyable wasm ABI value from an entry buffer.
     * @details This is the inverse of `write_wasm_entry_value` and is used only for verbose diagnostics after packing.
     *          It uses `memcpy` so the diagnostic path does not impose alignment requirements on the packed ABI buffer.
     */
    template <typename T>
    [[nodiscard]] inline constexpr T read_wasm_entry_value(::uwvm2::utils::container::vector<::std::byte> const& buffer, ::std::size_t& offset) noexcept
    {
        static_assert(::std::is_trivially_copyable_v<T>);
        if(offset > buffer.size() || sizeof(T) > buffer.size() - offset) [[unlikely]] { ::fast_io::fast_terminate(); }
        T value{};
        ::std::memcpy(::std::addressof(value), buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    /**
     * @brief   Parse and pack one explicit entry argument.
     * @details The selected function type drives the parser.  Only scalar wasm value types currently have a command-line
     *          representation in this layer.  Unsupported types such as `v128`, `funcref`, and `externref` are rejected
     *          before the runtime call is configured.
     *
     * @param   buffer    Destination ABI byte buffer.
     * @param   offset    Current byte offset; advanced by the encoded scalar width on success.
     * @param   arg       User-provided argument token.
     * @param   type_code Expected wasm value-type code.
     * @param   arg_index Zero-based argument index used in diagnostics.
     */
    inline constexpr void pack_wasm_entry_argument(::uwvm2::utils::container::vector<::std::byte> & buffer,
                                                   ::std::size_t& offset,
                                                   ::uwvm2::utils::container::u8string_view arg,
                                                   ::std::uint_least8_t type_code,
                                                   ::std::size_t arg_index) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(type_code))
        {
            case wasm_value_type::i32:
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 value{};
                if(!parse_wasm_entry_i32(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected i32, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::i64:
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 value{};
                if(!parse_wasm_entry_i64(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected i64, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::f32:
            {
                float value{};
                if(!parse_wasm_entry_float_range(arg.cbegin(), arg.cend(), value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected f32, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::f64:
            {
                double value{};
                if(!parse_wasm_entry_float_range(arg.cbegin(), arg.cend(), value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected f64, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            default:
            {
                wasm_set_start_func_fatal(u8"Unsupported --wasm-set-start-func parameter type: ",
                                          wasm_entry_type_name(type_code),
                                          u8". Only i32, i64, f32, and f64 are currently supported.");
            }
        }
    }

    /**
     * @brief   Print one integer entry value in several debugging-friendly bases.
     * @details Both the signed value and the raw unsigned bit pattern are displayed.  This is important for wasm integer
     *          arguments because command-line users may intentionally pass a raw bit pattern whose signed decimal spelling
     *          is not the most useful representation.
     */
    template <typename Output, typename Signed, typename Unsigned>
    inline constexpr void print_wasm_entry_integer_formats(Output & output, Signed value) noexcept
    {
        static_assert(sizeof(Signed) == sizeof(Unsigned));
        auto const bits{::std::bit_cast<Unsigned>(value)};
        ::fast_io::io::perr(output,
                            u8"bin=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::bin0b(bits),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8", oct=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::oct0o(bits),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8", sdec=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::dec(value),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8", udec=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::dec(bits),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8", hex=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::hex0x(bits),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
    }

    /**
     * @brief   Print a verbose record for one packed explicit-entry argument.
     * @details The function re-reads the packed bytes rather than reparsing the original token, so the verbose output
     *          reports exactly what will be handed to the runtime entry ABI.  For floating-point values it prints both
     *          numeric and raw-bit forms.
     */
    template <typename Output>
    inline constexpr void print_wasm_entry_argument_verbose(Output & output,
                                                            ::uwvm2::utils::container::vector<::std::byte> const& buffer,
                                                            ::std::size_t& offset,
                                                            ::uwvm2::utils::container::u8string_view arg,
                                                            ::std::uint_least8_t type_code,
                                                            ::std::size_t arg_index) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

        ::fast_io::io::perr(output,
                            u8"#",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            arg_index,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8" ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            wasm_entry_type_name(type_code),
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8" input=\"",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                            arg,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"\" => ");

        switch(static_cast<wasm_value_type>(type_code))
        {
            case wasm_value_type::i32:
            {
                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
                auto const value{read_wasm_entry_value<wasm_i32>(buffer, offset)};
                print_wasm_entry_integer_formats<Output, wasm_i32, wasm_u32>(output, value);
                break;
            }
            case wasm_value_type::i64:
            {
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
                auto const value{read_wasm_entry_value<wasm_i64>(buffer, offset)};
                print_wasm_entry_integer_formats<Output, wasm_i64, wasm_u64>(output, value);
                break;
            }
            case wasm_value_type::f32:
            {
                auto const value{read_wasm_entry_value<float>(buffer, offset)};
                ::std::uint32_t bits{};
                ::std::memcpy(::std::addressof(bits), ::std::addressof(value), sizeof(bits));
                ::fast_io::io::perr(output,
                                    u8"value=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::general(value),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", hexfloat=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hexfloat0x(value),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", bitfloat(hex)=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hex0x</*full=*/true>(bits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
                break;
            }
            case wasm_value_type::f64:
            {
                auto const value{read_wasm_entry_value<double>(buffer, offset)};
                ::std::uint64_t bits{};
                ::std::memcpy(::std::addressof(bits), ::std::addressof(value), sizeof(bits));
                ::fast_io::io::perr(output,
                                    u8"value=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::general(value),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", hexfloat=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hexfloat0x(value),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", bitfloat(hex)=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hex0x</*full=*/true>(bits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
                break;
            }
            [[unlikely]] default:
            {
                ::fast_io::fast_terminate();
            }
        }

        ::fast_io::io::perr(output, u8"\n");
    }

    /**
     * @brief   Print the resolved `--wasm-set-start-func` entry configuration.
     * @details The log contains both local-defined and import-inclusive function indices because the command-line option
     *          accepts a local-defined index, while the runtime library consumes the import-inclusive wasm function index.
     *          The multi-line record is emitted under one logger lock to preserve readability in verbose concurrent runs.
     */
    template <typename FunctionType, typename Tokens>
    inline constexpr void print_wasm_set_start_func_verbose(::std::uint32_t local_function_index,
                                                            ::std::size_t function_index,
                                                            ::std::size_t import_count,
                                                            FunctionType const& ft,
                                                            Tokens const& argument_tokens,
                                                            ::uwvm2::utils::container::vector<::std::byte> const& param_buffer) noexcept
    {
        // Emit the multi-line verbose record as one locked output unit.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        ::fast_io::io::perr(u8log_output_ul,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"--wasm-set-start-func resolved.\n"
                            // body_indent (begin)
                            u8"              "
                            // body_indent (end)
                            u8"local-defined func index: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            local_function_index,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"\n"
                            // body_indent (begin)
                            u8"              "
                            // body_indent (end)
                            u8"wasm func index:          ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            function_index,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8" (import-inclusive, import-count=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                            import_count,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8")\n"
                            // body_indent (begin)
                            u8"              "
                            // body_indent (end)
                            u8"function type:            ");
        print_wasm_entry_type_span(u8log_output_ul, ft.parameter.begin, ft.parameter.end);
        ::fast_io::io::perr(u8log_output_ul, u8" -> ");
        print_wasm_entry_type_span(u8log_output_ul, ft.result.begin, ft.result.end);
        ::fast_io::io::perr(u8log_output_ul,
                            u8"\n"
                            // body_indent (begin)
                            u8"              "
                            // body_indent (end)
                            u8"arguments:\n");

        if(argument_tokens.empty())
        {
            ::fast_io::io::perr(u8log_output_ul,
                                // argument_indent (begin)
                                u8"                  "
                                // argument_indent (end)
                                u8"<none>\n");
        }
        else
        {
            ::std::size_t offset{};
            for(::std::size_t i{}; i != argument_tokens.size(); ++i)
            {
                // The caller already checked arity, so this token and parameter-type lookup are range-proven.
                ::fast_io::io::perr(u8log_output_ul,
                                    // argument_indent (begin)
                                    u8"                  "
                                    // argument_indent (end)
                );

                print_wasm_entry_argument_verbose(u8log_output_ul,
                                                  param_buffer,
                                                  offset,
                                                  argument_tokens.index_unchecked(i),
                                                  wasm_entry_type_code(ft.parameter.begin[i]),
                                                  i);
            }
            if(offset != param_buffer.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        }

        ::fast_io::io::perr(u8log_output_ul,
                            // body_indent (begin)
                            u8"              "
                            // body_indent (end)
                            ,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                            u8"[",
                            ::uwvm2::uwvm::io::get_local_realtime(),
                            u8"] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                            u8"(verbose)\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
    }

    /**
     * @brief   Calculate the byte length needed for a packed entry ABI type span.
     * @details Each supported wasm scalar is laid out tightly using its natural wasm storage width.  Unsupported value
     *          types trigger a user-facing fatal error, while size_t overflow triggers termination as an internal
     *          invariant failure.
     *
     * @param   begin     First value type in the span.
     * @param   end       One-past-last value type in the span.
     * @param   is_result Selects whether diagnostics say "parameter" or "result".
     * @return  Required packed ABI byte length.
     */
    template <typename ValueTypePtr>
    [[nodiscard]] inline constexpr ::std::size_t calculate_wasm_entry_abi_bytes(ValueTypePtr begin, ValueTypePtr end, bool is_result) noexcept
    {
        ::std::size_t total{};
        for(auto curr{begin}; curr != end; ++curr)
        {
            auto const type_code{wasm_entry_type_code(*curr)};
            auto const size{wasm_entry_scalar_abi_size(type_code)};
            if(size == 0uz) [[unlikely]]
            {
                auto const kind{is_result ? ::uwvm2::utils::container::u8string_view{u8"result"} : ::uwvm2::utils::container::u8string_view{u8"parameter"}};
                wasm_set_start_func_fatal(u8"Unsupported --wasm-set-start-func ",
                                          kind,
                                          u8" type: ",
                                          wasm_entry_type_name(type_code),
                                          u8". Only i32, i64, f32, and f64 are currently supported.");
            }
            if(total > (::std::numeric_limits<::std::size_t>::max() - size)) [[unlikely]] { ::fast_io::fast_terminate(); }
            total += size;
        }
        return total;
    }

    /**
     * @brief   Copy an entry invocation descriptor into a runtime-library run config.
     * @details Runtime library configs store raw pointer/length pairs instead of owning vectors.  The caller must keep
     *          `entry` alive until the runtime function returns.  Empty parameter/result vectors are represented as null
     *          pointers with zero byte counts to avoid exposing implementation-specific empty-vector data pointers.
     */
    inline constexpr void configure_runtime_entry_buffers(auto& cfg, runtime_entry_invocation& entry) noexcept
    {
        cfg.entry_function_index = entry.function_index;
        cfg.entry_abi_buffers.param_buffer = entry.param_buffer.empty() ? nullptr : entry.param_buffer.data();
        cfg.entry_abi_buffers.param_bytes = entry.param_buffer.size();
        cfg.entry_abi_buffers.result_buffer = entry.result_buffer.empty() ? nullptr : entry.result_buffer.data();
        cfg.entry_abi_buffers.result_bytes = entry.result_buffer.size();
    }

    /**
     * @brief   Resolve the actual runtime entry invocation for the main module.
     * @details Without `--wasm-set-start-func`, this delegates to the default entry resolver and produces empty ABI
     *          buffers because default entries must be `() -> ()`.  With `--wasm-set-start-func`, the command-line index
     *          is interpreted as a local-defined function index of the main module.  The function validates arity,
     *          computes packed parameter/result storage, converts the local-defined index into the runtime's
     *          import-inclusive function index, and packs every provided argument according to the selected function type.
     *
     * @param   main_module_name Name key of the executable module.
     * @return  Fully configured entry invocation descriptor.
     * @warning Invalid user input is diagnosed and terminates the process; internal storage inconsistencies terminate
     *          directly because they indicate earlier loader/runtime initialization bugs.
     */
    inline constexpr runtime_entry_invocation resolve_runtime_entry_invocation(::uwvm2::utils::container::u8string_view main_module_name) noexcept
    {
        runtime_entry_invocation entry{};
        auto const& requested{::uwvm2::uwvm::wasm::storage::start_func_call};
        if(!requested.enabled)
        {
            entry.function_index = resolve_default_first_entry_function_index(main_module_name);
            return entry;
        }

        auto const rt_it{::uwvm2::uwvm::runtime::storage::active_runtime_registry().find(main_module_name)};
        if(rt_it == ::uwvm2::uwvm::runtime::storage::active_runtime_registry().end()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& rt{rt_it->second};
        auto const local_index{static_cast<::std::size_t>(requested.local_function_index)};
        auto const local_n{rt.local_defined_function_vec_storage.size()};
        if(local_index >= local_n) [[unlikely]]
        {
            wasm_set_start_func_fatal(u8"--wasm-set-start-func selected local function index ",
                                      requested.local_function_index,
                                      u8", but the main module only has ",
                                      local_n,
                                      u8" local defined functions. The index is local-defined, not import-inclusive.");
        }

        auto const& runtime_func{rt.local_defined_function_vec_storage.index_unchecked(local_index)};
        auto const ft{runtime_func.function_type_ptr};
        if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const param_count{static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)};
        if(requested.argument_tokens.size() != param_count) [[unlikely]]
        {
            wasm_set_start_func_arity_mismatch_fatal(requested.local_function_index, *ft, requested.argument_tokens);
        }

        auto const param_bytes{calculate_wasm_entry_abi_bytes(ft->parameter.begin, ft->parameter.end, false)};
        auto const result_bytes{calculate_wasm_entry_abi_bytes(ft->result.begin, ft->result.end, true)};

        auto const import_count{rt.imported_function_vec_storage.size()};
        entry.function_index = import_count + local_index;
        entry.param_buffer.resize(param_bytes);
        entry.result_buffer.resize(result_bytes);

        ::std::size_t offset{};
        for(::std::size_t i{}; i != param_count; ++i)
        {
            // The arity check above proves `argument_tokens.size() == param_count`; this unchecked lookup is bounded by the loop.
            pack_wasm_entry_argument(entry.param_buffer, offset, requested.argument_tokens.index_unchecked(i), wasm_entry_type_code(ft->parameter.begin[i]), i);
        }
        if(offset != param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            print_wasm_set_start_func_verbose(requested.local_function_index,
                                              entry.function_index,
                                              import_count,
                                              *ft,
                                              requested.argument_tokens,
                                              entry.param_buffer);
        }

        return entry;
    }

    /**
     * @brief Execute preloaded start sections and the selected main entry using the full backend.
     * @details Runtime storage and imports are already initialized. Eager compilation is cached across these entries.
     */
    template <typename RunConfig, typename RunEntry>
    inline constexpr void run_initialized_module_graph(::uwvm2::utils::container::u8string_view main_module_name,
                                                       RunConfig cfg, RunEntry run_entry) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        if constexpr(::std::is_same_v<RunConfig, ::uwvm2::runtime::lib::full_compile_run_config>)
        {
            if(::uwvm2::runtime::lib::runtime_llvm_jit_debug_registered_worker_stopping_host_api()) { return; }
        }
#endif
        ::uwvm2::runtime::gc::scoped_cli_gc_execution gc_cli_execution{};
        // Derived by runtime from the actual initializer-owned native map and
        // mode. No external bool or raw module/handle can assert this closure.
        if constexpr(::std::is_same_v<RunConfig, ::uwvm2::runtime::lib::full_compile_run_config>)
        {
            static_cast<void>(::uwvm2::runtime::lib::runtime_gc_prepare_cli_collection_host_api());
            // Core 3 instantiation rejects every invalid body before any active
            // segment can mutate an imported memory or table. Full compilation
            // fuses that proof with its one translation of every body.
            if(!::uwvm2::runtime::lib::full_compile_prepare_host_api()) [[unlikely]]
            {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                if(::uwvm2::runtime::lib::runtime_llvm_jit_debug_registered_worker_stopping_host_api()) { return; }
#endif
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"Cannot prepare the initialized module graph for the current full runtime mode.\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
        }
        if constexpr(::std::is_same_v<RunConfig, ::uwvm2::runtime::lib::lazy_compile_run_config>)
        {
            // The deferred segment contract applies to lazy/tiered too. Every
            // body is fused into its actual checked artifact BEFORE any preload
            // segment/start can change an imported memory/table. The later run
            // entry only consumes the already admitted publication.
            if(!::uwvm2::runtime::lib::lazy_compile_prepare_host_api(main_module_name, cfg)) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"Cannot prepare the initialized module graph for the current lazy runtime mode.\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
        }
        using start_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t;
        // Preloaded Wasm modules are instances too: run each actual start section once, in preload order.
        // Do not invoke library exports named _start/main, and do not apply the main-module CLI override to libraries.
        for(auto const& module: ::uwvm2::uwvm::wasm::storage::active_preloaded_wasm())
        {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
            if constexpr(::std::is_same_v<RunConfig, ::uwvm2::runtime::lib::full_compile_run_config>)
            {
                if(::uwvm2::runtime::lib::runtime_llvm_jit_debug_registered_worker_stopping_host_api()) { return; }
            }
#endif
            if(module.binfmt_ver != 1u) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(module.module_name);
            auto const& start{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_t>(
                module.wasm_module_storage.wasm_binfmt_ver1_storage.sections)};
            if(start.sec_span.sec_begin == nullptr) { continue; }
            RunConfig start_cfg{};
            if constexpr(requires { start_cfg.assume_full_code_verified; })
            { start_cfg.assume_full_code_verified = cfg.assume_full_code_verified; }
            start_cfg.entry_function_index = static_cast<::std::size_t>(start.start_idx);
            start_cfg.module_start = true;
            run_entry(module.module_name, start_cfg);
        }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        if constexpr(::std::is_same_v<RunConfig, ::uwvm2::runtime::lib::full_compile_run_config>)
        {
            if(::uwvm2::runtime::lib::runtime_llvm_jit_debug_registered_worker_stopping_host_api()) { return; }
        }
#endif
        auto const& main_module{::uwvm2::uwvm::wasm::storage::active_execute_wasm()};
        ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(main_module_name);
        auto const& start{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_t>(
            main_module.wasm_module_storage.wasm_binfmt_ver1_storage.sections)};
        cfg.module_start = !::uwvm2::uwvm::wasm::storage::start_func_call.enabled &&
                           start.sec_span.sec_begin != nullptr && cfg.entry_function_index == start.start_idx;
        run_entry(main_module_name, cfg);
    }

    inline constexpr void run_full_module_graph(::uwvm2::utils::container::u8string_view main_module_name,
                                                 ::uwvm2::runtime::lib::full_compile_run_config cfg) noexcept
    {
        run_initialized_module_graph(main_module_name, cfg, ::uwvm2::runtime::lib::full_compile_and_run_main_module);
    }

#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    [[nodiscard]] inline bool install_debug_source_maps(::uwvm2::uwvm::debugger::controller& control)
    {
        namespace parser = ::uwvm2::parser::wasm;
        namespace wasm = ::uwvm2::uwvm::wasm;
        auto const& runtime_modules{::uwvm2::uwvm::runtime::storage::active_runtime_registry()};
        ::std::uint64_t module_id{};
        // LLVM full assigns dense IDs by iterating this same, unchanged
        // runtime_modules table in compile_all_modules_if_needed(). This runs
        // after prepare_debug_host_api() has completed that assignment.
        for(auto const& runtime_entry : runtime_modules)
        {
            if(module_id == ::std::numeric_limits<::std::uint64_t>::max()) { return false; }
            auto const parsed{wasm::storage::all_module.find(runtime_entry.first)};
            if(parsed == wasm::storage::all_module.end() ||
               (parsed->second.type != wasm::type::module_type_t::exec_wasm &&
                parsed->second.type != wasm::type::module_type_t::preloaded_wasm))
            { ++module_id; continue; }
            auto const* file{parsed->second.module_storage_ptr.wf};
            if(file == nullptr || file->binfmt_ver != 1u) { return false; }
            auto const& module{file->wasm_module_storage.wasm_binfmt_ver1_storage};
            bool installed{false};
            [&]<typename... Fs>(::uwvm2::utils::container::tuple<Fs...>)
            {
                using code_section_t = parser::standard::wasm1::features::code_section_storage_t<Fs...>;
                using custom_section_t = parser::standard::wasm1::features::custom_section_storage_t;
                auto const& code{parser::concepts::operation::get_first_type_in_tuple<code_section_t>(module.sections)};
                auto const& custom{parser::concepts::operation::get_first_type_in_tuple<custom_section_t>(module.sections)};
                ::uwvm2::uwvm::debugger::source_map_sections sections{};
                bool invalid{};
                ::std::array<bool, 4u> seen_custom{};
                ::std::span<::std::byte const> source_url{};
                ::fast_io::string source_json{}, source_directory{};
                auto const file_begin{reinterpret_cast<::std::uintptr_t>(module.module_span.module_begin)};
                auto const file_end{reinterpret_cast<::std::uintptr_t>(module.module_span.module_end)};
                auto const code_begin{reinterpret_cast<::std::uintptr_t>(code.sec_span.sec_begin)};
                auto const code_end{reinterpret_cast<::std::uintptr_t>(code.sec_span.sec_end)};
                if(file_begin == 0u || file_end < file_begin || code_begin < file_begin ||
                   code_end < code_begin || code_end > file_end) { invalid = true; }
                else
                { sections.code_section_content_size = code_end - code_begin;
                  sections.code_section_file_offset = code_begin - file_begin;
                  sections.module_file_size = file_end - file_begin; }
                for(auto const& entry : custom.customs)
                {
                    ::std::span<::std::byte const>* destination{};
                    bool* seen{};
                    if(entry.custom_name == u8".debug_line") { destination = ::std::addressof(sections.debug_line); seen = ::std::addressof(seen_custom[0u]); }
                    else if(entry.custom_name == u8".debug_line_str") { destination = ::std::addressof(sections.debug_line_str); seen = ::std::addressof(seen_custom[1u]); }
                    else if(entry.custom_name == u8".debug_str") { destination = ::std::addressof(sections.debug_str); seen = ::std::addressof(seen_custom[2u]); }
                    else if(entry.custom_name == u8"sourceMappingURL") { destination = ::std::addressof(source_url); seen = ::std::addressof(seen_custom[3u]); }
                    if(destination == nullptr) { continue; }
                    auto const begin{reinterpret_cast<::std::uintptr_t>(entry.custom_begin)};
                    auto const end{reinterpret_cast<::std::uintptr_t>(entry.sec_span.sec_end)};
                    if(*seen || begin < file_begin || end < begin || end > file_end)
                    { invalid = true; continue; }
                    *seen = true;
                    // [validated mapped module; custom payload, section end]
                    // [safe                                               ] end is one-past within the loader-owned bytes.
                    //  ^^ no pointer advance; span length follows checked integer bounds.
                    *destination = {reinterpret_cast<::std::byte const*>(begin), end - begin};
                }
                if(sections.debug_line.empty() && seen_custom[3u] && !invalid)
                {
                    sections.source_map_declared = true;
                    auto const name{::std::string_view{reinterpret_cast<char const*>(file->file_name.data()),file->file_name.size()}};
                    if(::uwvm2::uwvm::debugger::read_source_map_sidecar(source_url,name,source_json,source_directory))
                    {
                        sections.source_map_json = {source_json.data(),source_json.size()};
                        sections.source_map_directory = {source_directory.data(),source_directory.size()};
                    }
                    else { invalid = true; }
                }
                auto const& runtime{runtime_entry.second};
                ::std::vector<::uwvm2::uwvm::debugger::source_function_span> functions{};
                auto const imported{runtime.imported_function_vec_storage.size()};
                auto const count{runtime.local_defined_function_vec_storage.size()};
                functions.reserve(count);
                for(::std::size_t index{}; index != count; ++index)
                {
                    auto const* body{runtime.local_defined_function_vec_storage.index_unchecked(index).wasm_code_ptr};
                    if(body == nullptr || index > ::std::numeric_limits<::std::uint64_t>::max() - imported)
                    { invalid = true; break; }
                    auto const expr{reinterpret_cast<::std::uintptr_t>(body->body.expr_begin)};
                    auto const end{reinterpret_cast<::std::uintptr_t>(body->body.code_end)};
                    if(expr < code_begin || end <= expr || end > code_end)
                    { invalid = true; break; }
                    // [Code content] [body locals] [expression ... code_end]
                    // [safe        ] [safe       ] [safe                 ] one-past
                    //                               ^^ expression offset is checked before any source lookup.
                    functions.push_back({static_cast<::std::uint64_t>(imported + index),
                                         static_cast<::std::uint64_t>(expr - code_begin),
                                         static_cast<::std::uint64_t>(end - expr)});
                }
                installed = control.install_source_module(module_id, sections, ::std::move(functions), invalid);
            }(wasm::feature::wasm_binfmt1_features);
            if(!installed) { return false; }
            // Only the runtime's actual canonical source/native publication can
            // mint a binding. Preloaded/foreign modules remain metadata-unavailable.
            auto binding{::uwvm2::runtime::lib::llvm_jit_debug_bind_source_host_api(static_cast<::std::size_t>(module_id))};
            // Optional metadata failure leaves the existing Wasm/line/native
            // controls usable; no partial index or binding is queried.
            if(binding) { static_cast<void>(control.install_source_metadata(module_id, ::std::move(binding))); }
            ++module_id;
        }
        return true;
    }
#endif

    // Default adaptive compilation limit: floor(log2(hardware concurrency)), at least one.
    inline constexpr ::std::size_t calculate_default_runtime_compile_threads(::std::size_t max_compile_threads) noexcept
    {
        // Follow the default policy upper bound: roughly one compiler thread per log2(N) CPUs.
        ::std::size_t compiler_threads{};
        while(max_compile_threads > 1uz)
        {
            max_compile_threads >>= 1u;
            ++compiler_threads;
        }
        return compiler_threads == 0uz ? 1uz : compiler_threads;
    }

    /**
     * @brief   Compute the aggressive adaptive runtime compile-thread limit.
     * @details The aggressive policy targets `floor(max * 2 / 3)` while avoiding multiplication overflow.  A detected
     *          maximum below two resolves to zero, which keeps tiny/single-threaded environments on the main thread.
     *
     * @param   max_compile_threads Detected platform concurrency used as the policy input.
     * @return  Aggressive compile-thread limit selected by policy.
     */
    inline constexpr ::std::size_t calculate_aggressive_runtime_compile_threads(::std::size_t max_compile_threads) noexcept
    {
        if(max_compile_threads < 2uz) [[unlikely]] { return 0uz; }

        // Prevent overflow
        auto const quotient{max_compile_threads / 3uz};
        auto const remainder{max_compile_threads % 3uz};
        return quotient * 2uz + (remainder * 2uz) / 3uz;
    }

    /**
     * @brief   Resolve `--runtime-compile-threads` into the global runtime compile-thread setting.
     * @details Command-line parsing preserves the raw user policy (`default`, `aggressive`, or numeric).  This function
     *          performs the platform-dependent resolution:
     *
     *          - omitted or `default`: use the default adaptive policy when native threads are available;
     *          - `aggressive`: use the aggressive adaptive policy when native threads are available;
     *          - non-negative numeric values: use the exact requested value, warning if it exceeds detected concurrency;
     *          - negative numeric values: interpret `-N` as `detected_max - N`, rejecting magnitudes above detected max;
     *          - no `fast_io::native_thread`: force zero extra compile threads and optionally warn for unsupported requests.
     *
     *          The resolved value is stored in `global_runtime_compile_threads_resolved` for the runtime library.  Full
     *          compilation scheduling may still reduce effective parallelism per module based on task count and code size.
     *
     *          Lazy scheduling also treats this as an upper bound: background lazy workers may consume it directly, while
     *          LLVM/tiered urgent JIT schedulers are separate global lanes with at most one worker each.  A running thread
     *          may help compile queued lazy work while waiting, but no per-running-thread urgent JIT worker is created here.
     *
     * @return  The resolved runtime compile-thread value stored globally.
     * @warning Some invalid numeric settings are fatal.  Warnings can also be promoted to fatal errors by the global
     *          runtime warning policy.
     */
    inline constexpr ::std::size_t resolve_runtime_compile_threads() noexcept
    {
        using runtime_compile_threads_type = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_type;
        using runtime_compile_threads_unsigned_type = ::std::make_unsigned_t<runtime_compile_threads_type>;
        using runtime_compile_threads_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t;

        // Warning helper specific to runtime compile-thread policy.  It deliberately uses the same warning category
        // suffix as the command-line warning toggles so users can suppress or promote this warning class consistently.
        constexpr auto runtime_compile_threads_warn{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8" (runtime-compile-threads)\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            }};

        // Warning-to-fatal escalation is split from the warning helper so diagnostics keep the original warning text
        // before the generic fatal conversion notice.
        constexpr auto runtime_compile_threads_warn_to_fatal{
            []() constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime-compile-threads)\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }};

        // Fatal helper for policy values that are invalid regardless of warning configuration.
        constexpr auto runtime_compile_threads_fatal{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    u8"\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }};

        // Verbose helper for the final resolved policy.  It includes the wall-clock timestamp used by other UWVM verbose
        // runtime messages, making compile-thread diagnostics easy to correlate with later scheduling logs.
        constexpr auto runtime_compile_threads_verbose_info{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            }};

# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        // Treat a failed hardware-concurrency probe as a single-threaded platform.  The rest of the policy code can then
        // operate on a non-zero detected maximum.
        ::std::size_t max_compile_threads{::uwvm2::utils::thread::hardware_concurrency()};
        if(max_compile_threads == 0uz) [[unlikely]] { max_compile_threads = 1uz; }
        auto const default_compile_threads{calculate_default_runtime_compile_threads(max_compile_threads)};
        auto const aggressive_compile_threads{calculate_aggressive_runtime_compile_threads(max_compile_threads)};
# else
        // When native threads are not compiled in, the detected max is only a diagnostic baseline.  All effective
        // parallel compile-thread counts are forced to zero below.
        constexpr ::std::size_t max_compile_threads{1uz};
# endif

        // Omitted policy starts at the default adaptive value on threaded builds and at zero otherwise.  The preprocessor
        // branch keeps the initializer valid even when `default_compile_threads` is not declared.
        ::std::size_t resolved_compile_threads{
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            default_compile_threads
# endif
        };

        // Explicit command-line policy overrides the omitted default.  Numeric settings have two meanings:
        // non-negative values are exact requests; negative values are offsets from detected maximum concurrency.
        if(::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed)
        {
            auto const requested_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};

            if(requested_compile_threads_policy == runtime_compile_threads_policy_t::default_policy)
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                resolved_compile_threads = default_compile_threads;
# else
                resolved_compile_threads = 0uz;
# endif
            }
            else if(requested_compile_threads_policy == runtime_compile_threads_policy_t::aggressive)
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                resolved_compile_threads = aggressive_compile_threads;
# else
                resolved_compile_threads = 0uz;
                if(::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                {
                    runtime_compile_threads_warn(u8"Requested runtime compile thread policy \"",
                                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                 u8"aggressive",
                                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                 u8"\", but this platform does not provide fast_io::native_thread. Falling back to 0 extra compile threads.");

                    if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                }
# endif
            }
            else
            {
                auto const requested_compile_threads{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads};
                if(requested_compile_threads >= 0)
                {
                    resolved_compile_threads = 0uz;

# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    resolved_compile_threads = static_cast<::std::size_t>(requested_compile_threads);
                    if(resolved_compile_threads > max_compile_threads && ::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                    {
                        runtime_compile_threads_warn(u8"Requested runtime compile thread count (requested=",
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                     u8", detected-max=",
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                     max_compile_threads,
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                     u8"). The requested value will still be used, but performance may be suboptimal.");

                        if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                    }
# else
                    if(requested_compile_threads != 0 && ::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                    {
                        runtime_compile_threads_warn(
                            u8"Requested runtime compile thread count (requested=",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            requested_compile_threads,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"), but this platform does not provide fast_io::native_thread. Falling back to 0 extra compile threads.");

                        if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                    }
# endif
                }
                else
                {
                    auto const requested_compile_threads_abs{static_cast<runtime_compile_threads_unsigned_type>(0u) -
                                                             static_cast<runtime_compile_threads_unsigned_type>(requested_compile_threads)};

                    // Compute the absolute value in the unsigned domain so the most-negative signed value is handled
                    // without signed overflow.  It is rejected below if it exceeds the detected maximum.
                    if(requested_compile_threads_abs > static_cast<runtime_compile_threads_unsigned_type>(max_compile_threads)) [[unlikely]]
                    {
                        runtime_compile_threads_fatal(u8"Invalid negative runtime compile thread count (requested=",
                                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                      requested_compile_threads,
                                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                      u8", detected-max=",
                                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                      max_compile_threads,
                                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                      u8"): the absolute value of a negative setting must not exceed the detected max thread count.");
                    }

                    resolved_compile_threads = max_compile_threads - static_cast<::std::size_t>(requested_compile_threads_abs);
                }
            }
        }

        // Publish the resolved value before verbose logging so lower layers and diagnostics observe the same state.
        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved = resolved_compile_threads;

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            // Verbose output distinguishes exact numeric settings from adaptive upper bounds.  Adaptive scheduling is
            // allowed to choose fewer workers later when a module does not have enough compile tasks to justify them.
            if(::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed)
            {
                auto const requested_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};

                if(requested_compile_threads_policy == runtime_compile_threads_policy_t::default_policy)
                {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         u8"default",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8", detected-max=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         max_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8", per-module full-compile scheduling may adapt below this upper bound). ");
# else
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         u8"default",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8", fast_io::native_thread unavailable on this platform). ");
# endif
                }
                else if(requested_compile_threads_policy == runtime_compile_threads_policy_t::aggressive)
                {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    runtime_compile_threads_verbose_info(
                        u8"Runtime compile thread upper bound resolved to ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                        resolved_compile_threads,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                        u8" (requested=",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                        u8"aggressive",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                        u8", detected-max=",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                        max_compile_threads,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                        u8", aggressive-policy=floor(max*2/3), per-module full-compile scheduling may adapt below this upper bound). ");
# else
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         u8"aggressive",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8", fast_io::native_thread unavailable on this platform). ");
# endif
                }
                else
                {
                    runtime_compile_threads_verbose_info(u8"Runtime compile threads resolved to ",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8", detected-max=",
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                         max_compile_threads,
                                                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
            }
            else
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                     u8" by the default policy (detected-max=",
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                     max_compile_threads,
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                     u8", per-module full-compile scheduling may adapt below this upper bound). ");
# else
                runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                     u8" by the default policy (fast_io::native_thread unavailable on this platform). ");
# endif
            }
        }

        return resolved_compile_threads;
    }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    // `-Rint` is a shortcut-only auto mode. Prefer full translation for ordinary executable-only uwvm-int runs because full
    // translation is cheap; prefer lazy for preload-heavy runs so cold preload code is not translated up front.
    //
    // The two thresholds intentionally model different costs:
    // - main-only: the executable module is hot by definition, so full translation usually pays for itself up to a larger size;
    // - preload-total: full mode translates every loaded Wasm module, including preloads that may never be called, so use a lower cap.
    inline constexpr ::std::size_t runtime_int_auto_main_full_threshold{1024uz * 1024uz};
    inline constexpr ::std::size_t runtime_int_auto_preload_total_full_threshold{256uz * 1024uz};

    [[nodiscard]] inline constexpr ::std::size_t saturating_add_size(::std::size_t lhs, ::std::size_t rhs) noexcept
    {
        // A malicious or unusual process configuration should not make size accounting wrap and accidentally select the
        // smaller/full threshold.  Saturating to max keeps overflow on the conservative lazy side.
        constexpr auto max_size{::std::numeric_limits<::std::size_t>::max()};
        if(max_size - lhs < rhs) [[unlikely]] { return max_size; }
        return lhs + rhs;
    }

    [[nodiscard]] inline constexpr ::std::size_t loaded_wasm_file_byte_size(::uwvm2::uwvm::wasm::type::wasm_file_t const& wf) noexcept
    {
        // Use the parser's module span instead of re-statting paths.  At this point all executable/preload Wasm files have
        // already been loaded, and the span describes the exact byte range that participated in parsing.
        switch(wf.binfmt_ver)
        {
            case 1u:
            {
                auto const& module_storage{wf.wasm_module_storage.wasm_binfmt_ver1_storage};
                auto const module_begin{module_storage.module_span.module_begin};
                auto const module_end{module_storage.module_span.module_end};
                if(module_begin == nullptr || module_end == nullptr || module_end < module_begin) [[unlikely]] { return 0uz; }
                return static_cast<::std::size_t>(module_end - module_begin);
            }
            [[unlikely]] default:
            {
                return 0uz;
            }
        }
    }

    inline constexpr void resolve_runtime_int_auto_mode() noexcept
    {
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode != ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::auto_compile) { return; }

        // `auto_compile` is deliberately not a general runtime mode knob.  It is the uwvm-int auto policy used by `-Rint`
        // and by `-Rcc int` when `-Rcm` is omitted; JIT/tiered invocations must choose their mode explicitly.
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler != ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only)
            [[unlikely]]
        {
            ::fast_io::io::perr(
                ::uwvm2::uwvm::io::u8log_output,
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                u8"uwvm: ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                u8"[fatal] ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                u8"auto_compile runtime mode is only supported by the uwvm-int backend (-Rint, or -Rcc int without -Rcm). " u8"Use -Rcm lazy|full with -Rcc jit|tiered to select LLVM-JIT or tiered runtime modes explicitly. ",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                u8"(runtime)\n\n",
                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        auto const main_wasm_bytes{loaded_wasm_file_byte_size(::uwvm2::uwvm::wasm::storage::active_execute_wasm())};
        ::std::size_t preload_wasm_bytes{};
        for(auto const& preloaded_wasm: ::uwvm2::uwvm::wasm::storage::active_preloaded_wasm())
        {
            preload_wasm_bytes = saturating_add_size(preload_wasm_bytes, loaded_wasm_file_byte_size(preloaded_wasm));
        }

        auto const total_wasm_bytes{saturating_add_size(main_wasm_bytes, preload_wasm_bytes)};
        auto const has_preload_wasm{!::uwvm2::uwvm::wasm::storage::active_preloaded_wasm().empty()};

        // With preloads, total loaded Wasm size is the relevant full-compile cost because full mode translates all loaded
        // Wasm modules.  Without preloads, the executable module size is the useful hot-code proxy.
        auto const threshold{has_preload_wasm ? runtime_int_auto_preload_total_full_threshold : runtime_int_auto_main_full_threshold};
        auto const selected_full{has_preload_wasm ? total_wasm_bytes <= threshold : main_wasm_bytes <= threshold};

        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = selected_full ? ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile
                                                                                  : ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile;

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            // Keep the auto decision visible under verbose logging so benchmark runs can explain why the uwvm-int auto
            // policy behaved like `-Rcm full` or `-Rcm lazy` without adding noise to normal program output.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"uwvm-int auto selected ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                ::fast_io::mnp::cond(selected_full, u8"full", u8"lazy"),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8" compile for uwvm-int (main-wasm-bytes=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                main_wasm_bytes,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", preload-wasm-bytes=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                preload_wasm_bytes,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", total-wasm-bytes=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                total_wasm_bytes,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", threshold=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                threshold,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                ::fast_io::mnp::cond(has_preload_wasm, u8", policy=preload-total", u8", policy=main-only"),
                                u8"). ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                u8"[",
                                ::uwvm2::uwvm::io::get_local_realtime(),
                                u8"] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        }
    }
#endif

    /**
     * @brief   Execute the requested UWVM action.
     * @details This is the top-level driver called by the CRT entry wrapper.  It first performs loader-level work that is
     *          shared by all modes, then returns early for inspection-only modes, and finally performs runtime-specific
     *          initialization and backend dispatch for executable mode.
     *
     *          High-level control flow:
     *          1. load the executable module, local modules, and weak-symbol modules;
     *          2. construct the global `all_module` registry and reject duplicate module names;
     *          3. serve `section_details` and parser `validation` modes without initializing executable runtime state;
     *          4. reject executable mode in builds compiled without runtime backends;
     *          5. check cross-module imports, detect import cycles, and initialize runtime storage;
     *          6. normalize runtime-mode constraints such as debug-interpreter requiring full compilation;
     *          7. resolve compile-thread policy and entry invocation;
     *          8. dispatch to the selected runtime mode/compiler pair.
     *
     * @return  Process-style integer return code from `uwvm2::uwvm::run::retval`.
     * @warning Fatal configuration/runtime invariants can terminate the process through `fast_io::fast_terminate`.
     */
    // Explicit debugger mode supplies LLVM/full defaults only when the user did
    // not choose a backend/mode. An explicit incompatible selection is fatal,
    // before loading imports or executing any start section.
    inline constexpr bool prepare_debug_jit_mode() noexcept
    {
        if(::uwvm2::uwvm::wasm::storage::execute_wasm_mode != ::uwvm2::uwvm::wasm::base::mode::debug_jit) { return true; }
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
#if defined(UWVM_RUNTIME_LLVM_JIT)
        bool preset{mode::is_runtime_mode_code_jit_existed || mode::is_runtime_mode_code_aot_existed};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        preset = preset || mode::is_runtime_mode_code_int_existed;
# endif
# if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
        preset = preset || mode::is_runtime_mode_code_debug_existed;
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        preset = preset || mode::is_runtime_mode_code_tiered_existed;
# endif
        if(!preset && !mode::custom_runtime_mode_existed) { mode::global_runtime_mode = mode::runtime_mode_t::full_compile; }
        if(!preset && !mode::custom_runtime_compiler_existed) { mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only; }
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        if(mode::global_runtime_mode == mode::runtime_mode_t::full_compile &&
           mode::global_runtime_compiler == mode::runtime_compiler_t::llvm_jit_only) { return true; }
#endif
        auto const compiler{[]() constexpr noexcept -> ::uwvm2::utils::container::u8string_view
        {
            switch(mode::global_runtime_compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case mode::runtime_compiler_t::uwvm_interpreter_only: return u8"uwvm-int";
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case mode::runtime_compiler_t::llvm_jit_only: return u8"llvm-jit";
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered: return u8"tiered";
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                case mode::runtime_compiler_t::debug_interpreter: return u8"debug-interpreter";
#endif
                default: return u8"unavailable";
            }
        }()};
        auto const compilation{[]() constexpr noexcept -> ::uwvm2::utils::container::u8string_view
        {
            switch(mode::global_runtime_mode)
            {
                case mode::runtime_mode_t::full_compile: return u8"full";
                case mode::runtime_mode_t::auto_compile: return u8"auto";
                case mode::runtime_mode_t::lazy_compile: return u8"lazy";
                case mode::runtime_mode_t::lazy_compile_with_full_code_verification: return u8"lazy+verification";
                default: return u8"unknown";
            }
        }()};
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
            u8"debug-jit is unsupported in the current mode: ", compiler, u8"/", compilation,
            u8". It requires LLVM JIT full compilation and native thread support. ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        return false;
    }

    inline bool prepare_debug_console_input() noexcept
    {
        if(::uwvm2::uwvm::wasm::storage::execute_wasm_mode != ::uwvm2::uwvm::wasm::base::mode::debug_jit) { return true; }
        // External channel adoption authenticates and seals its actual endpoint
        // before guest setup. Stdin is guest input in that launch, not a second
        // command source competing for the one protected console-input owner.
        if(::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd >= 0 ||
           ::uwvm2::uwvm::cmdline::params::details::debug_jit_control_handle != 0u) { return true; }
        namespace control = ::uwvm2::utils::control;
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__)) || (defined(_WIN32) && !defined(__CYGWIN__))
        // Host launch authority only, before constructing any guest file table.
        // A retained native duplicate pins the input object; guest path_open
        // checks the opened OS object, not a guest-provided path label.
        auto const status{control::seal_console_input_host_api(0)};
#else
        auto const status{control::sealed_input_status::unsupported_platform};
#endif
        if(status == control::sealed_input_status::ok)
        {
            // Check the host console's own sinks before emitting any prompt.
            // A regular-file/FIFO alias feeds output back into command input;
            // same-slave-terminal output is allowed because its direction differs.
            auto const stdout_status{control::inspect_guest_output_host_api(1)};
            auto const stderr_status{control::inspect_guest_output_host_api(2)};
            if(stdout_status == control::sealed_input_decision::allow && stderr_status == control::sealed_input_decision::allow) { return true; }
            auto const report{[](auto output) noexcept
            {
                ::fast_io::io::perr(output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"Unable to isolate debug-jit command input: console output aliases protected input or its identity cannot be verified. ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            }};
            // Never send even the failure diagnostic to a rejected command-input
            // alias. If neither sink is safe, fail with a nonzero exit silently.
            if(stderr_status == control::sealed_input_decision::allow) { report(::fast_io::u8err()); }
            else if(stdout_status == control::sealed_input_decision::allow) { report(::fast_io::u8out()); }
            return false;
        }
        auto const reason{[&]() noexcept -> ::uwvm2::utils::container::u8string_view
        {
            switch(status)
            {
                case control::sealed_input_status::already_sealed: return u8"input already bound to another launch";
                case control::sealed_input_status::invalid_descriptor: return u8"input descriptor is not readable";
                case control::sealed_input_status::identity_unavailable: return u8"input identity cannot be protected";
                case control::sealed_input_status::unsupported_platform: return u8"secure console input is unavailable on this platform";
                default: return u8"unknown input isolation failure";
            }
        }()};
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
            u8"Unable to isolate debug-jit command input: ", reason, u8". ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        return false;
    }

    inline bool prepare_debug_control_fd_mode() noexcept
    {
        if(::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd < 0) { return true; }
#if ((defined(__linux__) && defined(SYS_pidfd_open) && defined(SCM_CREDENTIALS)) || \
     (defined(__APPLE__) && defined(__MACH__))) && defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if((::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::run ||
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit) &&
           mode::global_runtime_mode == mode::runtime_mode_t::full_compile &&
           mode::global_runtime_compiler == mode::runtime_compiler_t::llvm_jit_only) { return true; }
#endif
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
            u8"debug-jit control FD is unsupported in the current mode. It requires supported Linux or macOS, native threads, and LLVM JIT full run mode. ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        return false;
    }

    inline bool prepare_debug_control_handle_mode() noexcept
    {
        if(::uwvm2::uwvm::cmdline::params::details::debug_jit_control_handle == 0u) { return true; }
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd < 0 &&
           (::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::run ||
            ::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit) &&
           mode::global_runtime_mode == mode::runtime_mode_t::full_compile &&
           mode::global_runtime_compiler == mode::runtime_compiler_t::llvm_jit_only) { return true; }
#endif
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
            u8"debug-jit control HANDLE is unsupported in the current mode. It requires Windows, native threads, and LLVM JIT full run mode. ",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        return false;
    }

    inline constexpr int run() noexcept
    {
        if(!prepare_debug_jit_mode() || !prepare_debug_control_fd_mode() ||
           !prepare_debug_control_handle_mode() || !prepare_debug_console_input())
        { return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error); }
#if ((defined(__linux__) && defined(SYS_pidfd_open) && defined(SCM_CREDENTIALS)) || \
     (defined(__APPLE__) && defined(__MACH__)) || (defined(_WIN32) && !defined(__CYGWIN__))) && \
    defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# if defined(_WIN32) && !defined(__CYGWIN__)
        using debug_control_channel = ::uwvm2::uwvm::debugger::windows_control_handle;
# elif defined(__APPLE__) && defined(__MACH__)
        using debug_control_channel = ::uwvm2::uwvm::debugger::macos_control_fd;
# else
        using debug_control_channel = ::uwvm2::uwvm::debugger::linux_control_fd;
# endif
        ::std::unique_ptr<debug_control_channel> debug_channel{};
# if defined(_WIN32) && !defined(__CYGWIN__)
        if(auto const value{::uwvm2::uwvm::cmdline::params::details::debug_jit_control_handle}; value != 0u)
        {
            // Host-only inherited client HANDLE is authenticated and sealed
            // before any guest import, file table or JIT code can be admitted.
            debug_channel = debug_control_channel::adopt(value);
            if(!debug_channel)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"Unable to authorize debug-jit control HANDLE: expected the direct launcher parent's inherited, single-instance local message pipe client. ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
            }
        }
# else
        if(auto const fd{::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd}; fd >= 0)
        {
            // Adopt and seal before loading guest imports, file tables or code.
            debug_channel = debug_control_channel::adopt(fd);
            if(!debug_channel)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"Unable to authorize debug-jit control FD: expected a live external peer on an inherited private Unix socket. ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE), u8"(runtime)\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
            }
        }
# endif
#endif
        // Preloaded wasm modules and dynamic-link bindings are prepared before this function is entered.  This driver
        // consumes the resulting global command-line/storage state and performs the final ordered load/execute sequence.

        bool owned_full_prepared{};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
        namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
        // Debug/reserved LLVM-full may own a pure-Wasm preload graph. Ordinary
        // full with preload providers retains its previous mapped loader path.
        // This supplies lifetime only: source values still need a completed
        // validation epoch, stopped ticket and actual function/code generation.
        bool own_single_source{(::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::run ||
                                ::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit) &&
            runtime_mode::global_runtime_mode == runtime_mode::runtime_mode_t::full_compile &&
            runtime_mode::global_runtime_compiler == runtime_mode::runtime_compiler_t::llvm_jit_only &&
            (::uwvm2::uwvm::wasm::storage::preloaded_wasm.empty() ||
             ::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit ||
             ::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd >= 0 ||
             ::uwvm2::uwvm::cmdline::params::details::debug_jit_control_handle != 0u)};
# if defined(UWVM_SUPPORT_PRELOAD_DL)
        own_single_source = own_single_source && ::uwvm2::uwvm::wasm::storage::preloaded_dl.empty();
# endif
# if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        own_single_source = own_single_source && ::uwvm2::uwvm::wasm::storage::weak_symbol.empty();
# endif
        if(own_single_source)
        {
            // Only debug/reserved modes select an immutable owned preparse
            // image; ordinary full retains its existing mapped source behavior.
            // The FD/HANDLE was authenticated above before any input effects.
            bool const retain_debug_image{
                ::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit ||
                ::uwvm2::uwvm::cmdline::params::details::debug_jit_control_fd >= 0 ||
                ::uwvm2::uwvm::cmdline::params::details::debug_jit_control_handle != 0u};
            auto const prepared{prepare_owned_full_cli_source(retain_debug_image)};
            if(prepared != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) { return prepared; }
            owned_full_prepared = true;
        }
#endif
        if(!owned_full_prepared)
        {
        // Load the executable wasm module first.  Later local/weak modules may satisfy imports used by the executable.
        if(auto const ret{::uwvm2::uwvm::run::load_exec_wasm_module()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // Load explicitly-provided local modules.  These participate in normal import resolution and duplicate checks.
        if(auto const ret{::uwvm2::uwvm::run::load_local_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // Load weak-symbol modules after normal modules so they can provide fallback definitions without changing the
        // primary module load order.
        if(auto const ret{::uwvm2::uwvm::run::load_weak_symbol_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // Build the unified module table used by later export/import/runtime resolution.  Duplicate module names are
        // rejected before any mode tries to inspect or execute ambiguous module storage.
        if(auto const ret{::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        }

        // Inspection-only modes intentionally run before dependency checks.  A user may inspect sections or parser-level
        // validity even when imports are unresolved for execution.
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details:
            {
                // All requested modules are loaded and registered, which is enough for section-detail reporting.  Runtime
                // initialization is skipped because this mode does not instantiate or execute code.
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"Start printing section details. ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                }

                ::uwvm2::uwvm::wasm::section_detail::print_section_details();

                // Return directly: section-detail mode is complete and must not fall through into runtime checks.
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                // Validate all wasm code with the parser/runtime validator entry point, but do not initialize executable
                // runtime state.  This path is for validity checks, not compilation, partitioning, or backend execution.
                // `validate_all_wasm_code` already emits its own verbose progress diagnostics.
                if(!::uwvm2::uwvm::runtime::validator::validate_all_wasm_code()) [[unlikely]]
                {
                    return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
                }

                // Return directly: validation mode is complete and must not require executable backends.
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            default:
            {
                break;
            }
        }

#if !defined(UWVM_RUNTIME_HAS_BACKEND)
        // A backendless build is still useful for parsing, section printing, and validation.  Executable mode is rejected
        // only after those non-executing modes have had a chance to return successfully.
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"This build was configured without executable runtime backends. Only validation and section details are available.\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
        return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
#else
        // Executable mode begins here.  Import existence and import-cycle validation must precede runtime initialization
        // because runtime storage assumes all module links are resolvable and acyclic.
        if(!owned_full_prepared)
        if(auto const ret{::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        // Initialize runtime storage, link metadata, and backend-visible module data after import resolution succeeds.
        if(!owned_full_prepared) { ::uwvm2::uwvm::runtime::initializer::initialize_runtime(true); }

# if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
        // The debug interpreter backend is modeled as a full-compile backend.  If the command line selected a lazy mode,
        // preserve compatibility by forcing full compilation, subject to the global warning/fatal policy.
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter &&
           ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode != ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile) [[unlikely]]
        {
            if(::uwvm2::uwvm::io::show_runtime_warning)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"Debug interpreter requires full compile; forcing full compile.",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8" (runtime)\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                        u8"(runtime)\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile;
        }
# endif

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // Resolve uwvm-int auto after loading/import initialization, when executable and preloaded Wasm byte spans are
        // known, but before the runtime dispatch switch where only concrete lazy/full modes should remain.
        resolve_runtime_int_auto_mode();
# endif

        // Resolve global execution knobs after runtime storage exists.  Entry resolution needs runtime function type
        // storage, and compile-thread resolution publishes the value consumed by full-translation runtime code.
        resolve_runtime_compile_threads();
        auto runtime_entry{resolve_runtime_entry_invocation(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name)};

        // Dispatch matrix:
        //
        // 1. `execute_wasm_mode` separates executable `run` from non-executing modes.  The latter must have returned
        //    before runtime initialization and entry-buffer resolution.
        // 2. `global_runtime_mode` selects the execution strategy: pure lazy, lazy after whole-code validation, or full
        //    compilation before entry.
        // 3. Full compilation then dispatches by runtime compiler backend.  Lazy modes use one runtime-library entry point
        //    after checking that the selected backend can support on-demand compilation/materialization.
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details: [[fallthrough]];
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                /// This is a VM control-flow bug: non-executing modes must have returned before runtime dispatch.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::std::unreachable();
            }
            case ::uwvm2::uwvm::wasm::base::mode::debug_jit:
            {
# if ((defined(__linux__) && defined(SYS_pidfd_open) && defined(SCM_CREDENTIALS)) || \
      (defined(__APPLE__) && defined(__MACH__)) || (defined(_WIN32) && !defined(__CYGWIN__))) && \
     defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                // The authenticated channel is the sole command consumer in
                // an external launch. Its execution branch below also honors
                // debug mode's initial pause; stdin remains guest input.
                if(!debug_channel)
# endif
                {
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                namespace debugger = ::uwvm2::uwvm::debugger;
                namespace control = ::uwvm2::utils::control;
                control::launch_config config{};
                config.debug_enabled = true;
                // Let the authenticated host controller validate replace requests; the runtime currently rejects
                // publication until an independently validated single-function compiler and old-code owner exist.
                config.replacement_enabled = true;
                config.compiler = control::backend::llvm;
                config.mode = control::compile_mode::full;
                config.origin = control::launch_origin::console;
                // A process-local routing label, never an authentication secret.
                // The console authority is an owning host capability, not these bytes.
                config.instance[0] = 1u;
#  if defined(_WIN32) && !defined(__CYGWIN__)
                config.vm_process = ::fast_io::win32::GetCurrentProcessId();
#  elif defined(__unix__) || defined(__APPLE__)
                config.vm_process = static_cast<::std::uint64_t>(::uwvm2::runtime::lib::posix_abi::getpid_noexcept());
#  endif
                // Actual initialized CLI owner requests precise roots BEFORE
                // selected debug compilation. Empty/non-GC cohorts remain unchanged.
                ::uwvm2::runtime::gc::scoped_cli_gc_execution debug_gc_compilation_owner{};
                static_cast<void>(::uwvm2::runtime::lib::runtime_gc_prepare_cli_collection_host_api());
                auto controller{debugger::controller::create(config, 256u,
                    ::uwvm2::runtime::lib::llvm_jit_capture_debug_stack_host_api,
                    ::uwvm2::runtime::lib::llvm_jit_debug_read_memory_host_api)};
                if(!controller ||
                   ::uwvm2::runtime::lib::llvm_jit_configure_debug_session_host_api(controller->domain(), controller->observer(),
                       ::uwvm2::runtime::lib::llvm_jit_debug_safe_point_granularity::instruction) !=
                       ::uwvm2::runtime::lib::llvm_jit_debug_configure_result::ok ||
                       ::uwvm2::runtime::lib::llvm_jit_configure_debug_value_observation_host_api() !=
                       ::uwvm2::runtime::lib::llvm_jit_debug_configure_result::ok ||
                   !::uwvm2::runtime::lib::llvm_jit_prepare_debug_host_api() ||
                   !install_debug_source_maps(*controller) || !controller->arm_initial_pause())
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                        u8"Unable to prepare the LLVM full debugger before guest execution.\n\n",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
                }
                ::uwvm2::runtime::lib::full_compile_run_config run_config{};
                configure_runtime_entry_buffers(run_config, runtime_entry);
                // The actual launched FastIO owner remains exclusive until a
                // real OS join. notify_guest_exit is a debugger event, never a
                // physical thread-retirement receipt (TLS/launch RAII may follow).
                struct actual_guest_launch_state final
                {
                    ::std::shared_ptr<debugger::controller> control;
                    ::uwvm2::runtime::lib::full_compile_run_config config;
                };
                ::std::shared_ptr<actual_guest_launch_state> launch_state{};
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                { launch_state = ::std::make_shared<actual_guest_launch_state>(actual_guest_launch_state{controller, run_config}); }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(...) { return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error); }
#endif
                auto launched{::uwvm2::runtime::lib::runtime_launch_llvm_jit_debug_guest_worker_host_api(
                    controller->domain(), ::std::move(launch_state), +[](void* state) noexcept
                    {
                        // Runtime retains this real HOST recipe to actual OS/TLS
                        // join. The guest receives no native/thread/controller address.
                        auto& actual{*static_cast<actual_guest_launch_state*>(state)};
                        run_full_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, actual.config);
                        actual.control->notify_guest_exit(
                            ::uwvm2::runtime::lib::runtime_llvm_jit_debug_registered_worker_exit_code_host_api());
                    })};
                if(launched.status != ::uwvm2::runtime::lib::llvm_jit_debug_guest_worker_launch_status::started || !launched.worker)
                { return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error); }
                debugger::managed_cli_shutdown shutdown{controller, launched.worker};
                // Debug-mode WASI initialization never grants the management
                // input descriptor to guests (including through output handles).
                // This standalone launcher explicitly selects process
                // containment after actual physical EOF if finite managed
                // cleanup is still pending. Terminal CtrlD has a distinct editor
                // event and can retain the prompt for quit retry / quit force.
                auto result{debugger::run_console(*controller, ::std::addressof(shutdown), true)};
                if(result == debugger::console_exit::input_failure)
                {
                    // The native input adapter has restored its terminal/signal
                    // state. Still try the real finite cleanup pipeline; failure
                    // never destroys a joinable guest or native/code borrower.
                    auto const actual{shutdown.attempt_until(::std::chrono::steady_clock::now() + ::std::chrono::seconds{2})};
                    if(actual.phase == debugger::managed_cli_shutdown_phase::completed)
                    {
                        ::uwvm2::runtime::lib::reset_runtime_state_host_api();
                        return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
                    }
                    debugger::write_console_shutdown_result({nullptr, nullptr,
                        +[](void*, ::fast_io::string_view text) noexcept { ::fast_io::print(::fast_io::out(), text); }}, actual);
                }
                if(result == debugger::console_exit::managed_exit)
                {
                    // Native event/TLS borrower, runtime entry/cache workers and
                    // the actual launched guest OS thread all gave real ACKs.
                    // Only this path reaches the existing ordinary runtime reset.
                    break;
                }
                // The console writes directly to fast_io::out(), a native
                // observer. Diagnostic/runtime logs also use native files or
                // observers, so none retains a C stdio or fast_io output buffer.
                // Section-detail buffers finish in their separate mode; cache
                // file buffers belong to workers, whose owners remain live.
                // Only explicit quit force, the preselected physical-EOF policy
                // or failed input reaches process containment. It is not a join
                // or cleanup claim: pending request/controller/guest/code owners
                // remain live until the operating system terminates this process.
                #  if defined(_WIN32) && !defined(__CYGWIN__)
                ::fast_io::win32::ExitProcess(result == debugger::console_exit::input_failure ? 1u : 0u);
                ::std::unreachable(); // ExitProcess terminates this process; no owner may destruct early.
#  elif defined(__linux__) || defined(__APPLE__)
                ::uwvm2::runtime::lib::posix_abi::_exit_noexcept(result == debugger::console_exit::input_failure ? 1 : 0);
#  else
                ::std::_Exit(result == debugger::console_exit::input_failure ? 1 : 0);
#  endif
# else
                return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
# endif
                }
            }
            [[fallthrough]];
            case ::uwvm2::uwvm::wasm::base::mode::run:
            {
# if ((defined(__linux__) && defined(SYS_pidfd_open) && defined(SCM_CREDENTIALS)) || \
      (defined(__APPLE__) && defined(__MACH__)) || (defined(_WIN32) && !defined(__CYGWIN__))) && \
     defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                if(debug_channel)
                {
                    namespace debugger = ::uwvm2::uwvm::debugger;
                    namespace control = ::uwvm2::utils::control;
                    control::launch_config config{};
                    config.debug_enabled = true;
                    // The externally authorized debug channel shares the console's fail-closed replacement protocol.
                    config.replacement_enabled = true;
                    config.compiler = control::backend::llvm;
                    config.mode = control::compile_mode::full;
                    config.origin = control::launch_origin::console;
                    config.instance[0] = 1u; // Routing label, not an authentication secret.
#  if defined(_WIN32) && !defined(__CYGWIN__)
                    config.vm_process = ::fast_io::win32::GetCurrentProcessId();
#  else
                    config.vm_process = static_cast<::std::uint64_t>(::uwvm2::runtime::lib::posix_abi::getpid_noexcept());
#  endif
                    // Actual initialized CLI owner requests precise roots BEFORE
                    // selected debug compilation. Empty/non-GC cohorts remain unchanged.
                    ::uwvm2::runtime::gc::scoped_cli_gc_execution debug_gc_compilation_owner{};
                    static_cast<void>(::uwvm2::runtime::lib::runtime_gc_prepare_cli_collection_host_api());
                    auto controller{debugger::controller::create(config, 256u,
                        ::uwvm2::runtime::lib::llvm_jit_capture_debug_stack_host_api,
                        ::uwvm2::runtime::lib::llvm_jit_debug_read_memory_host_api)};
                    if(!controller ||
                       ::uwvm2::runtime::lib::llvm_jit_configure_debug_session_host_api(controller->domain(), controller->observer(),
                           ::uwvm2::runtime::lib::llvm_jit_debug_safe_point_granularity::instruction) !=
                           ::uwvm2::runtime::lib::llvm_jit_debug_configure_result::ok ||
                           ::uwvm2::runtime::lib::llvm_jit_configure_debug_value_observation_host_api() !=
                           ::uwvm2::runtime::lib::llvm_jit_debug_configure_result::ok ||
                       !::uwvm2::runtime::lib::llvm_jit_prepare_debug_host_api() ||
                       !install_debug_source_maps(*controller) ||
                       (::uwvm2::uwvm::wasm::storage::execute_wasm_mode == ::uwvm2::uwvm::wasm::base::mode::debug_jit &&
                        !controller->arm_initial_pause()))
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED), u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"Unable to prepare late LLVM full debugger before guest execution.\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
                    }
                    ::std::thread management{[controller, channel = ::std::move(debug_channel)]() noexcept
                    { channel->serve(*controller); }};
                    ::uwvm2::runtime::lib::full_compile_run_config cfg{};
                    configure_runtime_entry_buffers(cfg, runtime_entry);
                    run_full_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, cfg);
                    controller->notify_guest_exit(0);
                    management.join();
                    break;
                }
# endif
                // Only `run` reaches the runtime library.  `runtime_entry` has already been resolved so every branch below
                // can forward the same packed entry ABI buffers to its selected runtime entry point.
                // Runtime mode chooses the broad compilation strategy; runtime compiler chooses the backend that realizes
                // that strategy.  Each branch validates unsupported mode/backend combinations before calling runtime lib.
                switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode)
                {
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::auto_compile:
                    {
                        // `auto_compile` must be resolved before dispatch.  Reaching this branch means a future code path
                        // introduced auto mode without going through `resolve_runtime_int_auto_mode()`, or tried to combine
                        // it with a non-int backend.  Fail loudly instead of silently treating it like lazy/full.
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"auto_compile runtime mode was not resolved before runtime dispatch. auto_compile is only supported by the "
                                            u8"uwvm-int auto policy (-Rint, or -Rcc int without -Rcm), and LLVM-JIT/tiered backends require an explicit "
                                            u8"runtime mode (-Rcm lazy|full). ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                            u8"(runtime)\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile:
                    {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
                        // Lazy execution is available for backends that can compile/materialize functions on demand.
                        // Build-time feature macros determine which backend enum values can actually be selected, so the
                        // support check is assembled from the backend features present in this binary.
                        bool lazy_backend_supported{};
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_LLVM_JIT)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
                        if(!lazy_backend_supported) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"Lazy compilation currently supports the uwvm-int, llvm-jit, and tiered backends (-Rcc int|jit|tiered). ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                                u8"(runtime)\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        // Mandatory all-body fused admission occurs inside lazy initialization; native publication remains deferred.
                        ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
                        configure_runtime_entry_buffers(cfg, runtime_entry);
                        cfg.assume_full_code_verified = false;
                        run_initialized_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, cfg,
                                                     ::uwvm2::runtime::lib::lazy_compile_and_run_main_module);
# else
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"Lazy compilation is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                            u8"(runtime)\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
# endif

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile_with_full_code_verification:
                    {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
                        // Both lazy spellings require the same all-body fused
                        // admission. This alias never pure-validates then walks
                        // the same original body to translate it again.
                        bool lazy_backend_supported{};
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_LLVM_JIT)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
                        if(!lazy_backend_supported) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"Lazy compilation currently supports the uwvm-int, llvm-jit, and tiered backends (-Rcc int|jit|tiered). ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                                u8"(runtime)\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        // No speculative validity certificate is passed. The
                        // artifact factory completes authoritative validation
                        // and translation together before any guest publication.
                        ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
                        configure_runtime_entry_buffers(cfg, runtime_entry);
                        cfg.assume_full_code_verified = false;
                        run_initialized_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, cfg,
                                                     ::uwvm2::runtime::lib::lazy_compile_and_run_main_module);
# else
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"Lazy compilation with full code verification is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                            u8"(runtime)\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
# endif

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile:
                    {
                        // Full compilation hands the complete main module to the selected backend before running the entry.
                        // The same runtime config shape is used for interpreter translation and LLVM JIT full translation.
                        switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
                        {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only:
                            {
                                // Full compile with the uwvm-int interpreter backend.
                                ::uwvm2::runtime::lib::full_compile_run_config cfg{};
                                configure_runtime_entry_buffers(cfg, runtime_entry);
                                run_full_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, cfg);

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter:
                            {
                                // The enum can be selected in debug-interpreter builds, but this runtime path is not
                                // implemented yet, so fail explicitly instead of falling into another backend.
                                ::fast_io::io::perr(
                                    ::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"Debug Interpreter is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime)\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                            {
                                // Tiered compilation is inherently a lazy/tiered strategy and conflicts with the full
                                // compile runtime mode.  Report the conflict before runtime library entry.
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"Tiered compilation conflicts with full compilation. ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                                    u8"(runtime)\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                            {
                                // Full compile with the LLVM JIT backend.  LLVM policy details are resolved inside the
                                // runtime library from the globally configured runtime-mode storage.
                                ::uwvm2::runtime::lib::full_compile_run_config cfg{};
                                configure_runtime_entry_buffers(cfg, runtime_entry);
                                run_full_module_graph(::uwvm2::uwvm::wasm::storage::active_execute_wasm().module_name, cfg);

                                break;
                            }
# endif
                            [[unlikely]] default:
                            {
/// @warning Unhandled runtime compiler value for full compilation.  This indicates a missing implementation branch.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
                break;
            }
            /// @warning Extension point: new executable modes require dispatch, teardown, and diagnostics here.
            /// @todo add more modes here
            [[unlikely]] default:
            {
                /// @warning Extension point: unhandled execute mode; add a dispatch branch before introducing a new executable mode.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::std::unreachable();
            }
        }

        // Normal executable-mode exit invalidates backend-neutral caches, stops
        // lazy workers, and releases selected-backend artifacts before their
        // runtime module storage is destroyed.
        ::uwvm2::runtime::lib::reset_runtime_state_host_api();

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
#endif
    }
}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
