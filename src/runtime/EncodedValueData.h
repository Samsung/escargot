/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotEncodedValueData__
#define __EscargotEncodedValueData__

namespace Escargot {

struct EncodedValueData {
    intptr_t payload;
    EncodedValueData()
        : payload(0)
    {
    }

    explicit EncodedValueData(void* ptr)
        : payload((intptr_t)ptr)
    {
    }
};

#if defined(ESCARGOT_64) && defined(ESCARGOT_USE_32BIT_IN_64BIT)
struct EncodedSmallValueData {
    int32_t payload;
    EncodedSmallValueData()
        : payload(0)
    {
    }
};
#endif
} // namespace Escargot


#endif
