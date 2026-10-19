/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <cerrno>
# include <cstddef>
# include <cstdint>
# include <cstdio>
# include <cstring>
# include <limits>
# include <memory>
# include <new>
# include <string_view>
# include <utility>
# if defined(__linux__)
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/statfs.h>
#  include <sys/sysmacros.h>
#  include <unistd.h>
# endif
# include <fast_io_crypto.h>
# include "runtime_image_anchor.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
# pragma GCC visibility push(hidden)
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache::self_executable
{
    using digest_type = ::std::array<::std::byte, 32uz>;

#if defined(__linux__)
    // These descriptors are never reconstructed from a pathname after capture.
    // No close retry: Linux may already have released/reused an fd after EINTR.
    struct owned_fd
    {
        int value{-1};
        owned_fd() noexcept = default;
        explicit owned_fd(int fd) noexcept : value{fd} {}
        owned_fd(owned_fd const&) = delete;
        owned_fd& operator=(owned_fd const&) = delete;
        owned_fd(owned_fd&& other) noexcept : value{::std::exchange(other.value, -1)} {}
        owned_fd& operator=(owned_fd&& other) noexcept
        {
            if(this != ::std::addressof(other))
            {
                if(value >= 0) { ::close(value); }
                value = ::std::exchange(other.value, -1);
            }
            return *this;
        }
        ~owned_fd() { if(value >= 0) { ::close(value); } }
        [[nodiscard]] explicit operator bool() const noexcept { return value >= 0; }
    };

    [[nodiscard]] inline bool same_content_identity(struct ::stat const& a, struct ::stat const& b) noexcept
    {
        // rename/unlink can change ctime and nlink without changing executable
        // bytes. Linux's executing-image write denial protects this real exe
        // object; do not reopen its new pathname or require nlink > 0.
        return S_ISREG(a.st_mode) && S_ISREG(b.st_mode) && a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
               a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
               a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_mode == b.st_mode;
    }

    // Only the actual Runtime TU's non-interposable code address can establish
    // that this runtime lives in the executable being hashed. A cache helper or
    // arbitrary host symbol would not cover an independently updated DSO.
    class identity_state;
# if defined(__GNUC__) || defined(__clang__)
    [[gnu::visibility("hidden"), gnu::noinline]] inline identity_state& state() noexcept;
# else
    inline identity_state& state() noexcept;
# endif

    [[nodiscard]] inline bool anchor_is_main_image(int proc_fd, struct ::stat const& image) noexcept
    {
# if !defined(__GNUC__) && !defined(__clang__)
        return false;
# else
        if(::uwvm2_runtime_cache_image_anchor == nullptr) { return false; }
        auto const address{reinterpret_cast<::std::uintptr_t>(&::uwvm2_runtime_cache_image_anchor)};
        owned_fd maps{::openat(proc_fd, "maps", O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
        if(!maps) { return false; }
        char line[1024]{};
        ::std::size_t used{};
        ::std::size_t total{};
        char buffer[4096]{};
        for(;;)
        {
            auto const count{::read(maps.value, buffer, sizeof(buffer))};
            if(count < 0) { if(errno == EINTR) { continue; } return false; }
            if(count == 0) { return false; }
            if(total > 16uz * 1024uz * 1024uz - static_cast<::std::size_t>(count)) { return false; }
            total += static_cast<::std::size_t>(count);
            for(::std::size_t i{}; i != static_cast<::std::size_t>(count); ++i)
            {
                if(buffer[i] != '\n')
                {
                    if(used == sizeof(line) - 1uz) { return false; }
                    line[used++] = buffer[i];
                    continue;
                }
                line[used] = 0;
                unsigned long long start{}, end{}, offset{}, inode{};
                unsigned device_major{}, device_minor{};
                char perms[5]{};
                if(::std::sscanf(line, "%llx-%llx %4s %llx %x:%x %llu", &start, &end, perms, &offset,
                                 &device_major, &device_minor, &inode) != 7) { return false; }
                if(start <= address && address < end)
                {
                    return perms[2] == 'x' && inode == static_cast<unsigned long long>(image.st_ino) &&
                           device_major == static_cast<unsigned>(::major(image.st_dev)) &&
                           device_minor == static_cast<unsigned>(::minor(image.st_dev));
                }
                used = 0uz;
            }
        }
# endif
    }

    class identity_state final
    {
        friend identity_state& state() noexcept;
        owned_fd image{};
        struct ::stat captured{};
        digest_type digest{};
        bool initialized{};
        ::std::atomic_bool invalidated{};

        identity_state() noexcept
        {
            if(::getuid() != ::geteuid() || ::getgid() != ::getegid()) { return; }
            owned_fd proc{::open("/proc/self", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
            if(!proc) { return; }
            struct ::statfs proc_type{};
            // Authenticate the kernel procfs directory before following its one
            // intentional magic symlink, exe. Arbitrary path aliases are unused.
            if(::fstatfs(proc.value, &proc_type) != 0 || proc_type.f_type != 0x9fa0 /* PROC_SUPER_MAGIC */) { return; }
            owned_fd candidate{::openat(proc.value, "exe", O_RDONLY | O_CLOEXEC)};
            if(!candidate || ::fstat(candidate.value, &captured) != 0 || !S_ISREG(captured.st_mode) || captured.st_size <= 0 ||
               static_cast<::std::uintmax_t>(captured.st_size) > (1ull << 34u) || !anchor_is_main_image(proc.value, captured)) { return; }

            ::fast_io::sha256_context hash{};
            ::std::byte bytes[4096]{};
            ::off_t offset{};
            while(offset != captured.st_size)
            {
                auto remaining{static_cast<::std::uintmax_t>(captured.st_size - offset)};
                auto const amount{remaining > sizeof(bytes) ? sizeof(bytes) : static_cast<::std::size_t>(remaining)};
                auto const count{::pread(candidate.value, bytes, amount, offset)};
                if(count < 0) { if(errno == EINTR) { continue; } return; }
                if(count == 0) { return; }
                hash.update(bytes, bytes + count);
                offset += count;
            }
            struct ::stat after{};
            ::std::byte extra{};
            if(::pread(candidate.value, &extra, 1uz, captured.st_size) != 0 || ::fstat(candidate.value, &after) != 0 ||
               !same_content_identity(captured, after)) { return; }
            hash.do_final();
            hash.digest_to_byte_ptr(digest.data());
            image = ::std::move(candidate);
            initialized = true;
        }

        [[nodiscard]] bool current() noexcept
        {
            if(!initialized || invalidated.load(::std::memory_order_acquire)) { return false; }
            struct ::stat now{};
            if(::getuid() != ::geteuid() || ::getgid() != ::getegid() || ::fstat(image.value, &now) != 0 ||
               !same_content_identity(captured, now))
            {
                invalidated.store(true, ::std::memory_order_release);
                return false;
            }
            return !invalidated.load(::std::memory_order_acquire);
        }

    public:
        ~identity_state() = default;
        identity_state(identity_state const&) = delete;
        identity_state& operator=(identity_state const&) = delete;
        [[nodiscard]] bool read_current(digest_type& out) noexcept
        {
            if(!current()) { return false; }
            out = digest;
            return true;
        }
    };

# if defined(__GNUC__) || defined(__clang__)
    [[gnu::visibility("hidden"), gnu::noinline]] inline identity_state& state() noexcept
# else
    inline identity_state& state() noexcept
# endif
    {
        // Function-local initialization is the only binary hash in this process.
        // The one retained image fd deliberately lasts until process teardown,
        // even if an embedder instantiated the async writer before first cache
        // use. No static-destructor ordering can close it under a live writer.
        static identity_state* const instance{[]() noexcept
        {
            alignas(identity_state) static ::std::byte storage[sizeof(identity_state)]{};
            return ::new(static_cast<void*>(storage)) identity_state{};
        }()};
        return *instance;
    }
#endif

    [[nodiscard]] inline bool current_digest(digest_type& out) noexcept
    {
#if defined(__linux__)
        return state().read_current(out);
#else
        // A path obtained from GetModuleFileName/GetMappedFileName or a later
        // CreateFile is not proof of the currently mapped main image object.
        // No pathname/constant/version fallback on unsupported platforms.
        (void)out;
        return false;
#endif
    }
}

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
# pragma GCC visibility pop
#endif
