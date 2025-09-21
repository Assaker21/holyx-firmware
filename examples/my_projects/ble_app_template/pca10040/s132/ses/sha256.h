#ifndef SHA256_H
#define SHA256_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool sha256_hex(const char *msg, size_t mlen, char *hex_out /*65*/);

#endif