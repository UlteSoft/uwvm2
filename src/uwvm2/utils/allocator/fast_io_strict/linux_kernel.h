#pragma once

namespace uwvm2::utils::allocator::fast_io_strict
{
    inline constexpr int unsigned linux_kernel_gfp_kernel{0x400u | 0x800u | 0x40u | 0x80u};

    inline constexpr int unsigned linux_kernel_gfp_kernel_zero{linux_kernel_gfp_kernel | 0x100u};

    class fast_io_strict_linux_kmalloc_allocator
    {
    public:
        inline static void* allocate(::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            void* p = ::fast_io::linux_kernel_kmalloc(n, linux_kernel_gfp_kernel);

            return p;
        }

        inline static void* reallocate(void* p, ::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            p = ::fast_io::linux_kernel_krealloc(p, n, linux_kernel_gfp_kernel);

            return p;
        }

        inline static void* allocate_zero(::std::size_t n) noexcept
        {
            if(n == 0) { n = 1; }
            void* p = ::fast_io::linux_kernel_kmalloc(n, linux_kernel_gfp_kernel_zero);

            return p;
        }

        inline static void deallocate(void* p) noexcept
        {
            if(p == nullptr) [[unlikely]] { return; }
            ::fast_io::linux_kernel_kfree(p);
        }
    };

}  // namespace uwvm2::utils::allocator::fast_io_strict
