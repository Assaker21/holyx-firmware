/* --------------------------------------------------------------------------
 * Minimal, dependency-free SHA-256 – fits in <4 kB of flash
 * Charbel Assaker – May 2025
 * -------------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---------- internal helpers ------------------------------------------------*/
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/* Compress a single 512-bit block */
static void sha256_compress(uint32_t s[8], const uint8_t block[64])
{
    uint32_t W[64];

    /* message schedule */
    for (int t = 0; t < 16; ++t)
        W[t] = (uint32_t)block[t * 4] << 24 | (uint32_t)block[t * 4 + 1] << 16 |
               (uint32_t)block[t * 4 + 2] << 8 | (uint32_t)block[t * 4 + 3];

    for (int t = 16; t < 64; ++t)
        W[t] = SSIG1(W[t - 2]) + W[t - 7] + SSIG0(W[t - 15]) + W[t - 16];

    /* working vars */
    uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];

    for (int t = 0; t < 64; ++t)
    {
        uint32_t T1 = h + BSIG1(e) + CH(e, f, g) + K[t] + W[t];
        uint32_t T2 = BSIG0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
    s[5] += f;
    s[6] += g;
    s[7] += h;
}

/* One-shot SHA-256 – usable on arbitrarily sized inputs */
/* One-shot SHA-256 – corrected total-length handling */
static void sha256(const uint8_t *data, size_t len, uint8_t hash[32])
{
    /* initial hash values */
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    uint64_t total_len = 0; /* counts every byte fed */

    /* --- process full 64-byte blocks --------------------------------- */
    while (len >= 64)
    {
        sha256_compress(state, data);
        data += 64;
        len -= 64;
        total_len += 64;
    }

    /* --- final block with padding ------------------------------------ */
    uint8_t block[64] = {0};
    memcpy(block, data, len); /* copy the remaining tail */
    block[len] = 0x80;        /* append the single ‘1’ bit */

    total_len += len;                 /* now we have the *full* length */
    uint64_t bit_len = total_len * 8; /* convert to bits            */

    /* if not enough room for length field, compress and start a new blk */
    if (len >= 56)
    {
        sha256_compress(state, block);
        memset(block, 0, 56); /* clear first 56 bytes */
    }

    /* append original length in big-endian (last 8 bytes) */
    for (int i = 0; i < 8; ++i)
        block[63 - i] = (uint8_t)(bit_len >> (i * 8));

    sha256_compress(state, block);

    /* --- output big-endian digest ------------------------------------ */
    for (int i = 0; i < 8; ++i)
    {
        hash[i * 4] = (uint8_t)(state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(state[i]);
    }
}

/* --------------------------------------------------------------------------
 * Public helper: returns SHA-256 digest of `msg` as 64-char hex string.
 * hex_out needs space for 65 bytes (64 + terminating NUL).
 * Always returns true (kept the same signature for drop-in use).
 * -------------------------------------------------------------------------- */
bool sha256_hex(const char *msg, size_t mlen, char *hex_out /*65*/)
{
    uint8_t digest[32];

    sha256((const uint8_t *)msg, mlen, digest);

    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
    {
        hex_out[i * 2] = HEX[digest[i] >> 4];
        hex_out[i * 2 + 1] = HEX[digest[i] & 0x0F];
    }
    hex_out[64] = '\0';
    return true;
}