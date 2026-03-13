#ifndef BLU_UTF_H
#define BLU_UTF_H

#include <stdbool.h>
#include <stdint.h>

bool utf_grapheme_break(uint32_t *state, uint32_t cp);

#endif
