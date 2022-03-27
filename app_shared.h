#include "inttypes.h"
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef s8 b8; // booleans
typedef s16 b16;
typedef s64 b64;
typedef s32 b32;


#define debug_break() do{if(IsDebuggerPresent()) {fflush(stdout); __debugbreak();}}while(0)
// Usually I enable my asserts for non-shipping builds only
#define assert(Expression) do{ if(!(Expression)) { debug_break(); *((s32 volatile*)0) = 1; ExitProcess(1); }}while(0)
#define break_at(Expression) do{ if (!(Expression)) { debug_break(); } }while(0)

#define get_min(a, b) (((a) > (b)) ? (b) : (a))
#define get_max(a, b) (((a) > (b)) ? (a) : (b))
#define array_count(a) ((sizeof(a))/(sizeof(*a)))

// Assumes 0 initialized memory in 2 places
#define allocate_memory(Bytes) VirtualAlloc(0, Bytes, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);


struct v2
{
    f32 x, y;
};

struct Rect
{
    f32 x, y;
    f32 w, h;
};

static b32 v2_in_rect(Rect rect, v2 vec)
{
    return (vec.x >= rect.x && vec.x < rect.x + rect.w &&
            vec.y >= rect.y && vec.y < rect.y + rect.h);
}

static Rect rect_contract(Rect rect, f32 contract)
{
    rect.x += contract;
    rect.y += contract;
    rect.w -= contract*2.f;
    rect.h -= contract*2.f;
    return rect;
}








static s64 time_perf()
{
    LARGE_INTEGER large;
    QueryPerformanceCounter(&large);
    s64 result = large.QuadPart;
    return result;
}

static f32 time_elapsed(s64 recent, s64 old)
{
    LARGE_INTEGER perfomance_freq;
    QueryPerformanceFrequency(&perfomance_freq);
    f32 inv_freq = 1.f / (f32)perfomance_freq.QuadPart;
    
    s64 delta = recent - old;
    f32 result = ((f32)delta * inv_freq);
    return result;
}




struct Bit_Scan_Result
{
    u32 index;
    b32 found;
};

static Bit_Scan_Result find_most_significant_bit(u64 value)
{
    Bit_Scan_Result result = {};
#if _MSC_VER
    unsigned long index;
    result.found = _BitScanReverse64(&index, value);
    result.index = index;
#else
    if (value) {
        result.found = true;
        result.index = 63 - __builtin_clzll(value);
    }
#endif
    return result;
}
static Bit_Scan_Result find_most_significant_bit(s64 value) {
    return find_most_significant_bit((u64)value);
}

static Bit_Scan_Result find_most_significant_bit(u32 value)
{
    Bit_Scan_Result result = {};
#if _MSC_VER
    unsigned long index;
    result.found = _BitScanReverse(&index, value);
    result.index = index;
#else
    if (value) {
        result.found = true;
        result.index = 31 - __builtin_clz(value);
    }
#endif
    return result;
}
static Bit_Scan_Result find_most_significant_bit(s32 value) {
    return find_most_significant_bit((u32)value);
}

static Bit_Scan_Result find_least_significant_bit(u32 value)
{
    Bit_Scan_Result result = {};
#if _MSC_VER
    unsigned long index;
    result.found = _BitScanForward(&index, value);
    result.index = index;
#else
    if (value) {
        result.found = true;
        result.index = __builtin_ctz(value);
    }
#endif
    return result;
}
static Bit_Scan_Result find_least_significant_bit(s32 value) {
    return find_least_significant_bit((u32)value);
};

static Bit_Scan_Result
find_least_significant_bit(u64 value)
{
    Bit_Scan_Result result = {};
#if _MSC_VER
    unsigned long index;
    result.found = _BitScanForward64(&index, value);
    result.index = index;
#else
    if (value) {
        result.found = true;
        result.index = __builtin_ctzll(value);
    }
#endif
    return result;
}
static Bit_Scan_Result find_least_significant_bit(s64 value) {
    return find_least_significant_bit((u64)value);
};
