#include <cstddef>
#include <cstdint>

using capi_wasm_function = void (*)(unsigned char*, unsigned char*);

struct capi_module_name_t
{
    char const* name;
    std::size_t name_length;
};

using imported_c_handlefunc_ptr_t = void (*)(void);

struct capi_custom_handler_t
{
    char const* custom_name_ptr;
    std::size_t custom_name_length;
    imported_c_handlefunc_ptr_t custom_handle_func;
};

struct capi_custom_handler_vec_t
{
    capi_custom_handler_t const* custom_handler_begin;
    std::size_t custom_handler_size;
};

struct capi_function_t
{
    char const* func_name_ptr;
    std::size_t func_name_length;
    std::uint_least8_t const* para_type_vec_begin;
    std::size_t para_type_vec_size;
    std::uint_least8_t const* res_type_vec_begin;
    std::size_t res_type_vec_size;
    capi_wasm_function func_ptr;
};

struct capi_function_vec_t
{
    capi_function_t const* function_begin;
    std::size_t function_size;
};

enum : std::uint_least8_t
{
    WASM_VALTYPE_I32 = 0x7F,
    WASM_VALTYPE_I64 = 0x7E,
    WASM_VALTYPE_F32 = 0x7D,
    WASM_VALTYPE_F64 = 0x7C,
};

#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_para_t
{
    std::int_least32_t a;
    std::int_least32_t b;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_res_t
{
    std::int_least32_t sum;
};
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

static void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    auto const* para = reinterpret_cast<add_i32_para_t const*>(para_bytes);
    auto* res = reinterpret_cast<add_i32_res_t*>(res_bytes);
    res->sum = para->a + para->b;
}

static void do_nothing_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    (void)res_bytes;
    (void)para_bytes;
}

static void demo_custom_handle(void) {}

static std::uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static std::uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};

static char const module_name_str[] = "example";
static char const func_add_i32_name[] = "add_i32";
static char const func_do_nothing_name[] = "do_nothing";
static char const custom_name_demo[] = "demo_custom";

extern "C" capi_module_name_t uwvm_get_module_name(void)
{
    capi_module_name_t r;
    r.name = module_name_str;
    r.name_length = (std::size_t)(sizeof(module_name_str) - 1u);
    return r;
}

extern "C" capi_custom_handler_vec_t uwvm_get_custom_handler(void)
{
    static capi_custom_handler_t const handlers[] = {
        {custom_name_demo, (std::size_t)(sizeof(custom_name_demo) - 1u), &demo_custom_handle},
    };
    capi_custom_handler_vec_t v;
    v.custom_handler_begin = handlers;
    v.custom_handler_size = (std::size_t)(sizeof(handlers) / sizeof(handlers[0]));
    return v;
}

extern "C" capi_function_vec_t uwvm_function(void)
{
    static capi_function_t const functions[] = {
        {func_add_i32_name,
         (std::size_t)(sizeof(func_add_i32_name) - 1u),
         add_i32_para_types,           
         (std::size_t)(sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0])),
         add_i32_res_types,                                                                                                                     
         (std::size_t)(sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0])),
         (capi_wasm_function)&add_i32_impl                                                                                                                                                                                                                   
        },
        {func_do_nothing_name,
         (std::size_t)(sizeof(func_do_nothing_name) - 1u),
         (std::uint_least8_t const*)0,
         0u,                                                                                                      
         (std::uint_least8_t const*)0,
         0u,                                                                                                                                                                                                             
         (capi_wasm_function)&do_nothing_impl},
    };
    capi_function_vec_t vec;
    vec.function_begin = functions;
    vec.function_size = (std::size_t)(sizeof(functions) / sizeof(functions[0]));
    return vec;
}
