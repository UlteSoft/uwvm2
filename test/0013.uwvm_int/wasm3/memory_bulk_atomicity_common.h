#pragma once

// The executable parent owns one initialized runtime module. A forked child
// invokes one deliberately invalid operation and reads both live memories
// inside the trap callback before runtime cleanup. This establishes the
// no-partial-write property in the *same* module instance that trapped.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string_view>
#include <type_traits>
#include <sys/wait.h>
#include <unistd.h>

namespace uwvm2test::wasm3_bulk_atomicity
{
    using namespace uwvm2test::uwvm_int_strict;
    constexpr std::size_t page_size{65536};
    constexpr std::string_view passive_payload{"QRSTUVWXYZabcdefghijklmnopqrstuv"};
    static_assert(passive_payload.size() == 32);

    inline std::byte volatile const* source_bytes{};
    inline std::byte volatile const* target_bytes{};
    inline std::size_t committed_length{page_size};

    [[nodiscard]] constexpr std::byte source_pattern(std::size_t i) noexcept { return std::byte{static_cast<unsigned char>((i * 37u + 13u) & 255u)}; }

    inline void reset_memories(std::byte* source, std::byte* target, std::size_t length) noexcept
    {
        for(std::size_t i{}; i != length; ++i) { source[i] = source_pattern(i); }
        std::memset(target, 0xa5, length);
        source_bytes = source;
        target_bytes = target;
        committed_length = length;
    }

    [[noreturn]] inline void verify_unchanged_at_trap() noexcept
    {
        // No allocation or stdio in this cold callback. The prepared module
        // and both committed mappings stay alive until _exit. Volatile loads
        // observe even a short partial prefix written before the trap.
        for(std::size_t i{}; i != committed_length; ++i)
        {
            if(source_bytes[i] != source_pattern(i)) { ::_exit(71); }
        }
        for(std::size_t i{}; i != committed_length; ++i)
        {
            if(target_bytes[i] != std::byte{0xa5}) { ::_exit(72); }
        }
        // write is async-signal-safe. The parent captures one marker for every
        // trap, then archives its scenario and exact byte count in results.json.
        constexpr char marker[]{"READBACK-OK\n"};
        if(::write(STDOUT_FILENO, marker, sizeof(marker) - 1) != static_cast<ssize_t>(sizeof(marker) - 1)) { ::_exit(76); }
        ::_exit(0);
    }

    [[nodiscard]] inline bool observer_selftest(std::byte* source, std::byte* target) noexcept
    {
        // Negative controls run only in COW children. They prove that the
        // callback fails on a one-byte mutation in either selected or
        // unselected memory before any real Wasm trap is accepted.
        for(unsigned selected{}; selected != 2; ++selected)
        {
            auto const child{::fork()};
            if(child < 0) { return false; }
            if(child == 0)
            {
                (selected == 0 ? source : target)[128] = std::byte{0x5a};
                verify_unchanged_at_trap();
            }
            int status{};
            if(::waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != (selected == 0 ? 71 : 72)) { return false; }
        }
        return true;
    }

    [[nodiscard]] inline bool verify_positive(std::byte const* source,
                                              std::byte const* target,
                                              std::size_t length,
                                              std::uint64_t destination,
                                              std::uint64_t from,
                                              std::uint64_t count,
                                              bool copy) noexcept
    {
        for(std::size_t i{}; i != length; ++i)
        {
            if(source[i] != source_pattern(i)) { return false; }
            std::byte expected{0xa5};
            if(i >= destination && i - destination < count)
            {
                auto const index{static_cast<std::size_t>(from + i - destination)};
                expected = copy ? source_pattern(index) : std::byte{static_cast<unsigned char>(passive_payload[index])};
            }
            if(target[i] != expected) { return false; }
        }
        return true;
    }

    [[nodiscard]] inline byte_vec parameters(bool copy, bool memory64, std::uint64_t dst, std::uint64_t src, std::uint64_t count)
    {
        byte_vec result{};
        auto const append{[&](auto const& value)
                          {
                              auto const* bytes{reinterpret_cast<std::byte const*>(std::addressof(value))};
                              result.insert(result.end(), bytes, bytes + sizeof(value));
                          }};
        if(memory64)
        {
            append(dst);
            if(copy)
            {
                append(src);
                append(count);
            }
            else
            {
                append(static_cast<std::uint32_t>(src));
                append(static_cast<std::uint32_t>(count));
            }
        }
        else
        {
            append(static_cast<std::uint32_t>(dst));
            append(static_cast<std::uint32_t>(src));
            append(static_cast<std::uint32_t>(count));
        }
        return result;
    }

    [[nodiscard]] inline byte_vec read_wasm(char const* path)
    {
        std::ifstream input(path, std::ios::binary);
        if(!input) { return {}; }
        byte_vec bytes{};
        for(char c; input.get(c);) { bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
        return input.eof() ? bytes : byte_vec{};
    }

    struct scenario
    {
        char const* name;
        std::uint64_t dst{}, src{}, len{};
    };

    [[nodiscard]] inline std::array<scenario, 4> invalid_scenarios(bool copy, std::size_t length) noexcept
    {
        if(copy)
        {
            return {
                {{"source-partial", 128, length - 8, 16},
                 {"destination-partial", length - 8, 0, 16},
                 {"source-zero-beyond", 128, length + 1, 0},
                 {"destination-zero-beyond", length + 1, 0, 0}}
            };
        }
        return {
            {{"data-source-partial", 128, passive_payload.size() - 8, 16},
             {"destination-partial", length - 8, 0, 16},
             {"data-source-zero-beyond", 128, passive_payload.size() + 1, 0},
             {"destination-zero-beyond", length + 1, 0, 0}}
        };
    }
}  // namespace uwvm2test::wasm3_bulk_atomicity
