#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>

#include <array>
#include <cstdint>
#include <cstring>

// Exercise the actual bounded Apple record parser on any DWARF build. No JIT
// code or fake metadata is registered with the process unwinder by this test.
#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
namespace
{
    using bytes_t = ::std::array<::std::uint8_t, 64>;

    template <typename Integer>
    void put(bytes_t& bytes, ::std::size_t offset, Integer value)
    {
        ::std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    bool visits(bytes_t& bytes, ::std::size_t size, ::std::size_t expected_count,
                ::std::size_t first = 0, ::std::size_t second = 0)
    {
        ::std::size_t count{};
        bool valid{true};
        ::uwvm2::runtime::compiler::llvm_jit::details::visit_runtime_llvm_jit_eh_frame_fdes(
            bytes.data(), size,
            [&](::std::uint8_t* frame) noexcept
            {
                auto const offset{static_cast<::std::size_t>(frame - bytes.data())};
                valid = valid && count < expected_count && offset == (count == 0 ? first : second);
                ++count;
            });
        return valid && count == expected_count;
    }
}
#endif

int main()
{
#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
    bytes_t bytes{};
    if(!visits(bytes, 0, 0) || !visits(bytes, 3, 0) || !visits(bytes, 4, 0)) { return 1; }

    // CIE (length 4, ID 0), then two FDEs, then a terminator; bytes after the
    // terminator must not be visited even if they resemble another FDE.
    put(bytes, 0, ::std::uint32_t{4});
    put(bytes, 8, ::std::uint32_t{4});
    put(bytes, 12, ::std::uint32_t{12});
    put(bytes, 16, ::std::uint32_t{4});
    put(bytes, 20, ::std::uint32_t{20});
    put(bytes, 28, ::std::uint32_t{4});
    put(bytes, 32, ::std::uint32_t{32});
    if(!visits(bytes, bytes.size(), 2, 8, 16)) { return 2; }
    if(!visits(bytes, 15, 0) || !visits(bytes, 16, 1, 8)) { return 3; }

    // Truncated extended length, extended FDE, and an oversized length.
    bytes = {};
    put(bytes, 0, ::std::uint32_t{0xffffffffu});
    if(!visits(bytes, 11, 0)) { return 4; }
    put(bytes, 4, ::std::uint64_t{8});
    put(bytes, 12, ::std::uint64_t{12});
    if(!visits(bytes, 20, 1) || !visits(bytes, 19, 0)) { return 5; }
    put(bytes, 12, ::std::uint64_t{0});
    if(!visits(bytes, 20, 0)) { return 6; }
    put(bytes, 4, ::std::uint64_t{0xffffffffffffffffull});
    if(!visits(bytes, bytes.size(), 0)) { return 7; }

    // Malformed short payload and normal length beyond the given section.
    bytes = {};
    put(bytes, 0, ::std::uint32_t{3});
    if(!visits(bytes, 7, 0)) { return 8; }
    put(bytes, 0, ::std::uint32_t{64});
    if(!visits(bytes, bytes.size(), 0)) { return 9; }
#else
    // An unavailable backend is a skipped test, never a successful parser run.
    return 77;
#endif
}

#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
