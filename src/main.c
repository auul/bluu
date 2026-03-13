#include "utf8.h"
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **args)
{
    if (argc < 2) {
        return 0;
    }

    uint32_t state = 0;
    unsigned width = 0;
    for (unsigned i = 0; args[1][i]; i++) {
        if (utf8_gcb(&state, args[1][i])) {
            printf("\n");
        }
        printf("%c", args[1][i]);
    }

    return 0;
}
