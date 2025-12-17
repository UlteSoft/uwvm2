#pragma once

namespace uwvm2::utils::allocator::fast_io_strict
{
    class fast_io_strict_c_malloc_allocator
    {
    public:
#if __has_cpp_attribute(__gnu__::__returns_nonnull__)
        [[__gnu__::__returns_nonnull__]]
#endif
        inline static void* allocate(::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            void* p = ::std::malloc(n);
            return p;
        }
#if __has_cpp_attribute(__gnu__::__returns_nonnull__)
        [[__gnu__::__returns_nonnull__]]
#endif
        inline static void* reallocate(void* p, ::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            ::std::size_t const to_allocate{n};
            p = ::std::realloc(p, to_allocate);
            return p;
        }
#if __has_cpp_attribute(__gnu__::__returns_nonnull__)
        [[__gnu__::__returns_nonnull__]]
#endif
        inline static void* allocate_zero(::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            void* p = ::std::calloc(1, n);
            return p;
        }
#if (__has_include(<malloc.h>) || __has_include(<malloc_np.h>)) && !defined(__MSDOS__) && !defined(__LLVM_LIBC__)
        inline static allocation_least_result allocate_at_least(::std::size_t n) noexcept
        {
            auto p{::fast_io::c_malloc_allocator::allocate(n)};
            return {p, ::fast_io::details::c_malloc_usable_size_impl(p)};
        }

        inline static allocation_least_result allocate_zero_at_least(::std::size_t n) noexcept
        {
            auto p{::fast_io::c_malloc_allocator::allocate_zero(n)};
            return {p, ::fast_io::details::c_malloc_usable_size_impl(p)};
        }

        inline static allocation_least_result reallocate_at_least(void* oldp, ::std::size_t n) noexcept
        {
            auto p{::fast_io::c_malloc_allocator::reallocate(oldp, n)};
            return {p, ::fast_io::details::c_malloc_usable_size_impl(p)};
        }
#endif

#if defined(_WIN32) && !defined(__WINE__) && !defined(__CYGWIN__)
# if __has_cpp_attribute(__gnu__::__returns_nonnull__)
        [[__gnu__::__returns_nonnull__]]
# endif
        inline static void* allocate_aligned(::std::size_t alignment, ::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            void* p;
            if(alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) { p = ::std::malloc(n); }
            else
            {
                p = ::fast_io::noexcept_call(_aligned_malloc, n, alignment);
            }
            return p;
        }
# if __has_cpp_attribute(__gnu__::__returns_nonnull__)
        [[__gnu__::__returns_nonnull__]]
# endif
        inline static void* reallocate_aligned(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            if(alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) { p = ::std::realloc(p, n); }
            else
            {
                p = ::fast_io::noexcept_call(_aligned_realloc, p, n, alignment);
            }
            return p;
        }

        inline static void deallocate_aligned(void* p, ::std::size_t alignment) noexcept
        {
            if(p == nullptr) { return; }
            if(alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) { ::std::free(p); }
            else
            {
                ::fast_io::noexcept_call(_aligned_free, p);
            }
        }
#endif
        inline static void deallocate(void* p) noexcept
        {
            if(p == nullptr) { return; }

            ::std::free(p);
        }
    };

}  // namespace uwvm2::utils::allocator::fast_io_strict
