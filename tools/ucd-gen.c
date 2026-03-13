#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define len_of(array) (sizeof(array) / sizeof(*(array)))

enum {
    UCD_NFKC_QC_YES,
    UCD_NFKC_QC_NO,
    UCD_NFKC_QC_MAYBE,
};

enum {
    UCD_GCB__NIL,
    UCD_GCB_CR,
    UCD_GCB_LF,
    UCD_GCB_CONTROL,
    UCD_GCB_PREPEND,
    UCD_GCB_LVT,
    UCD_GCB_LV,
    UCD_GCB_L,
    UCD_GCB_V,
    UCD_GCB_T,
    UCD_GCB_REGIONAL_INDICATOR,
    UCD_GCB_EXTENDED_PICTOGRAPHIC,
    UCD_GCB_INCB_CONSONANT,
    UCD_GCB_INCB_EXTEND,
    UCD_GCB_INCB_LINKER,
    UCD_GCB_EXTEND,
    UCD_GCB_ZWJ,
    UCD_GCB_SPACING_MARK,
    UCD_GCB__MAX,
};

static inline void print_gcb(unsigned gcb)
{
    switch (gcb) {
    case UCD_GCB__NIL:
        printf("other");
        break;
    case UCD_GCB_REGIONAL_INDICATOR:
        printf("regional_indicator");
        break;
    case UCD_GCB_EXTENDED_PICTOGRAPHIC:
        printf("extended_pictographic");
        break;
    case UCD_GCB_INCB_CONSONANT:
        printf("incb=consonant");
        break;
    case UCD_GCB_SPACING_MARK:
        printf("spacing_mark");
        break;
    case UCD_GCB_INCB_EXTEND:
        printf("incb=extend");
        break;
    case UCD_GCB_INCB_LINKER:
        printf("incb=linker");
        break;
    case UCD_GCB_CONTROL:
        printf("control");
        break;
    case UCD_GCB_PREPEND:
        printf("prepend");
        break;
    case UCD_GCB_EXTEND:
        printf("extend");
        break;
    case UCD_GCB_ZWJ:
        printf("zwj");
        break;
    case UCD_GCB_LVT:
        printf("lvt");
        break;
    case UCD_GCB_CR:
        printf("cr");
        break;
    case UCD_GCB_LF:
        printf("lf");
        break;
    case UCD_GCB_LV:
        printf("lv");
        break;
    case UCD_GCB_L:
        printf("l");
        break;
    case UCD_GCB_V:
        printf("v");
        break;
    case UCD_GCB_T:
        printf("t");
        break;
    default:
        printf("%u\n", gcb);
        assert(UCD_GCB__NIL <= gcb && gcb < UCD_GCB__MAX);
        break;
    }
}

#define MAX_BITS (sizeof(uintmax_t) * CHAR_BIT)

static uintmax_t g_fce[(0x110000 + MAX_BITS - 1) / MAX_BITS];

static inline void set_fce(uint32_t lo, uint32_t hi)
{
    for (uint32_t cp = lo; cp <= hi; cp++) {
        g_fce[cp / MAX_BITS] |= (uintmax_t)1 << (cp % MAX_BITS);
    }
}

static inline bool get_fce(uint32_t cp)
{
    return (g_fce[cp / MAX_BITS] >> (cp % MAX_BITS)) & 1;
}

static uint16_t ucd_info[0x110000 * 3];

static inline uintmax_t bit_max(unsigned bits)
{
    return (UINTMAX_MAX >> (MAX_BITS - bits));
}

static inline unsigned get_nfkc_qc(uint32_t cp)
{
    return ucd_info[cp * 3] & 0x3;
}

static inline void set_nfkc_qc(uint32_t lo, uint32_t hi, unsigned value)
{
    assert(value <= bit_max(2));
    for (uint32_t cp = lo; cp <= hi; cp++) {
        ucd_info[cp * 3] |= value;
    }
}

static inline unsigned get_decomp_idx(uint32_t cp)
{
    return ucd_info[cp * 3] >> 2;
}

static inline void set_decomp_idx(uint32_t cp, uint32_t value)
{
    assert(value <= bit_max(14));
    ucd_info[cp * 3] |= value << 2;
}

static inline unsigned get_recomp_idx(uint32_t cp)
{
    return ucd_info[cp * 3 + 1] >> 5;
}

static inline void set_recomp_idx(uint32_t cp, uint32_t value)
{
    assert(value <= bit_max(11));
    ucd_info[cp * 3 + 1] |= value << 5;
}

static inline unsigned get_gcb(uint32_t cp)
{
    return ucd_info[cp * 3 + 1] & 0x1f;
}

static inline void set_gcb(uint32_t lo, uint32_t hi, unsigned value)
{
    assert(UCD_GCB__NIL < value && value < UCD_GCB__MAX);
    assert(value <= bit_max(5));
    for (uint32_t cp = lo; cp <= hi; cp++) {
        unsigned current = get_gcb(cp);
        switch (value) {
        case UCD_GCB_ZWJ:
        case UCD_GCB_CR:
        case UCD_GCB_LF:
            break;
        case UCD_GCB_INCB_EXTEND:
        case UCD_GCB_INCB_LINKER:
            if (current == UCD_GCB_ZWJ) {
                continue;
            }
            assert(!current || current == UCD_GCB_EXTEND);
            break;
        case UCD_GCB_EXTEND:
            if (current == UCD_GCB_INCB_EXTEND ||
                current == UCD_GCB_INCB_LINKER) {
                continue;
            }
            assert(!current);
            break;
        default:
            assert(!current);
            break;
        }
        ucd_info[cp * 3 + 1] = (ucd_info[cp * 3 + 1] & ~UINT16_C(0x1f)) | value;
    }
}

static inline unsigned get_ccc(uint32_t cp)
{
    return ucd_info[cp * 3 + 2] >> 8;
}

static inline void set_ccc(uint32_t cp, unsigned value)
{
    ucd_info[cp * 3 + 2] |= value << 8;
}

static inline unsigned get_eaw(uint32_t cp)
{
    return (ucd_info[cp * 3 + 2] >> 6) & 0x3;
}

static inline void set_eaw(uint32_t lo, uint32_t hi, unsigned value)
{
    switch (value) {
    case 'A':
        for (uint32_t cp = lo; cp <= hi; cp++) {
            ucd_info[cp * 3 + 2] |= 0x80;
        }
        break;
    case 'F':
    case 'W':
        for (uint32_t cp = lo; cp <= hi; cp++) {
            ucd_info[cp * 3 + 2] |= 0x40;
        }
        break;
    default:
        assert(value == 'A' || value == 'F' || value == 'W');
        break;
    }
}

static inline bool is_default_ignorable_code_point(uint32_t cp)
{
    return ucd_info[cp * 3 + 2] & 0x4;
}

static inline void set_default_ignorable_code_point(uint32_t lo, uint32_t hi)
{
    for (uint32_t cp = lo; cp <= hi; cp++) {
        ucd_info[cp * 3 + 2] |= 0x4;
    }
}

static inline bool is_xid_start(uint32_t cp)
{
    return ucd_info[cp * 3 + 2] & 0x2;
}

static inline void set_xid_start(uint32_t lo, uint32_t hi)
{
    for (uint32_t cp = lo; cp <= hi; cp++) {
        ucd_info[cp * 3 + 2] |= 0x2;
    }
}

static inline bool is_xid_continue(uint32_t cp)
{
    return ucd_info[cp * 3 + 2] & 0x1;
}

static inline void set_xid_continue(uint32_t lo, uint32_t hi)
{
    for (uint32_t cp = lo; cp <= hi; cp++) {
        ucd_info[cp * 3 + 2] |= 0x1;
    }
}

static uint32_t g_decomp_buf[0x4000];
static unsigned g_decomp_at = 1;

static uint32_t g_recomp_map[600][600];
static unsigned g_recomp_count = 1;

static inline const uint32_t *get_decomp(uint32_t cp)
{
    unsigned idx = get_decomp_idx(cp);
    return idx ? g_decomp_buf + idx : NULL;
}

static inline unsigned acquire_recomp_idx(uint32_t cp)
{
    unsigned idx = get_recomp_idx(cp);
    if (!idx) {
        idx = g_recomp_count++;
        set_recomp_idx(cp, idx);
    }
    return idx;
}

static inline void
set_decomp(uint32_t cp, bool compat, const uint32_t *src, unsigned len)
{
    unsigned idx = g_decomp_at;
    g_decomp_at += len + 1;
    assert(g_decomp_at <= len_of(g_decomp_buf));
    memcpy(g_decomp_buf + idx, src, len * sizeof(*src));
    g_decomp_buf[idx + len] = 0;
    set_decomp_idx(cp, idx);

    if (compat || len != 2 || get_fce(cp)) {
        return;
    }

    unsigned lhs = acquire_recomp_idx(src[0]);
    unsigned rhs = acquire_recomp_idx(src[1]);
    g_recomp_map[lhs][rhs] = cp;
}

static char g_file_text[0x400000];

static inline void *error_load_file(FILE *f, const char *path)
{
    fprintf(
        stderr, "ucd-gen: Failed to open file '%s': %s", path, strerror(errno));
    if (f) {
        fclose(f);
    }
    exit(EXIT_FAILURE);
    return NULL;
}

static const char *load_file(const char *path)
{
    long len;
    FILE *f;
    if (!(f = fopen(path, "rb")) || fseek(f, 0, SEEK_END) ||
        (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        return error_load_file(f, path);
    }

    assert(len < len_of(g_file_text));
    if (fread(g_file_text, 1, len, f) != len) {
        return error_load_file(f, path);
    }

    g_file_text[len] = 0;
    fclose(f);

    return g_file_text;
}

/* clang-format off */
static const uint8_t gcb_dfa[13][18] = {
    {0xB,0x1,0xC,0xC,0x2,0x3,0x4,0x5,0x4,0x3,0x6,0x7,0x9,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0xC,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0},
    {0xB,0x0,0x0,0x0,0x2,0x3,0x4,0x5,0x4,0x3,0x6,0x7,0x9,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x4,0x3,0x0,0x0,0x0,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x3,0x4,0x5,0x4,0x0,0x0,0x0,0x0,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xB,0x0,0x0,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7,0x7,0x7,0x8,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7,0x0,0x8,0x8,0x8,0x8,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x9,0xA,0xB,0x9,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x9,0xA,0xA,0xB,0xA,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xB,0xB,0xB,0xB,0xB},
    {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0}
};
/* clang-format on */

const uint32_t *ucd_grapheme_cluster(const uint32_t *src, const uint32_t *end)
{
    if (src == end) {
        return src;
    }

    const uint32_t *prev;
    uint8_t state = 0x0;
    do {
        state = gcb_dfa[state][get_gcb(*src)];
        prev = src;
        src += !!state;
    } while (src != prev && src != end);
    return src;
}

size_t ucd_grapheme_cluster_len(const uint32_t *src, size_t src_len)
{
    return ucd_grapheme_cluster(src, src + src_len) - src;
}

#define SBASE 0xac00
#define LBASE 0x1100
#define VBASE 0x1161
#define TBASE 0x11A7

#define LCOUNT 19
#define VCOUNT 21
#define TCOUNT 28

#define NCOUNT (VCOUNT * TCOUNT)
#define SCOUNT (LCOUNT * NCOUNT)

static inline bool is_l(uint32_t cp)
{
    return LBASE <= cp && cp < LBASE + LCOUNT;
}

static inline bool is_v(uint32_t cp)
{
    return VBASE <= cp && cp < VBASE + VCOUNT;
}

static inline bool is_lv(uint32_t cp)
{
    return SBASE <= cp && cp < SBASE + SCOUNT && (cp - SBASE) % TCOUNT == 0;
}

static inline bool is_t(uint32_t cp)
{
    return TBASE <= cp && cp < TBASE + TCOUNT;
}

static inline uint32_t get_recomp(uint32_t lhs, uint32_t rhs)
{
    if (is_l(lhs) && is_v(rhs)) {
        unsigned l_idx = lhs - LBASE;
        unsigned v_idx = rhs - VBASE;
        return SBASE + (l_idx * NCOUNT) + (v_idx * TCOUNT);
    } else if (is_lv(lhs) && is_t(rhs)) {
        unsigned t_idx = rhs - TBASE;
        return lhs + t_idx;
    }

    unsigned lhs_idx = get_recomp_idx(lhs);
    unsigned rhs_idx = get_recomp_idx(rhs);
    return lhs_idx && rhs_idx ? g_recomp_map[lhs_idx][rhs_idx] : 0;
}

static inline bool is_hangul_composite(uint32_t cp)
{
    return 0xac00 <= cp && cp <= 0xd7a3;
}

static uint32_t *
write_hangul_nfkd(uint32_t *dest, const uint32_t *end, uint32_t cp)
{
    unsigned s_idx = cp - SBASE;
    unsigned l_idx = s_idx / NCOUNT;
    unsigned v_idx = (s_idx % NCOUNT) / TCOUNT;
    unsigned t_idx = s_idx % TCOUNT;

    *dest = LBASE + l_idx;
    dest++;
    if (dest == end) {
        return NULL;
    }

    *dest = VBASE + v_idx;
    dest++;

    if (!t_idx) {
        return dest;
    } else if (dest == end) {
        return NULL;
    }

    *dest = TBASE + t_idx;
    return dest + 1;
}

static uint32_t *write_nfkd(uint32_t *dest, const uint32_t *end, uint32_t cp)
{
    if (!dest || dest == end) {
        return NULL;
    } else if (is_hangul_composite(cp)) {
        return write_hangul_nfkd(dest, end, cp);
    }

    const uint32_t *src = get_decomp(cp);
    if (!src) {
        *dest = cp;
        return dest + 1;
    }

    for (unsigned i = 0; src[i]; i++) {
        dest = write_nfkd(dest, end, src[i]);
        if (!dest) {
            return NULL;
        }
    }
    return dest;
}

static inline void sort_canon(uint32_t *buf, size_t len)
{
    for (size_t i = 1; i < len; i++) {
        while (i > 0 && get_ccc(buf[i - 1]) && get_ccc(buf[i]) &&
               get_ccc(buf[i - 1]) > get_ccc(buf[i])) {
            uint32_t swap = buf[i - 1];
            buf[i - 1] = buf[i];
            buf[i] = swap;
            i--;
        }
    }
}

static inline size_t
norm_nfkd(uint32_t *dest, size_t dest_cap, const uint32_t *src, size_t src_len)
{
    uint32_t *start = dest;
    const uint32_t *end = dest + dest_cap;
    for (size_t i = 0; i < src_len; i++) {
        dest = write_nfkd(dest, end, src[i]);
        if (!dest) {
            return 0;
        }
    }

    size_t len = dest - start;
    sort_canon(start, len);

    return len;
}

static inline size_t
norm_nfkc(uint32_t *dest, size_t dest_cap, const uint32_t *src, size_t src_len)
{
    size_t len = norm_nfkd(dest, dest_cap, src, src_len);
    if (len < 2) {
        return len;
    }

    size_t out = 1;
    size_t starter_pos = 0;
    uint32_t starter = dest[0];
    uint8_t last_ccc = 0;

    for (size_t i = 1; i < len; i++) {
        uint32_t ch = dest[i];
        uint8_t ccc = get_ccc(ch);
        uint32_t composed = get_recomp(starter, ch);
        if (composed && (last_ccc < ccc || last_ccc == 0)) {
            dest[starter_pos] = composed;
            starter = composed;
            continue;
        }

        dest[out++] = ch;
        if (ccc == 0) {
            starter_pos = out - 1;
            starter = ch;
            last_ccc = 0;
        } else {
            last_ccc = ccc;
        }
    }

    return out;
}

#define PARENS ()
#define LPAREN (
#define RPAREN )
#define EAT(...)
#define OPT_SWITCH(...)                                           \
    EVAL1(__VA_OPT__(EAT LPAREN)                                  \
              OPT_SWITCH__ELSE __VA_OPT__(RPAREN OPT_SWITCH__IF))
#define OPT_SWITCH__IF(...)   __VA_ARGS__ EAT
#define OPT_SWITCH__ELSE(...) EVAL1

#define EVAL        EVAL16
#define EVAL1(...)  __VA_ARGS__
#define EVAL2(...)  EVAL1(EVAL1(__VA_ARGS__))
#define EVAL4(...)  EVAL2(EVAL2(__VA_ARGS__))
#define EVAL8(...)  EVAL4(EVAL4(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))

#define parse_commit(step, ...)                                  \
    do {                                                         \
        intptr_t parse_value = true;                             \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        if (!parse_value) {                                      \
            return false;                                        \
        }                                                        \
    } while (0)

#define parse_return(step, ...)                                  \
    do {                                                         \
        intptr_t parse_value = true;                             \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        return parse_value;                                      \
    } while (0)

#define parse_return_ptr(step, ...)                              \
    do {                                                         \
        intptr_t parse_value = true;                             \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        return (void *)parse_value;                              \
    } while (0)

#define parse_try(step, ...)                                     \
    do {                                                         \
        typeof(*src_p) parse_mark = *src_p;                      \
        intptr_t parse_value = true;                             \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        if (parse_value) {                                       \
            return parse_value;                                  \
        }                                                        \
        *src_p = parse_mark;                                     \
    } while (0)

#define parse_try_ptr(step, ...)                                 \
    do {                                                         \
        typeof(*src_p) parse_mark = *src_p;                      \
        intptr_t parse_value = true;                             \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        if (parse_value) {                                       \
            return (void *)parse_value;                          \
        }                                                        \
        *src_p = parse_mark;                                     \
    } while (0)

#define parse_seq(step, ...)                                              \
    (typeof(parse_value))step __VA_OPT__(; PARSE_SEQ__BLOCK(__VA_ARGS__))
#define PARSE_SEQ__BLOCK(...)               \
    do {                                    \
        EVAL(PARSE_SEQ__STEP(__VA_ARGS__)); \
    } while (0)
#define PARSE_SEQ__STEP(step, ...)                      \
    if (!parse_value) {                                 \
        break;                                          \
    }                                                   \
    parse_value = (typeof(parse_value))step __VA_OPT__( \
        ; PARSE_SEQ__CONT PARENS(__VA_ARGS__))
#define PARSE_SEQ__CONT() PARSE_SEQ__STEP

#define parse_or(alt, ...)                                      \
    OPT_SWITCH(__VA_ARGS__)(PARSE_OR__BLOCK(alt, __VA_ARGS__))( \
        (typeof(parse_value))alt)
#define PARSE_OR__BLOCK(...)                          \
    parse_value;                                      \
    do {                                              \
        typeof(*src_p) parse_mark = *src_p;           \
        typeof(parse_value) parse_hold = parse_value; \
        EVAL(PARSE_OR__ALT(__VA_ARGS__));             \
    } while (0)
#define PARSE_OR__ALT(alt, ...)                                             \
    parse_value =                                                           \
        (typeof(parse_value))alt __VA_OPT__(; PARSE_OR__CHECK(__VA_ARGS__))
#define PARSE_OR__CHECK(...)           \
    if (parse_value) {                 \
        break;                         \
    }                                  \
    *src_p = parse_mark;               \
    parse_value = parse_hold;          \
    PARSE_OR__CONT PARENS(__VA_ARGS__)
#define PARSE_OR__CONT() PARSE_OR__ALT

#define parse_not(step, ...)                                     \
    parse_value;                                                 \
    do {                                                         \
        typeof(*src_p) parse_mark = *src_p;                      \
        typeof(parse_value) parse_hold = parse_value;            \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        *src_p = parse_mark;                                     \
        if (parse_value) {                                       \
            parse_value = false;                                 \
            break;                                               \
        }                                                        \
        *src_p = parse_mark;                                     \
        parse_value = parse_hold;                                \
    } while (0)

#define parse_opt(step, ...)                                  \
    parse_or(parse_seq(step __VA_OPT__(, __VA_ARGS__)), true)

#define parse_star(step, ...)                                    \
    parse_value;                                                 \
    do {                                                         \
        typeof(*src_p) parse_mark = *src_p;                      \
        typeof(parse_value) parse_hold = parse_value;            \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        if (!parse_value || parse_mark == *src_p) {              \
            parse_value = parse_hold;                            \
            *src_p = parse_mark;                                 \
            break;                                               \
        }                                                        \
    } while (true)

#define parse_count(n, step, ...)                                \
    parse_value;                                                 \
    for (unsigned N = (n), i = 0; i < N; i++) {                  \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
        if (!parse_value) {                                      \
            break;                                               \
        }                                                        \
    }

#define parse_repeat(lo, hi, step, ...)                              \
    parse_value;                                                     \
    do {                                                             \
        unsigned LO = (lo);                                          \
        unsigned HI = (hi);                                          \
        for (unsigned i = 0; i < LO; i++) {                          \
            parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
            if (!parse_value) {                                      \
                break;                                               \
            }                                                        \
        }                                                            \
        if (!parse_value) {                                          \
            break;                                                   \
        }                                                            \
        for (unsigned i = LO; i < HI; i++) {                         \
            typeof(*src_p) mark = *src_p;                            \
            typeof(parse_value) parse_hold = parse_value;            \
            parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__)); \
            if (!parse_value) {                                      \
                *src_p = mark;                                       \
                parse_value = parse_hold;                            \
                break;                                               \
            }                                                        \
        }                                                            \
    } while (0)

#define parse_filter(cond) (cond ? parse_value : false)

#define parse_effect(stmnt, ...)                               \
    parse_value;                                               \
    stmnt __VA_OPT__(; EVAL(PARSE_EFFECT__STMNT(__VA_ARGS__)))
#define PARSE_EFFECT__STMNT(stmnt, ...)                        \
    stmnt __VA_OPT__(; PARSE_EFFECT__CONT PARENS(__VA_ARGS__))
#define PARSE_EFFECT__CONT() PARSE_EFFECT__STMNT

#define parse_echo(step, ...)                                          \
    parse_value;                                                       \
    do {                                                               \
        const char *parse_mark = *src_p;                               \
        parse_value = parse_seq(step __VA_OPT__(, __VA_ARGS__));       \
        if (!parse_value) {                                            \
            break;                                                     \
        }                                                              \
        printf("%.*s\n", (unsigned)(*src_p - parse_mark), parse_mark); \
    } while (0)

#define parse_until(step, ...)                                              \
    parse_star(parse_not(step __VA_OPT__(, __VA_ARGS__)), parse_dot(src_p))

static inline char parse_dot(const char **src_p)
{
    char c = **src_p;
    return c ? (*src_p += 1, c) : false;
}

static inline char parse_char(const char **src_p, char c)
{
    return c == **src_p ? (*src_p += 1, c) : false;
}

static inline char parse_range(const char **src_p, char lo, char hi)
{
    char c = **src_p;
    return (unsigned char)lo <= c && c <= (unsigned char)hi ? (*src_p += 1, c)
                                                            : false;
}

static inline const char *parse_cstr(const char **src_p, const char *cstr)
{
    const char *src = *src_p;
    unsigned i;

    for (i = 0; cstr[i]; i++) {
        if (src[i] != cstr[i]) {
            return false;
        }
    }
    *src_p = src + i;

    return cstr;
}

static inline bool parse_peek_line_end(const char **src_p)
{
    switch (**src_p) {
    case 0:
    case '\n':
    case '\r':
        return true;
    default:
        return false;
    }
}

static inline bool parse_line_end(const char **src_p)
{
    const char *src = *src_p;
    switch (src[0]) {
    case 0:
        return true;
    case '\n':
        switch (src[1]) {
        case '\r':
            return (*src_p = src + 2);
        default:
            return (*src_p = src + 1);
        }
    case '\r':
        return (*src_p = src + 1);
    default:
        return false;
    }
}

static inline bool parse_blank_opt(const char **src_p)
{
    parse_return(
        parse_star(parse_dot(src_p), parse_filter(isblank(parse_value))));
}

static inline bool debug_echo_line(const char **src_p)
{
    parse_return(parse_echo(parse_until(parse_peek_line_end(src_p))));
}

static inline uint32_t *parse_hex_accum(uint32_t *dest, const char **src_p)
{
    parse_try_ptr(
        parse_range(src_p, '0', '9'),
        parse_effect(*dest = (*dest << 4) | (parse_value - '0')), dest);
    parse_try_ptr(
        parse_range(src_p, 'A', 'F'),
        parse_effect(*dest = (*dest << 4) | (parse_value - 'A' + 10)), dest);
    parse_try_ptr(
        parse_range(src_p, 'a', 'f'),
        parse_effect(*dest = (*dest << 4) | (parse_value - 'a' + 10)), dest);
    return false;
}

static inline uint32_t *parse_codepoint(uint32_t *dest, const char **src_p)
{
    *dest = 0;
    parse_return_ptr(parse_repeat(4, 6, parse_hex_accum(dest, src_p)));
}

static inline bool
parse_codepoint_range(uint32_t *lo, uint32_t *hi, const char **src_p)
{
    parse_commit(parse_codepoint(lo, src_p));
    parse_try(parse_cstr(src_p, ".."), parse_codepoint(hi, src_p));
    return (*hi = *lo, true);
}

static inline uint32_t *
parse_codepoint_seq(uint32_t *dest, unsigned *len, const char **src_p)
{
    unsigned cap = *len;
    *len = 0;
    parse_return_ptr(
        parse_effect(assert(*len < cap)), parse_codepoint(dest, src_p),
        parse_effect(*len += 1),
        parse_star(
            parse_char(src_p, ' '), parse_effect(assert(*len < cap)),
            parse_codepoint(&dest[*len], src_p), parse_effect(*len += 1)),
        dest);
}

static inline uint8_t *parse_ccc(uint8_t *dest, const char **src_p)
{
    *dest = 0;
    parse_return_ptr(
        parse_repeat(
            1, 3, parse_range(src_p, '0', '9'),
            parse_filter(*dest < 25 || (*dest == 25 && parse_value <= '5')),
            parse_effect(*dest = (*dest * 10) + parse_value - '0')),
        dest);
}

static inline uint32_t *
parse_decomp(uint32_t *dest, unsigned *len, bool *compat, const char **src_p)
{
    parse_commit(parse_opt(
        parse_char(src_p, '<'), parse_until(parse_char(src_p, '>')),
        parse_cstr(src_p, "> "), parse_effect(*compat = true)));
    parse_return_ptr(parse_codepoint_seq(dest, len, src_p));
}

#define parse_field(step, ...)                                  \
    parse_seq(                                                  \
        parse_blank_opt(src_p), step __VA_OPT__(, __VA_ARGS__), \
        parse_blank_opt(src_p),                                 \
        parse_or(                                               \
            parse_char(src_p, ';'),                             \
            parse_seq(                                          \
                parse_char(src_p, '#'),                         \
                parse_until(parse_peek_line_end(src_p))),       \
            parse_peek_line_end(src_p)))

static inline bool parse_skip_field(const char **src_p)
{
    parse_return(parse_field(parse_until(parse_or(
        parse_char(src_p, ';'), parse_char(src_p, '#'),
        parse_peek_line_end(src_p)))));
}

static inline bool parse_empty_line(const char **src_p)
{
    parse_return(
        parse_blank_opt(src_p),
        parse_opt(
            parse_char(src_p, '#'), parse_until(parse_peek_line_end(src_p))),
        parse_peek_line_end(src_p));
}

static inline bool parse_derived_normalization_props(const char **src_p)
{
    uint32_t lo, hi;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(parse_codepoint_range(&lo, &hi, src_p)));
    parse_try(parse_field(
        parse_cstr(src_p, "Full_Composition_Exclusion"),
        parse_effect(set_fce(lo, hi))));
    parse_try(
        parse_field(parse_cstr(src_p, "NFKC_QC")),
        parse_field(parse_or(
            parse_seq(
                parse_char(src_p, 'N'),
                parse_effect(set_nfkc_qc(lo, hi, UCD_NFKC_QC_NO))),
            parse_seq(
                parse_char(src_p, 'M'),
                parse_effect(set_nfkc_qc(lo, hi, UCD_NFKC_QC_MAYBE))))));
    parse_try(parse_field(parse_or(
        parse_cstr(src_p, "Expands_On_NFD"),
        parse_cstr(src_p, "Expands_On_NFC"),
        parse_cstr(src_p, "Expands_On_NFKC"),
        parse_cstr(src_p, "Expands_On_NFKD"),
        parse_cstr(src_p, "Changes_When_NFKC_Casefolded"))));
    parse_try(
        parse_field(parse_or(
            parse_cstr(src_p, "FC_NFKC"), parse_cstr(src_p, "NFKC_CF"),
            parse_cstr(src_p, "NFKC_SCF"), parse_cstr(src_p, "NFD_QC"),
            parse_cstr(src_p, "NFC_QC"), parse_cstr(src_p, "NFKD_QC"))),
        parse_skip_field(src_p));
    return false;
}

static inline bool parse_unicode_data(const char **src_p)
{
    uint32_t cp = 0, decomp[32] = {0};
    unsigned decomp_len = len_of(decomp);
    uint8_t ccc = 0;
    bool compat = false;
    parse_return(
        parse_field(parse_codepoint(&cp, src_p)),
        parse_count(2, parse_skip_field(src_p)),
        parse_field(parse_ccc(&ccc, src_p), parse_effect(set_ccc(cp, ccc))),
        parse_skip_field(src_p),
        parse_field(parse_opt(
            parse_decomp(decomp, &decomp_len, &compat, src_p),
            parse_effect(set_decomp(cp, compat, decomp, decomp_len)))),
        parse_count(9, parse_skip_field(src_p)));
}

static inline bool parse_emoji_data(const char **src_p)
{
    uint32_t lo, hi;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(parse_codepoint_range(&lo, &hi, src_p)));
    parse_try(parse_field(
        parse_cstr(src_p, "Extended_Pictographic"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_EXTENDED_PICTOGRAPHIC))));
    parse_return(parse_skip_field(src_p));
}

static inline bool parse_grapheme_break_property(const char **src_p)
{
    uint32_t lo, hi;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(parse_codepoint_range(&lo, &hi, src_p)));
    parse_try(parse_field(
        parse_cstr(src_p, "Regional_Indicator"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_REGIONAL_INDICATOR))));
    parse_try(parse_field(
        parse_cstr(src_p, "SpacingMark"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_SPACING_MARK))));
    parse_try(parse_field(
        parse_cstr(src_p, "Control"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_CONTROL))));
    parse_try(parse_field(
        parse_cstr(src_p, "Prepend"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_PREPEND))));
    parse_try(parse_field(
        parse_cstr(src_p, "Extend"),
        parse_effect(set_gcb(lo, hi, UCD_GCB_EXTEND))));
    parse_try(parse_field(
        parse_cstr(src_p, "LVT"), parse_effect(set_gcb(lo, hi, UCD_GCB_LVT))));
    parse_try(parse_field(
        parse_cstr(src_p, "ZWJ"), parse_effect(set_gcb(lo, hi, UCD_GCB_ZWJ))));
    parse_try(parse_field(
        parse_cstr(src_p, "CR"), parse_effect(set_gcb(lo, hi, UCD_GCB_CR))));
    parse_try(parse_field(
        parse_cstr(src_p, "LF"), parse_effect(set_gcb(lo, hi, UCD_GCB_LF))));
    parse_try(parse_field(
        parse_cstr(src_p, "LV"), parse_effect(set_gcb(lo, hi, UCD_GCB_LV))));
    parse_try(parse_field(
        parse_cstr(src_p, "L"), parse_effect(set_gcb(lo, hi, UCD_GCB_L))));
    parse_try(parse_field(
        parse_cstr(src_p, "V"), parse_effect(set_gcb(lo, hi, UCD_GCB_V))));
    parse_try(parse_field(
        parse_cstr(src_p, "T"), parse_effect(set_gcb(lo, hi, UCD_GCB_T))));
    parse_return(parse_skip_field(src_p));
}

static inline bool parse_derived_core_properties(const char **src_p)
{
    uint32_t lo, hi;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(parse_codepoint_range(&lo, &hi, src_p)));
    parse_try(
        parse_field(parse_cstr(src_p, "InCB")),
        parse_field(parse_or(
            parse_seq(
                parse_cstr(src_p, "Consonant"),
                parse_effect(set_gcb(lo, hi, UCD_GCB_INCB_CONSONANT))),
            parse_seq(
                parse_cstr(src_p, "Extend"),
                parse_effect(set_gcb(lo, hi, UCD_GCB_INCB_EXTEND))),
            parse_seq(
                parse_cstr(src_p, "Linker"),
                parse_effect(set_gcb(lo, hi, UCD_GCB_INCB_LINKER))))));
    parse_try(parse_field(
        parse_cstr(src_p, "Default_Ignorable_Code_Point"),
        parse_effect(set_default_ignorable_code_point(lo, hi))));
    parse_try(parse_field(
        parse_cstr(src_p, "XID_Start"), parse_effect(set_xid_start(lo, hi))));
    parse_try(parse_field(
        parse_cstr(src_p, "XID_Continue"),
        parse_effect(set_xid_continue(lo, hi))));
    parse_return(parse_skip_field(src_p));
}

static inline bool parse_east_asian_width(const char **src_p)
{
    uint32_t lo, hi;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(parse_codepoint_range(&lo, &hi, src_p)));
    parse_try(parse_field(
        parse_or(
            parse_char(src_p, 'A'), parse_char(src_p, 'F'),
            parse_char(src_p, 'W')),
        parse_effect(set_eaw(lo, hi, parse_value))));
    parse_return(parse_skip_field(src_p));
}

static inline bool parse_grapheme_break_test(const char **src_p)
{
    uint32_t cp[32] = {0};
    unsigned cp_len = 0, seg[32] = {0}, seg_len = 0;
    parse_try(parse_empty_line(src_p));
    parse_commit(parse_field(
        parse_cstr(src_p, "÷ "), parse_codepoint(&cp[cp_len++], src_p),
        parse_star(
            parse_or(
                parse_cstr(src_p, " × "),
                parse_seq(
                    parse_cstr(src_p, " ÷ "),
                    parse_effect(seg[seg_len++] = cp_len))),
            parse_codepoint(&cp[cp_len], src_p), parse_effect(cp_len++)),
        parse_cstr(src_p, " ÷")));
    seg[seg_len++] = cp_len;

    unsigned test_len = 0, test[32];
    for (size_t idx = 0; idx < cp_len;) {
        size_t grapheme = ucd_grapheme_cluster_len(cp + idx, cp_len - idx);
        test[test_len++] = grapheme;
        idx += grapheme;
    }

    bool success = (seg[0] == test[0]);
    for (unsigned i = 1; i < seg_len; i++) {
        if (i >= test_len || seg[i] - seg[i - 1] != test[i]) {
            success = false;
            break;
        }
    }

    if (success) {
        return true;
    }

    printf("\n÷ ");
    for (unsigned i = 0; i < cp_len - 1; i++) {
        printf("%04X:", cp[i]);
        print_gcb(get_gcb(cp[i]));

        bool join = true;
        for (unsigned j = 0; j < seg_len; j++) {
            if (i == seg[j] - 1) {
                printf(" ÷ ");
                join = false;
                break;
            } else if (i < seg[j]) {
                break;
            }
        }
        if (join) {
            printf(" × ");
        } else {
        }
    }
    printf("%04X:", cp[cp_len - 1]);
    print_gcb(get_gcb(cp[cp_len - 1]));
    printf(" ÷\n");

    printf("%u ", seg[0]);
    for (unsigned i = 1; i < seg_len; i++) {
        printf("%u ", seg[i] - seg[i - 1]);
    }
    printf("\n");

    for (unsigned i = 0; i < test_len; i++) {
        printf("%u ", test[i]);
    }
    printf("\n\n");

    return success;
}

static inline void print_decomp(uint32_t cp)
{
    unsigned idx = get_decomp_idx(cp);
    if (!idx) {
        printf("%04x ", cp);
        return;
    }

    printf("%04x => ", cp);
    const uint32_t *src = g_decomp_buf + idx;
    for (unsigned i = 0; src[i]; i++) {
        print_decomp(src[i]);
    }
}

static inline bool parse_normalization_test(const char **src_p)
{
    uint32_t source[32] = {0}, nfkc[32] = {0}, norm[32] = {0};
    unsigned source_len = len_of(source), nfkc_len = len_of(nfkc);

    const char *start = *src_p;
    ((void)(start));
    parse_try(parse_empty_line(src_p));
    parse_try(parse_char(src_p, '@'), parse_until(parse_peek_line_end(src_p)));
    parse_commit(
        parse_field(parse_codepoint_seq(source, &source_len, src_p)),
        parse_count(2, parse_skip_field(src_p)),
        parse_field(parse_codepoint_seq(nfkc, &nfkc_len, src_p)),
        parse_count(2, parse_skip_field(src_p)));

    size_t norm_len = norm_nfkc(norm, len_of(norm), source, source_len);
    assert(norm_len);

    for (size_t i = 0; i < norm_len; i++) {
        if (i >= nfkc_len || nfkc[i] != norm[i]) {
            printf("\nTest: ");
            for (size_t j = 0; j < source_len; j++) {
                printf("%04x ", source[j]);
            }
            printf("=> ");
            for (size_t j = 0; j < nfkc_len; j++) {
                printf("%04x ", nfkc[j]);
            }
            printf("\n");
            for (size_t j = 0; j < norm_len; j++) {
                printf("%04x:%u ", norm[j], get_ccc(norm[j]));
            }
            printf("\n");
            return false;
        }
    }

    return true;
}

#define parse_file(path, line_fn)                          \
    true;                                                  \
    do {                                                   \
        const char *src = load_file(path);                 \
        if (!src) {                                        \
            retval = false;                                \
            break;                                         \
        }                                                  \
                                                           \
        while (*src) {                                     \
            const char *mark = src;                        \
            ((void)(mark));                                \
            if (!line_fn(&src) || !parse_line_end(&src)) { \
                debug_echo_line(&src);                     \
                retval = false;                            \
                break;                                     \
            }                                              \
        }                                                  \
    } while (0)

int main(int argc, char **args)
{
    if (argc < 3) {
        fprintf(stderr, "Must specify two filenames\n");
        return EXIT_FAILURE;
    }

    bool retval = true;
    retval &= parse_file(
        "data/ucd/DerivedNormalizationProps.txt",
        parse_derived_normalization_props);
    retval &= parse_file("data/ucd/UnicodeData.txt", parse_unicode_data);
    retval &= parse_file(
        "data/ucd/DerivedCoreProperties.txt", parse_derived_core_properties);
    retval &= parse_file("data/ucd/emoji/emoji-data.txt", parse_emoji_data);
    retval &= parse_file(
        "data/ucd/auxiliary/GraphemeBreakProperty.txt",
        parse_grapheme_break_property);
    retval &= parse_file("data/ucd/EastAsianWidth.txt", parse_east_asian_width);
    retval &= parse_file(
        "data/ucd/auxiliary/GraphemeBreakTest.txt", parse_grapheme_break_test);
    retval &=
        parse_file("data/ucd/NormalizationTest.txt", parse_normalization_test);
    if (!retval) {
        return EXIT_FAILURE;
    }

    FILE *f = fopen(args[1], "wb");
    if (!f) {
        fprintf(
            stderr, "Failed to open file '%s' for writing: %s\n", args[1],
            strerror(errno));
        return EXIT_FAILURE;
    }

    fprintf(f, "#include \"ucd.h\"\n");
    fprintf(f, "#include <stdbool.h>\n");
    fprintf(f, "#include <stddef.h>\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "\n");

    size_t line_len =
        fprintf(f, "static const uint16_t g_ucd_info[] = {%#x", ucd_info[0]);
    for (size_t i = 1; i < len_of(ucd_info); i++) {
        if (line_len + snprintf(NULL, 0, ", %#x", ucd_info[i]) > 79) {
            fprintf(f, ",\n");
            line_len = fprintf(f, "    %#x", ucd_info[i]);
        } else {
            line_len += fprintf(f, ", %#x", ucd_info[i]);
        }
    }
    fprintf(f, "\n};\n\n");

    line_len = fprintf(
        f, "static const uint32_t g_ucd_recomp[] = {%#x", g_recomp_map[0][0]);
    for (size_t row = 0; row < g_recomp_count; row++) {
        for (size_t col = (row ? 0 : 1); col < g_recomp_count; col++) {
            if (line_len + snprintf(NULL, 0, ", %#x", g_recomp_map[row][col]) >
                79) {
                fprintf(f, ",\n");
                line_len = fprintf(f, "    %#x", g_recomp_map[row][col]);
            } else {
                line_len += fprintf(f, ", %#x", g_recomp_map[row][col]);
            }
        }
    }
    fprintf(f, "\n};\n\n");

    line_len = fprintf(
        f, "static const uint32_t g_ucd_decomp[] = {%#x", g_decomp_buf[0]);
    for (size_t i = 1; i < g_decomp_at; i++) {
        if (line_len + snprintf(NULL, 0, ", %#x", g_decomp_buf[i]) > 79) {
            fprintf(f, ",\n");
            line_len = fprintf(f, "    %#x", g_decomp_buf[i]);
        } else {
            line_len += fprintf(f, ", %#x", g_decomp_buf[i]);
        }
    }
    fprintf(f, "\n};\n\n");

    fprintf(f, "const uint32_t *ucd_decomp(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    unsigned idx = g_ucd_info[cp * 3] >> 2;\n");
    fprintf(f, "    return idx ? g_ucd_decomp + idx : NULL;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "uint8_t ucd_nfkc_qc(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3] & 0x3;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "uint32_t ucd_recomp(uint32_t lhs, uint32_t rhs)\n");
    fprintf(f, "{\n");
    fprintf(f, "    unsigned lhs_idx = g_ucd_info[lhs * 3 + 1] >> 5;\n");
    fprintf(f, "    unsigned rhs_idx = g_ucd_info[rhs * 3 + 1] >> 5;\n");
    fprintf(
        f, "    return g_ucd_recomp[lhs_idx * %u + rhs_idx];\n",
        g_recomp_count);
    fprintf(f, "}\n\n");

    fprintf(f, "uint8_t ucd_gcb(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3 + 1] & 0x1f;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "uint8_t ucd_ccc(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3 + 2] >> 8;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "uint8_t ucd_eaw(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return (g_ucd_info[cp * 3 + 2] >> 6) & 0x3;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool ucd_default_ignorable(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3 + 2] & 0x4;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool ucd_xid_start(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3 + 2] & 0x2;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "bool ucd_xid_continue(uint32_t cp)\n");
    fprintf(f, "{\n");
    fprintf(f, "    return g_ucd_info[cp * 3 + 2] & 0x1;\n");
    fprintf(f, "}\n");

    fclose(f);

    f = fopen(args[2], "wb");
    if (!f) {
        fprintf(
            stderr, "Failed to open file '%s' for writing: %s\n", args[2],
            strerror(errno));
        return EXIT_FAILURE;
    }

    fprintf(f, "#ifndef BLU_UCD_H\n");
    fprintf(f, "#define BLU_UCD_H\n\n");

    fprintf(f, "#include <stdbool.h>\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "\n");

    fprintf(f, "enum {\n");
    fprintf(f, "    UCD_GCB__NIL,\n");
    fprintf(f, "    UCD_GCB_CR,\n");
    fprintf(f, "    UCD_GCB_LF,\n");
    fprintf(f, "    UCD_GCB_CONTROL,\n");
    fprintf(f, "    UCD_GCB_PREPEND,\n");
    fprintf(f, "    UCD_GCB_LVT,\n");
    fprintf(f, "    UCD_GCB_LV,\n");
    fprintf(f, "    UCD_GCB_L,\n");
    fprintf(f, "    UCD_GCB_V,\n");
    fprintf(f, "    UCD_GCB_T,\n");
    fprintf(f, "    UCD_GCB_REGIONAL_INDICATOR,\n");
    fprintf(f, "    UCD_GCB_EXTENDED_PICTOGRAPHIC,\n");
    fprintf(f, "    UCD_GCB_INCB_CONSONANT,\n");
    fprintf(f, "    UCD_GCB_INCB_EXTEND,\n");
    fprintf(f, "    UCD_GCB_INCB_LINKER,\n");
    fprintf(f, "    UCD_GCB_EXTEND,\n");
    fprintf(f, "    UCD_GCB_ZWJ,\n");
    fprintf(f, "    UCD_GCB_SPACING_MARK,\n");
    fprintf(f, "    UCD_GCB__MAX,\n");
    fprintf(f, "};\n\n");

    fprintf(f, "const uint32_t *ucd_decomp(uint32_t cp);\n");
    fprintf(f, "uint8_t ucd_nfkc_qc(uint32_t cp);\n");
    fprintf(f, "uint32_t ucd_recomp(uint32_t lhs, uint32_t rhs);\n");
    fprintf(f, "uint8_t ucd_gcb(uint32_t cp);\n");
    fprintf(f, "uint8_t ucd_ccc(uint32_t cp);\n");
    fprintf(f, "uint8_t ucd_eaw(uint32_t cp);\n");
    fprintf(f, "bool ucd_default_ignorable(uint32_t cp);\n");
    fprintf(f, "bool ucd_xid_start(uint32_t cp);\n");
    fprintf(f, "bool ucd_xid_continue(uint32_t cp);\n");
    fprintf(f, "\n");
    fprintf(f, "#endif\n");

    fclose(f);

    return 0;
}
