#pragma once

namespace uwvm2::utils::allocator::fast_io_strict
{
    namespace details
    {

        inline void* nt_rtlallocate_heap_handle_common_impl(void* heaphandle, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            if(n == 0) { n = 1; }
            auto p{::fast_io::win32::nt::RtlAllocateHeap(heaphandle, flag, n)};

            return p;
        }

        inline void* nt_rtlreallocate_heap_handle_common_impl(void* heaphandle, void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            if(n == 0) { n = 1; }
            if(addr == nullptr)
#if __has_cpp_attribute(unlikely)
                [[unlikely]]
#endif
            {
                return details::nt_rtlallocate_heap_handle_common_impl(heaphandle, n, flag);
            }
            auto p{::fast_io::win32::nt::RtlReAllocateHeap(heaphandle, flag, addr, n)};

            return p;
        }

        inline void* nt_rtlallocate_heap_common_impl(::std::size_t n, ::std::uint_least32_t flag) noexcept
        { return details::nt_rtlallocate_heap_handle_common_impl(::fast_io::win32::nt::rtl_get_process_heap(), n, flag); }

        inline void* nt_rtlreallocate_heap_common_impl(void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        { return details::nt_rtlreallocate_heap_handle_common_impl(::fast_io::win32::nt::rtl_get_process_heap(), addr, n, flag); }

        inline ::fast_io::allocation_least_result nt_rtlallocate_heap_least_common_impl(::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            auto processheap{::fast_io::win32::nt::rtl_get_process_heap()};
            auto ptr{details::nt_rtlallocate_heap_handle_common_impl(processheap, n, flag)};
            if(ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
            return {ptr, ::fast_io::win32::nt::RtlSizeHeap(processheap, 0, ptr)};
        }

        inline ::fast_io::allocation_least_result nt_rtlreallocate_heap_least_common_impl(void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            auto processheap{::fast_io::win32::nt::rtl_get_process_heap()};
            auto ptr{details::nt_rtlreallocate_heap_handle_common_impl(processheap, addr, n, flag)};
            if(ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
            return {ptr, ::fast_io::win32::nt::RtlSizeHeap(processheap, 0, ptr)};
        }

    }  // namespace details

    class fast_io_strict_nt_rtlallocateheap_allocator
    {
    public:
#if __has_cpp_attribute(__gnu__::__malloc__)
        [[__gnu__::__malloc__]]
#endif
        static inline void* allocate(::std::size_t n) noexcept
        { return details::nt_rtlallocate_heap_common_impl(n, 0u); }
#if __has_cpp_attribute(__gnu__::__malloc__)
        [[__gnu__::__malloc__]]
#endif
        static inline void* allocate_zero(::std::size_t n) noexcept
        { return details::nt_rtlallocate_heap_common_impl(n, 0x00000008u); }

        inline static void* reallocate(void* addr, ::std::size_t n) noexcept { return details::nt_rtlreallocate_heap_common_impl(addr, n, 0u); }

        inline static void* reallocate_zero(void* addr, ::std::size_t n) noexcept { return details::nt_rtlreallocate_heap_common_impl(addr, n, 0x00000008u); }

        inline static void deallocate(void* addr) noexcept
        {
            if(addr == nullptr) [[unlikely]] { return; }
            ::fast_io::win32::nt::RtlFreeHeap(::fast_io::win32::nt::rtl_get_process_heap(), 0u, addr);
        }
#if 0
	static inline ::fast_io::allocation_least_result allocate_at_least(::std::size_t n) noexcept
	{
		return details::nt_rtlallocate_heap_least_common_impl(n, 0u);
	}
	static inline ::fast_io::allocation_least_result allocate_zero_at_least(::std::size_t n) noexcept
	{
		return details::nt_rtlallocate_heap_least_common_impl(n, 0x00000008u);
	}
	static inline ::fast_io::allocation_least_result reallocate_at_least(void *addr, ::std::size_t n) noexcept
	{
		return details::nt_rtlreallocate_heap_least_common_impl(addr, n, 0u);
	}
	static inline ::fast_io::allocation_least_result reallocate_zero_at_least(void *addr, ::std::size_t n) noexcept
	{
		return details::nt_rtlreallocate_heap_least_common_impl(addr, n, 0x00000008u);
	}
#endif
    };

}  // namespace uwvm2::utils::allocator::fast_io_strict
