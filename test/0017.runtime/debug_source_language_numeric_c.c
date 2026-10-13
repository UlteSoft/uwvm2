/* Independent C17/C23 same-type conditional native type/value oracle. */
int main(void) {
unsigned checks=0;
_Static_assert(_Generic((1 ? (_Bool)0 : (_Bool)0),int:1,default:0),"C same-type narrow conditional promotes to int");
for(int x=-4;x<=4;++x) for(int y=-4;y<=4;++y) for(int yes=0;yes<=1;++yes){_Bool a=(_Bool)x,b=(_Bool)y;int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (signed char)0 : (signed char)0),int:1,default:0),"C same-type narrow conditional promotes to int");
for(int x=-4;x<=4;++x) for(int y=-4;y<=4;++y) for(int yes=0;yes<=1;++yes){signed char a=(signed char)x,b=(signed char)y;int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned char)0 : (unsigned char)0),int:1,default:0),"C same-type narrow conditional promotes to int");
for(int x=-4;x<=4;++x) for(int y=-4;y<=4;++y) for(int yes=0;yes<=1;++yes){unsigned char a=(unsigned char)x,b=(unsigned char)y;int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (short)0 : (short)0),int:1,default:0),"C same-type narrow conditional promotes to int");
for(int x=-4;x<=4;++x) for(int y=-4;y<=4;++y) for(int yes=0;yes<=1;++yes){short a=(short)x,b=(short)y;int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned short)0 : (unsigned short)0),int:1,default:0),"C same-type narrow conditional promotes to int");
for(int x=-4;x<=4;++x) for(int y=-4;y<=4;++y) for(int yes=0;yes<=1;++yes){unsigned short a=(unsigned short)x,b=(unsigned short)y;int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
return checks==810 ? 0 : 2;
}
