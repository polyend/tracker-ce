/* Portable C implementations of Teensy dspinst helpers for PLATFORM_HOST. */
#ifndef dspinst_host_h_
#define dspinst_host_h_

#include <stdint.h>

static inline int32_t signed_saturate_rshift(int32_t val, int bits, int rshift)
  __attribute__((always_inline, unused));
static inline int32_t signed_saturate_rshift(int32_t val, int bits, int rshift)
{
    int32_t out = val >> rshift;
    const int32_t max = 1 << (bits - 1);
    if (out > max - 1)
        out = max - 1;
    if (out < -max)
        out = -max;
    return out;
}

static inline int16_t saturate16(int32_t val) __attribute__((always_inline, unused));
static inline int16_t saturate16(int32_t val)
{
    if (val > 32767)
        return 32767;
    if (val < -32768)
        return -32768;
    return (int16_t)val;
}

static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b)
{
    return (int32_t)(((int64_t)a * (int16_t)(b & 0xFFFF)) >> 16);
}

static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b)
{
    return (int32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16);
}

static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * (int64_t)b) >> 32);
}

static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b)
{
    return (int32_t)((((int64_t)a * (int64_t)b) + 0x80000000LL) >> 32);
}

static inline int32_t multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
{
    return sum + multiply_32x32_rshift32_rounded(a, b);
}

static inline int32_t multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b)
{
    return sum - multiply_32x32_rshift32_rounded(a, b);
}

static inline uint32_t pack_16t_16t(int32_t a, int32_t b) __attribute__((always_inline, unused));
static inline uint32_t pack_16t_16t(int32_t a, int32_t b)
{
    return ((uint32_t)a & 0xFFFF0000u) | ((uint32_t)b >> 16);
}

static inline uint32_t pack_16t_16b(int32_t a, int32_t b) __attribute__((always_inline, unused));
static inline uint32_t pack_16t_16b(int32_t a, int32_t b)
{
    return ((uint32_t)a & 0xFFFF0000u) | ((uint32_t)b & 0x0000FFFFu);
}

static inline uint32_t pack_16b_16b(int32_t a, int32_t b) __attribute__((always_inline, unused));
static inline uint32_t pack_16b_16b(int32_t a, int32_t b)
{
    return ((uint32_t)a << 16) | ((uint32_t)b & 0x0000FFFFu);
}

static inline int16_t sat16_add(int32_t x)
{
    if (x > 32767)
        return 32767;
    if (x < -32768)
        return -32768;
    return (int16_t)x;
}

static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b)
{
    int16_t ah = (int16_t)(a >> 16);
    int16_t al = (int16_t)(a & 0xFFFF);
    int16_t bh = (int16_t)(b >> 16);
    int16_t bl = (int16_t)(b & 0xFFFF);
    return ((uint32_t)(uint16_t)sat16_add((int32_t)ah + bh) << 16) |
           (uint16_t)sat16_add((int32_t)al + bl);
}

static inline int32_t signed_subtract_16_and_16(int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_subtract_16_and_16(int32_t a, int32_t b)
{
    int16_t ah = (int16_t)((uint32_t)a >> 16);
    int16_t al = (int16_t)(a & 0xFFFF);
    int16_t bh = (int16_t)((uint32_t)b >> 16);
    int16_t bl = (int16_t)(b & 0xFFFF);
    return ((int32_t)(uint16_t)sat16_add((int32_t)ah - bh) << 16) |
           (uint16_t)sat16_add((int32_t)al - bl);
}

static inline int32_t signed_halving_add_16_and_16(int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_halving_add_16_and_16(int32_t a, int32_t b)
{
    int16_t ah = (int16_t)((uint32_t)a >> 16);
    int16_t al = (int16_t)(a & 0xFFFF);
    int16_t bh = (int16_t)((uint32_t)b >> 16);
    int16_t bl = (int16_t)(b & 0xFFFF);
    return ((int32_t)(uint16_t)(((int32_t)ah + bh) >> 1) << 16) |
           (uint16_t)(((int32_t)al + bl) >> 1);
}

static inline int32_t signed_halving_subtract_16_and_16(int32_t a, int32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_halving_subtract_16_and_16(int32_t a, int32_t b)
{
    int16_t ah = (int16_t)((uint32_t)a >> 16);
    int16_t al = (int16_t)(a & 0xFFFF);
    int16_t bh = (int16_t)((uint32_t)b >> 16);
    int16_t bl = (int16_t)(b & 0xFFFF);
    return ((int32_t)(uint16_t)(((int32_t)ah - bh) >> 1) << 16) |
           (uint16_t)(((int32_t)al - bl) >> 1);
}

static inline int32_t signed_multiply_accumulate_32x16b(int32_t sum, int32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_multiply_accumulate_32x16b(int32_t sum, int32_t a, uint32_t b)
{
    return sum + signed_multiply_32x16b(a, b);
}

static inline int32_t signed_multiply_accumulate_32x16t(int32_t sum, int32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t signed_multiply_accumulate_32x16t(int32_t sum, int32_t a, uint32_t b)
{
    return sum + signed_multiply_32x16t(a, b);
}

static inline uint32_t logical_and(uint32_t a, uint32_t b) __attribute__((always_inline, unused));
static inline uint32_t logical_and(uint32_t a, uint32_t b)
{
    return a & b;
}

static inline int32_t multiply_16tx16t_add_16bx16b(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16tx16t_add_16bx16b(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a >> 16)) * (int16_t)(b >> 16) +
           (int32_t)((int16_t)(a & 0xFFFF)) * (int16_t)(b & 0xFFFF);
}

static inline int32_t multiply_16tx16b_add_16bx16t(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16tx16b_add_16bx16t(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a >> 16)) * (int16_t)(b & 0xFFFF) +
           (int32_t)((int16_t)(a & 0xFFFF)) * (int16_t)(b >> 16);
}

static inline int64_t multiply_accumulate_16tx16t_add_16bx16b(int64_t sum, uint32_t a, uint32_t b)
{
    return sum + multiply_16tx16t_add_16bx16b(a, b);
}

static inline int64_t multiply_accumulate_16tx16b_add_16bx16t(int64_t sum, uint32_t a, uint32_t b)
{
    return sum + multiply_16tx16b_add_16bx16t(a, b);
}

static inline int32_t multiply_16bx16b(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16bx16b(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a & 0xFFFF)) * (int16_t)(b & 0xFFFF);
}

static inline int32_t multiply_16bx16t(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16bx16t(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a & 0xFFFF)) * (int16_t)(b >> 16);
}

static inline int32_t multiply_16tx16b(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16tx16b(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a >> 16)) * (int16_t)(b & 0xFFFF);
}

static inline int32_t multiply_16tx16t(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t multiply_16tx16t(uint32_t a, uint32_t b)
{
    return (int32_t)((int16_t)(a >> 16)) * (int16_t)(b >> 16);
}

static inline int32_t substract_32_saturate(uint32_t a, uint32_t b)
  __attribute__((always_inline, unused));
static inline int32_t substract_32_saturate(uint32_t a, uint32_t b)
{
    int64_t diff = (int64_t)(int32_t)a - (int64_t)(int32_t)b;
    if (diff > 2147483647LL)
        return 2147483647;
    if (diff < -2147483648LL)
        return (int32_t)-2147483648LL;
    return (int32_t)diff;
}

static inline uint32_t get_q_psr(void) __attribute__((always_inline, unused));
static inline uint32_t get_q_psr(void)
{
    return 0;
}

static inline void clr_q_psr(void) __attribute__((always_inline, unused));
static inline void clr_q_psr(void) {}

static inline int32_t multiply_32x32_rshift(int32_t x, int32_t y, int z)
{
    return (int32_t)(((int64_t)x * y) >> (31 - z));
}

#endif
