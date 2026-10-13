/* C17/C23 native type oracle; no IO and no guest runtime. */
int main(void) {
unsigned checks=0;
_Static_assert(_Generic((1 ? (_Bool)0 : (signed char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { _Bool a=(_Bool)x; signed char b=(signed char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (_Bool)0 : (unsigned char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { _Bool a=(_Bool)x; unsigned char b=(unsigned char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (_Bool)0 : (short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { _Bool a=(_Bool)x; short b=(short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (_Bool)0 : (unsigned short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { _Bool a=(_Bool)x; unsigned short b=(unsigned short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (signed char)0 : (_Bool)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { signed char a=(signed char)x; _Bool b=(_Bool)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (signed char)0 : (unsigned char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { signed char a=(signed char)x; unsigned char b=(unsigned char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (signed char)0 : (short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { signed char a=(signed char)x; short b=(short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (signed char)0 : (unsigned short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { signed char a=(signed char)x; unsigned short b=(unsigned short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned char)0 : (_Bool)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned char a=(unsigned char)x; _Bool b=(_Bool)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned char)0 : (signed char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned char a=(unsigned char)x; signed char b=(signed char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned char)0 : (short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned char a=(unsigned char)x; short b=(short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned char)0 : (unsigned short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned char a=(unsigned char)x; unsigned short b=(unsigned short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (short)0 : (_Bool)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { short a=(short)x; _Bool b=(_Bool)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (short)0 : (signed char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { short a=(short)x; signed char b=(signed char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (short)0 : (unsigned char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { short a=(short)x; unsigned char b=(unsigned char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (short)0 : (unsigned short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { short a=(short)x; unsigned short b=(unsigned short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned short)0 : (_Bool)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned short a=(unsigned short)x; _Bool b=(_Bool)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned short)0 : (signed char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned short a=(unsigned short)x; signed char b=(signed char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned short)0 : (unsigned char)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned short a=(unsigned short)x; unsigned char b=(unsigned char)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
_Static_assert(_Generic((1 ? (unsigned short)0 : (short)0), int:1,default:0), "distinct narrow common type is int");
for (int x=-1;x<=1;++x) for (int y=-1;y<=1;++y) for(int yes=0;yes<=1;++yes) { unsigned short a=(unsigned short)x; short b=(short)y; int expected=yes?(int)a:(int)b;if((yes?a:b)!=expected)return 1;++checks;}
return checks==360 ? 0 : 2;
}
