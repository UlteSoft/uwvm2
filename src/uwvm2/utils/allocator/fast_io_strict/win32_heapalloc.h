#pragma once

namespace uwvm2::utils::allocator::fast_io_strict
{
    namespace details
    {

        inline void* win32_heapalloc_handle_common_impl(void* heaphandle, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            if(n == 0) { n = 1; }
            auto p{::fast_io::win32::HeapAlloc(heaphandle, flag, n)};

            return p;
        }

        inline void* win32_heaprealloc_handle_common_impl(void* heaphandle, void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            if(n == 0) { n = 1; }
            if(addr == nullptr)
#if __has_cpp_attribute(unlikely)
                [[unlikely]]
#endif
            {
                return win32_heapalloc_handle_common_impl(heaphandle, n, flag);
            }
            auto p{::fast_io::win32::HeapReAlloc(heaphandle, flag, addr, n)};

            return p;
        }

#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        [[nodiscard]]
#if __has_cpp_attribute(__gnu__::__artificial__)
        [[__gnu__::__artificial__]]
#endif
#if __has_cpp_attribute(__gnu__::__const__)
        [[__gnu__::__const__]]
#endif
        inline void* win32_get_process_heap() noexcept
        {
            constexpr bool intrinsicssupported{
#if (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)) &&                                                                                          \
    (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_AMD64) || defined(__aarch64__) || defined(__arm64ec__) || defined(_M_ARM64) || \
     defined(_M_ARM64EC))
                true
#endif
            };
            if constexpr(intrinsicssupported) { return ::fast_io::win32::nt::rtl_get_process_heap(); }
            else
            {
                return ::fast_io::win32::GetProcessHeap();
            }
        }

        inline void* win32_heapalloc_common_impl(::std::size_t n, ::std::uint_least32_t flag) noexcept
        { return details::win32_heapalloc_handle_common_impl(details::win32_get_process_heap(), n, flag); }

        inline void* win32_heaprealloc_common_impl(void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        { return details::win32_heaprealloc_handle_common_impl(details::win32_get_process_heap(), addr, n, flag); }

        inline ::fast_io::allocation_least_result win32_heapalloc_least_common_impl(::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            auto processheap{details::win32_get_process_heap()};
            auto ptr{details::win32_heapalloc_handle_common_impl(processheap, n, flag)};
            if(ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
            return {ptr, ::fast_io::win32::HeapSize(processheap, 0, ptr)};
        }

        inline ::fast_io::allocation_least_result win32_heaprealloc_least_common_impl(void* addr, ::std::size_t n, ::std::uint_least32_t flag) noexcept
        {
            auto processheap{details::win32_get_process_heap()};
            auto ptr{details::win32_heaprealloc_handle_common_impl(processheap, addr, n, flag)};
            if(ptr == nullptr) [[unlikely]] { return {nullptr, 0}; }
            return {ptr, ::fast_io::win32::HeapSize(processheap, 0, ptr)};
        }

    }  // namespace details

    class fast_io_strict_win32_heapalloc_allocator
    {
    public:
#if __has_cpp_attribute(__gnu__::__malloc__)
        [[__gnu__::__malloc__]]
#endif
        inline static void* allocate(::std::size_t n) noexcept
        { return details::win32_heapalloc_common_impl(n, 0u); }

#if __has_cpp_attribute(__gnu__::__malloc__)
        [[__gnu__::__malloc__]]
#endif
        inline static void* allocate_zero(::std::size_t n) noexcept
        { return details::win32_heapalloc_common_impl(n, 0x00000008u); }

        inline static void* reallocate(void* addr, ::std::size_t n) noexcept { return details::win32_heaprealloc_common_impl(addr, n, 0u); }

        inline static void* reallocate_zero(void* addr, ::std::size_t n) noexcept { return details::win32_heaprealloc_common_impl(addr, n, 0x00000008u); }

        inline static void deallocate(void* addr) noexcept
        {
            if(addr == nullptr) [[unlikely]] { return; }
            ::fast_io::win32::HeapFree(details::win32_get_process_heap(), 0u, addr);
        }
#if 0
	static inline ::fast_io::allocation_least_result allocate_at_least(::std::size_t n) noexcept
	{
		return details::win32_heapalloc_least_common_impl(n, 0u);
	}
	static inline ::fast_io::allocation_least_result allocate_zero_at_least(::std::size_t n) noexcept
	{
		return details::win32_heapalloc_least_common_impl(n, 0x00000008u);
	}
	static inline ::fast_io::allocation_least_result reallocate_at_least(void *addr, ::std::size_t n) noexcept
	{
		return details::win32_heaprealloc_least_common_impl(addr, n, 0u);
	}
	static inline ::fast_io::allocation_least_result reallocate_zero_at_least(void *addr, ::std::size_t n) noexcept
	{
		return details::win32_heaprealloc_least_common_impl(addr, n, 0x00000008u);
	}
#endif
    };

}  // namespace uwvm2::utils::allocator::fast_io_strict
