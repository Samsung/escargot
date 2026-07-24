/*
 * Copyright (c) 2021-present Samsung Electronics Co., Ltd
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

#ifndef __EscargotSerializedValue__
#define __EscargotSerializedValue__

#include "runtime/Value.h"
#include <cstring>
#include <string>

namespace Escargot {

class SerializedValue {
    friend class Serializer;

public:
#define FOR_EACH_SERIALIZABLE_TYPE(F) \
    F(Undefined)                      \
    F(Null)                           \
    F(Boolean)                        \
    F(Number)                         \
    F(String)                         \
    F(Symbol)                         \
    F(BigInt)

    enum Type {
#define DECLARE_SERIALIZABLE_TYPE(name) name,
        FOR_EACH_SERIALIZABLE_TYPE(DECLARE_SERIALIZABLE_TYPE)
#undef DECLARE_SERIALIZABLE_TYPE
#if defined(ENABLE_THREADING)
            SharedArrayBufferObject
#endif
    };
    virtual ~SerializedValue() {}
    virtual Type type() = 0;
    virtual Value toValue(ExecutionState& state) = 0;

    void serializeInto(std::string& output)
    {
        output.push_back(static_cast<char>(type()));
        serializeValueData(output);
    }

protected:
    virtual void serializeValueData(std::string& output) {}
};

namespace SerializerDetail {
template <typename T>
inline void writePOD(std::string& out, const T& v)
{
    out.append(reinterpret_cast<const char*>(&v), sizeof(T));
}
inline void writeBytes(std::string& out, const void* p, size_t n)
{
    out.append(reinterpret_cast<const char*>(p), n);
}
struct Reader {
    const char* data;
    size_t len;
    size_t pos;
    Reader(const char* d, size_t l, size_t p)
        : data(d), len(l), pos(p) {}
    template <typename T>
    bool readPOD(T& v)
    {
        if (pos + sizeof(T) > len) { return false; }
        memcpy(&v, data + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }
    bool readBytes(void* dst, size_t n)
    {
        if (pos + n > len) { return false; }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
    bool readString(std::string& s, size_t n)
    {
        if (pos + n > len) { return false; }
        s.assign(data + pos, n);
        pos += n;
        return true;
    }
};
} // namespace SerializerDetail

} // namespace Escargot

#endif
