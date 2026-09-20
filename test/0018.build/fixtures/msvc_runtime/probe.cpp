// No Windows SDK headers or installed CRT are needed to generate this COFF
// object. Its .drectve defaultlib and preprocessor-dependent return value expose
// CMake's actual choice; this is not a linked Windows/LLVM runtime test.
#if defined(UWVM_TEST_EXPECT_DEBUG)
# if defined(_DEBUG)
static_assert(UWVM_TEST_EXPECT_DEBUG == 1);
# else
static_assert(UWVM_TEST_EXPECT_DEBUG == 0);
# endif
# if defined(_DLL)
static_assert(UWVM_TEST_EXPECT_DLL == 1);
# else
static_assert(UWVM_TEST_EXPECT_DLL == 0);
# endif
#endif
extern "C" int uwvm_crt_probe()
{
    int flags{};
#if defined(_DEBUG)
    flags |= 1;
#endif
#if defined(_DLL)
    flags |= 2;
#endif
    return flags;
}
