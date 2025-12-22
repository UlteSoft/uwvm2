export namespace fast_io
{

    using ::fast_io::nt_domain_value;

    namespace win32::nt
    {
        using ::fast_io::win32::nt::nt_get_current_peb;
        using ::fast_io::win32::nt::CsrClientCallServer;
        using ::fast_io::win32::nt::RtlQueryEnvironmentVariable_U;
        using ::fast_io::win32::nt::unicode_string;
        using ::fast_io::win32::nt::nt_set_information_virtual_memory;
        using ::fast_io::win32::nt::memory_range_entry;
        using ::fast_io::win32::nt::virtual_memory_information_class;
        using ::fast_io::win32::nt::nt_query_system_information;
        using ::fast_io::win32::nt::system_information_class;
        using ::fast_io::win32::nt::nt_allocate_virtual_memory;
        using ::fast_io::win32::nt::nt_free_virtual_memory;

    } // namespace win32::nt
}