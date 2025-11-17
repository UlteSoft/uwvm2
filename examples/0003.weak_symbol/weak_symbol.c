#include <stddef.h>
#include <stdint.h>

// Basic C ABI matching UWVM2 loader (same layout as regdl.c)
typedef void (*capi_wasm_function)(unsigned char* res, unsigned char* para);

typedef void (*imported_c_handlefunc_ptr_t)(void);

typedef struct capi_custom_handler_t_def
{
    char const* custom_name_ptr;
    size_t custom_name_length;
    imported_c_handlefunc_ptr_t custom_handle_func;
} capi_custom_handler_t;

typedef struct capi_custom_handler_vec_t_def
{
    capi_custom_handler_t const* custom_handler_begin;
    size_t custom_handler_size;
} capi_custom_handler_vec_t;

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

// Weak symbol C ABI bridge (must match uwvm2::uwvm::run::uwvm_weak_symbol_module_c)
typedef struct uwvm_weak_symbol_module_c_def
{
    char const* module_name_ptr;
    size_t module_name_length;
    capi_custom_handler_vec_t custom_handler_vec;
    capi_function_vec_t function_vec;
} uwvm_weak_symbol_module_c;

typedef struct uwvm_weak_symbol_module_vector_c_def
{
    uwvm_weak_symbol_module_c const* module_ptr;
    size_t module_count;
} uwvm_weak_symbol_module_vector_c;

// WASM value types encodings
enum
{
    WASM_VALTYPE_I32 = 0x7F,
    WASM_VALTYPE_I64 = 0x7E,
    WASM_VALTYPE_F32 = 0x7D,
    WASM_VALTYPE_F64 = 0x7C
};

// Packed param/result for add_i32
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_para_t
{
    int_least32_t a;
    int_least32_t b;
};

struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    add_i32_res_t
{
    int_least32_t sum;
};
#if defined(_MSC_VER) && !defined(__clang__)
# pragma pack(pop)
#endif

static void add_i32_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    const struct add_i32_para_t* para = (const struct add_i32_para_t*)para_bytes;
    struct add_i32_res_t* res = (struct add_i32_res_t*)res_bytes;
    res->sum = para->a + para->b;
}

static void do_nothing_impl(unsigned char* res_bytes, unsigned char* para_bytes)
{
    (void)res_bytes;
    (void)para_bytes;
}

static void demo_custom_handle(void) {}

static uint_least8_t const add_i32_para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
static uint_least8_t const add_i32_res_types[] = {WASM_VALTYPE_I32};

static char const module_name_str[] = "weak.example";
static char const func_add_i32_name[] = "add_i32";
static char const func_do_nothing_name[] = "do_nothing";
static char const custom_name_demo[] = "demo_custom";

// Provide handlers/functions like DL example
static capi_custom_handler_t const handlers[] = {
    {custom_name_demo, (size_t)(sizeof(custom_name_demo) - 1u), &demo_custom_handle},
};

static capi_function_t const functions[] = {
    {func_add_i32_name,
     (size_t)(sizeof(func_add_i32_name) - 1u),
     add_i32_para_types,      
     (size_t)(sizeof(add_i32_para_types) / sizeof(add_i32_para_types[0])),
     add_i32_res_types,                                                                                                      
     (size_t)(sizeof(add_i32_res_types) / sizeof(add_i32_res_types[0])),
     (capi_wasm_function)&add_i32_impl                                                                                                                                                                                               
    },
    {func_do_nothing_name,
     (size_t)(sizeof(func_do_nothing_name) - 1u),
     (uint_least8_t const*)0,
     0u,                                                                                            
     (uint_least8_t const*)0,
     0u,                                                                                                                                                                                         
     (capi_wasm_function)&do_nothing_impl},
};

// Strong definition to override weak fallback in header
uwvm_weak_symbol_module_vector_c const* uwvm_weak_symbol_module(void)
{
    static uwvm_weak_symbol_module_c const mods[] = {
        {module_name_str,
         (size_t)(sizeof(module_name_str) - 1u),
         (capi_custom_handler_vec_t){handlers, (size_t)(sizeof(handlers) / sizeof(handlers[0]))},
         (capi_function_vec_t){functions, (size_t)(sizeof(functions) / sizeof(functions[0]))}},
    };
    static uwvm_weak_symbol_module_vector_c const vec = {
        mods,
        (size_t)(sizeof(mods) / sizeof(mods[0]))
    };
    return &vec;
}

// Currently, weak symbols only support the ELF file format.

// clang -c weak_symbol.c --sysroot="$SYSROOT" # or: g++ -c weak_symbol.c
// xmake f ... --ldflags="examples/0003.weak_symbol/weak_symbol.o"

// IMPORTANT:
// Do NOT archive the weak fallback object into a static library via 
//     ar rcs libweak_symbol.a weak_symbol.o
//
// Reason:
//   ELF static libraries use "lazy extraction" semantics. When a static
//   library is passed to the linker, individual .o members are only pulled
//   in on-demand when a symbol is first referenced. If the weak definition
//   of uwvm_weak_symbol_module() resides inside a .a archive, and the linker
//   resolves that symbol from the archive before seeing a strong override
//   definition, the weak .o will be extracted and permanently selected.
//   The linker will NOT backtrack and replace it later when the strong
//   definition appears. As a result, the strong override is effectively
//   ignored.
//
// Recommended usage:
//   • Link the weak fallback implementation directly as an object file:
//         ... weak_symbol.o ...
//     This guarantees that strong symbols always override weak ones.
//
// If a static library must be used:
//   • Maintain strict ordering in the final link command.
//   • The strong definition (.o) must appear BEFORE the library containing
//     the weak one (.a).
//   • Example:
//         clang++ main.o strong_impl.o -lweak_symbol
//     (Strong override first, weak library last.)
//
// Summary:
//   Avoid placing weak definitions inside .a archives unless you have full
//   control of symbol resolution order. Prefer linking .o directly or place
//   strong definitions before the weak library to ensure correct override
//   semantics.
