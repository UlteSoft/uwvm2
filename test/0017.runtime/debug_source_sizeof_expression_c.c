/* Native C17/C23 compiler witnesses; no IO or runtime operand evaluation. */
volatile int guarded_value;
#define WITNESS(expr,extent) _Static_assert(sizeof(expr)==(extent),"native unevaluated sizeof witness")
WITNESS((short)guarded_value,sizeof(short));
WITNESS(+(short)guarded_value,sizeof(int));
WITNESS((unsigned char)guarded_value,sizeof(unsigned char));
WITNESS((_Bool)guarded_value,sizeof(_Bool));
WITNESS(guarded_value+guarded_value,sizeof(int));
WITNESS(guarded_value/0,sizeof(int));
WITNESS(guarded_value<<-1,sizeof(int));
WITNESS((long)guarded_value,sizeof(long));
WITNESS((float)guarded_value+(double)guarded_value,sizeof(double));
WITNESS(1 ? (short)guarded_value : (short)guarded_value,sizeof(int));
WITNESS(1 ? (short)guarded_value : (long long)guarded_value,sizeof(long long));
WITNESS(sizeof(guarded_value)+1,sizeof(__SIZE_TYPE__));
WITNESS('('+')',sizeof(int));
WITNESS(guarded_value<1,sizeof(int));
int main(void)
{ return sizeof(guarded_value/0)!=sizeof(int) || sizeof(guarded_value<<-1)!=sizeof(int); }
