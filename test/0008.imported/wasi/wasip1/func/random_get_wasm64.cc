#include <cstddef>
#include <cstdint>
#include <cstring>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/random_get_wasm64.h>
#ifdef UWVM_DLLIMPORT
# error "UWVM_DLLIMPORT existed"
#endif

#ifdef UWVM_WASM_SUPPORT_WASM1
# error "UWVM_WASM_SUPPORT_WASM1 existed"
#endif

#ifdef UWVM_AES_RST_ALL
# error "UWVM_AES_RST_ALL existed"
#endif

#ifdef UWVM_COLOR_RST_ALL
# error "UWVM_COLOR_RST_ALL existed"
#endif

#ifdef UWVM_WIN32_TEXTATTR_RST_ALL
# error "UWVM_WIN32_TEXTATTR_RST_ALL existed"
#endif

#ifdef UWVM_IMPORT_WASI
# error "UWVM_IMPORT_WASI existed"
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1
# error "UWVM_IMPORT_WASI_WASIP1 existed"
#endif
int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Case 0: zero-length buffer should succeed and not touch memory around it
    {
        constexpr wasi_void_ptr_wasm64_t buf{1024u};
        constexpr wasi_size_wasm64_t len{0u};

        unsigned char pattern[32];
        ::std::memset(pattern, 0x7Au, sizeof(pattern));
        ::std::memset(memory.memory_begin + buf, 0x7Au, sizeof(pattern));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::random_get_wasm64(env, buf, len);
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

        if(::std::memcmp(memory.memory_begin + buf, pattern, sizeof(pattern)) != 0) { ::fast_io::fast_terminate(); }
    }

    // Case 1: non-zero buffer should be filled, guards must remain unchanged
    {
        constexpr ::std::size_t GUARD_SIZE{16u};
        constexpr ::std::size_t BUF_LEN{64u};

        constexpr wasi_void_ptr_wasm64_t guard_before_ptr{4096u};
        constexpr wasi_void_ptr_wasm64_t buf{static_cast<wasi_void_ptr_wasm64_t>(guard_before_ptr + static_cast<wasi_void_ptr_wasm64_t>(GUARD_SIZE))};
        constexpr wasi_void_ptr_wasm64_t guard_after_ptr{static_cast<wasi_void_ptr_wasm64_t>(buf + static_cast<wasi_void_ptr_wasm64_t>(BUF_LEN))};

        unsigned char guard_before_pattern[GUARD_SIZE];
        unsigned char guard_after_pattern[GUARD_SIZE];
        ::std::memset(guard_before_pattern, 0x11, GUARD_SIZE);
        ::std::memset(guard_after_pattern, 0x22, GUARD_SIZE);

        // Initialize whole region to zero first
        ::std::memset(memory.memory_begin + guard_before_ptr, 0x00, GUARD_SIZE + BUF_LEN + GUARD_SIZE);

        // Write guard patterns
        ::std::memcpy(memory.memory_begin + guard_before_ptr, guard_before_pattern, GUARD_SIZE);
        ::std::memcpy(memory.memory_begin + guard_after_ptr, guard_after_pattern, GUARD_SIZE);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::random_get_wasm64(env, buf, static_cast<wasi_size_wasm64_t>(BUF_LEN));
        if(ret != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

        // Guards must be preserved
        if(::std::memcmp(memory.memory_begin + guard_before_ptr, guard_before_pattern, GUARD_SIZE) != 0) { ::fast_io::fast_terminate(); }
        if(::std::memcmp(memory.memory_begin + guard_after_ptr, guard_after_pattern, GUARD_SIZE) != 0) { ::fast_io::fast_terminate(); }

        // Buffer should not be all zeros after random_get_wasm64
        unsigned char zeros[BUF_LEN]{};
        if(::std::memcmp(memory.memory_begin + buf, zeros, BUF_LEN) == 0) { ::fast_io::fast_terminate(); }
    }
}
