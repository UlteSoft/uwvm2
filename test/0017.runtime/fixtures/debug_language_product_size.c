/* Compiler-produced live values are the independent expression oracle. */
#ifdef __cplusplus
typedef bool ProbeBool;
#define TYPE(expr,T) static_assert(__is_same(decltype(expr),T),"target type")
#else
typedef _Bool ProbeBool;
#define TYPE(expr,T) _Static_assert(__builtin_types_compatible_p(__typeof__(expr),T),"target type")
#endif
TYPE(sizeof(int),unsigned long);
TYPE(sizeof(sizeof(int)),unsigned long);
TYPE(sizeof(int)+(long)1,unsigned long);
static volatile unsigned long long observed;
struct Packet { int value; char payload[24]; };
__attribute__((noinline,export_name("language_product_size"))) void language_product_size(void)
{
    volatile signed char small=-1;
    volatile unsigned char byte=255;
    volatile int value=7;
    volatile long wide=1;
    volatile long long wider=1;
    volatile ProbeBool flag=1;
    volatile struct Packet packet={7,{0}};
    volatile unsigned long long oracle0=value+2;
    volatile unsigned long long oracle1=sizeof(value);
    volatile unsigned long long oracle2=sizeof(packet);
    volatile unsigned long long oracle3=sizeof(sizeof(value));
    volatile unsigned long long oracle4=sizeof(sizeof(value)+wide);
    volatile unsigned long long oracle5=sizeof(sizeof(value)+wider);
    volatile unsigned long long oracle6=sizeof(1?sizeof(value):wide);
    volatile unsigned long long oracle7=sizeof(1?small:small);
    volatile unsigned long long oracle8=sizeof(1&&value);
    volatile unsigned long long oracle9=sizeof('a');
    volatile unsigned long long oracle10=sizeof(flag);
    volatile unsigned long long oracle11=sizeof(+small);
    volatile unsigned long long oracle12=(unsigned long)1+sizeof(value);
    volatile unsigned long long oracle13=1?sizeof(value):wide;
    volatile unsigned long long oracle14=small+byte;
    volatile unsigned long long oracle15=sizeof(0?sizeof(packet):sizeof(value));
    volatile unsigned long long oracle16=value==7&&flag;
    volatile unsigned long long oracle17=value<<1;
    observed=value; /* LANGUAGE_PRODUCT_READY */
    if(value!=7 || packet.value!=7 || !flag || wide!=1 || wider!=1 || small!=-1 || byte!=255)
        __builtin_trap();
    observed=oracle0+oracle1+oracle2+oracle3+oracle4+oracle5+oracle6+oracle7+oracle8+
        oracle9+oracle10+oracle11+oracle12+oracle13+oracle14+oracle15+oracle16+oracle17;
}
__attribute__((export_name("_start"))) void _start(void) { language_product_size(); }
