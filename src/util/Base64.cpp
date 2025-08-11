/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
   Copyright (C) 2007-2024 Apple Inc. All rights reserved.
   Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

   This code is based on the java implementation in HTTPClient
   package by Ronald Tschalär Copyright (C) 1996-1999.
*/
/*
 * Copyright (c) 2025-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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

#include "Escargot.h"
#include "Base64.h"
#include "runtime/ErrorObject.h"

#include <limits.h>
#include <simdutf.h>

namespace Escargot {

#define nonAlphabet -1
#define encodeMapSize 64
#define decodeMapSize 128

static inline simdutf::base64_options toSIMDUTFEncodeOptions(Base64EncodeOption options)
{
    if ((int)options & (int)Base64EncodeOption::URL) {
        if ((int)options & (int)Base64EncodeOption::OmitPadding)
            return simdutf::base64_url;
        return simdutf::base64_url_with_padding;
    }
    if ((int)options & (int)Base64EncodeOption::OmitPadding)
        return simdutf::base64_default_no_padding;
    return simdutf::base64_default;
}

template <typename T>
void base64EncodeInternal(T* inputDataBuffer, size_t inputDataBufferLen, char* destinationDataBuffer, const std::array<char, encodeMapSize>& encodeMap, size_t& sidx, size_t& didx)
{
    if (inputDataBufferLen > 1) {
        while (sidx < inputDataBufferLen - 2) {
            destinationDataBuffer[didx++] = encodeMap[(inputDataBuffer[sidx] >> 2) & 077];
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 1] >> 4) & 017) | ((inputDataBuffer[sidx] << 4) & 077)];
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 2] >> 6) & 003) | ((inputDataBuffer[sidx + 1] << 2) & 077)];
            destinationDataBuffer[didx++] = encodeMap[inputDataBuffer[sidx + 2] & 077];
            sidx += 3;
        }
    }

    if (sidx < inputDataBufferLen) {
        destinationDataBuffer[didx++] = encodeMap[(inputDataBuffer[sidx] >> 2) & 077];
        if (sidx < inputDataBufferLen - 1) {
            destinationDataBuffer[didx++] = encodeMap[((inputDataBuffer[sidx + 1] >> 4) & 017) | ((inputDataBuffer[sidx] << 4) & 077)];
            destinationDataBuffer[didx++] = encodeMap[(inputDataBuffer[sidx + 1] << 2) & 077];
        } else {
            destinationDataBuffer[didx++] = encodeMap[(inputDataBuffer[sidx] << 4) & 077];
        }
    }
}


String* base64EncodeToString(uint8_t* src, size_t srcLength, Base64EncodeOption options, Optional<ExecutionState*> state)
{
    constexpr std::array<char, encodeMapSize> base64EncMap{
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
        0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F
    };

    constexpr std::array<char, encodeMapSize> base64URLEncMap{
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
        0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2D, 0x5F
    };

    const auto& encodeMap = (int)options & (int)Base64EncodeOption::URL ? base64URLEncMap : base64EncMap;

    auto inputDataBuffer = src;
    size_t dstLength = simdutf::base64_length_from_binary(srcLength, toSIMDUTFEncodeOptions(options));

    if (state && UNLIKELY(dstLength > STRING_MAXIMUM_LENGTH)) {
        ErrorObject::throwBuiltinError(*state.value(), ErrorCode::RangeError, ErrorObject::Messages::String_InvalidStringLength);
    }

    size_t sidx = 0;
    size_t didx = 0;

    ASCIIStringData dstBuffer;
    dstBuffer.resizeWithUninitializedValues(dstLength);

    base64EncodeInternal(src, srcLength, dstBuffer.data(), encodeMap, sidx, didx);

    if (!((int)options & (int)Base64EncodeOption::OmitPadding)) {
        while (didx < dstLength) {
            dstBuffer.data()[didx++] = '=';
        }
    }

    return new ASCIIString(std::move(dstBuffer));
}
/*
template<typename T>
static Optional<String*> base64DecodeInternal(T* inputDataBuffer, size_t inputDataBufferSize, Base64DecodeOption options)
{
    if (!inputDataBufferSize) {
        return std::vector<uint8_t> { };
    }

    constexpr std::array<char, decodeMapSize> base64DecMap {
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, 0x3E, nonAlphabet, nonAlphabet, nonAlphabet, 0x3F,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
    };

    constexpr std::array<char, decodeMapSize> base64URLDecMap {
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, 0x3E, nonAlphabet, nonAlphabet,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
        nonAlphabet, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, 0x3F,
        nonAlphabet, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
    };

    const auto& decodeMap = (int)options & (int)Base64DecodeOption::URL ? base64URLDecMap : base64DecMap;
    auto validatePadding = (int)options & (int)Base64DecodeOption::ValidatePadding;
    auto ignoreWhitespace = (int)options & (int)Base64DecodeOption::IgnoreWhitespace;

    std::vector<uint8_t> destination;
    destination.resize(inputDataBufferSize);

    unsigned equalsSignCount = 0;
    unsigned destinationLength = 0;
    for (unsigned idx = 0; idx < inputDataBufferSize; ++idx) {
        unsigned ch = inputDataBuffer[idx];
        if (ch == '=') {
            ++equalsSignCount;
            // There should never be more than 2 padding characters.
            if (validatePadding && equalsSignCount > 2) {
                return NullOption;
            }
        } else {
            char decodedCharacter = ch < decodeMapSize ? decodeMap[ch] : nonAlphabet;
            if (decodedCharacter != nonAlphabet) {
                if (equalsSignCount) {
                    return NullOption;
                }
                destination[destinationLength++] = decodedCharacter;
            } else if (!ignoreWhitespace || !isASCIIWhitespace(ch)) {
                return NullOption;
            }
        }
    }

    // Make sure we shrink back the Vector before returning. destinationLength may be shorter than expected
    // in case of error or in case of ignored spaces.
    if (destinationLength < destination.size()) {
        destination.resize(destinationLength);
    }

    if (!destinationLength) {
        if (equalsSignCount) {
            return NullOption;
        }
        return std::vector<uint8_t> { };
    }

    // The should be no padding if length is a multiple of 4.
    // We use (destinationLength + equalsSignCount) instead of length because we don't want to account for ignored characters (i.e. spaces).
    if (validatePadding && equalsSignCount && (destinationLength + equalsSignCount) % 4) {
        return NullOption;
    }

    // Valid data is (n * 4 + [0,2,3]) characters long.
    if ((destinationLength % 4) == 1) {
        return NullOption;
    }

    // 4-byte to 3-byte conversion
    destinationLength -= (destinationLength + 3) / 4;
    if (!destinationLength) {
        return NullOption;
    }

    unsigned sidx = 0;
    unsigned didx = 0;
    if (destinationLength > 1) {
        while (didx < destinationLength - 2) {
            destination[didx    ] = (((destination[sidx    ] << 2) & 255) | ((destination[sidx + 1] >> 4) & 003));
            destination[didx + 1] = (((destination[sidx + 1] << 4) & 255) | ((destination[sidx + 2] >> 2) & 017));
            destination[didx + 2] = (((destination[sidx + 2] << 6) & 255) |  (destination[sidx + 3]       & 077));
            sidx += 4;
            didx += 3;
        }
    }

    if (didx < destinationLength)
        destination[didx] = (((destination[sidx    ] << 2) & 255) | ((destination[sidx + 1] >> 4) & 003));

    if (++didx < destinationLength)
        destination[didx] = (((destination[sidx + 1] << 4) & 255) | ((destination[sidx + 2] >> 2) & 017));

    if (destinationLength < destination.size()) {
        destination.resize(destinationLength);
    }
    return destination;
}

Optional<std::vector<uint8_t>> base64Decode(String* string, Base64DecodeOption options)
{
    auto bad = string->bufferAccessData();
    if (bad.has8BitContent) {
        return base64DecodeInternal(bad.bufferAs8Bit, bad.length, options);
    } else {
        return base64DecodeInternal(bad.bufferAs16Bit, bad.length, options);
    }
}
*/

static inline simdutf::base64_options toSIMDUTFDecodeOptions(Alphabet alphabet)
{
    switch (alphabet) {
    case Alphabet::Base64:
        return simdutf::base64_default;
    case Alphabet::Base64URL:
        return simdutf::base64_url;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static inline simdutf::last_chunk_handling_options toSIMDUTFLastChunkHandling(LastChunkHandling lastChunkHandling)
{
    switch (lastChunkHandling) {
    case LastChunkHandling::Loose:
        return simdutf::last_chunk_handling_options::loose;
    case LastChunkHandling::Strict:
        return simdutf::last_chunk_handling_options::strict;
    case LastChunkHandling::StopBeforePartial:
        return simdutf::last_chunk_handling_options::stop_before_partial;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template <typename T>
static inline size_t fixSIMDUTFStopBeforePartialReadLength(T* src, size_t srcLength, size_t readLengthFromSIMDUTF)
{
    // Work around simdutf bug for stop-before-partial read length.
    // FIXME: Remove once fixed in simdutf.

    // First go backwards to find the last non-whitespace character read.
    size_t idx = readLengthFromSIMDUTF;
    while (idx && isASCIIWhitespace(src[idx - 1]))
        idx--;

    // This is the correct read length if the input doesn't end in trailing whitespace.
    size_t fixedReadLength = idx;

    // If the input ends in trailing whitespace, the correct read length is the input size.
    while (idx < srcLength && isASCIIWhitespace(src[idx]))
        idx++;
    if (idx == srcLength)
        fixedReadLength = srcLength;

    return fixedReadLength;
}

template <typename T>
static std::tuple<FromBase64ShouldThrowError, size_t, size_t, std::vector<uint8_t>> fromBase64Impl(T* src, size_t srcLength, Optional<uint8_t*> dst, size_t dstLength, Alphabet alphabet, LastChunkHandling lastChunkHandling)
{
    if (dst) {
        auto result = simdutf::base64_to_binary_safe(src, srcLength, (char*)dst.value(), dstLength,
                                                     toSIMDUTFDecodeOptions(alphabet), toSIMDUTFLastChunkHandling(lastChunkHandling), true);
        switch (result.error) {
        case simdutf::error_code::SUCCESS: {
            size_t read;
            if (lastChunkHandling == LastChunkHandling::StopBeforePartial) {
                read = fixSIMDUTFStopBeforePartialReadLength(src, srcLength, result.count);
            } else {
                read = srcLength;
            }
            return std::make_tuple(FromBase64ShouldThrowError::No, read, dstLength, std::vector<uint8_t>());
        }
        case simdutf::error_code::OUTPUT_BUFFER_TOO_SMALL: {
            return std::make_tuple(FromBase64ShouldThrowError::No, result.count, dstLength, std::vector<uint8_t>());
        }
        default:
            return { FromBase64ShouldThrowError::Yes, result.count, dstLength, std::vector<uint8_t>() };
        }
    }
    size_t outputLength = simdutf::maximal_binary_length_from_base64(src, srcLength);
    std::vector<uint8_t> output;
    output.resize(outputLength);

    auto result = simdutf::base64_to_binary_safe(src, srcLength, (char*)output.data(), outputLength,
                                                 toSIMDUTFDecodeOptions(alphabet), toSIMDUTFLastChunkHandling(lastChunkHandling), true);
    switch (result.error) {
    case simdutf::error_code::SUCCESS: {
        size_t read;
        if (lastChunkHandling == LastChunkHandling::StopBeforePartial) {
            read = fixSIMDUTFStopBeforePartialReadLength(src, srcLength, result.count);
        } else {
            read = srcLength;
        }
        output.resize(outputLength);
        return std::make_tuple(FromBase64ShouldThrowError::No, read, outputLength, output);
    }
    case simdutf::error_code::OUTPUT_BUFFER_TOO_SMALL: {
        ASSERT_NOT_REACHED();
        FALLTHROUGH;
    }
    default:
        return { FromBase64ShouldThrowError::Yes, result.count, outputLength, output };
    }
}

std::tuple<FromBase64ShouldThrowError, size_t, size_t, std::vector<uint8_t>> fromBase64(String* string, Optional<uint8_t*> dst, size_t dstLength, Alphabet alphabet, LastChunkHandling lastChunkHandling)
{
    auto bad = string->bufferAccessData();
    if (bad.has8BitContent) {
        return fromBase64Impl(bad.bufferAs8Bit, bad.length, dst, dstLength, alphabet, lastChunkHandling);
    } else {
        return fromBase64Impl(bad.bufferAs16Bit, bad.length, dst, dstLength, alphabet, lastChunkHandling);
    }
}

} // namespace Escargot
