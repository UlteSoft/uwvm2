#pragma once
// Private source candidate. This header does not register wasi/thread-spawn.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stop_token>
#include <uwvm2/uwvm/runtime/initializer/threaded_child_world.h>
#include <uwvm2/runtime/wasm_threads/thread_group.h>

#if defined(__linux__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE)
namespace uwvm2::runtime::compiler::uwvm_int::threaded
{
    using child_world = ::uwvm2::uwvm::runtime::initializer::ThreadedChildWorld;
    enum class status : unsigned
    {
        ok, unsupported_profile, invalid_world, validation_failed, resource_limit,
        cancelled, trap_unreachable, trap_divide_by_zero, trap_integer_overflow,
        trap_memory, trap_unaligned, trap_table, trap_null, trap_signature,
        proc_exit, host_denied, host_failure, invariant_failure, already_entered,
        trap_invalid_conversion, trap_uncaught_exception, memory_failure
    };
    struct execution_result
    {
        status code{status::ok};
        ::std::size_t module{}, function{}, wasm_offset{};
        ::std::uint32_t exit_code{};
        [[nodiscard]] bool normal() const noexcept { return code == status::ok; }
    };
    struct limits
    {
        // Zero is never interpreted as unlimited. Publication must charge these
        // actual private allocations to the native group's active/total budget.
        ::std::size_t storage_bytes{4uz << 20u};
        ::std::size_t source_bytes{4uz << 20u};
        ::std::size_t instruction_bytes{32uz << 20u};
        ::std::size_t functions{65536uz};
        ::std::size_t values{65536uz};
        ::std::size_t controls{4096uz};
        ::std::size_t calls{256uz};
        ::std::size_t host_bytes{65536uz};
        ::std::size_t exception_objects{1024uz};
        ::std::size_t exception_values{65536uz};
    };
    // ThreadedGroup owns the only constructible ChildLease. The actual Runtime
    // supplies admit_import/dispatch_import; missing implementation is a link
    // failure, never a default-allow provider or a fabricated readiness boolean.
}

#endif // Linux native-thread, C++ exceptions, non-module threaded source profile
