// Actual Windows shared-memory ownership regression. Header-only direct API;
// the fresh fast_io named-module provider is separately precompiled EH/noEH.
// Execute only through the owned VM/cgroup keeper. The two noEH failure cases
// require separate child executables and must never be treated as recoverable.
#include <cstddef>
#include <cstdint>
#include <utility>
#include <fast_io.h>
#if defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
namespace
{
bool check(bool success, char const* description)
{
    if(!success) { ::fast_io::perrln("FAIL ", ::fast_io::mnp::os_c_str(description)); }
    return success;
}
bool retired(void* handle) noexcept
{
    ::std::uint_least32_t flags{};
    int const result{::fast_io::win32::GetHandleInformation(handle, __builtin_addressof(flags))};
    auto const error{::fast_io::win32::GetLastError()};
    return result == 0 && error == 6u; // Actual ERROR_INVALID_HANDLE, no later native open.
}
::fast_io::win32_file readonly_section()
{
    void* const invalid{reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1))};
    void* const handle{::fast_io::win32::CreateFileMappingW(invalid, nullptr, 0x08000002u, 0u, 4096u, nullptr)};
    // Immediate existing fast_io ownership, before any fallible mapping operation.
    return ::fast_io::win32_file{handle};
}
::std::uint_least32_t nt_readonly_write_probe(void* handle) noexcept
{
    void* address{}; ::std::size_t bytes{};
    void* const process{reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1))};
    // Native NtMapViewOfSection borrows exactly one complete address cell and
    // one size cell until synchronous return; no input/output pointer advances.
    auto const status{::fast_io::win32::nt::nt_map_view_of_section<false>(handle, process,
        __builtin_addressof(address), 0u, 0u, nullptr, __builtin_addressof(bytes),
        ::fast_io::win32::nt::section_inherit::ViewShare, 0u, 0x00000004u)};
    if(status == 0u) { (void)::fast_io::win32::nt::nt_unmap_view_of_section<false>(process, address); }
    return status;
}
template<typename Shared>
bool exercise_mapping()
{
    void* first{}; void* second{};
    {
        Shared original{4096uz};
        if(!check(original && original.size() >= 4096uz && original.data() != nullptr, "create actual 4KiB mapping")) { return false; }
        // [safe mapped byte 0] ... [mapping end at or beyond byte4096]
        //       ^ size>=4096 was proved; only this complete byte is accessed.
        original[0] = ::std::byte{0x53};
        Shared duplicate{original};
        if(!check(duplicate && duplicate.size() >= 4096uz && duplicate.data() != nullptr, "duplicate actual mapping")) { return false; }
        if(!check(duplicate[0] == ::std::byte{0x53}, "duplicate observes shared byte")) { return false; }
        duplicate[0] = ::std::byte{0xa7};
        if(!check(original[0] == ::std::byte{0xa7}, "original observes duplicate write")) { return false; }
        first = original.native_handle(); second = duplicate.native_handle();
        if(!check(first != nullptr && second != nullptr && first != second, "independent owned section handles")) { return false; }
        Shared moved{::std::move(duplicate)};
        if(!check(!duplicate && moved.native_handle() == second, "move preserves sole ownership")) { return false; }
        moved.close();
        if(!check(retired(second), "duplicate retired exactly once")) { return false; }
        // Closing the duplicate leaves the original view usable.
        if(!check(original[0] == ::std::byte{0xa7}, "original view survives duplicate close")) { return false; }
        original.close();
        if(!check(retired(first), "original retired exactly once")) { return false; }
    }
    // Destruction after close must not release or resurrect another handle.
    return check(retired(first) && retired(second), "closed owners remain invalid after destruction");
}
}
int main()
{
#if defined(UWVM_TEST_SHMEM_FAIL_WIN32) || defined(UWVM_TEST_SHMEM_FAIL_NT)
    auto raw{readonly_section()};
    if(!check(static_cast<bool>(raw), "create readonly kernel section before terminal test")) { return 2; }
    ::fast_io::println("BEGIN expected terminal readonly mapping failure");
# if defined(UWVM_TEST_SHMEM_FAIL_WIN32)
    ::fast_io::win32_shared_memory_ntw failure{raw.release(), 4096uz, ::fast_io::ipc_mode::out};
# else
    ::fast_io::nt_shared_memory failure{raw.release(), ::fast_io::ipc_mode::out};
# endif
    // In noEH mode throw_error must terminate; reaching here is failure.
    ::fast_io::perrln("FAIL terminal mapping unexpectedly returned");
    return 3;
#else
    if(!exercise_mapping<::fast_io::win32_shared_memory_ntw>() || !exercise_mapping<::fast_io::nt_shared_memory>() ||
        !exercise_mapping<::fast_io::zw_shared_memory>()) { return 1; }
# if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    {
        auto raw{readonly_section()};
        if(!check(static_cast<bool>(raw), "create readonly kernel section for Win32 failure")) { return 1; }
        void* const handle{raw.native_handle()};
        void* const probe{::fast_io::win32::MapViewOfFile(handle, 0x00000002u, 0u, 0u, 4096uz)};
        auto const native_error{::fast_io::win32::GetLastError()};
        ::fast_io::win32::details::map_guard unexpected_view{probe};
        if(!check(probe == nullptr && native_error != 0u, "native Win32 readonly write mapping actually fails")) { return 1; }
        bool caught{};
        try { ::fast_io::win32_shared_memory_ntw failure{raw.release(), 4096uz, ::fast_io::ipc_mode::out}; }
        catch(::fast_io::error const& error)
        { caught = error.domain == ::fast_io::win32_domain_value && error.code == native_error; }
        if(!check(caught && retired(handle), "Win32 EH error preserved and adopted handle retired")) { return 1; }
    }
    {
        auto raw{readonly_section()};
        if(!check(static_cast<bool>(raw), "create readonly kernel section for NT failure")) { return 1; }
        void* const handle{raw.native_handle()};
        auto const native_status{nt_readonly_write_probe(handle)};
        if(!check(native_status != 0u, "native NT readonly write mapping actually fails")) { return 1; }
        bool caught{};
        try { ::fast_io::nt_shared_memory failure{raw.release(), ::fast_io::ipc_mode::out}; }
        catch(::fast_io::error const& error)
        { caught = error.domain == ::fast_io::nt_domain_value && error.code == native_status; }
        if(!check(caught && retired(handle), "NT EH error preserved and adopted handle retired")) { return 1; }
    }
# endif
    ::fast_io::println("PASS actual Win32/Nt/Zw map, duplicate, move, closure and applicable EH cleanup");
#endif
}
#else
int main() { return 77; }
#endif
