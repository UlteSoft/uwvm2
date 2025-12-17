#pragma once

namespace uwvm2::utils::allocator::fast_io_strict
{
    class fast_io_strict_mimalloc_allocator
    {
    public:
        inline static void* allocate(::std::size_t n) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            void* p = ::fast_io::mimalloc::mi_malloc(n);

            return p;
        }

        inline static void* reallocate(void* p, ::std::size_t n) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            p = ::fast_io::mimalloc::mi_realloc(p, n);

            return p;
        }

        inline static void* allocate_zero(::std::size_t n) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            void* p = ::fast_io::mimalloc::mi_calloc(1, n);

            return p;
        }

        inline static void deallocate(void* p) noexcept { ::fast_io::mimalloc::mi_free(p); }

        inline static void* allocate_aligned(::std::size_t alignment, ::std::size_t n) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            void* p = ::fast_io::mimalloc::mi_malloc_aligned(n, alignment);

            return p;
        }

        inline static void* reallocate_aligned(void* p, ::std::size_t alignment, ::std::size_t n) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            p = ::fast_io::mimalloc::mi_ralloc_aligned(p, n, alignment);

            return p;
        }

        inline static void* allocate_zero_aligned(::std::size_t n, ::std::size_t alignment) noexcept
        {
            if(n == 0) [[unlikely]] { n = 1; }
            void* p = ::fast_io::mimalloc::mi_calloc_aligned(1, n, alignment);

            return p;
        }

        inline static void deallocate_aligned(void* p, ::std::size_t alignment) noexcept
        {
            if(p == nullptr) [[unlikely]] { return; }
            ::fast_io::mimalloc::mi_free_aligned(p, alignment);
        }
    };

}  // namespace uwvm2::utils::allocator::fast_io_strict
