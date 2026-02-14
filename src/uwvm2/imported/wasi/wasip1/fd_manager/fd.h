/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <limits>
# include <type_traits>
# include <memory>
# include <new>
# include <atomic>
# include <utility>
# include <algorithm>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::fd_manager
{
    template <typename... Ty>
    inline consteval ::std::size_t get_union_size() noexcept
    {
        ::std::size_t max_size{};
        [&max_size]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        { ((max_size = ::std::max(max_size, sizeof(Ty...[I]))), ...); }(::std::make_index_sequence<sizeof...(Ty)>{});
        return max_size;
    }

    /// @brief Preload elements into the directory stack
    /// @note On the Win32 platform, when ::fast_io::open_mode::shared_delete is enabled, any process (including the current one) can rename an already opened
    ///       file or path, thereby removing its binding to the root. Consequently, subsequent attempts to open the directory cannot be performed using the
    ///       system's . (current path) and .. (parent directory) to access the directory, which could cause out-of-bounds issues. Therefore, the handle of the
    ///       already-opened directory file descriptor must be deeply bound internally to ensure safe reading.
    ///
    ///       On POSIX systems, as long as a file remains open via a directory file descriptor, you can continue performing any operations on that directory
    ///       even if it has been renamed within the filesystem path.
    /// @note Since the handle in the Win9x directory system is not stored, fast_io stores the filename. This may cause TOCTOU and sandbox escape issues, but
    ///       it's merely a compromise for such ancient platforms.
    struct dir_stack_entry_t
    {
        inline static constexpr ::std::size_t sizeof_dir_stack_entry_u{get_union_size<::fast_io::dir_file, ::fast_io::dir_io_observer>()};

        union storage_u 
        {
            // dir file
            ::fast_io::dir_file file;

            // dir io observer
            ::fast_io::dir_io_observer observer;

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte storage_u_reserve[sizeof_dir_stack_entry_u]{};

            // destructor of 'storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~storage_u() {}

            // The release of table_idx is managed by struct wasm1_element_t, there is no issue of raii resources being unreleased.
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_dir_stack_entry_u, "sizeof(storage_t) not equal to sizeof_dir_stack_entry_u");

        // In wasm1, type stands for table index, which conceptually can be any value, but since the standard specifies only 1 table, it can only be 0. Here
        // union does not need to make any type-safe judgments since there is only one type.

        bool is_observer{};

        // For the stack low, it is the full WASTI path name of the preload directory.
        // For the upward path, it is the name of each level of the directory.
        ::uwvm2::utils::container::u8string name{};

        inline explicit constexpr dir_stack_entry_t() noexcept { ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){}; }

        inline explicit constexpr dir_stack_entry_t(bool is_observer) noexcept : is_observer{is_observer}
        {
            if(is_observer) { ::new(::std::addressof(this->storage.observer)) decltype(this->storage.observer){}; }
            else
            {
                ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){};
            }
        }

        inline constexpr dir_stack_entry_t(dir_stack_entry_t const& other) noexcept : is_observer{other.is_observer}, name{other.name}
        {
            if(this->is_observer) { ::new(::std::addressof(this->storage.observer)) decltype(this->storage.observer){other.storage.observer}; }
            else
            {
                ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){other.storage.file};
            }
        }

        inline constexpr dir_stack_entry_t(dir_stack_entry_t&& other) noexcept : is_observer{other.is_observer}, name{::std::move(other.name)}
        {
            if(this->is_observer) { ::new(::std::addressof(this->storage.observer)) decltype(this->storage.observer){::std::move(other.storage.observer)}; }
            else
            {
                ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){::std::move(other.storage.file)};
            }
        }

        inline constexpr dir_stack_entry_t& operator= (dir_stack_entry_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->is_observer) { ::std::destroy_at(::std::addressof(this->storage.observer)); }
            else
            {
                ::std::destroy_at(::std::addressof(this->storage.file));
            }

            this->is_observer = other.is_observer;
            this->name = other.name;

            if(this->is_observer) { ::new(::std::addressof(this->storage.observer)) decltype(this->storage.observer){other.storage.observer}; }
            else
            {
                ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){other.storage.file};
            }

            return *this;
        }

        inline constexpr dir_stack_entry_t& operator= (dir_stack_entry_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->is_observer) { ::std::destroy_at(::std::addressof(this->storage.observer)); }
            else
            {
                ::std::destroy_at(::std::addressof(this->storage.file));
            }

            this->is_observer = other.is_observer;
            this->name = ::std::move(other.name);

            if(this->is_observer) { ::new(::std::addressof(this->storage.observer)) decltype(this->storage.observer){::std::move(other.storage.observer)}; }
            else
            {
                ::new(::std::addressof(this->storage.file)) decltype(this->storage.file){::std::move(other.storage.file)};
            }

            return *this;
        }

        inline constexpr ~dir_stack_entry_t()
        {
            if(this->is_observer) { ::std::destroy_at(::std::addressof(this->storage.observer)); }
            else
            {
                ::std::destroy_at(::std::addressof(this->storage.file));
            }

            // multiple calls to the destructor are ub, no need to set it to nullptr here
        }
    };

    /// @brief Shared body of the preloaded directory stack with atomic reference count
    struct dir_stack_entry_rc_t
    {
        ::std::atomic_size_t refcount{};
        dir_stack_entry_t dir_stack{};
    };

    /// @brief Reference wrapper of the preloaded directory stack (RC)
    struct dir_stack_entry_ref_t
    {
        using allocator_t = ::fast_io::native_typed_global_allocator<dir_stack_entry_rc_t>;

        dir_stack_entry_rc_t* ptr{};

        inline constexpr dir_stack_entry_ref_t() noexcept
        {
            this->ptr = allocator_t::allocate(1uz);
            // ptr will never be null because the fast_io allocator terminates upon allocation failure.
            ::new(this->ptr) dir_stack_entry_rc_t{};
            this->ptr->refcount.store(1uz, ::std::memory_order_relaxed);
        }

        inline constexpr dir_stack_entry_ref_t(dir_stack_entry_ref_t const& other) noexcept : ptr{other.ptr}
        {
            if(this->ptr) [[likely]] { this->ptr->refcount.fetch_add(1uz, ::std::memory_order_relaxed); }
        }

        inline constexpr dir_stack_entry_ref_t(dir_stack_entry_ref_t&& other) noexcept : ptr{other.ptr} { other.ptr = nullptr; }

        inline constexpr dir_stack_entry_ref_t& operator= (dir_stack_entry_ref_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->reset();
            this->ptr = other.ptr;
            if(this->ptr) [[likely]] { this->ptr->refcount.fetch_add(1uz, ::std::memory_order_relaxed); }
            return *this;
        }

        inline constexpr dir_stack_entry_ref_t& operator= (dir_stack_entry_ref_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->reset();
            this->ptr = other.ptr;
            other.ptr = nullptr;
            return *this;
        }

        inline constexpr ~dir_stack_entry_ref_t()
        {
            if(this->ptr) [[likely]]
            {
                auto const p{this->ptr};
                if(p->refcount.fetch_sub(1uz, ::std::memory_order_acq_rel) == 1uz)
                {
                    ::std::destroy_at(p);
                    allocator_t::deallocate_n(p, 1uz);
                }
                // Do not set it to nullptr, as multiple calls to the destructor are undefined behavior.
            }
        }

        inline constexpr void reset() noexcept
        {
            if(ptr) [[likely]]
            {
                auto const p{this->ptr};
                this->ptr = nullptr;
                if(p->refcount.fetch_sub(1uz, ::std::memory_order_acq_rel) == 1uz)
                {
                    ::std::destroy_at(p);
                    allocator_t::deallocate_n(p, 1uz);
                }
            }
        }
    };

    struct dir_stack_t
    {
        ::uwvm2::utils::container::vector<dir_stack_entry_ref_t> dir_stack{};

        inline constexpr ::std::size_t stack_size() const noexcept { return this->dir_stack.size(); }

        inline constexpr bool empty() const noexcept { return this->stack_size() == 0uz; }

        inline constexpr bool is_preload_dir() const noexcept { return this->stack_size() == 1uz; }
    };

    enum class wasi_fd_type_e : unsigned
    {
        null,
        file,
        dir,
#if defined(_WIN32) && !defined(__CYGWIN__)
        socket,
        socket_observer,
#endif
        file_observer
    };

#if defined(_WIN32) && !defined(__CYGWIN__)
    struct win32_native_file_with_flags_t
    {
        // Automatically select NT or Win32
        ::fast_io::native_file file{};
        // Since flags created in Win32 are fixed starting from the handle and cannot be modified, a flag must be created for storage.
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t fdflags{};
    };
#endif

    using wasi_file_fd_t =
#if defined(_WIN32) && !defined(__CYGWIN__)
        win32_native_file_with_flags_t;
#else
        ::fast_io::native_file;
#endif

    struct wasi_fd_storage_t
    {
        inline static constexpr ::std::size_t sizeof_wasi_fd_storage_u{get_union_size<wasi_file_fd_t,
                                                                                      dir_stack_t
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                                                      ,
                                                                                      ::fast_io::win32_socket_file,
                                                                                      ::fast_io::win32_socket_io_observer
#endif
                                                                                      ,
                                                                                      ::fast_io::native_io_observer>()};

        union storage_u 
        {
            // native file (with flags)
            wasi_file_fd_t file_fd;

            // dir stack
            dir_stack_t dir_stack;

            // win32 ws2 socket
#if defined(_WIN32) && !defined(__CYGWIN__)
            /// @note Before using win32_socket_file, you must first create a ::fast_io::win32_wsa_service.
            // Since wasi lacks a socket API, it is impossible to capture fdflags during creation.
            ::fast_io::win32_socket_file socket_fd;
            ::fast_io::win32_socket_io_observer socket_observer;
#endif

            // native file observer
            ::fast_io::native_io_observer file_observer;

            // The directory does not need to support observer mode:
            // On platforms that support duplicate file operations, file systems use handles. On platforms that do not support duplicate file operations, such
            // as Windows 9x, storage is handled via strings (which introduces TOCTOU issues), and direct copying is also possible.

            // Full occupancy is used to initialize the union, set the union to all zero.
            [[maybe_unused]] ::std::byte sizeof_wasi_fd_storage_u_reserve[sizeof_wasi_fd_storage_u]{};

            // destructor of 'storage_u' is implicitly deleted because variant field 'typeidx_u8_vector' has a non-trivial destructor
            inline constexpr ~storage_u() {}

            // The release of table_idx is managed by struct wasm1_element_t, there is no issue of raii resources being unreleased.
        } storage{};

        static_assert(sizeof(storage_u) == sizeof_wasi_fd_storage_u, "sizeof(storage_t) not equal to sizeof_storage_u");

        // In wasm1, type stands for table index, which conceptually can be any value, but since the standard specifies only 1 table, it can only be 0. Here
        // union does not need to make any type-safe judgments since there is only one type.

        wasi_fd_type_e type{};

        inline explicit constexpr wasi_fd_storage_t() noexcept : type{wasi_fd_type_e::null} {}

        inline explicit constexpr wasi_fd_storage_t(wasi_fd_type_e new_type) noexcept : type{new_type}
        {
            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr wasi_fd_storage_t(wasi_fd_storage_t const& other) noexcept : type{other.type}
        {
            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){other.storage.file_fd};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){other.storage.dir_stack};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){other.storage.socket_fd};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){other.storage.socket_observer};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){other.storage.file_observer};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr wasi_fd_storage_t(wasi_fd_storage_t&& other) noexcept : type{other.type}
        {
            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){::std::move(other.storage.file_fd)};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){::std::move(other.storage.dir_stack)};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){::std::move(other.storage.socket_fd)};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){::std::move(other.storage.socket_observer)};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){::std::move(other.storage.file_observer)};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr wasi_fd_storage_t& operator= (wasi_fd_storage_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_fd));
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::std::destroy_at(::std::addressof(this->storage.dir_stack));
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_fd));
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_observer));
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_observer));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            this->type = other.type;

            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){other.storage.file_fd};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){other.storage.dir_stack};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){other.storage.socket_fd};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){other.storage.socket_observer};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){other.storage.file_observer};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            return *this;
        }

        inline constexpr wasi_fd_storage_t& operator= (wasi_fd_storage_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_fd));
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::std::destroy_at(::std::addressof(this->storage.dir_stack));
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_fd));
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_observer));
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_observer));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            this->type = other.type;

            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){::std::move(other.storage.file_fd)};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){::std::move(other.storage.dir_stack)};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){::std::move(other.storage.socket_fd)};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){::std::move(other.storage.socket_observer)};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){::std::move(other.storage.file_observer)};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            return *this;
        }

        inline constexpr ~wasi_fd_storage_t()
        {
            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_fd));
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::std::destroy_at(::std::addressof(this->storage.dir_stack));
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_fd));
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_observer));
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_observer));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            // multiple calls to the destructor are ub, no need to set it to nullptr here
        }

        inline constexpr void reset_type(wasi_fd_type_e new_type) noexcept
        {
            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_fd));
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::std::destroy_at(::std::addressof(this->storage.dir_stack));
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_fd));
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.socket_observer));
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::std::destroy_at(::std::addressof(this->storage.file_observer));
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            this->type = new_type;

            switch(this->type)
            {
                case wasi_fd_type_e::null:
                {
                    break;
                }
                case wasi_fd_type_e::file:
                {
                    ::new(::std::addressof(this->storage.file_fd)) decltype(this->storage.file_fd){};
                    break;
                }
                case wasi_fd_type_e::dir:
                {
                    ::new(::std::addressof(this->storage.dir_stack)) decltype(this->storage.dir_stack){};
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case wasi_fd_type_e::socket:
                {
                    ::new(::std::addressof(this->storage.socket_fd)) decltype(this->storage.socket_fd){};
                    break;
                }
                case wasi_fd_type_e::socket_observer:
                {
                    ::new(::std::addressof(this->storage.socket_observer)) decltype(this->storage.socket_observer){};
                    break;
                }
#endif
                case wasi_fd_type_e::file_observer:
                {
                    ::new(::std::addressof(this->storage.file_observer)) decltype(this->storage.file_observer){};
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }
    };

    /// @brief Provided for dup/dup2. For certain systems and file types, the system's dup function may not be usable.
    /// @note  This is a reserved feature, but it will not be used in wasip1 because renumbering involves moving rather than copying.
    struct wasi_fd_rc_t
    {
        ::std::atomic_size_t refcount{};
        wasi_fd_storage_t wasi_fd_storage{};
    };

    /// @brief Used to prevent default construction.
    struct wasi_no_construct_t
    {
        inline explicit constexpr wasi_no_construct_t() noexcept = default;
    };

    inline constexpr wasi_no_construct_t wasi_no_construct{};

    struct wasi_fd_ref_t
    {
        using allocator_t = ::fast_io::native_typed_global_allocator<wasi_fd_rc_t>;

        wasi_fd_rc_t* ptr{};

        inline constexpr wasi_fd_ref_t(wasi_no_construct_t) noexcept {}

        inline constexpr wasi_fd_ref_t() noexcept
        {
            this->ptr = allocator_t::allocate(1uz);
            // ptr will never be null because the fast_io allocator terminates upon allocation failure.
            ::new(this->ptr) wasi_fd_rc_t{};
            this->ptr->refcount.store(1uz, ::std::memory_order_relaxed);
        }

        inline constexpr wasi_fd_ref_t(wasi_fd_ref_t const& other) noexcept : ptr{other.ptr}
        {
            if(this->ptr) [[likely]] { this->ptr->refcount.fetch_add(1uz, ::std::memory_order_relaxed); }
        }

        inline constexpr wasi_fd_ref_t(wasi_fd_ref_t&& other) noexcept : ptr{other.ptr} { other.ptr = nullptr; }

        inline constexpr wasi_fd_ref_t& operator= (wasi_fd_ref_t const& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->reset();
            this->ptr = other.ptr;
            if(this->ptr) [[likely]] { this->ptr->refcount.fetch_add(1uz, ::std::memory_order_relaxed); }
            return *this;
        }

        inline constexpr wasi_fd_ref_t& operator= (wasi_fd_ref_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }
            this->reset();
            this->ptr = other.ptr;
            other.ptr = nullptr;
            return *this;
        }

        inline constexpr ~wasi_fd_ref_t()
        {
            if(this->ptr) [[likely]]
            {
                auto const p{this->ptr};
                if(p->refcount.fetch_sub(1uz, ::std::memory_order_acq_rel) == 1uz)
                {
                    ::std::destroy_at(p);
                    allocator_t::deallocate_n(p, 1uz);
                }
                // Do not set it to nullptr, as multiple calls to the destructor are undefined behavior.
            }
        }

        inline constexpr void reset() noexcept
        {
            // Reset releases itself and decrements the reference count.
            if(this->ptr) [[likely]]
            {
                auto const p{this->ptr};
                this->ptr = nullptr;
                if(p->refcount.fetch_sub(1uz, ::std::memory_order_acq_rel) == 1uz)
                {
                    ::std::destroy_at(p);
                    allocator_t::deallocate_n(p, 1uz);
                }
            }
        }

        inline constexpr void create_new() noexcept
        {
            if(!this->ptr) [[likely]]
            {
                this->ptr = allocator_t::allocate(1uz);
                // ptr will never be null because the fast_io allocator terminates upon allocation failure.
                ::new(this->ptr) wasi_fd_rc_t{};
                this->ptr->refcount.store(1uz, ::std::memory_order_relaxed);
            }
        }
    };

    /// @brief    WASI file descriptor
    /// @details  Using a singleton ensures that when encountering multithreaded scaling during usage, the file descriptors currently in use remain unaffected.
    struct wasi_fd_t
    {
        using mutex_t = ::uwvm2::utils::mutex::mutex_t;
        using mutex_alloc_t = ::fast_io::native_typed_global_allocator<mutex_t>;

        // ====== for wasi ======
        // Wasi does not differentiate between text and binary modes, so on Win32 systems, files can be opened using CreateFile even if the Win32 layer does not
        // provide CRLF escaping.
        wasi_fd_ref_t wasi_fd{};

        ::uwvm2::imported::wasi::wasip1::abi::rights_t rights_base{};
        ::uwvm2::imported::wasi::wasip1::abi::rights_t rights_inherit{};

        // ====== for vm ======
        mutex_t fd_mutex{};  // [singleton]

        // The "close pos" is only valid for those in the "close" list and invalid for those in the "renumber map".
        // note: Since SIZE_MAX is used to mark closed files, the maximum number of available files is only SIZE_MAX - 1uz.
        ::std::size_t close_pos{SIZE_MAX};

        inline constexpr wasi_fd_t() noexcept = default;

        inline constexpr wasi_fd_t(wasi_fd_t const& other) noexcept = delete;

        inline constexpr wasi_fd_t& operator= (wasi_fd_t const& other) noexcept = delete;

        inline constexpr wasi_fd_t(wasi_fd_t&& other) noexcept = delete;

        inline constexpr wasi_fd_t& operator= (wasi_fd_t&& other) noexcept = delete;

        inline ~wasi_fd_t() = default;
    };

    inline constexpr void destroy_wasi_fd(wasi_fd_t * fd_p) noexcept
    {
        using wasi_fd_t_fast_io_type_allocator = ::fast_io::native_typed_global_allocator<wasi_fd_t>;

        ::std::destroy_at(fd_p);
        wasi_fd_t_fast_io_type_allocator::deallocate_n(fd_p, 1uz);
    }

    struct wasi_fd_unique_ptr_t 
    {
        using wasi_fd_t_fast_io_type_allocator = ::fast_io::native_typed_global_allocator<wasi_fd_t>;

        wasi_fd_t* fd_p{};

        inline constexpr wasi_fd_unique_ptr_t(wasi_no_construct_t) noexcept {}

        inline constexpr wasi_fd_unique_ptr_t() noexcept
        {
            this->fd_p = wasi_fd_t_fast_io_type_allocator::allocate(1uz);
            ::new(this->fd_p) wasi_fd_t{};
        }

        inline constexpr wasi_fd_unique_ptr_t(wasi_fd_t* fd_p_o) noexcept : fd_p{fd_p_o} {}

        inline constexpr wasi_fd_unique_ptr_t(wasi_fd_unique_ptr_t const&) noexcept = delete;
        inline constexpr wasi_fd_unique_ptr_t& operator= (wasi_fd_unique_ptr_t const&) noexcept = delete;

        inline constexpr wasi_fd_unique_ptr_t(wasi_fd_unique_ptr_t&& other) noexcept
        {
            this->fd_p = other.fd_p;
            other.fd_p = nullptr;
        }

        inline constexpr wasi_fd_unique_ptr_t& operator= (wasi_fd_unique_ptr_t&& other) noexcept
        {
            if(::std::addressof(other) == this) [[unlikely]] { return *this; }

            if(this->fd_p) [[likely]]
            {
                ::std::destroy_at(this->fd_p);
                wasi_fd_t_fast_io_type_allocator::deallocate_n(this->fd_p, 1uz);
            }

            this->fd_p = other.fd_p;
            other.fd_p = nullptr;

            return *this;
        }

        inline constexpr void clear_destroy() noexcept
        {
            if(this->fd_p) [[likely]]
            {
                ::std::destroy_at(this->fd_p);
                wasi_fd_t_fast_io_type_allocator::deallocate_n(this->fd_p, 1uz);

                this->fd_p = nullptr;
            }
        }

        inline constexpr wasi_fd_t* release() noexcept
        {
            auto const ret{this->fd_p};
            this->fd_p = nullptr;
            return ret;
        }

        inline constexpr void reconstruct() noexcept
        {
            if(!this->fd_p) [[likely]]
            {
                this->fd_p = wasi_fd_t_fast_io_type_allocator::allocate(1uz);
                ::new(this->fd_p) wasi_fd_t{};
            }
        }

        inline constexpr void clear_destroy_and_assign(wasi_fd_t* fd_p_o) noexcept
        {
            if(this->fd_p) [[likely]]
            {
                ::std::destroy_at(this->fd_p);
                wasi_fd_t_fast_io_type_allocator::deallocate_n(this->fd_p, 1uz);
            }

            this->fd_p = fd_p_o;
        }

        inline constexpr ~wasi_fd_unique_ptr_t()
        {
            if(this->fd_p) [[likely]]
            {
                ::std::destroy_at(this->fd_p);
                wasi_fd_t_fast_io_type_allocator::deallocate_n(this->fd_p, 1uz);

                // Multiple calls to the destructor are ub, no need to set it to nullptr here
            }
        }
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
