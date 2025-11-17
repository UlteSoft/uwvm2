#include <cstddef>
#include <cstdint>

using capi_wasm_function = void (*)(unsigned char*, unsigned char*);
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

// C-ABI bridge for weak symbol registration
struct uwvm_weak_symbol_module_c
{
    char const* module_name_ptr;
    std::size_t module_name_length;
    capi_custom_handler_vec_t custom_handler_vec;
    capi_function_vec_t function_vec;
};

struct uwvm_weak_symbol_module_vector_c
{
    uwvm_weak_symbol_module_c const* module_ptr;
    std::size_t module_count;
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

static char const module_name_str[] = "weak.example";
static char const func_add_i32_name[] = "add_i32";
static char const func_do_nothing_name[] = "do_nothing";
static char const custom_name_demo[] = "demo_custom";

static capi_custom_handler_t const handlers[] = {
    {custom_name_demo, (std::size_t)(sizeof(custom_name_demo) - 1u), &demo_custom_handle},
};

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

extern "C" uwvm_weak_symbol_module_vector_c uwvm_weak_symbol(void)
{
    static uwvm_weak_symbol_module_c const mods[] = {
        {module_name_str,
         (std::size_t)(sizeof(module_name_str) - 1u),
         capi_custom_handler_vec_t{handlers, (std::size_t)(sizeof(handlers) / sizeof(handlers[0]))},
         capi_function_vec_t{functions, (std::size_t)(sizeof(functions) / sizeof(functions[0]))}},
    };

    uwvm_weak_symbol_module_vector_c v;
    v.module_ptr = mods;
    v.module_count = (std::size_t)(sizeof(mods) / sizeof(mods[0]));
    return v;
}
