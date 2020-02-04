/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

/*
 * This implementation is based on:
 * Author:     Brad Conte (brad AT bradconte.com)
 * Disclaimer: This code is presented "as is" without any guarantees.
 * Details:    Implementation of the SHA1 hashing algorithm.
               Algorithm specification can be found here:
                * http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
               This implementation uses little endian byte order.
*/

#include "Escargot.h"
#include "DebuggerTcp.h"

#ifdef ESCARGOT_DEBUGGER
namespace Escargot {

struct Sha1Context {
    uint8_t data[64];
    uint32_t dataLength;
    uint32_t state[5];
    uint32_t k[4];
};

static void sha1Init(Sha1Context* ctx)
{
    ctx->dataLength = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xc3d2e1f0;
    ctx->k[0] = 0x5a827999;
    ctx->k[1] = 0x6ed9eba1;
    ctx->k[2] = 0x8f1bbcdc;
    ctx->k[3] = 0xca62c1d6;
}

#define SHA1_ROTATE_LEFT(a, b) ((a << b) | (a >> (32 - b)))

static void sha1Transform(Sha1Context* ctx)
{
    uint8_t* data = ctx->data;
    uint32_t a, b, c, d, e, i, t, m[80];

    /* Copy data in big endian format (could be memcpy on big endian machines). */
    for (i = 0; i < 16; ++i) {
        m[i] = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + (data[3]);
        data += 4;
    }

    for (; i < 80; ++i) {
        m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
        m[i] = (m[i] << 1) | (m[i] >> 31);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 20; ++i) {
        t = SHA1_ROTATE_LEFT(a, 5) + ((b & c) ^ (~b & d)) + e + ctx->k[0] + m[i];
        e = d;
        d = c;
        c = SHA1_ROTATE_LEFT(b, 30);
        b = a;
        a = t;
    }
    for (; i < 40; ++i) {
        t = SHA1_ROTATE_LEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[1] + m[i];
        e = d;
        d = c;
        c = SHA1_ROTATE_LEFT(b, 30);
        b = a;
        a = t;
    }
    for (; i < 60; ++i) {
        t = SHA1_ROTATE_LEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d)) + e + ctx->k[2] + m[i];
        e = d;
        d = c;
        c = SHA1_ROTATE_LEFT(b, 30);
        b = a;
        a = t;
    }
    for (; i < 80; ++i) {
        t = SHA1_ROTATE_LEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[3] + m[i];
        e = d;
        d = c;
        c = SHA1_ROTATE_LEFT(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

#undef SHA1_ROTATE_LEFT

static void sha1Update(Sha1Context* ctx, const uint8_t* source, size_t length)
{
    uint8_t* data = ctx->data;
    const uint8_t* sourceEnd = source + length;
    uint32_t dataLength = ctx->dataLength;

    while (source < sourceEnd) {
        data[dataLength] = *source++;
        dataLength++;

        if (dataLength == 64) {
            sha1Transform(ctx);
            dataLength = 0;
        }
    }

    ctx->dataLength = dataLength;
}

static void sha1Finish(Sha1Context* ctx, uint8_t hash[20], size_t totalLength)
{
    uint8_t* data = ctx->data;
    uint32_t dataLength, i;

    dataLength = ctx->dataLength;
    data[dataLength++] = 0x80;

    // Pad whatever data is left in the buffer.
    if (dataLength <= 56) {
        memset(data + dataLength, 0, 56 - dataLength);
    } else {
        memset(data + dataLength, 0, 64 - dataLength);
        sha1Transform(ctx);
        memset(data, 0, 56);
    }

    // Currently we don't support large messages.
    ASSERT(totalLength < (UINT32_MAX >> 3));
    totalLength <<= 3;

    data[63] = (uint8_t)totalLength;
    data[62] = (uint8_t)(totalLength >> 8);
    data[61] = (uint8_t)(totalLength >> 16);
    data[60] = (uint8_t)(totalLength >> 24);
    data[59] = 0;
    data[58] = 0;
    data[57] = 0;
    data[56] = 0;

    sha1Transform(ctx);

    for (i = 0; i < 5; ++i) {
        uint32_t state = ctx->state[i];

        /* Copy data in big endian format (could be memcpy on big endian machines). */
        hash[0] = (uint8_t)(state >> 24);
        hash[1] = (uint8_t)(state >> 16);
        hash[2] = (uint8_t)(state >> 8);
        hash[3] = (uint8_t)state;
        hash += 4;
    }
}

void DebuggerTcp::computeSha1(const uint8_t* source1, size_t source1Length,
                              const uint8_t* source2, size_t source2Length,
                              uint8_t destination[20])
{
    Sha1Context sha1Context;

    sha1Init(&sha1Context);
    sha1Update(&sha1Context, source1, source1Length);
    sha1Update(&sha1Context, source2, source2Length);
    sha1Finish(&sha1Context, destination, source1Length + source2Length);
}
}
#endif /* ESCARGOT_DEBUGGER */
