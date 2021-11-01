//
// Created by LawrenceEsswood on 25/03/2020.
//

#ifndef CHERIOS_MATH_UTILS_H
#define CHERIOS_MATH_UTILS_H

#ifndef __ASSEMBLY__

#include "stddef.h"

static inline int imax(int a, int b) {
    return (a>b ? a : b);
}

static inline int imin(int a, int b) {
    return (a<b ? a : b);
}

static inline size_t umax(size_t a, size_t b) {
    return (a>b ? a : b);
}

static inline size_t umin(size_t a, size_t b) {
    return (a<b ? a : b);
}

static inline int slog2(size_t s) {
    int i=-1;
    while(s) {
        i++;
        s >>= 1;
    }
    return i;
}

static inline int is_power_2(size_t x) {
    return (x & (x-1)) == 0;
}

static inline size_t align_up_to(size_t size, size_t align) {
    size_t mask = align - 1;
    return (size + mask) & ~mask;
}

static inline size_t align_down_to(size_t size, size_t align) {
    return size & ~(align-1);
}

static inline size_t round_up_to_nearest_power_2(size_t v) {
    v--;
    v |= v >> 1L;
    v |= v >> 2L;
    v |= v >> 4L;
    v |= v >> 8L;
    v |= v >> 16L;
    v |= v >> 32L;
    v++;
    return v;
}

#define SLICE_W(Val, Width, LowNdx, Bits) (((Val) << ((Width) - ((LowNdx) + (Bits)))) >> ((Width) - (Bits)))

#define SLICE_64(Val, LowNdx, Bits) SLICE_W(Val, 64, LowNdx, Bits)

#else // __ASSEMBLY__

#define ALIGN_UP_2(X, P)   		(((X) + ((1 << (P)) - 1)) &~ ((1 << (P)) - 1))
#define ALIGN_DOWN_2(X, P)  	((X) &~ ((1 << (P)) - 1))

#endif // __ASSEMBLY__

#endif //CHERIOS_MATH_UTILS_H
