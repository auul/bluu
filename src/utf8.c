#include "utf8.h"
#include "ucd.h"
#include "utf.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define UTF8_ACCEPT 0x00
#define UTF8_CONT   0x01
#define UTF8_MULT   0x02
#define UTF8_OL2    0x04
#define UTF8_OL3    0x08
#define UTF8_OL4    0x10
#define UTF8_SUR    0x20
#define UTF8_RANGE  0x40
#define UTF8_INV    0x80

static const uint8_t utf8_table0[16] = {
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_ACCEPT,
    UTF8_CONT,
    UTF8_CONT,
    UTF8_CONT,
    UTF8_CONT,
    UTF8_MULT | UTF8_OL2,
    UTF8_MULT,
    UTF8_MULT | UTF8_OL3 | UTF8_SUR,
    UTF8_MULT | UTF8_OL4 | UTF8_RANGE | UTF8_INV,
};

static const uint8_t utf8_table1[16] = {
    UTF8_CONT | UTF8_MULT | UTF8_OL2 | UTF8_OL3 | UTF8_OL4,
    UTF8_CONT | UTF8_MULT | UTF8_OL2,
    UTF8_CONT | UTF8_MULT,
    UTF8_CONT | UTF8_MULT,
    UTF8_CONT | UTF8_MULT | UTF8_RANGE,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_SUR | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
};

static const uint8_t utf8_table2[16] = {
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_OL2 | UTF8_OL3 | UTF8_OL4 | UTF8_INV,
    UTF8_CONT | UTF8_OL2 | UTF8_OL3 | UTF8_RANGE | UTF8_INV,
    UTF8_CONT | UTF8_OL2 | UTF8_OL3 | UTF8_RANGE | UTF8_INV,
    UTF8_CONT | UTF8_OL2 | UTF8_OL3 | UTF8_RANGE | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
    UTF8_CONT | UTF8_MULT | UTF8_INV,
};

static const uint8_t utf8_state_table[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0x01, 0x01, 0x02, 0x03,
};

static inline uint8_t validate_byte(uint8_t *state, uint8_t c, uint8_t next)
{
    uint8_t n0 = c >> 4;
    uint8_t n1 = c & 0x0f;
    uint8_t n2 = next >> 4;
    uint8_t value = utf8_table0[n0] & utf8_table1[n1] & utf8_table2[n2];
    value ^= (*state != 0);
    *state += utf8_state_table[n0];
    return value;
}

static inline int convert_status(uint8_t status)
{
    if (status & UTF8_INV) {
        return BLU_E_UTF8_INVALID_B;
    } else if (status & UTF8_RANGE) {
        return BLU_E_UTF8_TOO_LARGE;
    } else if (status & UTF8_SUR) {
        return BLU_E_UTF8_SURROGATE;
    } else if (status & UTF8_OL4) {
        return BLU_E_UTF8_OVERLONG4;
    } else if (status & UTF8_OL3) {
        return BLU_E_UTF8_OVERLONG3;
    } else if (status & UTF8_OL2) {
        return BLU_E_UTF8_OVERLONG2;
    } else if (status & UTF8_CONT) {
        return BLU_E_UTF8_BARE_CONT;
    }
    return BLU_E_UTF8_TOO_SHORT;
}

int utf8_validate_n(const char *src, size_t *n_p)
{
    size_t n = *n_p;
    if (n == 0) {
        return 0;
    }

    uint8_t state = 0;
    uint8_t retval;

    size_t i;
    for (i = 0; i < n - 1; i++) {
        retval = validate_byte(&state, src[i], src[i + 1]);
        if (retval) {
            *n_p = i;
            return retval;
        }
    }

    retval = validate_byte(&state, src[i], 0x00);
    if (retval) {
        *n_p = i;
        return retval;
    } else if (state) {
        *n_p = i;
        return BLU_SUCCESS;
    }
    return 0;
}

/* Decoding Prevalidated UTF8 */

#define UTF8_XMASK 0x3f
#define UTF8_2MASK 0x1f
#define UTF8_3MASK 0x0f
#define UTF8_4MASK 0x07
#define UTF8_SHIFT 6

static const uint8_t utf8_bytes_table[16] = {1, 1, 1, 1, 1, 1, 1, 1,
                                             0, 0, 0, 0, 2, 2, 3, 4};

static inline unsigned utf8_bytes_c(uint8_t c)
{
    return utf8_bytes_table[c >> 4];
}

uint32_t utf8_decode(uint32_t *state, uint32_t byte)
{
    uint32_t retval = 0;
    uint32_t cont = *state >> 21;
    uint32_t cp = *state & 0x10fffful;
    switch (cont) {
    case 0:
        switch (utf8_bytes_c(byte)) {
        case 1:
            retval = byte;
            cont = 0;
            cp = 0;
            break;
        case 2:
            cont = 1;
            cp = (cp << 6) | (byte & 0x1f);
            break;
        case 3:
            cont = 2;
            cp = (cp << 6) | (byte & 0xf);
            break;
        case 4:
            cont = 3;
            cp = (cp << 6) | (byte & 0x7);
            break;
        default:
            assert(1 <= utf8_bytes_c(byte) && utf8_bytes_c(byte) <= 4);
            return 0;
        }
        break;
    case 1:
        retval = (cp << 6) | (byte & 0x3f);
        cont = 0;
        cp = 0;
        break;
    case 2:
        cont = 1;
        cp = (cp << 6) | (byte & 0x3f);
        break;
    case 3:
        cont = 2;
        cp = (cp << 6) | (byte & 0x3f);
        break;
    default:
        assert(cont < 4);
        return 0;
    }

    *state = (cont << 21) | cp;
    return retval;
}

bool utf8_gcb(uint32_t *state, uint32_t byte)
{
    uint32_t cp_state = *state & 0xffffff;
    uint32_t gcb_state = *state >> 24;
    uint32_t cp = utf8_decode(&cp_state, byte);
    bool retval = cp ? utf_grapheme_break(&gcb_state, cp) : false;
    if (retval) {
        cp_state |= cp;
    }
    *state = (gcb_state << 24) | cp_state;
    return retval;
}
