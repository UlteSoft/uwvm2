/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string_view.h>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::wasm_events
{
    // Opcode categories are BEFORE-INSTRUCTION observations, not committed
    // effects. The separate trap category requires a real runtime failure. A throw
    // can trap while evaluating a reference, a memory instruction can trap, and
    // a GC instruction does not imply the collector ran. Successful catch and
    // allocation/collection events require separate, actual runtime witnesses.
    enum class category : ::std::uint32_t
    {
        all = 0u, throw_instruction = 1u, exception_instruction = 2u,
        gc_instruction = 4u, memory_instruction = 8u, table_instruction = 16u,
        atomic_instruction = 32u, call_instruction = 64u, reference_instruction = 128u,
        simd_instruction = 256u, control_instruction = 512u, trap = 1024u, uncaught = 2048u
    };
    enum class command_kind
    { trace_enable, trace_disable, trace_clear, trace_read, catch_set, catch_list,
      catch_delete, catch_enable, catch_disable };
    // Keep even worst-case decimal records inside the 64 KiB management
    // reply. The sequence cursor is a diagnostic label, never an authority.
    inline constexpr ::std::uint64_t maximum_trace_page_records{128u};
    struct request
    {
        command_kind command{command_kind::trace_read};
        category event{category::all};
        ::std::uint64_t module{}, function{}, identifier{};
        ::std::uint64_t after_sequence{}, maximum_records{maximum_trace_page_records};
        bool all_functions{};
    };
    struct opcode
    {
        ::std::uint32_t categories{}, extended{};
        ::std::uint8_t primary{};
        bool available{}, prefixed{};
    };
    [[nodiscard]] inline constexpr bool known_category(category value) noexcept
    {
        switch(value)
        {
            case category::all: case category::throw_instruction: case category::exception_instruction:
            case category::gc_instruction: case category::memory_instruction: case category::table_instruction:
            case category::atomic_instruction: case category::call_instruction: case category::reference_instruction:
            case category::simd_instruction: case category::control_instruction: case category::trap: case category::uncaught: return true;
        }
        return false;
    }
    [[nodiscard]] inline constexpr bool matches(category filter, opcode instruction) noexcept
    { return filter == category::all || (instruction.available && (instruction.categories & static_cast<::std::uint32_t>(filter)) != 0u); }
    [[nodiscard]] inline constexpr ::fast_io::string_view name(category value) noexcept
    {
        switch(value)
        {
            case category::all: return "all";
            case category::throw_instruction: return "throw";
            case category::exception_instruction: return "exception";
            case category::gc_instruction: return "gc";
            case category::memory_instruction: return "memory";
            case category::table_instruction: return "table";
            case category::atomic_instruction: return "atomic";
            case category::call_instruction: return "call";
            case category::reference_instruction: return "reference";
            case category::simd_instruction: return "simd";
            case category::control_instruction: return "control";
            case category::trap: return "trap";
            case category::uncaught: return "uncaught";
        }
        return "invalid";
    }
    [[nodiscard]] inline constexpr bool parse_category(::fast_io::string_view text, category& value) noexcept
    {
        constexpr ::fast_io::array<category, 13u> values{category::all, category::throw_instruction,
            category::exception_instruction, category::gc_instruction, category::memory_instruction,
            category::table_instruction, category::atomic_instruction, category::call_instruction,
            category::reference_instruction, category::simd_instruction, category::control_instruction, category::trap, category::uncaught};
        for(auto candidate : values)
        { if(text == name(candidate)) { value = candidate; return true; } }
        return false;
    }
    // The caller must independently prove offset is a real emitted safe point
    // of this exact privately bound function generation. This diagnostic helper
    // only reads the first opcode and bounded prefix LEB; it does not validate,
    // guess instruction alignment, or follow guest/request-supplied pointers.
    [[nodiscard]] inline opcode decode(::std::span<::std::byte const> expression, ::std::uint64_t offset) noexcept
    {
        if(offset >= expression.size()) { return {}; }
        opcode result{};
        auto const index{static_cast<::std::size_t>(offset)};
        // [owned expression bytes ... index ...] expression_end
        // [safe                               ] unsafe (one-past)
        //                            ^^ index < size precedes this byte read.
        result.primary = ::std::to_integer<::std::uint8_t>(expression[index]);
        result.available = true;
        auto add = [&](category value) { result.categories |= static_cast<::std::uint32_t>(value); };
        auto const op{result.primary};
        if(op == 0xfbu || op == 0xfcu || op == 0xfdu || op == 0xfeu)
        {
            if(expression.size() - index <= 1u) { return {}; }
            // [owned expression ... prefix][at least one LEB byte ...] end
            // [safe                                              ] unsafe (one-past)
            //                               ^^ index+1 <= size proven above;
            // endpoints retain this same live owned expression, never wire addresses.
            auto const* first{reinterpret_cast<unsigned char const*>(expression.data()) + index + 1u};
            auto const* end{reinterpret_cast<unsigned char const*>(expression.data()) + expression.size()};
            auto const decoded{::fast_io::parse_by_scan(first, end, ::fast_io::mnp::leb128_get(result.extended))};
            if(decoded.code != ::fast_io::parse_code::ok || decoded.iter <= first || decoded.iter > end) { return {}; }
            // [prefix][bounded complete LEB][remaining expression bytes] end
            // [safe                                                  ] unsafe (one-past)
            //                                 ^^ decoded.iter is not retained or advanced.
            result.prefixed = true;
            if(op == 0xfbu)
            {
                add(category::gc_instruction); add(category::reference_instruction);
                if(result.extended == 24u || result.extended == 25u) { add(category::control_instruction); }
            }
            else if(op == 0xfdu)
            {
                add(category::simd_instruction);
                if(result.extended <= 11u || (result.extended >= 84u && result.extended <= 93u)) { add(category::memory_instruction); }
            }
            else if(op == 0xfeu) { add(category::atomic_instruction); if(result.extended != 3u) { add(category::memory_instruction); } }
            else if(result.extended >= 8u && result.extended <= 11u) { add(category::memory_instruction); }
            else if(result.extended >= 12u && result.extended <= 17u) { add(category::table_instruction); }
            return result;
        }
        if(op == 0x08u || op == 0x0au) { add(category::throw_instruction); add(category::exception_instruction); }
        if(op == 0x1fu) { add(category::exception_instruction); }
        if((op >= 0x28u && op <= 0x40u)) { add(category::memory_instruction); }
        if(op == 0x25u || op == 0x26u) { add(category::table_instruction); }
        if((op >= 0x10u && op <= 0x15u)) { add(category::call_instruction); }
        if(op >= 0xd0u && op <= 0xd6u) { add(category::reference_instruction); }
        if(op <= 0x05u || op == 0x08u || op == 0x0au || (op >= 0x0bu && op <= 0x15u) || op == 0x1fu || op == 0xd5u || op == 0xd6u)
        { add(category::control_instruction); }
        return result;
    }
    struct record
    {
        ::std::uint64_t sequence{}, participant{}, module{}, function{}, offset{}, runtime_epoch{}, function_generation{};
        opcode instruction{};
        bool trapped{}; // real failure witnessed; saved operands remain before-opcode
        bool uncaught{}; // actual throw with no matching current Wasm handler; before unwind
    };
    struct catchpoint
    {
        ::std::uint64_t identifier{}, module{}, function{}, runtime_epoch{};
        category event{category::all};
        bool all_functions{}, enabled{};
    };
    // Fixed storage: recording on the genuine debug callback never allocates,
    // performs I/O or invokes a script. Sequence labels never wrap/reappear.
    class trace_buffer
    {
        ::fast_io::array<record, 512u> records_{};
        ::std::size_t begin_{}, size_{};
        ::std::uint64_t next_{1u}, overwritten_{};
    public:
        [[nodiscard]] bool append(record entry) noexcept
        {
            if(next_ == 0u) { return false; }
            entry.sequence = next_++;
            if(size_ == records_.size())
            {
                records_[begin_] = entry;
                begin_ = (begin_ + 1u) % records_.size();
                if(overwritten_ != (::std::numeric_limits<::std::uint64_t>::max)()) { ++overwritten_; }
            }
            else { records_[(begin_ + size_++) % records_.size()] = entry; }
            return true;
        }
        void clear() noexcept { begin_ = size_ = 0u; overwritten_ = 0u; }
        [[nodiscard]] ::std::size_t size() const noexcept { return size_; }
        [[nodiscard]] ::std::uint64_t overwritten() const noexcept { return overwritten_; }
        [[nodiscard]] bool exhausted() const noexcept { return next_ == 0u; }
        [[nodiscard]] record const& at(::std::size_t index) const noexcept
        {
            if(index >= size_) { ::fast_io::fast_terminate(); }
            return records_[(begin_ + index) % records_.size()];
        }
    };
}
