#ifndef __EscargotValueInlines__
#define __EscargotValueInlines__


namespace Escargot {

// The fast double-to-(unsigned-)int conversion routine does not guarantee
// rounding towards zero.
// The result is unspecified if x is infinite or NaN, or if the rounded
// integer value is outside the range of type int.
inline int FastD2I(double x)
{
    return static_cast<int32_t>(x);
}


inline double FastI2D(int x)
{
    // There is no rounding involved in converting an integer to a
    // double, so this code should compile to a few instructions without
    // any FPU pipeline stalls.
    return static_cast<double>(x);
}

// ==============================================================================
// ===32-bit architecture========================================================
// ==============================================================================

#ifdef ESCARGOT_32

inline Value::Value(ForceUninitializedTag)
{
}

inline Value::Value()
{
    u.asBits.tag = UndefinedTag;
    u.asBits.payload = 0;
}

inline Value::Value(NullInitTag)
{
    u.asBits.tag = NullTag;
    u.asBits.payload = 0;
}

inline Value::Value(UndefinedInitTag)
{
    u.asBits.tag = UndefinedTag;
    u.asBits.payload = 0;
}

inline Value::Value(EmptyValueInitTag)
{
    u.asBits.tag = EmptyValueTag;
    u.asBits.payload = 0;
}

inline Value::Value(DeletedValueInitTag)
{
    u.asBits.tag = DeletedValueTag;
    u.asBits.payload = 0;
}

inline Value::Value(TrueInitTag)
{
    u.asBits.tag = BooleanTag;
    u.asBits.payload = 1;
}

inline Value::Value(FalseInitTag)
{
    u.asBits.tag = BooleanTag;
    u.asBits.payload = 0;
}

inline Value::Value(bool b)
{
    u.asBits.tag = BooleanTag;
    u.asBits.payload = b;
}

inline Value::Value(PointerValue* ptr)
{
    if (LIKELY(ptr != NULL)) {
        u.asBits.tag = PointerTag;
    } else {
        u.asBits.tag = EmptyValueTag;
    }
    u.asBits.payload = reinterpret_cast<int32_t>(ptr);
}

inline Value::Value(const PointerValue* ptr)
{
    if (LIKELY(ptr != NULL)) {
        u.asBits.tag = PointerTag;
    } else {
        u.asBits.tag = EmptyValueTag;
    }
    u.asBits.payload = reinterpret_cast<int32_t>(const_cast<PointerValue*>(ptr));
}

inline Value::Value(EncodeAsDoubleTag, double d)
{
    u.asDouble = d;
}

inline Value::Value(int i)
{
    u.asBits.tag = Int32Tag;
    u.asBits.payload = i;
}

inline Value::operator bool() const
{
    return u.asInt64;
}

inline bool Value::operator==(const Value& other) const
{
    return u.asInt64 == other.u.asInt64;
}

inline bool Value::operator!=(const Value& other) const
{
    return u.asInt64 != other.u.asInt64;
}

inline uint32_t Value::tag() const
{
    return u.asBits.tag;
}

inline int32_t Value::payload() const
{
    return u.asBits.payload;
}

ALWAYS_INLINE bool Value::isInt32() const
{
    return tag() == Int32Tag;
}

ALWAYS_INLINE bool Value::isDouble() const
{
    return tag() < LowestTag;
}

inline int32_t Value::asInt32() const
{
    ASSERT(isInt32());
    return u.asBits.payload;
}

inline bool Value::asBoolean() const
{
    ASSERT(isBoolean());
    return u.asBits.payload;
}

inline double Value::asDouble() const
{
    ASSERT(isDouble());
    return u.asDouble;
}

inline bool Value::isEmpty() const
{
    return tag() == EmptyValueTag;
}

inline bool Value::isDeleted() const
{
    return tag() == DeletedValueTag;
}

ALWAYS_INLINE bool Value::isNumber() const
{
    return isInt32() || isDouble();
}

inline bool Value::isPointerValue() const
{
    return tag() == PointerTag;
}

inline bool Value::isUndefined() const
{
    return tag() == UndefinedTag;
}

inline bool Value::isNull() const
{
    return tag() == NullTag;
}

inline bool Value::isBoolean() const
{
    return tag() == BooleanTag;
}

inline bool Value::isTrue() const
{
    return tag() == BooleanTag && asBoolean();
}

inline bool Value::isFalse() const
{
    return tag() == BooleanTag && !asBoolean();
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return reinterpret_cast<PointerValue*>(u.asBits.payload);
}

inline bool Value::isString() const
{
    return isPointerValue() && asPointerValue()->isString();
}

inline String* Value::asString() const
{
    ASSERT(isString());
    return asPointerValue()->asString();
}

#else

// ==============================================================================
// ===64-bit architecture========================================================
// ==============================================================================


inline Value::Value()
{
    u.asInt64 = ValueUndefined;
}

inline Value::Value(ForceUninitializedTag)
{
}

inline Value::Value(NullInitTag)
{
    u.asInt64 = ValueNull;
}

inline Value::Value(UndefinedInitTag)
{
    u.asInt64 = ValueUndefined;
}

inline Value::Value(EmptyValueInitTag)
{
    u.asInt64 = ValueEmpty;
}

inline Value::Value(DeletedValueInitTag)
{
    u.asInt64 = ValueDeleted;
}

inline Value::Value(TrueInitTag)
{
    u.asInt64 = ValueTrue;
}

inline Value::Value(FalseInitTag)
{
    u.asInt64 = ValueFalse;
}

inline Value::Value(bool b)
{
    u.asInt64 = (TagBitTypeOther | TagBitBool | b);
}

inline Value::Value(PointerValue* ptr)
{
    u.ptr = ptr;
}

inline Value::Value(const PointerValue* ptr)
{
    u.ptr = const_cast<PointerValue*>(ptr);
}

inline int64_t reinterpretDoubleToInt64(double value)
{
    return bitwise_cast<int64_t>(value);
}
inline double reinterpretInt64ToDouble(int64_t value)
{
    return bitwise_cast<double>(value);
}

inline Value::Value(EncodeAsDoubleTag, double d)
{
    u.asInt64 = reinterpretDoubleToInt64(d) + DoubleEncodeOffset;
}

inline Value::Value(int i)
{
    u.asInt64 = TagTypeNumber | static_cast<uint32_t>(i);
}

inline Value::operator bool() const
{
    ASSERT(u.asInt64 != ValueDeleted);
    return u.asInt64 != ValueEmpty;
}

inline bool Value::operator==(const Value& other) const
{
    return u.asInt64 == other.u.asInt64;
}

inline bool Value::operator!=(const Value& other) const
{
    return u.asInt64 != other.u.asInt64;
}

ALWAYS_INLINE bool Value::isInt32() const
{
#ifdef ESCARGOT_LITTLE_ENDIAN
    ASSERT(sizeof(short) == 2);
    unsigned short* firstByte = (unsigned short*)&u.asInt64;
    return firstByte[3] == 0xffff;
#else
    return (u.asInt64 & TagTypeNumber) == TagTypeNumber;
#endif
}

inline bool Value::isDouble() const
{
    return isNumber() && !isInt32();
}

inline int32_t Value::asInt32() const
{
    ASSERT(isInt32());
    return static_cast<int32_t>(u.asInt64);
}

inline bool Value::asBoolean() const
{
    ASSERT(isBoolean());
    return u.asInt64 == ValueTrue;
}

inline double Value::asDouble() const
{
    ASSERT(isDouble());
    return reinterpretInt64ToDouble(u.asInt64 - DoubleEncodeOffset);
}

inline bool Value::isEmpty() const
{
    return u.asInt64 == ValueEmpty;
}

inline bool Value::isDeleted() const
{
    return u.asInt64 == ValueDeleted;
}

ALWAYS_INLINE bool Value::isNumber() const
{
#ifdef ESCARGOT_LITTLE_ENDIAN
    ASSERT(sizeof(short) == 2);
    unsigned short* firstByte = (unsigned short*)&u.asInt64;
    return firstByte[3];
#else
    return u.asInt64 & TagTypeNumber;
#endif
}

inline bool Value::isString() const
{
    return isPointerValue() && asPointerValue()->isString();
}

inline String* Value::asString() const
{
    ASSERT(isString());
    return asPointerValue()->asString();
}

inline bool Value::isPointerValue() const
{
    return !(u.asInt64 & TagMask);
}

inline bool Value::isUndefined() const
{
    return u.asInt64 == ValueUndefined;
}

inline bool Value::isNull() const
{
    return u.asInt64 == ValueNull;
}

inline bool Value::isBoolean() const
{
    return u.asInt64 == ValueTrue || u.asInt64 == ValueFalse;
}

inline bool Value::isTrue() const
{
    return u.asInt64 == ValueTrue;
}

inline bool Value::isFalse() const
{
    return u.asInt64 == ValueFalse;
}

inline PointerValue* Value::asPointerValue() const
{
    ASSERT(isPointerValue());
    return u.ptr;
}

#endif

// ==============================================================================
// ===common architecture========================================================
// ==============================================================================

inline Value::Value(double d)
{
    const int32_t asInt32 = static_cast<int32_t>(d);
    if (asInt32 != d || (!asInt32 && std::signbit(d))) { // true for -0.0
        *this = Value(EncodeAsDouble, d);
        return;
    }
    *this = Value(static_cast<int32_t>(d));
}

inline Value::Value(char i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned char i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(short i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned short i)
{
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned i)
{
    if (static_cast<int32_t>(i) < 0) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(long i)
{
    if (static_cast<int32_t>(i) != i) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned long i)
{
    if (static_cast<uint32_t>(i) != i) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<uint32_t>(i));
}

inline Value::Value(long long i)
{
    if (static_cast<int32_t>(i) != i) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<int32_t>(i));
}

inline Value::Value(unsigned long long i)
{
    if (static_cast<uint32_t>(i) != i) {
        *this = Value(EncodeAsDouble, static_cast<double>(i));
        return;
    }
    *this = Value(static_cast<uint32_t>(i));
}

inline bool Value::isUInt32() const
{
    return isInt32() && asInt32() >= 0;
}

inline uint32_t Value::asUInt32() const
{
    ASSERT(isUInt32());
    return asInt32();
}

ALWAYS_INLINE double Value::asNumber() const
{
    ASSERT(isNumber());
    return isInt32() ? asInt32() : asDouble();
}

inline bool Value::isPrimitive() const
{
    // return isUndefined() || isNull() || isNumber() || isESString() || isBoolean();
    return !isPointerValue() || asPointerValue()->isString();
}


inline bool Value::isObject() const
{
    return isPointerValue() && asPointerValue()->isObject();
}

inline Object* Value::asObject() const
{
    return asPointerValue()->asObject();
}

inline bool Value::isFunction() const
{
    return isPointerValue() && asPointerValue()->isFunctionObject();
}

inline FunctionObject* Value::asFunction() const
{
    return asPointerValue()->asFunctionObject();
}

inline double Value::toNumber(ExecutionState& state) const
{
// http://www.ecma-international.org/ecma-262/6.0/#sec-tonumber
#ifdef ESCARGOT_64
    auto n = u.asInt64 & TagTypeNumber;
    if (LIKELY(n)) {
        if (n == TagTypeNumber) {
            return FastI2D(asInt32());
        } else {
            return asDouble();
        }
    }
#else
    if (LIKELY(isInt32()))
        return FastI2D(asInt32());
    else if (isDouble())
        return asDouble();
#endif
    else if (isUndefined())
        return std::numeric_limits<double>::quiet_NaN();
    else if (isNull())
        return 0;
    else if (isBoolean())
        return asBoolean() ? 1 : 0;
    else {
        return toNumberSlowCase(state);
    }
}

ALWAYS_INLINE Object* Value::toObject(ExecutionState& ec) const // $7.1.13 ToObject
{
    if (LIKELY(isObject())) {
        return asObject();
    } else {
        return toObjectSlowCase(ec);
    }
}

inline Value Value::toPrimitive(ExecutionState& ec, PrimitiveTypeHint preferredType) const // $7.1.1 ToPrimitive
{
    if (UNLIKELY(!isPrimitive())) {
        return toPrimitiveSlowCase(ec, preferredType);
    } else {
        return *this;
    }
}

inline bool Value::abstractEqualsTo(ExecutionState& state, const Value& val) const
{
    if (isInt32() && val.isInt32()) {
#ifdef ESCARGOT_64
        if (u.asInt64 == val.u.asInt64)
#else
        if (u.asBits.payload == val.u.asBits.payload)
#endif
            return true;
        return false;
    } else {
        return abstractEqualsToSlowCase(state, val);
    }
}

inline bool Value::toBoolean(ExecutionState& ec) const // $7.1.2 ToBoolean
{
#ifdef ESCARGOT_32
    if (LIKELY(isBoolean()))
        return payload();
#else
    if (*this == Value(true))
        return true;

    if (*this == Value(false))
        return false;
#endif

    if (isInt32())
        return asInt32();

    if (isDouble()) {
        double d = asDouble();
        if (std::isnan(d))
            return false;
        if (d == 0.0)
            return false;
        return true;
    }

    if (isUndefinedOrNull())
        return false;

    ASSERT(isPointerValue());
    if (asPointerValue()->isString())
        return asString()->length();
    return true;
}

inline int32_t Value::toInt32(ExecutionState& state) const // $7.1.5 ToInt3
{
    // consume fast case
    if (LIKELY(isInt32()))
        return asInt32();

    return toInt32SlowCase(state);
}

inline uint32_t Value::toUint32(ExecutionState& state) const // http://www.ecma-international.org/ecma-262/5.1/#sec-9.6
{
    return toInt32(state);
}

inline Value::ValueIndex Value::toIndex(ExecutionState& state) const // $7.1.15 ToLength
{
    int32_t i;
    if (LIKELY(isInt32()) && LIKELY((i = asInt32()) >= 0)) {
        return i;
    } else {
        auto newLen = toInteger(state);
        if (newLen != toNumber(state) || std::isinf(newLen)) {
            return Value::InvalidIndexValue;
        } else {
            return newLen;
        }
    }
}

inline uint64_t Value::toArrayIndex(ExecutionState& state) const
{
    int32_t i;
    if (LIKELY(isInt32()) && LIKELY((i = asInt32()) >= 0)) {
        return i;
    } else {
        uint32_t newLen = toUint32(state);
        if (newLen != toNumber(state)) {
            return Value::InvalidArrayIndexValue;
        } else {
            return newLen;
        }
    }
}

inline double Value::toInteger(ExecutionState& state) const
{
    if (isInt32())
        return asInt32();
    double d = toNumber(state);
    if (std::isnan(d))
        return 0;
    if (d == 0 || d == std::numeric_limits<double>::infinity() || d == -std::numeric_limits<double>::infinity())
        return d;
    return (d < 0 ? -1 : 1) * std::floor(std::abs(d));
}

inline double Value::toLength(ExecutionState& state) const
{
    double len = toInteger(state);
    if (len <= 0.0) {
        return 0.0;
    }
    if (len > 0 && std::isinf(len)) {
        return std::pow(2, 32) - 1;
    }
    return std::min(len, std::pow(2, 32) - 1);
}
}

#endif
