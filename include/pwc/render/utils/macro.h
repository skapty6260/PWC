#ifndef _PWC_RENDER_UTILS_MACRO
#define _PWC_RENDER_UTILS_MACRO

#if defined(NDEBUG) && defined(__GNUC__)
#define U_ASSERT_ONLY __attribute__((unused))
#else
#define U_ASSERT_ONLY
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#endif