// SOURCE ONLY. Linux kernel-origin secret; never a filesystem key fallback.
#pragma once
#ifndef UWVM_MODULE
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <string_view>
#include <type_traits>
#include <fast_io_crypto.h>
#include "runtime_origin.h"
#if defined(__linux__)
# include <linux/keyctl.h>
# include <sys/random.h>
# include <sys/syscall.h>
# include <sys/prctl.h>
# include <unistd.h>
#endif

#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::llvm_jit_cache::kernel_origin
{
    [[nodiscard]] inline bool dumping_disabled() noexcept
    {
#if defined(__linux__) && defined(PR_GET_DUMPABLE)
        // Never change process policy. Third-party crypto/compiled-stack spill
        // erasure cannot prove that a later core dump would contain no secret.
        return ::prctl(PR_GET_DUMPABLE, 0ul, 0ul, 0ul, 0ul) == 0;
#else
        return false;
#endif
    }
    inline void erase(void* pointer, ::std::size_t length) noexcept
    {
        auto* current{static_cast<volatile unsigned char*>(pointer)};
        while(length != 0u) { *current++ = 0u; --length; }
    }
    template<::std::size_t N> struct secret_bytes
    {
        ::std::array<::std::byte, N> value{};
        secret_bytes() noexcept = default;
        secret_bytes(secret_bytes const&) = delete;
        secret_bytes& operator=(secret_bytes const&) = delete;
        ~secret_bytes() { erase(value.data(), value.size()); }
    };

#if defined(__linux__) && defined(SYS_keyctl) && defined(SYS_add_key) && defined(SYS_getrandom) && defined(KEYCTL_MOVE_EXCL)
    namespace details
    {
        using serial = ::std::int32_t;
        constexpr unsigned long immutable_user_permissions{0x001b0000ul}; // user VIEW|READ|SEARCH|LINK only
        constexpr unsigned long private_ring_permissions{0x003f0000ul}; // user only; no possessor/group/other
        constexpr char description[]{"uwvm:llvm-jit-origin:v1"};

        [[nodiscard]] inline long control(int operation, unsigned long a = 0ul, unsigned long b = 0ul,
            unsigned long c = 0ul, unsigned long d = 0ul) noexcept
        { return ::syscall(SYS_keyctl, operation, a, b, c, d); }
        [[nodiscard]] inline unsigned long number(serial value) noexcept
        { return static_cast<unsigned long>(static_cast<long>(value)); }
        [[nodiscard]] inline unsigned long address(void const* value) noexcept
        { return reinterpret_cast<unsigned long>(value); }
        [[nodiscard]] inline bool serial_value(long value) noexcept
        { return value > 0 && value <= (::std::numeric_limits<serial>::max)(); }
        [[nodiscard]] inline bool decimal(::std::string_view text, unsigned long& value, unsigned base) noexcept
        {
            if(text.empty()) { return false; }
            value = 0ul;
            for(char character : text)
            {
                unsigned digit{};
                if(character >= '0' && character <= '9') { digit = static_cast<unsigned>(character - '0'); }
                else if(base == 16u && character >= 'a' && character <= 'f') { digit = static_cast<unsigned>(character - 'a') + 10u; }
                else { return false; }
                if(digit >= base || value > ((::std::numeric_limits<unsigned long>::max)() - digit) / base) { return false; }
                value = value * base + digit;
            }
            return true;
        }
        struct key_description
        {
            ::std::array<char, 256u> bytes{};
            ::std::string_view type{}, name{};
            unsigned long uid{}, gid{}, permissions{};
        };
        [[nodiscard]] inline bool describe(serial id, key_description& result) noexcept
        {
            auto const count{control(KEYCTL_DESCRIBE, number(id), address(result.bytes.data()), result.bytes.size())};
            if(count < 2 || static_cast<unsigned long>(count) > result.bytes.size() || result.bytes[static_cast<::std::size_t>(count) - 1u] != 0)
            { return false; }
            ::std::string_view remaining{result.bytes.data(), static_cast<::std::size_t>(count) - 1u};
            if(remaining.find('\0') != ::std::string_view::npos) { return false; }
            ::std::array<::std::string_view, 4u> prefix{};
            for(auto& field : prefix)
            {
                auto const separator{remaining.find(';')};
                if(separator == ::std::string_view::npos) { return false; }
                field = remaining.substr(0u, separator); remaining.remove_prefix(separator + 1u);
            }
            result.type = prefix[0u]; result.name = remaining;
            return decimal(prefix[1u], result.uid, 10u) && decimal(prefix[2u], result.gid, 10u) &&
                prefix[3u].size() == 8u && decimal(prefix[3u], result.permissions, 16u);
        }
        [[nodiscard]] inline bool exact_key(serial id, ::uid_t uid, ::gid_t gid) noexcept
        {
            key_description metadata{};
            return describe(id, metadata) && metadata.type == "user" && metadata.name == description &&
                metadata.uid == uid && metadata.gid == gid && metadata.permissions == immutable_user_permissions;
        }
        // SEARCH is recursive. Require the chosen key also be a DIRECT member
        // of the actual kernel-selected UID keyring, not an arbitrary subring.
        [[nodiscard]] inline bool member(serial ring, serial key) noexcept
        {
            ::std::array<serial, 4096u> contents{};
            auto const size{control(KEYCTL_READ, number(ring), address(contents.data()), sizeof(contents))};
            if(size < 0 || static_cast<unsigned long>(size) > sizeof(contents) || size % sizeof(serial) != 0u) { return false; }
            auto const count{static_cast<::std::size_t>(size) / sizeof(serial)};
            for(::std::size_t index{}; index != count; ++index) { if(contents[index] == key) { return true; } }
            return false;
        }
        [[nodiscard]] inline long search(serial ring) noexcept
        { return control(KEYCTL_SEARCH, number(ring), address("user"), address(description), 0ul); }
        [[nodiscard]] inline bool read_exact(serial ring, serial key, ::uid_t uid, ::gid_t gid, secret_bytes<32u>& value) noexcept
        {
            if(!exact_key(key, uid, gid) || !member(ring, key)) { return false; }
            if(control(KEYCTL_READ, number(key), address(value.value.data()), value.value.size()) != 32l) { return false; }
            // The immutable payload may be revoked or its link replaced by a
            // trusted native process. Do not publish a different key silently.
            return exact_key(key, uid, gid) && member(ring, key) && search(ring) == key;
        }
        [[nodiscard]] inline bool random(secret_bytes<64u>& bytes) noexcept
        {
            ::std::size_t offset{};
            for(unsigned attempts{}; attempts != 8u && offset != bytes.value.size(); ++attempts)
            {
                // Use the actual syscall: no libc/vDSO userspace random-state
                // buffer is allowed to retain material beyond this operation.
                auto const amount{::syscall(SYS_getrandom, bytes.value.data() + offset, bytes.value.size() - offset, GRND_NONBLOCK)};
                if(amount > 0) { offset += static_cast<::std::size_t>(amount); }
                else if(amount < 0 && errno == EINTR) { continue; }
                else { return false; }
            }
            return offset == bytes.value.size();
        }

        // Only these freshly created candidate serials may be unlinked. Never
        // clear/revoke/setattr the UID ring or another process's published key.
        struct candidate
        {
            serial parent{}, ring{}, key{};
            ::uid_t uid{}; ::gid_t gid{};
            ::std::array<char, 96u> name{};
            bool moved{}, cleaned{};
            [[nodiscard]] bool cleanup() noexcept
            {
                if(cleaned || ring == 0) { return true; }
                key_description metadata{};
                if(!describe(ring, metadata) || metadata.type != "keyring" || metadata.name != name.data() ||
                   metadata.uid != uid || metadata.gid != gid || !member(parent, ring)) { return false; }
                if(key != 0 && !moved && member(ring, key))
                { if(control(KEYCTL_UNLINK, number(key), number(ring)) != 0l) { return false; } }
                if(control(KEYCTL_UNLINK, number(ring), number(parent)) != 0l) { return false; }
                cleaned = true; return true;
            }
            ~candidate() { if(!cleaned) { (void)cleanup(); } }
        };
        [[nodiscard]] inline bool publish(serial uid_ring, ::uid_t uid, ::gid_t gid, serial& winner) noexcept
        {
            secret_bytes<64u> bytes{};
            if(!random(bytes)) { return false; }
            auto const parent{control(KEYCTL_GET_KEYRING_ID, number(KEY_SPEC_THREAD_KEYRING), 1ul)};
            if(!serial_value(parent)) { return false; }
            candidate temporary{}; temporary.parent = static_cast<serial>(parent); temporary.uid = uid; temporary.gid = gid;
            constexpr char prefix[]{"uwvm:ljc-pending:v1:"};
            ::std::memcpy(temporary.name.data(), prefix, sizeof(prefix) - 1u);
            constexpr char digits[]{"0123456789abcdef"};
            for(::std::size_t index{}; index != 32u; ++index)
            {
                auto const value{::std::to_integer<unsigned>(bytes.value[index + 32u])};
                temporary.name[sizeof(prefix) - 1u + 2u * index] = digits[value >> 4u];
                temporary.name[sizeof(prefix) + 2u * index] = digits[value & 15u];
            }
            // A cryptographically unique private ring name is separate from the
            // shared final key name. Only the empty PRIVATE ring receives add_key.
            auto const ring{::syscall(SYS_add_key, "keyring", temporary.name.data(), nullptr, 0ul, temporary.parent)};
            if(!serial_value(ring)) { return false; }
            temporary.ring = static_cast<serial>(ring);
            if(control(KEYCTL_SETPERM, number(temporary.ring), private_ring_permissions) != 0l) { return false; }
            key_description ring_info{};
            if(!describe(temporary.ring, ring_info) || ring_info.type != "keyring" || ring_info.name != temporary.name.data() ||
               ring_info.uid != uid || ring_info.gid != gid || ring_info.permissions != private_ring_permissions) { return false; }
            auto const key{::syscall(SYS_add_key, "user", description, bytes.value.data(), 32ul, temporary.ring)};
            if(!serial_value(key)) { return false; }
            temporary.key = static_cast<serial>(key);
            if(control(KEYCTL_SETPERM, number(temporary.key), immutable_user_permissions) != 0l || !exact_key(temporary.key, uid, gid)) { return false; }
            auto const moved{control(KEYCTL_MOVE, number(temporary.key), number(temporary.ring), number(uid_ring), KEYCTL_MOVE_EXCL)};
            if(moved == 0l) { temporary.moved = true; winner = temporary.key; }
            else
            {
                if(errno != EEXIST) { return false; }
                auto const other{search(uid_ring)};
                if(!serial_value(other)) { return false; }
                winner = static_cast<serial>(other);
            }
            return temporary.cleanup();
        }
        [[nodiscard]] inline ::std::array<::std::byte, 32u> fingerprint(secret_bytes<32u> const& bytes) noexcept
        {
            ::fast_io::sha256_context hash{};
            hash.update(bytes.value.data(), bytes.value.data() + bytes.value.size()); hash.do_final();
            ::std::array<::std::byte, 32u> value{}; hash.digest_to_byte_ptr(value.data());
            static_assert(::std::is_trivially_destructible_v<decltype(hash)>);
            erase(::std::addressof(hash), sizeof(hash)); return value;
        }
        class state
        {
            ::std::mutex mutex_{};
            bool initialized_{}, disabled_{};
            serial ring_{}, key_{};
            ::uid_t uid_{}; ::gid_t gid_{};
            ::std::array<::std::byte, 32u> fingerprint_{}; // not a secret or a signing seed
        public:
            [[nodiscard]] bool read(runtime_origin::use const& actual, secret_bytes<32u>& output) noexcept
            {
                if(!dumping_disabled() || !runtime_origin::secret_reads_allowed(actual)) { return false; } // BEFORE any secret lookup/read
                ::std::lock_guard lock{mutex_};
                if(disabled_) { return false; }
                auto fail{[&]() noexcept { disabled_ = true; erase(output.value.data(), output.value.size()); return false; }};
                if(::getuid() != ::geteuid() || ::getgid() != ::getegid()) { return fail(); }
                if(!initialized_)
                {
                    uid_ = ::getuid(); gid_ = ::getgid();
                    auto const ring{control(KEYCTL_GET_KEYRING_ID, number(KEY_SPEC_USER_KEYRING), 1ul)};
                    if(!serial_value(ring)) { return fail(); }
                    ring_ = static_cast<serial>(ring);
                    key_description ring_info{};
                    if(!describe(ring_, ring_info) || ring_info.type != "keyring" || ring_info.uid != uid_) { return fail(); }
                    auto const existing{search(ring_)};
                    if(serial_value(existing)) { key_ = static_cast<serial>(existing); }
                    else
                    {
                        if(existing != -1l || errno != ENOKEY || !publish(ring_, uid_, gid_, key_)) { return fail(); }
                    }
                    if(!runtime_origin::secret_reads_allowed(actual) || !read_exact(ring_, key_, uid_, gid_, output)) { return fail(); }
                    fingerprint_ = fingerprint(output); initialized_ = true;
                }
                else if(::getuid() != uid_ || ::getgid() != gid_ ||
                    control(KEYCTL_GET_KEYRING_ID, number(KEY_SPEC_USER_KEYRING), 0ul) != ring_ ||
                    !runtime_origin::secret_reads_allowed(actual) || !read_exact(ring_, key_, uid_, gid_, output) || fingerprint(output) != fingerprint_)
                { return fail(); }
                return dumping_disabled() && runtime_origin::secret_reads_allowed(actual) ? true : fail();
            }
            // Async publication never reloads secret payload after Guest code
            // may run. The exact immutable key cannot be updated/setattr by its
            // owner, and every nonsecret binding/permission is re-authenticated.
            [[nodiscard]] bool check(runtime_origin::use const& actual) noexcept
            {
                if(!dumping_disabled() || !runtime_origin::authorized(actual)) { return false; }
                ::std::lock_guard lock{mutex_};
                if(disabled_ || !initialized_) { return false; }
                if(::getuid() != uid_ || ::geteuid() != uid_ || ::getgid() != gid_ || ::getegid() != gid_ ||
                   control(KEYCTL_GET_KEYRING_ID, number(KEY_SPEC_USER_KEYRING), 0ul) != ring_ ||
                   !exact_key(key_, uid_, gid_) || !member(ring_, key_) || search(ring_) != key_)
                { disabled_ = true; return false; }
                return dumping_disabled() && runtime_origin::authorized(actual);
            }
        };
        [[nodiscard]] inline state& actual_state() noexcept
        {
            // The existing cache writer has its own explicit OS-thread
            // retirement. A function-static destructor must not destroy this
            // mutex before a late worker's final origin check.
            alignas(state) static ::std::byte storage[sizeof(state)];
            static state* const value{::new (static_cast<void*>(storage)) state{}};
            return *value;
        }
    }
#endif

    [[nodiscard]] inline bool read(runtime_origin::use const& actual, secret_bytes<32u>& output) noexcept
    {
#if defined(__linux__) && defined(SYS_keyctl) && defined(SYS_add_key) && defined(SYS_getrandom) && defined(KEYCTL_MOVE_EXCL)
        return details::actual_state().read(actual, output);
#else
        (void)actual; (void)output; return false;
#endif
    }
    [[nodiscard]] inline bool check_binding(runtime_origin::use const& actual) noexcept
    {
#if defined(__linux__) && defined(SYS_keyctl) && defined(SYS_add_key) && defined(SYS_getrandom) && defined(KEYCTL_MOVE_EXCL)
        return details::actual_state().check(actual);
#else
        (void)actual; return false;
#endif
    }
}
