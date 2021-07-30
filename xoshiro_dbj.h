#pragma once
/*
C port by DBJ
*/
#include <assert.h>

// The "xoshiro256** 1.0" generator.
// C++ port by Arthur O'Dwyer (2021).
// Based on the C version by David Blackman and Sebastiano Vigna (2018),
// https://prng.di.unimi.it/xoshiro256starstar.c

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    static_assert(sizeof(long long) == 8, "64-bit machines only");

    typedef unsigned long long xoshiro_dbj_u64;

    static struct xoshiro_dbj_
    {
        bool initiated_;
        xoshiro_dbj_u64 s[4];
    } xoshiro_dbj_instance_ = {false, {0, 0, 0, 0}};

    static inline xoshiro_dbj_u64 xoshiro_dbj_splitmix64(xoshiro_dbj_u64 x)
    {
        xoshiro_dbj_u64 z = (x += 0x9e3779b97f4a7c15uLL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9uLL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebuLL;
        return z ^ (z >> 31);
    }

    static inline void xoshiro_dbj_initor(xoshiro_dbj_u64 seed)
    {
        xoshiro_dbj_instance_.s[0] = xoshiro_dbj_splitmix64(seed);
        xoshiro_dbj_instance_.s[1] = xoshiro_dbj_splitmix64(seed);
        xoshiro_dbj_instance_.s[2] = xoshiro_dbj_splitmix64(seed);
        xoshiro_dbj_instance_.s[3] = xoshiro_dbj_splitmix64(seed);
        xoshiro_dbj_instance_.initiated_ = true;
    }

    static inline xoshiro_dbj_u64 xoshiro_dbj_min() { return 0; }
    static inline xoshiro_dbj_u64 xoshiro_dbj_max() { return xoshiro_dbj_u64(-1); }

    static inline xoshiro_dbj_u64 xoshiro_dbj_rotl(xoshiro_dbj_u64 x, int k)
    {
        return (x << k) | (x >> (64 - k));
    }

    inline xoshiro_dbj_u64 xoshiro_dbj_next()
    {
        assert(xoshiro_dbj_instance_.initiated_);

        xoshiro_dbj_u64 result = xoshiro_dbj_rotl(xoshiro_dbj_instance_.s[1] * 5, 7) * 9;
        xoshiro_dbj_u64 t = xoshiro_dbj_instance_.s[1] << 17;
        xoshiro_dbj_instance_.s[2] ^= xoshiro_dbj_instance_.s[0];
        xoshiro_dbj_instance_.s[3] ^= xoshiro_dbj_instance_.s[1];
        xoshiro_dbj_instance_.s[1] ^= xoshiro_dbj_instance_.s[2];
        xoshiro_dbj_instance_.s[0] ^= xoshiro_dbj_instance_.s[3];
        xoshiro_dbj_instance_.s[2] ^= t;
        xoshiro_dbj_instance_.s[3] = xoshiro_dbj_rotl(xoshiro_dbj_instance_.s[3], 45);
        return result;
    }

#ifndef __cplusplus
    static void xoshiro_dbj_ctor(void) __atribute__((constructor));
#endif
    static void xoshiro_dbj_ctor(void)
    {
        xoshiro_dbj_initor((xoshiro_dbj_u64)3.14159265358979323846);
    }

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus