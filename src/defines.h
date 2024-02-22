


typedef unsigned char u8;

typedef char s8;

typedef unsigned short u16;

typedef short s16;

typedef unsigned int u32;

typedef int s32;

typedef long long s64;

typedef unsigned long long u64;

typedef float f32;

typedef double f64;


#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#if defined(__clang__) || defined(__GNUC__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) ( sizeof((arr))/sizeof((arr)[0]) )
#endif

#ifndef SET_NTH_BIT
#define SET_NTH_BIT(bitfield, n_bit, onoff) \
    ( (onoff) ? ( (bitfield) |= ((onoff) << (n_bit)) ) : ( (bitfield) &= ~((onoff) << (n_bit)) ) )
#endif

#ifndef TOGGLE_NTH_BIT
#define TOGGLE_NTH_BIT(bitfield, n_bit) \
    ( (bitfield) ^ (1 << (n_bit)) )
#endif

#ifndef U32_INVALID_ID
#define U32_INVALID_ID 0xDEADBEEF
#endif
