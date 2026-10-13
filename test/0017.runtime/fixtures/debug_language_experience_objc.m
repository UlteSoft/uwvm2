typedef struct objc_selector *SEL;
typedef void *id;
__attribute__((objc_root_class))
@interface LanguageProbe { @public void *isa; int value; }
- (int)add:(int)amount;
@end
static volatile int observed;
@implementation LanguageProbe
- (int)add:(int)amount {
    volatile int cookie = self->value + amount;
    volatile float decimal32 = 1.25f;
    volatile double decimal64 = -2.5;
    observed = (int)decimal32; observed = (int)decimal64;
    observed = cookie; /* OBJC_METHOD_READY */
    return cookie;
}
@end
extern void __wasm_call_ctors(void);
extern void *objc_lookup_class(char const *name);
__attribute__((export_name("_start"))) void _start(void) {
    __wasm_call_ctors();
    struct { void *isa; int value; } object = {objc_lookup_class("LanguageProbe"), 7};
    int result = [(LanguageProbe *)&object add:4];
    if(result != 11) __builtin_trap();
}
/* A guest fixture runtime for the GNU v8 ABI emitted by the real compiler.
   It exercises actual selector dispatch; it is not a Foundation replacement. */
typedef int (*MethodImp)(id, SEL, int);
struct objc_selector { char const *name; char const *types; };
struct Method { char const *name; char const *types; MethodImp imp; };
struct Methods { struct Methods *next; int count; struct Method entries[1]; };
struct GuestClass {
    struct GuestClass *isa, *super; char const *name;
    int version, info, instance_size; void *ivars; struct Methods *methods;
};
struct Symtab { unsigned selector_count; SEL selectors; unsigned short class_count, category_count; void *defs[1]; };
struct Module { int version, size; char const *name; struct Symtab *symbols; };
static struct GuestClass *registered;
static int same_name(char const *a, char const *b) {
    for(unsigned i=0;i<64;++i) { if(a[i]!=b[i]) return 0; if(a[i]==0) return 1; }
    return 0;
}
void __objc_exec_class(struct Module *module) {
    if(module->version!=8 || module->size!=16 || module->symbols->class_count!=1) __builtin_trap();
    registered=module->symbols->defs[0];
    if(registered->instance_size!=8 || !same_name(registered->name,"LanguageProbe")) __builtin_trap();
}
void *objc_lookup_class(char const *name) {
    if(!registered || !same_name(name,registered->name)) __builtin_trap();
    return registered;
}
MethodImp objc_msg_lookup(id object, SEL selector) {
    struct GuestClass *cls=*(struct GuestClass **)object;
    if(cls!=registered || !selector) __builtin_trap();
    for(struct Methods *m=cls->methods;m;m=m->next) {
        if(m->count!=1) __builtin_trap();
        if(same_name(m->entries[0].name,selector->name)) return m->entries[0].imp;
    }
    __builtin_trap();
}
