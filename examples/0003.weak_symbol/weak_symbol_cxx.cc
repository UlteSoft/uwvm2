#include "../0002.dl/interface.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_para_t
{
    wasm_i32 lhs;
    wasm_i32 rhs;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_res_t
{
    wasm_i32 value;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    probe_host_apis_para_t
{
    wasi_void_ptr_t argc_ptr;
    wasi_void_ptr_t argv_buf_size_ptr;
};
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

namespace
{
    uwvm_preload_host_api_v1 const* g_preload_host_api{};
    uwvm_wasip1_host_api_v1 const* g_wasip1_host_api{};

    char const module_name_str[] = "weak.example";
    char const func_add_i32_name[] = "add_i32";
    char const func_inspect_memory_name[] = "inspect_memory";
    char const func_probe_host_apis_name[] = "probe_host_apis";
    char const custom_name_demo[] = "demo_custom";

    std::uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
    std::uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};
    std::uint_least8_t const inspect_memory_res_types[] = {WASM_VALTYPE_I32};
    std::uint_least8_t const probe_host_apis_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
    std::uint_least8_t const probe_host_apis_res_types[] = {WASM_VALTYPE_I32};

    bool range_is_valid(std::uint_least64_t byte_length, std::uint_least64_t offset, std::size_t size) noexcept
    {
        auto const size64{static_cast<std::uint_least64_t>(size)};
        return offset <= byte_length && size64 <= (byte_length - offset);
    }

    bool find_memory_descriptor(std::size_t memory_index, uwvm_preload_memory_descriptor_t& out) noexcept
    {
        if(g_preload_host_api == nullptr) { return false; }
        if(g_preload_host_api->abi_version != UWVM_EXAMPLE_PRELOAD_HOST_API_V1_ABI_VERSION) { return false; }
        if(g_preload_host_api->struct_size < sizeof(*g_preload_host_api)) { return false; }

        auto const count{g_preload_host_api->memory_descriptor_count()};
        for(std::size_t i{}; i != count; ++i)
        {
            uwvm_preload_memory_descriptor_t curr{};
            if(!g_preload_host_api->memory_descriptor_at(i, &curr)) { continue; }
            if(curr.memory_index == memory_index)
            {
                out = curr;
                return true;
            }
        }
        return false;
    }

    bool direct_range_is_valid(uwvm_preload_memory_descriptor_t const& descriptor, std::uint_least64_t offset, std::size_t size) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
                return range_is_valid(descriptor.byte_length, offset, size);
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
                return range_is_valid(descriptor.partial_protection_limit_bytes, offset, size) ||
                       range_is_valid(descriptor.byte_length, offset, size);
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
                return range_is_valid(descriptor.byte_length, offset, size);
            default:
                return false;
        }
    }

    bool read_memory_bytes(uwvm_preload_memory_descriptor_t const& descriptor, std::uint_least64_t offset, void* out, std::size_t size) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
                return g_preload_host_api != nullptr && g_preload_host_api->memory_read(descriptor.memory_index, offset, out, size);
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
                if(descriptor.mmap_view_begin == nullptr || !direct_range_is_valid(descriptor, offset, size)) { return false; }
                std::memcpy(out, static_cast<unsigned char const*>(descriptor.mmap_view_begin) + static_cast<std::size_t>(offset), size);
                return true;
            default:
                return false;
        }
    }

    bool write_memory_byte(uwvm_preload_memory_descriptor_t const& descriptor, std::uint_least64_t offset, std::uint_least8_t value) noexcept
    {
        switch(descriptor.delivery_state)
        {
            case UWVM_PRELOAD_MEMORY_DELIVERY_COPY:
                return g_preload_host_api != nullptr && g_preload_host_api->memory_write(descriptor.memory_index, offset, &value, sizeof(value));
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_FULL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_PARTIAL_PROTECTION:
            case UWVM_PRELOAD_MEMORY_DELIVERY_MMAP_DYNAMIC_BOUNDS:
                if(descriptor.mmap_view_begin == nullptr || !direct_range_is_valid(descriptor, offset, sizeof(value))) { return false; }
                std::memcpy(static_cast<unsigned char*>(descriptor.mmap_view_begin) + static_cast<std::size_t>(offset), &value, sizeof(value));
                return true;
            default:
                return false;
        }
    }

    bool read_wasm_u32_le(uwvm_preload_memory_descriptor_t const& descriptor, std::uint_least64_t offset, wasm_u32& out) noexcept
    {
        std::array<std::uint_least8_t, 4> bytes{};
        if(!read_memory_bytes(descriptor, offset, bytes.data(), bytes.size())) { return false; }
        out = static_cast<wasm_u32>(bytes[0]) | (static_cast<wasm_u32>(bytes[1]) << 8u) | (static_cast<wasm_u32>(bytes[2]) << 16u) |
              (static_cast<wasm_u32>(bytes[3]) << 24u);
        return true;
    }

    void write_i32_result(unsigned char* res_bytes, wasm_i32 value) noexcept
    {
        if(res_bytes != nullptr) { std::memcpy(res_bytes, &value, sizeof(value)); }
    }

    void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        if(res_bytes == nullptr || para_bytes == nullptr) { return; }
        add_i32_para_t para{};
        add_i32_res_t res{};
        std::memcpy(&para, para_bytes, sizeof(para));
        res.value = static_cast<wasm_i32>(para.lhs + para.rhs);
        std::memcpy(res_bytes, &res, sizeof(res));
    }

    void inspect_memory_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        static_cast<void>(para_bytes);
        uwvm_preload_memory_descriptor_t descriptor{};
        if(!find_memory_descriptor(0u, descriptor))
        {
            std::fprintf(stderr, "probe_host_apis: preload api missing\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -1);
            return;
        }

        std::uint_least8_t b0{}, b1{}, b2{}, b3{};
        if(!read_memory_bytes(descriptor, 0u, &b0, 1u) || !read_memory_bytes(descriptor, 1u, &b1, 1u) ||
           !read_memory_bytes(descriptor, 2u, &b2, 1u) || !read_memory_bytes(descriptor, 3u, &b3, 1u))
        {
            std::fprintf(stderr, "probe_host_apis: preload api abi mismatch\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -2);
            return;
        }
        if(!write_memory_byte(descriptor, 1u, static_cast<std::uint_least8_t>('Z')))
        {
            std::fprintf(stderr, "probe_host_apis: memory[0] descriptor missing\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -3);
            return;
        }

        auto const sum{static_cast<wasm_i32>(b0 + b1 + b2 + b3)};
        write_i32_result(res_bytes, sum);
        std::fprintf(stderr, "example-weak-cxx: bytes=[%u,%u,%u,%u] sum=%d\n", static_cast<unsigned>(b0), static_cast<unsigned>(b1), static_cast<unsigned>(b2), static_cast<unsigned>(b3), static_cast<int>(sum));
        std::fflush(stderr);
    }

    void probe_host_apis_impl(unsigned char* res_bytes, unsigned char* para_bytes)
    {
        if(res_bytes == nullptr || para_bytes == nullptr) { return; }
        probe_host_apis_para_t para{};
        std::memcpy(&para, para_bytes, sizeof(para));

        if(g_preload_host_api == nullptr)
        {
            write_i32_result(res_bytes, -1);
            return;
        }
        if(g_preload_host_api->abi_version != UWVM_EXAMPLE_PRELOAD_HOST_API_V1_ABI_VERSION || g_preload_host_api->struct_size < sizeof(*g_preload_host_api))
        {
            write_i32_result(res_bytes, -2);
            return;
        }

        uwvm_preload_memory_descriptor_t descriptor{};
        if(!find_memory_descriptor(0u, descriptor))
        {
            write_i32_result(res_bytes, -3);
            return;
        }
        if(g_wasip1_host_api == nullptr)
        {
            std::fprintf(stderr, "probe_host_apis: wasip1 api missing (gate off or setter not called)\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -4);
            return;
        }
        if(g_wasip1_host_api->abi_version != UWVM_EXAMPLE_WASIP1_HOST_API_V1_ABI_VERSION || g_wasip1_host_api->struct_size < sizeof(*g_wasip1_host_api))
        {
            std::fprintf(stderr, "probe_host_apis: wasip1 api abi mismatch\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -5);
            return;
        }

        auto const err{g_wasip1_host_api->args_sizes_get(para.argc_ptr, para.argv_buf_size_ptr)};
        if(err != UWVM_EXAMPLE_WASI_ERRNO_SUCCESS)
        {
            std::fprintf(stderr, "probe_host_apis: args_sizes_get returned error\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -6);
            return;
        }

        wasm_u32 argc_value{};
        wasm_u32 argv_buf_size_value{};
        if(!read_wasm_u32_le(descriptor, para.argc_ptr, argc_value))
        {
            std::fprintf(stderr, "probe_host_apis: failed to read argc value\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -7);
            return;
        }
        if(!read_wasm_u32_le(descriptor, para.argv_buf_size_ptr, argv_buf_size_value))
        {
            std::fprintf(stderr, "probe_host_apis: failed to read argv_buf_size value\n");
            std::fflush(stderr);
            write_i32_result(res_bytes, -8);
            return;
        }
        std::fprintf(stderr,
                     "example-weak-cxx: wasi argc=%" PRIuLEAST32 " argv_buf_size=%" PRIuLEAST32 "\n",
                     argc_value,
                     argv_buf_size_value);
        std::fflush(stderr);
        write_i32_result(res_bytes, 0);
    }

    void demo_custom_handle() {}

    void uwvm_set_preload_host_api_v1_impl(uwvm_preload_host_api_v1 const* api)
    {
        g_preload_host_api = api;
    std::fprintf(stderr, "setter: preload api=%p\n", static_cast<void const*>(api));
    std::fflush(stderr);
    }

    void uwvm_set_wasip1_host_api_v1_impl(uwvm_wasip1_host_api_v1 const* api)
    {
        g_wasip1_host_api = api;
    std::fprintf(stderr, "setter: wasip1 api=%p\n", static_cast<void const*>(api));
    std::fflush(stderr);
    }
}

extern "C" uwvm_weak_symbol_module_vector_c const* uwvm_weak_symbol_module()
{
    static capi_custom_handler_t const handlers[] = {
        {custom_name_demo, sizeof(custom_name_demo) - 1u, &demo_custom_handle},
    };
    static capi_function_t const functions[] = {
        {func_add_i32_name,
         sizeof(func_add_i32_name) - 1u,
         add_i32_para_types,
         sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0]),
         add_i32_res_types,
         sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0]),
         &add_i32_impl},
        {func_inspect_memory_name,
         sizeof(func_inspect_memory_name) - 1u,
         nullptr,
         0u,
         inspect_memory_res_types,
         sizeof(inspect_memory_res_types) / sizeof(inspect_memory_res_types[0]),
         &inspect_memory_impl},
        {func_probe_host_apis_name,
         sizeof(func_probe_host_apis_name) - 1u,
         probe_host_apis_para_types,
         sizeof(probe_host_apis_para_types) / sizeof(probe_host_apis_para_types[0]),
         probe_host_apis_res_types,
         sizeof(probe_host_apis_res_types) / sizeof(probe_host_apis_res_types[0]),
         &probe_host_apis_impl},
    };
    static uwvm_weak_symbol_module_c const modules[] = {
        {module_name_str,
         sizeof(module_name_str) - 1u,
         capi_custom_handler_vec_t{handlers, sizeof(handlers) / sizeof(handlers[0])},
         capi_function_vec_t{functions, sizeof(functions) / sizeof(functions[0])},
         &uwvm_set_preload_host_api_v1_impl,
         &uwvm_set_wasip1_host_api_v1_impl},
    };
    static uwvm_weak_symbol_module_vector_c const vec = {modules, sizeof(modules) / sizeof(modules[0])};
    return &vec;
}
