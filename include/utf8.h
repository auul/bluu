#ifndef BLU_UTF8_H
#define BLU_UTF8_H

#include <stddef.h>
#include <stdint.h>

enum {
    BLU_SUCCESS,
    BLU_E_UTF8_INVALID_B,
    BLU_E_UTF8_TOO_LARGE,
    BLU_E_UTF8_SURROGATE,
    BLU_E_UTF8_OVERLONG4,
    BLU_E_UTF8_OVERLONG3,
    BLU_E_UTF8_OVERLONG2,
    BLU_E_UTF8_BARE_CONT,
    BLU_E_UTF8_TOO_SHORT,
};

int utf8_validate_n(const char *src, size_t *n_p);
uint32_t utf8_decode(uint32_t *state, uint32_t byte);
bool utf8_gcb(uint32_t *state, uint32_t byte);

#endif
