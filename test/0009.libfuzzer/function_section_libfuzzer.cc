/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/base/impl.h>
#endif

static inline void push_byte(std::vector<std::byte>& buf, std::uint8_t v) { buf.push_back(static_cast<std::byte>(v)); }

static inline void push_mem(std::vector<std::byte>& buf, void const* p, std::size_t n)
{
    auto const* b = static_cast<std::byte const*>(p);
    buf.insert(buf.end(), b, b + n);
}

static inline void push_leb_u32(std::vector<std::byte>& buf, std::uint32_t v)
{
    while(true)
    {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
        v >>= 7u;
        if(v) { b |= 0x80u; push_byte(buf, b); }
        else { push_byte(buf, b); break; }
    }
}

static inline void push_header(std::vector<std::byte>& out)
{
    push_mem(out, "\0asm", 4);
    push_mem(out, "\x01\0\0\0", 4);
}

static inline void push_section(std::vector<std::byte>& out, std::uint8_t id, std::vector<std::byte> const& payload)
{
    push_byte(out, id);
    push_leb_u32(out, static_cast<std::uint32_t>(payload.size()));
    push_mem(out, payload.data(), payload.size());
}

// Create a type section from fuzz bytes: N types of (params/results)
static inline std::vector<std::byte> make_type_section_from(std::uint8_t const* data, std::size_t size, std::size_t& consumed)
{
    std::vector<std::byte> p;
    std::uint32_t type_count = (size ? (1u + (data[0] & 0x03u)) : 1u); // 1..4
    consumed = size ? 1 : 0;
    push_leb_u32(p, type_count);

    for(std::uint32_t i = 0; i < type_count; ++i)
    {
        // prefix: mostly 0x60, sometimes corrupt to trigger errors
        std::uint8_t prefix = 0x60;
        if(consumed < size)
        {
            std::uint8_t b = data[consumed++];
            if((b & 0x1Fu) == 0x1Fu) prefix = static_cast<std::uint8_t>(b); // occasional invalid prefix
        }
        push_byte(p, prefix);

        // param count 0..4
        std::uint32_t pc = 0;
        if(consumed < size) pc = static_cast<std::uint32_t>(data[consumed++] & 0x03u);
        push_leb_u32(p, pc);
        for(std::uint32_t k = 0; k < pc; ++k)
        {
            // mostly valid valtype, sometimes random
            std::uint8_t vt = 0x7F; // i32
            if(consumed < size)
            {
                std::uint8_t b = data[consumed++];
                static constexpr std::uint8_t candidates[4]{0x7F,0x7E,0x7D,0x7C};
                vt = (b & 0x80u) ? static_cast<std::uint8_t>(b) : candidates[b & 3u];
            }
            push_byte(p, vt);
        }

        // result count 0..1 (MVP)
        std::uint32_t rc = 0;
        if(consumed < size) rc = static_cast<std::uint32_t>(data[consumed++] & 0x01u);
        push_leb_u32(p, rc);
        for(std::uint32_t k = 0; k < rc; ++k)
        {
            std::uint8_t vt = 0x7F;
            if(consumed < size)
            {
                std::uint8_t b = data[consumed++];
                static constexpr std::uint8_t candidates[4]{0x7F,0x7E,0x7D,0x7C};
                vt = (b & 0x80u) ? static_cast<std::uint8_t>(b) : candidates[b & 3u];
            }
            push_byte(p, vt);
        }
    }
    return p;
}

static inline std::vector<std::byte> make_function_section_from(std::uint8_t const* data, std::size_t size, std::size_t& consumed, std::uint32_t type_count)
{
    std::vector<std::byte> p;
    std::uint32_t func_count = 0;
    if(consumed < size) func_count = static_cast<std::uint32_t>(data[consumed++] & 0x03u); // 0..3
    push_leb_u32(p, func_count);
    for(std::uint32_t i = 0; i < func_count; ++i)
    {
        std::uint32_t tidx = 0;
        if(consumed < size)
        {
            std::uint8_t b = data[consumed++];
            bool oob = (b & 0x80u) != 0u; // sometimes out-of-range
            if(oob) tidx = static_cast<std::uint32_t>(type_count + (b & 0x7Fu));
            else    tidx = static_cast<std::uint32_t>(b % (type_count ? type_count : 1u));
        }
        push_leb_u32(p, tidx);
    }
    return p;
}

// Minimal code section to avoid mismatch overshadowing function errors
static inline std::vector<std::byte> make_code_section(std::uint32_t code_count)
{
    std::vector<std::byte> p; push_leb_u32(p, code_count);
    for(std::uint32_t i = 0; i < code_count; ++i)
    {
        std::vector<std::byte> body; push_leb_u32(body, 0); push_byte(body, 0x0B);
        push_leb_u32(p, static_cast<std::uint32_t>(body.size())); push_mem(p, body.data(), body.size());
    }
    return p;
}

// Scalar correctness checker for function section type indices,
// adapted from wasm1::features::check_function_section in function_section.h.
template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
static inline void scalar_check_function_section(
    [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
        ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t> sec_adl,
    ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
    ::std::byte const* const section_begin,
    ::std::byte const* const section_end) noexcept
{
    // Note that section_begin may be equal to section_end
    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

    using namespace ::uwvm2::parser::wasm::standard::wasm1;
    using namespace ::uwvm2::parser::wasm::standard::wasm1::features;

    // get function_section_storage_t from storages
    auto const& functionsec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};

    // check has typesec
    auto const& typesec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

    auto section_curr{section_begin};

    // [before_section ... ] | func_count typeidx1 ...
    // [        safe       ] | unsafe (could be the section_end)
    //                         ^^ section_curr

    type::wasm_u32 func_count;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)
    auto const [func_count_next, func_count_err]{::fast_io::parse_by_scan(
        reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
        reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
        ::fast_io::mnp::leb128_get(func_count))};

    if(func_count_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
    }

    // [before_section ... | func_count ...] typeidx1 ...
    // [             safe                  ] unsafe (could be the section_end)
    //                       ^^ section_curr

    type::wasm_u32 func_counter{};  // use for check

    section_curr = reinterpret_cast<::std::byte const*>(func_count_next);  // never out of bounds

    // [before_section ... | func_count ...] typeidx1 ...
    // [              safe                 ] unsafe (could be the section_end)
    //                                       ^^ section_curr

    // handle it
    auto const type_section_count{static_cast<type::wasm_u32>(typesec.types.size())};

    ::std::size_t index{};

    while(section_curr != section_end) [[likely]]
    {
        // Ensuring the existence of valid information

        // [ ... typeidx1] ... typeidx2 ...
        // [     safe    ] unsafe (could be the section_end)
        //       ^^ section_curr

        // check function counter
        // Ensure content is available before counting (section_curr != section_end)

        auto const func_counter_end{static_cast<type::wasm_u32>(func_counter + 1u)};

        // check overflow
        if(func_counter_end < func_counter) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }

        func_counter = func_counter_end;

        if(func_counter > func_count) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }

        // [ ... typeidx1] ... typeidx2 ...
        // [     safe    ] unsafe (could be the section_end)
        //       ^^ section_curr

        type::wasm_u32 typeidx;  // No initialization necessary

        auto const [typeidx_next, typeidx_err]{::fast_io::parse_by_scan(
            reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
            ::fast_io::mnp::leb128_get(typeidx))};

        if(typeidx_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }

        // [ ... typeidx1 ...] typeidx2 ...
        // [      safe       ] unsafe (could be the section_end)
        //       ^^ section_curr

        // check: type_index should less than type_section_count
        if(typeidx >= type_section_count) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }

        if(typeidx != functionsec.funcs.index_unchecked(index)) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }

        section_curr = reinterpret_cast<::std::byte const*>(typeidx_next);

        // [ ... typeidx1 ...] typeidx2 ...
        // [      safe       ] unsafe (could be the section_end)
        //                     ^^ section_curr

        ++index;
    }

    // [before_section ... | func_count ... typeidx1 ... ...] (end)
    // [                       safe                         ] unsafe (could be the section_end)
    //                                                        ^^ section_curr
}

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    std::size_t off = 0;
    std::vector<std::byte> mod; mod.reserve(1024);
    push_header(mod);

    // Type section
    std::size_t type_used{};
    auto type_sec = make_type_section_from(data + off, (size > off ? size - off : 0), type_used);
    off += type_used;
    push_section(mod, 1, type_sec);

    // Import empty (to keep ordering canonical)
    std::vector<std::byte> import_payload; push_leb_u32(import_payload, 0); push_section(mod, 2, import_payload);

    // Function section
    std::size_t func_used{};
    std::uint32_t type_count = 0; // decode back type_count from payload front: first leb after header
    // Parse out type_count quickly: assume first bytes are leb128 of type_count
    // For robustness in fuzz, fallback to 1 when cannot decode simply
    {
        // naive decode from type_sec[0..5]
        std::uint32_t acc = 0; int shift = 0;
        for(std::size_t i = 0; i < type_sec.size() && i < 5; ++i)
        {
            std::uint8_t b = static_cast<std::uint8_t>(type_sec[i]);
            acc |= (static_cast<std::uint32_t>(b & 0x7Fu) << shift);
            if((b & 0x80u) == 0u) { type_count = acc; break; }
            shift += 7;
        }
        if(type_count == 0) type_count = 1; // guarantee at least 1 for modulo
    }
    auto func_sec = make_function_section_from(data + off, (size > off ? size - off : 0), func_used, type_count);
    off += func_used;
    push_section(mod, 3, func_sec);

    // Code section: align code_count with function_count to avoid overshadowing errors
    // Quick decode func_count from func_sec first leb
    std::uint32_t func_count = 0;
    {
        std::uint32_t acc = 0; int shift = 0;
        for(std::size_t i = 0; i < func_sec.size() && i < 5; ++i)
        {
            std::uint8_t b = static_cast<std::uint8_t>(func_sec[i]);
            acc |= (static_cast<std::uint32_t>(b & 0x7Fu) << shift);
            if((b & 0x80u) == 0u) { func_count = acc; break; }
            shift += 7;
        }
    }
    push_section(mod, 10, make_code_section(func_count));

    // Parse and run scalar correctness check on the decoded function section.
    try
    {
        using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
        ::uwvm2::parser::wasm::base::error_impl err{};
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Feature> fs_para{};
        auto module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Feature>(
            reinterpret_cast<::std::byte const*>(mod.data()),
            reinterpret_cast<::std::byte const*>(mod.data() + mod.size()),
            err,
            fs_para);

        // If the function section was successfully parsed, verify that the
        // scalar walk over its bytes matches the stored vectypeidx data.
        auto const& functionsec{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(module.sections)};

        if(functionsec.sec_span.sec_begin && functionsec.sec_span.sec_end)
        {
            auto const* const sec_begin{
                reinterpret_cast<::std::byte const*>(functionsec.sec_span.sec_begin)};
            auto const* const sec_end{
                reinterpret_cast<::std::byte const*>(functionsec.sec_span.sec_end)};

            scalar_check_function_section(
                ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<
                    ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>{},
                module,
                sec_begin,
                sec_end);
        }
    }
    catch(...)
    {
        // swallow
    }

    return 0;
}

