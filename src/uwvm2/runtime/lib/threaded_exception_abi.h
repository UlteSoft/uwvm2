#pragma once
// PRIVATE threaded JIT native ABI. Never a Wasm import, Guest pointer authority,
// ordinary g_runtime exception object, native unwind or Ready-provider callback.
#include <cstddef>
#include <cstdint>
namespace uwvm2::runtime::lib
{
    extern "C++"
    {
        struct threaded_exception_bridge_api final
        {
            [[nodiscard]] static ::std::uintptr_t pending() noexcept;
            static void publish(::std::uintptr_t module,::std::uintptr_t tag,
                ::std::uintptr_t tuple,::std::size_t bytes) noexcept;
            [[nodiscard]] static ::std::uintptr_t matches(::std::uintptr_t token,::std::uintptr_t tag) noexcept;
            static void copy(::std::uintptr_t token,::std::uintptr_t tag,
                ::std::uintptr_t destination,::std::size_t bytes) noexcept;
            static void make_reference(::std::uintptr_t token,::std::uintptr_t destination) noexcept;
            static void consume(::std::uintptr_t token) noexcept;
            static void throw_reference(::std::uintptr_t module,::std::uintptr_t reference) noexcept;
        };
    }
}
