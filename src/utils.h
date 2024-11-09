#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a < _b ? _a : _b;                                                                                             \
    })

#define MAX(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a > _b ? _a : _b;                                                                                             \
    })

#define FREAD_TYPE_LE(T, S)                                                                                            \
    inline T fread_##S(FILE* fp)                                                                                       \
    {                                                                                                                  \
        T result = 0;                                                                                                  \
        fread(&result, sizeof(T), 1, fp);                                                                              \
        return result;                                                                                                 \
    }

#define FREAD_TYPE_BE(T, C, S)                                                                                         \
    inline T fread_##S(FILE* fp)                                                                                       \
    {                                                                                                                  \
        T result = 0;                                                                                                  \
        fread(&result, sizeof(T), 1, fp);                                                                              \
        return C(result);                                                                                              \
    }

FREAD_TYPE_LE(int8_t, i8)
FREAD_TYPE_LE(uint8_t, u8)
FREAD_TYPE_LE(int16_t, i16le)
FREAD_TYPE_LE(uint16_t, u16le)
FREAD_TYPE_LE(int32_t, i32le)
FREAD_TYPE_LE(uint32_t, u32le)
FREAD_TYPE_BE(int16_t, __builtin_bswap16, i16be)
FREAD_TYPE_BE(uint16_t, __builtin_bswap16, u16be)
FREAD_TYPE_BE(int32_t, __builtin_bswap32, i32be)
FREAD_TYPE_BE(uint32_t, __builtin_bswap32, u32be)
