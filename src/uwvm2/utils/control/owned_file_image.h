#pragma once
#ifndef UWVM_MODULE
# include <climits>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <new>
# include <span>
# include <string>
# include <utility>
# include <vector>
# include <fast_io.h>
# include <fast_io_unit/string.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
    enum class owned_image_error : unsigned
    {
        none, invalid_path, unsupported_provider, unsupported_character_width,
        open_failed, status_failed, not_regular, empty_file, quota_exceeded,
        allocation_failed, read_failed, short_read, file_changed, close_failed, invalid_bytes
    };
    class owned_file_image;
    struct owned_image_result;
    // Stable compiler input DATA. The image/observed status/path are neither
    // guest-file isolation nor stop/census/restore/atomic host-version authority.
    // The private factory reads into private memory BEFORE parsing. It cannot
    // copy a map after parsing and rebind escaped code/type/segment pointers.
    // No mutable bytes, fd, adopted caller span or ready boolean is exported.
    class owned_file_image final
    {
        ::std::vector<::std::byte> bytes_{};
        ::fast_io::posix_file_status observed_status_{};
        explicit owned_file_image(::std::vector<::std::byte> bytes, ::fast_io::posix_file_status observed) noexcept
            : bytes_{::std::move(bytes)}, observed_status_{observed} {}
        template<typename ActualFile, typename Status, typename Read, typename Close>
        [[nodiscard]] static owned_image_result read_owned_file(ActualFile& file, ::std::size_t limit,
                                                                Status status, Read read, Close close);
        [[nodiscard]] static owned_image_result read_actual_file(::std::u8string path, ::std::size_t limit);
    public:
        using owner = ::std::unique_ptr<owned_file_image const>;
        inline static constexpr ::std::size_t maximum_bytes{1uz << 30u};
        inline static constexpr ::std::size_t default_bytes{1uz << 28u};
        inline static constexpr ::std::size_t maximum_path_code_units{1uz << 20u};
        owned_file_image(owned_file_image const&) = delete;
        owned_file_image& operator=(owned_file_image const&) = delete;
        owned_file_image(owned_file_image&&) = delete;
        owned_file_image& operator=(owned_file_image&&) = delete;
        ~owned_file_image() = default;
        [[nodiscard]] static owned_image_result read(::std::u8string path, ::std::size_t limit = default_bytes);
        // Copy an actual native byte extent BEFORE any parser borrows it. The
        // caller retains a valid, stable input span until this synchronous copy
        // returns; later input mutation/destruction cannot change the image.
        // This DATA factory performs no file observation and issues no source
        // seal, module/type/epoch, stop, execution or restore authority.
        [[nodiscard]] static owned_image_result copy_owned_bytes(::std::span<::std::byte const> input,
                                                                 ::std::size_t limit = default_bytes) noexcept;
        [[nodiscard]] ::std::span<::std::byte const> bytes() const noexcept { return {bytes_.data(), bytes_.size()}; }
        [[nodiscard]] ::std::size_t size() const noexcept { return bytes_.size(); }
        [[nodiscard]] char const* cbegin() const noexcept
        {
            // [owned initialized data(), size()] end
            // [safe] a const alias of SAME storage; no pointer advance/write.
            return reinterpret_cast<char const*>(bytes_.data());
        }
        [[nodiscard]] char const* cend() const noexcept
        {
            // Factory proves nonempty size<=PTRDIFF_MAX and actual allocation.
            // [data() ... data()+size()] one-past (never read)
            // [safe                  ] ^^ end formed only after owned bound.
            return cbegin() + bytes_.size();
        }
        [[nodiscard]] ::fast_io::posix_file_status const& observed_status() const noexcept { return observed_status_; }
    };
    struct owned_image_result
    {
        owned_file_image::owner image{};
        owned_image_error error{owned_image_error::none};
        ::fast_io::error native_error{};
        [[nodiscard]] explicit operator bool() const noexcept { return error == owned_image_error::none && image != nullptr; }
    };
    namespace owned_image_details
    {
        [[nodiscard]] inline ::fast_io::error native_error(::fast_io::error error) noexcept { return error; }
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        [[nodiscard]] inline ::fast_io::error native_error(int error) noexcept
        { return {::fast_io::posix_domain_value, static_cast<::std::size_t>(error)}; }
        [[nodiscard]] inline bool interrupted(int error) noexcept { return error == EINTR; }
#endif
        [[nodiscard]] inline bool interrupted(::fast_io::error) noexcept { return false; }
        [[nodiscard]] inline bool stable_observation(::fast_io::posix_file_status const& before,
                                                     ::fast_io::posix_file_status const& after) noexcept
        {
            // Disagreement rejects the read; agreement cannot prove atomic
            // content or grant any later access to a live host-file mapping.
            return after.type == ::fast_io::file_type::regular && before.dev == after.dev && before.ino == after.ino &&
                   before.size == after.size && before.mtim == after.mtim && before.ctim == after.ctim;
        }
    }
    template<typename ActualFile, typename Status, typename Read, typename Close>
    inline owned_image_result owned_file_image::read_owned_file(ActualFile& file, ::std::size_t limit,
                                                               Status status, Read read, Close close)
    {
        // This PRIVATE template is called only by the actual native factories
        // below. A public synthetic provider/file status cannot mint an image.
        // file is already owned by fast_io RAII BEFORE any allocation can throw.
        auto const before{status(file)};
        if(!before) { return {{}, owned_image_error::status_failed, owned_image_details::native_error(before.error)}; }
        if(before.value.type != ::fast_io::file_type::regular) { return {{}, owned_image_error::not_regular, {}}; }
        if(before.value.size == 0u) { return {{}, owned_image_error::empty_file, {}}; }
        ::std::vector<::std::byte> pending{};
        if(before.value.size > limit || before.value.size > static_cast<::std::uintmax_t>(pending.max_size()) ||
           before.value.size > static_cast<::std::uintmax_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
        { return {{}, owned_image_error::quota_exceeded, {}}; }
        auto const extent{static_cast<::std::size_t>(before.value.size)}; // size_t limit proves conversion
        pending.resize(extent);
        ::std::size_t consumed{};
        while(consumed != extent)
        {
            auto const remaining{extent - consumed};
            // [pending.data(), pending.data()+extent) actual private allocation
            // [initialized prefix][remaining writable bytes] one-past
            //                      ^^ destination: consumed<extent BEFORE +.
            // The synchronous provider borrows at most remaining; NT retains
            // the buffer/owned IOSB until actual pending completion, if any.
            auto const destination{pending.data() + consumed};
            auto const result{read(file, destination, remaining)};
            if(owned_image_details::interrupted(result.error)) { continue; }
            if(!result) { return {{}, owned_image_error::read_failed, owned_image_details::native_error(result.error)}; }
            if(result.transferred == 0uz) { return {{}, owned_image_error::short_read, {}}; }
            if(result.transferred > remaining) { return {{}, owned_image_error::read_failed, {}}; }
            // [initialized prefix][0<transferred<=remaining] end
            // consumed advances only AFTER extent proof; no pointer escapes.
            consumed += result.transferred;
        }
        ::std::byte extra{};
        auto result{read(file, ::std::addressof(extra), sizeof(extra))};
        while(owned_image_details::interrupted(result.error))
        { result = read(file, ::std::addressof(extra), sizeof(extra)); }
        // [one complete owned extra byte] end; exact sizeof(extra), no advance.
        if(!result) { return {{}, owned_image_error::read_failed, owned_image_details::native_error(result.error)}; }
        if(result.transferred != 0uz) { return {{}, owned_image_error::file_changed, {}}; }
        auto const after{status(file)};
        if(!after) { return {{}, owned_image_error::status_failed, owned_image_details::native_error(after.error)}; }
        if(!owned_image_details::stable_observation(before.value, after.value)) { return {{}, owned_image_error::file_changed, {}}; }
        auto const closed{close(file)};
        if(!closed) { return {{}, owned_image_error::close_failed, owned_image_details::native_error(closed.error)}; }
        // One checked close releases native ownership before publishing only
        // completely initialized const private bytes. Failure publishes nothing.
        return {owner{new owned_file_image{::std::move(pending), before.value}}, owned_image_error::none, {}};
    }
    inline owned_image_result owned_file_image::read_actual_file(::std::u8string path, ::std::size_t limit)
    {
        if constexpr(CHAR_BIT != 8) { return {{}, owned_image_error::unsupported_character_width, {}}; }
        if(path.empty() || path.size() > maximum_path_code_units || path.find(u8'\0') != ::std::u8string::npos)
        { return {{}, owned_image_error::invalid_path, {}}; }
        if(limit == 0uz || limit > maximum_bytes) { return {{}, owned_image_error::quota_exceeded, {}}; }
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
        auto const native_path{::fast_io::concat_std(::fast_io::mnp::code_cvt(
            ::fast_io::mnp::os_c_str_with_known_size(path.data(), path.size())))};
        auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_entry{AT_FDCWD}, native_path.c_str(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)};
        if(!opened) { return {{}, owned_image_error::open_failed, owned_image_details::native_error(opened.error)}; }
        return read_owned_file(opened.file, limit,
            [](auto const& file) { return ::fast_io::posix_status_nothrow(file); },
            [](auto& file, void* buffer, ::std::size_t count) { return ::fast_io::posix_read_nothrow(file, buffer, count); },
            [](auto& file) { return ::fast_io::posix_close_nothrow(file); });
#elif defined(_WIN32) && !defined(_WIN32_WINDOWS) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__)
        // The qualified factory accepts Win32 paths, not the loader's ::NT::
        // namespace. Unsupported input is explicit, with no mapped fallback.
        if(path.starts_with(u8"::NT::")) { return {{}, owned_image_error::unsupported_provider, {}}; }
        auto opened{::fast_io::nt_open_readonly_sync_nothrow(
            ::fast_io::mnp::os_c_str_with_known_size(path.data(), path.size()))};
        if(!opened) { return {{}, owned_image_error::open_failed, opened.error}; }
        return read_owned_file(opened.file, limit,
            [](auto const& file) { return ::fast_io::nt_readonly_sync_status_nothrow(file); },
            [](auto& file, void* buffer, ::std::size_t count) { return ::fast_io::nt_readonly_sync_read_nothrow(file, buffer, count); },
            [](auto& file) { return ::fast_io::nt_readonly_sync_close_nothrow(file); });
#else
        // No qualified error-returning provider yet; ordinary mapped loading
        // stays available. Throwing open/map/checksum are not a sealed image.
        return {{}, owned_image_error::unsupported_provider, {}};
#endif
    }
    inline owned_image_result owned_file_image::read(::std::u8string path, ::std::size_t limit)
    {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try { return read_actual_file(::std::move(path), limit); }
        catch(::std::bad_alloc const&) { return {{}, owned_image_error::allocation_failed, {}}; }
        catch(::fast_io::error const& error) { return {{}, owned_image_error::invalid_path, error}; }
#else
        // No throwing filesystem shim. Existing noEH allocation-exhaustion
        // termination policy remains; all sizes are checked before allocation.
        return read_actual_file(::std::move(path), limit);
#endif
    }
    inline owned_image_result owned_file_image::copy_owned_bytes(::std::span<::std::byte const> input,
                                                                 ::std::size_t limit) noexcept
    {
        if constexpr(CHAR_BIT != 8) { return {{}, owned_image_error::unsupported_character_width, {}}; }
        if(limit == 0uz || limit > maximum_bytes) { return {{}, owned_image_error::quota_exceeded, {}}; }
        auto const extent{input.size()};
        if(extent == 0uz) { return {{}, owned_image_error::empty_file, {}}; }
        ::std::vector<::std::byte> pending{};
        if(extent > limit || extent > pending.max_size() ||
           extent > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
        { return {{}, owned_image_error::quota_exceeded, {}}; }
        if(input.data() == nullptr) { return {{}, owned_image_error::invalid_bytes, {}}; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
#endif
        {
            pending.resize(extent);
            // [actual stable input.data(), extent][private pending, extent] end
            // [safe] nonempty actual native span, <=limit/max_size/PTRDIFF_MAX
            // BEFORE allocation/copy arithmetic; input is never adopted. The
            // fresh allocation cannot overlap the retained input. fast_io
            // advances only within both proven extents and discards its safe
            // one-past result; no source or destination pointer escapes.
            static_cast<void>(::fast_io::freestanding::non_overlapped_copy_n(input.data(), extent, pending.data()));
            // A copied byte image has NO observed host file. The zero status
            // retains file_type::none and is DATA, not a synthesized inode or
            // regular-file observation. Existing read/provider paths are intact.
            return {owner{new owned_file_image{::std::move(pending), {}}}, owned_image_error::none, {}};
        }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        catch(::std::bad_alloc const&) { return {{}, owned_image_error::allocation_failed, {}}; }
#endif
        // NoEH retains the existing bounded native allocation failure policy.
    }
}
