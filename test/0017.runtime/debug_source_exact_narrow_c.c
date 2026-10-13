/* Independent native C17/C23 expression type oracle. */
#include <stdbool.h>
#if __STDC_VERSION__ >= 202311L
_Static_assert(_Generic(true,bool:1,default:0),"C23 true Boolean type");
_Static_assert(_Generic(false,bool:1,default:0),"C23 false Boolean type");
_Static_assert(sizeof(true)==1,"supported Wasm/native C23 Boolean ABI");
#else
_Static_assert(_Generic(true,int:1,default:0),"C17 stdbool macro remains int");
#endif
_Static_assert(_Generic((true ? true : false),int:1,default:0),"C conditional promotes Boolean operands");
_Static_assert(_Generic((!true),int:1,default:0),"C logical result int");
_Static_assert(_Generic((true==false),int:1,default:0),"C comparison result int");
_Static_assert(_Generic('a',int:1,default:0),"C ordinary character constant int");
int main(void) {return true+true!=2 || (!false)!=1 || (true ? true : false)!=1;}
