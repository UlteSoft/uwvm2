#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#if !defined(_WIN32)
# include <sys/wait.h>
#endif

namespace
{

    // Generated from tiny WASI/module fixtures so this test stays independent
    // from a host-installed wat2wasm.
    inline constexpr ::std::array<unsigned char, 91uz> main_entry_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0au, 0x02u, 0x60u,
        0x02u, 0x7fu, 0x7fu, 0x01u, 0x7fu, 0x60u, 0x00u, 0x00u, 0x02u, 0x29u, 0x01u, 0x16u,
        0x77u, 0x61u, 0x73u, 0x69u, 0x5fu, 0x73u, 0x6eu, 0x61u, 0x70u, 0x73u, 0x68u, 0x6fu,
        0x74u, 0x5fu, 0x70u, 0x72u, 0x65u, 0x76u, 0x69u, 0x65u, 0x77u, 0x31u, 0x0eu, 0x61u,
        0x72u, 0x67u, 0x73u, 0x5fu, 0x73u, 0x69u, 0x7au, 0x65u, 0x73u, 0x5fu, 0x67u, 0x65u,
        0x74u, 0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x01u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u,
        0x08u, 0x01u, 0x01u, 0x0au, 0x0eu, 0x01u, 0x0cu, 0x00u, 0x41u, 0x00u, 0x41u, 0x04u,
        0x10u, 0x00u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::array<unsigned char, 147uz> preload_lib_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0bu, 0x02u, 0x60u,
        0x02u, 0x7fu, 0x7fu, 0x01u, 0x7fu, 0x60u, 0x00u, 0x01u, 0x7fu, 0x02u, 0x29u, 0x01u,
        0x16u, 0x77u, 0x61u, 0x73u, 0x69u, 0x5fu, 0x73u, 0x6eu, 0x61u, 0x70u, 0x73u, 0x68u,
        0x6fu, 0x74u, 0x5fu, 0x70u, 0x72u, 0x65u, 0x76u, 0x69u, 0x65u, 0x77u, 0x31u, 0x0eu,
        0x61u, 0x72u, 0x67u, 0x73u, 0x5fu, 0x73u, 0x69u, 0x7au, 0x65u, 0x73u, 0x5fu, 0x67u,
        0x65u, 0x74u, 0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x01u, 0x01u, 0x05u, 0x03u, 0x01u,
        0x00u, 0x01u, 0x07u, 0x20u, 0x02u, 0x08u, 0x67u, 0x65u, 0x74u, 0x5fu, 0x61u, 0x72u,
        0x67u, 0x63u, 0x00u, 0x01u, 0x11u, 0x67u, 0x65u, 0x74u, 0x5fu, 0x61u, 0x72u, 0x67u,
        0x76u, 0x5fu, 0x62u, 0x75u, 0x66u, 0x5fu, 0x73u, 0x69u, 0x7au, 0x65u, 0x00u, 0x02u,
        0x0au, 0x25u, 0x02u, 0x11u, 0x00u, 0x41u, 0x00u, 0x41u, 0x04u, 0x10u, 0x00u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x41u, 0x00u, 0x28u, 0x02u, 0x00u, 0x0bu, 0x11u, 0x00u, 0x41u,
        0x00u, 0x41u, 0x04u, 0x10u, 0x00u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x04u, 0x28u,
        0x02u, 0x00u, 0x0bu};

    inline constexpr ::std::array<unsigned char, 113uz> preload_main_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0du, 0x03u, 0x60u,
        0x00u, 0x01u, 0x7fu, 0x60u, 0x02u, 0x7fu, 0x7fu, 0x00u, 0x60u, 0x00u, 0x00u, 0x02u,
        0x32u, 0x02u, 0x08u, 0x6cu, 0x69u, 0x62u, 0x2eu, 0x74u, 0x65u, 0x73u, 0x74u, 0x08u,
        0x67u, 0x65u, 0x74u, 0x5fu, 0x61u, 0x72u, 0x67u, 0x63u, 0x00u, 0x00u, 0x08u, 0x6cu,
        0x69u, 0x62u, 0x2eu, 0x74u, 0x65u, 0x73u, 0x74u, 0x11u, 0x67u, 0x65u, 0x74u, 0x5fu,
        0x61u, 0x72u, 0x67u, 0x76u, 0x5fu, 0x62u, 0x75u, 0x66u, 0x5fu, 0x73u, 0x69u, 0x7au,
        0x65u, 0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x01u, 0x02u, 0x08u, 0x01u, 0x03u, 0x0au,
        0x1cu, 0x02u, 0x0bu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x0bu, 0x0eu, 0x00u, 0x10u, 0x00u, 0x41u, 0x03u, 0x10u, 0x02u, 0x10u, 0x01u,
        0x41u, 0x10u, 0x10u, 0x02u, 0x0bu};

    inline constexpr ::std::array<unsigned char, 190uz> dl_main_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x13u, 0x04u, 0x60u,
        0x02u, 0x7fu, 0x7fu, 0x01u, 0x7fu, 0x60u, 0x00u, 0x01u, 0x7fu, 0x60u, 0x02u, 0x7fu,
        0x7fu, 0x00u, 0x60u, 0x00u, 0x00u, 0x02u, 0x4fu, 0x03u, 0x0au, 0x64u, 0x6cu, 0x2eu,
        0x65u, 0x78u, 0x61u, 0x6du, 0x70u, 0x6cu, 0x65u, 0x07u, 0x61u, 0x64u, 0x64u, 0x5fu,
        0x69u, 0x33u, 0x32u, 0x00u, 0x00u, 0x0au, 0x64u, 0x6cu, 0x2eu, 0x65u, 0x78u, 0x61u,
        0x6du, 0x70u, 0x6cu, 0x65u, 0x0eu, 0x69u, 0x6eu, 0x73u, 0x70u, 0x65u, 0x63u, 0x74u,
        0x5fu, 0x6du, 0x65u, 0x6du, 0x6fu, 0x72u, 0x79u, 0x00u, 0x01u, 0x0au, 0x64u, 0x6cu,
        0x2eu, 0x65u, 0x78u, 0x61u, 0x6du, 0x70u, 0x6cu, 0x65u, 0x0fu, 0x70u, 0x72u, 0x6fu,
        0x62u, 0x65u, 0x5fu, 0x68u, 0x6fu, 0x73u, 0x74u, 0x5fu, 0x61u, 0x70u, 0x69u, 0x73u,
        0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x02u, 0x03u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u,
        0x08u, 0x01u, 0x04u, 0x0au, 0x35u, 0x02u, 0x0bu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u,
        0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu, 0x27u, 0x00u, 0x41u, 0x07u, 0x41u, 0x09u,
        0x10u, 0x00u, 0x41u, 0x10u, 0x10u, 0x03u, 0x10u, 0x01u, 0x41u, 0x8au, 0x02u, 0x10u,
        0x03u, 0x41u, 0x01u, 0x2du, 0x00u, 0x00u, 0x41u, 0xdau, 0x00u, 0x10u, 0x03u, 0x41u,
        0x20u, 0x41u, 0x24u, 0x10u, 0x02u, 0x41u, 0x00u, 0x10u, 0x03u, 0x0bu, 0x0bu, 0x0au,
        0x01u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x04u, 0x41u, 0x42u, 0x43u, 0x44u};

    inline constexpr ::std::array<unsigned char, 108uz> dl_group_shared_fd_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x08u, 0x02u, 0x60u,
        0x00u, 0x01u, 0x7fu, 0x60u, 0x00u, 0x00u, 0x02u, 0x37u, 0x02u, 0x08u, 0x64u, 0x6cu,
        0x2eu, 0x61u, 0x6cu, 0x70u, 0x68u, 0x61u, 0x0cu, 0x63u, 0x6cu, 0x6fu, 0x73u, 0x65u,
        0x5fu, 0x73u, 0x74u, 0x64u, 0x6fu, 0x75u, 0x74u, 0x00u, 0x00u, 0x07u, 0x64u, 0x6cu,
        0x2eu, 0x62u, 0x65u, 0x74u, 0x61u, 0x13u, 0x70u, 0x72u, 0x6fu, 0x62u, 0x65u, 0x5fu,
        0x73u, 0x74u, 0x64u, 0x6fu, 0x75u, 0x74u, 0x5fu, 0x63u, 0x6cu, 0x6fu, 0x73u, 0x65u,
        0x64u, 0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x01u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u,
        0x08u, 0x01u, 0x02u, 0x0au, 0x13u, 0x01u, 0x11u, 0x00u, 0x10u, 0x00u, 0x04u, 0x40u,
        0x00u, 0x0bu, 0x10u, 0x01u, 0x41u, 0x01u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::string_view dl_close_stdout_plugin_source = R"c(
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef int_least32_t wasm_i32;
typedef uint_least16_t uwvm_wasi_errno_t;
typedef int_least32_t wasi_fd_t;
typedef void (*capi_wasm_function)(unsigned char*, unsigned char*);

typedef struct capi_module_name_t_def
{
    char const* name;
    size_t name_length;
} capi_module_name_t;

typedef struct capi_function_t_def
{
    char const* func_name_ptr;
    size_t func_name_length;
    uint_least8_t const* para_type_vec_begin;
    size_t para_type_vec_size;
    uint_least8_t const* res_type_vec_begin;
    size_t res_type_vec_size;
    capi_wasm_function func_ptr;
} capi_function_t;

typedef struct capi_function_vec_t_def
{
    capi_function_t const* function_begin;
    size_t function_size;
} capi_function_vec_t;

typedef void (*uwvm_wasip1_placeholder_fn_t)(void);
typedef uwvm_wasi_errno_t (*uwvm_wasip1_fd_close_t)(wasi_fd_t);

typedef struct uwvm_wasip1_host_api_v1_def
{
    size_t struct_size;
    uint_least32_t abi_version;
    uwvm_wasip1_placeholder_fn_t args_get;
    uwvm_wasip1_placeholder_fn_t args_sizes_get;
    uwvm_wasip1_placeholder_fn_t clock_res_get;
    uwvm_wasip1_placeholder_fn_t clock_time_get;
    uwvm_wasip1_placeholder_fn_t environ_get;
    uwvm_wasip1_placeholder_fn_t environ_sizes_get;
    uwvm_wasip1_placeholder_fn_t fd_advise;
    uwvm_wasip1_placeholder_fn_t fd_allocate;
    uwvm_wasip1_fd_close_t fd_close;
} uwvm_wasip1_host_api_v1;

static uwvm_wasip1_host_api_v1 const* g_wasip1_host_api;
static char const module_name_str[] = "dl.alpha";
static char const func_name_str[] = "close_stdout";
static uint_least8_t const result_types[] = {0x7f};

typedef struct result_t_def
{
    wasm_i32 value;
} result_t;

static void write_i32_result(unsigned char* res_bytes, wasm_i32 value)
{
    result_t res;
    if(res_bytes == 0) { return; }
    res.value = value;
    memcpy(res_bytes, &res, sizeof(res));
}

static void close_stdout_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    (void)para_bytes;

    if(g_wasip1_host_api == 0 || g_wasip1_host_api->struct_size < offsetof(uwvm_wasip1_host_api_v1, fd_close) + sizeof(g_wasip1_host_api->fd_close) ||
       g_wasip1_host_api->fd_close == 0)
    {
        write_i32_result(res_bytes, -1);
        return;
    }

    write_i32_result(res_bytes, (wasm_i32)g_wasip1_host_api->fd_close((wasi_fd_t)1));
}

void uwvm_set_wasip1_host_api_v1(uwvm_wasip1_host_api_v1 const* api)
{
    g_wasip1_host_api = api;
}

capi_module_name_t uwvm_get_module_name(void)
{
    capi_module_name_t ret;
    ret.name = module_name_str;
    ret.name_length = sizeof(module_name_str) - 1u;
    return ret;
}

capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {func_name_str, sizeof(func_name_str) - 1u, 0, 0u, result_types, sizeof(result_types) / sizeof(result_types[0]), &close_stdout_impl},
    };
    capi_function_vec_t ret;
    ret.function_begin = functions;
    ret.function_size = sizeof(functions) / sizeof(functions[0]);
    return ret;
}
)c";

    inline constexpr ::std::string_view dl_probe_stdout_closed_plugin_source = R"c(
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef int_least32_t wasm_i32;
typedef uint_least32_t wasi_void_ptr_t;
typedef uint_least16_t uwvm_wasi_errno_t;
typedef int_least32_t wasi_fd_t;
typedef void (*capi_wasm_function)(unsigned char*, unsigned char*);

typedef struct capi_module_name_t_def
{
    char const* name;
    size_t name_length;
} capi_module_name_t;

typedef struct capi_function_t_def
{
    char const* func_name_ptr;
    size_t func_name_length;
    uint_least8_t const* para_type_vec_begin;
    size_t para_type_vec_size;
    uint_least8_t const* res_type_vec_begin;
    size_t res_type_vec_size;
    capi_wasm_function func_ptr;
} capi_function_t;

typedef struct capi_function_vec_t_def
{
    capi_function_t const* function_begin;
    size_t function_size;
} capi_function_vec_t;

typedef void (*uwvm_wasip1_placeholder_fn_t)(void);
typedef uwvm_wasi_errno_t (*uwvm_wasip1_fd_fdstat_get_t)(wasi_fd_t, wasi_void_ptr_t);

typedef struct uwvm_wasip1_host_api_v1_def
{
    size_t struct_size;
    uint_least32_t abi_version;
    uwvm_wasip1_placeholder_fn_t args_get;
    uwvm_wasip1_placeholder_fn_t args_sizes_get;
    uwvm_wasip1_placeholder_fn_t clock_res_get;
    uwvm_wasip1_placeholder_fn_t clock_time_get;
    uwvm_wasip1_placeholder_fn_t environ_get;
    uwvm_wasip1_placeholder_fn_t environ_sizes_get;
    uwvm_wasip1_placeholder_fn_t fd_advise;
    uwvm_wasip1_placeholder_fn_t fd_allocate;
    uwvm_wasip1_placeholder_fn_t fd_close;
    uwvm_wasip1_placeholder_fn_t fd_datasync;
    uwvm_wasip1_fd_fdstat_get_t fd_fdstat_get;
} uwvm_wasip1_host_api_v1;

static uwvm_wasip1_host_api_v1 const* g_wasip1_host_api;
static char const module_name_str[] = "dl.beta";
static char const func_name_str[] = "probe_stdout_closed";
static uint_least8_t const result_types[] = {0x7f};

typedef struct result_t_def
{
    wasm_i32 value;
} result_t;

static void write_i32_result(unsigned char* res_bytes, wasm_i32 value)
{
    result_t res;
    if(res_bytes == 0) { return; }
    res.value = value;
    memcpy(res_bytes, &res, sizeof(res));
}

static void probe_stdout_closed_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    uwvm_wasi_errno_t err;
    (void)para_bytes;

    if(g_wasip1_host_api == 0 ||
       g_wasip1_host_api->struct_size < offsetof(uwvm_wasip1_host_api_v1, fd_fdstat_get) + sizeof(g_wasip1_host_api->fd_fdstat_get) ||
       g_wasip1_host_api->fd_fdstat_get == 0)
    {
        write_i32_result(res_bytes, -1);
        return;
    }

    err = g_wasip1_host_api->fd_fdstat_get((wasi_fd_t)1, (wasi_void_ptr_t)0u);
    write_i32_result(res_bytes, err == 0u ? 0 : 1);
}

void uwvm_set_wasip1_host_api_v1(uwvm_wasip1_host_api_v1 const* api)
{
    g_wasip1_host_api = api;
}

capi_module_name_t uwvm_get_module_name(void)
{
    capi_module_name_t ret;
    ret.name = module_name_str;
    ret.name_length = sizeof(module_name_str) - 1u;
    return ret;
}

capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {func_name_str, sizeof(func_name_str) - 1u, 0, 0u, result_types, sizeof(result_types) / sizeof(result_types[0]), &probe_stdout_closed_impl},
    };
    capi_function_vec_t ret;
    ret.function_begin = functions;
    ret.function_size = sizeof(functions) / sizeof(functions[0]);
    return ret;
}
)c";

    struct command_result
    {
        int exit_code{};
        ::std::string output{};
    };

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    {
        return ::std::string{"\""} + path.string() + "\"";
    }

    [[nodiscard]] ::std::filesystem::path find_repo_root(::std::filesystem::path dir)
    {
        for(;;)
        {
            if(::std::filesystem::exists(dir / "xmake.lua")) [[likely]] { return dir; }
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path dir)
    {
        for(;;)
        {
            auto const candidate{dir / "uwvm"};
            if(::std::filesystem::exists(candidate)) [[likely]] { return candidate; }
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] int normalize_exit_code(int status) noexcept
    {
#if defined(_WIN32)
        return status;
#else
        if(status == -1) [[unlikely]] { return -1; }
        if(WIFEXITED(status)) [[likely]] { return WEXITSTATUS(status); }
        if(WIFSIGNALED(status)) { return 128 + WTERMSIG(status); }
        return status;
#endif
    }

    template <::std::size_t N>
    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path, ::std::array<unsigned char, N> const& wasm_bytes)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create fixture directory: " << wasm_path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to open fixture output: " << wasm_path << '\n';
            return false;
        }

        output.write(reinterpret_cast<char const*>(wasm_bytes.data()), static_cast<::std::streamsize>(wasm_bytes.size()));
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to write fixture output: " << wasm_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool write_text_fixture(::std::filesystem::path const& file_path, ::std::string_view text)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(file_path.parent_path(), ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create fixture directory: " << file_path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(file_path, ::std::ios::binary | ::std::ios::trunc);
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to open text fixture output: " << file_path << '\n';
            return false;
        }

        output.write(text.data(), static_cast<::std::streamsize>(text.size()));
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to write text fixture output: " << file_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] ::std::string read_text_file(::std::filesystem::path const& file_path)
    {
        ::std::ifstream input(file_path, ::std::ios::binary);
        if(!input) { return {}; }
        return {::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{}};
    }

    [[nodiscard]] command_result run_command_capture(::std::string const& command, ::std::filesystem::path const& capture_path)
    {
        ::std::error_code ec{};
        ::std::filesystem::remove(capture_path, ec);
        static_cast<void>(ec);

        auto const full_command{command + " > " + quote_argument(capture_path) + " 2>&1"};
        auto const exit_code{normalize_exit_code(::std::system(full_command.c_str()))};
        return command_result{.exit_code = exit_code, .output = read_text_file(capture_path)};
    }

    [[nodiscard]] bool expect_success(::std::string_view case_name, command_result const& result, ::std::string_view must_contain = {})
    {
        if(result.exit_code == 0 && (must_contain.empty() || result.output.find(must_contain) != ::std::string::npos)) [[likely]] { return true; }

        ::std::cerr << "[FAIL] " << case_name << ": expected success";
        if(!must_contain.empty()) { ::std::cerr << " with output containing `" << must_contain << '`'; }
        ::std::cerr << ", got exit=" << result.exit_code << '\n';
        if(!result.output.empty()) { ::std::cerr << result.output; }
        if(result.output.empty() || result.output.back() != '\n') { ::std::cerr << '\n'; }
        return false;
    }

    [[nodiscard]] bool expect_failure(::std::string_view case_name, command_result const& result, ::std::string_view must_contain = {})
    {
        if(result.exit_code != 0 && (must_contain.empty() || result.output.find(must_contain) != ::std::string::npos)) [[likely]] { return true; }

        ::std::cerr << "[FAIL] " << case_name << ": expected failure";
        if(!must_contain.empty()) { ::std::cerr << " with output containing `" << must_contain << '`'; }
        ::std::cerr << ", got exit=" << result.exit_code << '\n';
        if(!result.output.empty()) { ::std::cerr << result.output; }
        if(result.output.empty() || result.output.back() != '\n') { ::std::cerr << '\n'; }
        return false;
    }

    [[nodiscard]] bool build_shared_library(::std::filesystem::path const& source_path,
                                            ::std::filesystem::path const& plugin_path,
                                            ::std::filesystem::path const& capture_path)
    {
#if defined(__APPLE__)
        auto const command{"cc -std=c17 -dynamiclib " + quote_argument(source_path) + " -o " + quote_argument(plugin_path)};
#else
        auto const command{"cc -std=c17 -shared -fPIC " + quote_argument(source_path) + " -o " + quote_argument(plugin_path)};
#endif
        auto const result{run_command_capture(command, capture_path)};
        return expect_success(source_path.filename().string(), result);
    }

    [[nodiscard]] bool build_dl_plugin(::std::filesystem::path const& repo_root,
                                       ::std::filesystem::path const& plugin_path,
                                       ::std::filesystem::path const& capture_path)
    {
        return build_shared_library(repo_root / "examples/0002.dl/regdl.c", plugin_path, capture_path);
    }

}  // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
    ::std::cout << "[skip] wasip1_module_runtime requires POSIX-style shared library loading\n";
    return 0;
#else
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr) [[unlikely]]
    {
        ::std::cerr << "missing argv[0]\n";
        return 1;
    }

    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const executable_dir{executable.parent_path()};
    auto const repo_root{find_repo_root(executable_dir)};
    auto const uwvm_path{find_uwvm_binary(executable_dir)};

    if(repo_root.empty()) [[unlikely]]
    {
        ::std::cerr << "failed to locate repo root from " << executable << '\n';
        return 1;
    }
    if(uwvm_path.empty()) [[unlikely]]
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    auto const artifact_dir{executable_dir / "test-artifacts" / "0015.wasip1"};
    auto const main_wasm_path{artifact_dir / "main_entry.wasm"};
    auto const preload_lib_path{artifact_dir / "preload_lib.wasm"};
    auto const preload_main_path{artifact_dir / "preload_main.wasm"};
    auto const dl_main_path{artifact_dir / "dl_main.wasm"};
    auto const dl_group_main_path{artifact_dir / "dl_group_shared_fd.wasm"};
    auto const dl_close_source_path{artifact_dir / "dl_close_stdout.c"};
    auto const dl_probe_source_path{artifact_dir / "dl_probe_stdout_closed.c"};
# if defined(__APPLE__)
    auto const dl_plugin_path{artifact_dir / "libregdl_test.dylib"};
    auto const dl_close_plugin_path{artifact_dir / "libdl_close_stdout_test.dylib"};
    auto const dl_probe_plugin_path{artifact_dir / "libdl_probe_stdout_closed_test.dylib"};
# else
    auto const dl_plugin_path{artifact_dir / "libregdl_test.so"};
    auto const dl_close_plugin_path{artifact_dir / "libdl_close_stdout_test.so"};
    auto const dl_probe_plugin_path{artifact_dir / "libdl_probe_stdout_closed_test.so"};
# endif

    if(!write_fixture(main_wasm_path, main_entry_wasm) || !write_fixture(preload_lib_path, preload_lib_wasm) ||
       !write_fixture(preload_main_path, preload_main_wasm) || !write_fixture(dl_main_path, dl_main_wasm) ||
       !write_fixture(dl_group_main_path, dl_group_shared_fd_wasm) || !write_text_fixture(dl_close_source_path, dl_close_stdout_plugin_source) ||
       !write_text_fixture(dl_probe_source_path, dl_probe_stdout_closed_plugin_source)) [[unlikely]]
    {
        return 1;
    }

    if(!build_dl_plugin(repo_root, dl_plugin_path, artifact_dir / "build_dl_plugin.log")) [[unlikely]] { return 1; }
    if(!build_shared_library(dl_close_source_path, dl_close_plugin_path, artifact_dir / "build_dl_close_plugin.log")) [[unlikely]] { return 1; }
    if(!build_shared_library(dl_probe_source_path, dl_probe_plugin_path, artifact_dir / "build_dl_probe_plugin.log")) [[unlikely]] { return 1; }

    auto const uwvm{quote_argument(uwvm_path)};
    bool ok{true};

    ok = expect_success("main.enable_overrides_global_disable",
                        run_command_capture(uwvm + " -Rcc int -Rcm full --wasip1-disable --wasip1-module main enable --run " +
                                                quote_argument(main_wasm_path),
                                            artifact_dir / "main_enable.log")) &&
         ok;

    ok = expect_failure("main.disable_blocks_import",
                        run_command_capture(uwvm + " -Rcc int -Rcm full --wasip1-module main disable --run " + quote_argument(main_wasm_path),
                                            artifact_dir / "main_disable.log"),
                        "module-specific WASI Preview 1 setting disables import \"wasi_snapshot_preview1\"") &&
         ok;

    ok = expect_success(
             "preload.enable_and_argv0_override",
             run_command_capture(uwvm +
                                     " -Rcc int -Rcm full --wasip1-disable --wasip1-set-argv0 global-entry "
                                     "--wasip1-module preload-wasm lib.test enable "
                                     "--wasip1-module preload-wasm lib.test set-argv0 pre "
                                     "--wasm-preload-library " +
                                     quote_argument(preload_lib_path) + " lib.test --run " + quote_argument(preload_main_path) + " hello world",
                                 artifact_dir / "preload_enable_and_argv0.log")) &&
         ok;

    ok = expect_failure("preload.missing_enable_fails_dependency_check",
                        run_command_capture(uwvm + " -Rcc int -Rcm full --wasip1-disable --wasm-preload-library " +
                                                quote_argument(preload_lib_path) + " lib.test --run " + quote_argument(preload_main_path) +
                                                " hello world",
                                            artifact_dir / "preload_missing_enable.log"),
                        "Missing module dependency: \"wasi_snapshot_preview1\" required by \"lib.test\".") &&
         ok;

    ok = expect_failure(
             "preload.global_argv0_fallback_changes_module_env",
             run_command_capture(uwvm +
                                     " -Rcc int -Rcm full --wasip1-disable --wasip1-set-argv0 global-entry "
                                     "--wasip1-module preload-wasm lib.test enable "
                                     "--wasm-preload-library " +
                                     quote_argument(preload_lib_path) + " lib.test --run " + quote_argument(preload_main_path) + " hello world",
                                 artifact_dir / "preload_global_argv0_fallback.log"),
             "Runtime crash (catch unreachable)") &&
         ok;

    ok = expect_success(
             "dl.module_expose_and_argv0_override",
             run_command_capture(uwvm +
                                     " -Rcc int -Rcm full --wasip1-module dl dl.example expose-host-api "
                                     "--wasip1-set-argv0 global-entry --wasip1-module dl dl.example set-argv0 dl "
                                     "--wasm-register-dl " +
                                     quote_argument(dl_plugin_path) +
                                     " dl.example --wasm-set-preload-module-attribute dl.example copy all --run " +
                                     quote_argument(dl_main_path) + " hello world",
                                 artifact_dir / "dl_module_expose_and_argv0.log"),
             "example-dl-c: wasi argc=3 argv_buf_size=15") &&
         ok;

    ok = expect_failure("dl.module_hide_overrides_global_expose",
                        run_command_capture(uwvm +
                                                " -Rcc int -Rcm full --wasip1-expose-host-api "
                                                "--wasip1-module dl dl.example hide-host-api "
                                                "--wasm-register-dl " +
                                                quote_argument(dl_plugin_path) +
                                                " dl.example --wasm-set-preload-module-attribute dl.example copy all --run " +
                                                quote_argument(dl_main_path) + " hello world",
                                            artifact_dir / "dl_module_hide.log"),
                        "probe_host_apis: wasip1 api missing") &&
         ok;

    ok = expect_success("dl.group_shares_fd_table",
                        run_command_capture(uwvm +
                                                " -Rcc int -Rcm full "
                                                "--wasip1-module-group dl.alpha shared-fd expose-host-api "
                                                "--wasip1-module-group dl.beta shared-fd expose-host-api "
                                                "--wasm-register-dl " +
                                                quote_argument(dl_close_plugin_path) + " dl.alpha --wasm-register-dl " +
                                                quote_argument(dl_probe_plugin_path) + " dl.beta --run " + quote_argument(dl_group_main_path),
                                            artifact_dir / "dl_group_shared_fd.log")) &&
         ok;

    ok = expect_failure("dl.group_conflicts_with_anonymous_module_group",
                        run_command_capture(uwvm +
                                                " -Rcc int -Rcm full "
                                                "--wasip1-module dl dl.alpha expose-host-api "
                                                "--wasip1-module-group dl.alpha shared-fd expose-host-api "
                                                "--wasm-register-dl " +
                                                quote_argument(dl_close_plugin_path) + " dl.alpha --run " + quote_argument(main_wasm_path),
                                            artifact_dir / "dl_group_conflict.log"),
                        "bound to more than one WASI Preview 1 group") &&
         ok;

    return ok ? 0 : 1;
#endif
}
