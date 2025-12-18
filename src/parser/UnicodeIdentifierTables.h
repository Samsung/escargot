/* * Copyright (c) 2020-present Samsung Electronics Co., Ltd
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

#include <stdint.h>

#ifndef __EscargotUnicodeIdentifierTables__
#define __EscargotUnicodeIdentifierTables__
namespace Escargot {
namespace EscargotLexer {

extern const uint16_t identRangeStart[];
extern const uint16_t identRangeLength[];
extern const uint16_t identRangeLongLength[];
extern const uint32_t identRangeStartSupplementaryPlane[];
extern const uint16_t identRangeLengthSupplementaryPlane[];
extern const uint16_t basicPlaneLength;
extern const uint16_t supplementaryPlaneLength;

} // namespace EscargotLexer
} // namespace Escargot
#endif
