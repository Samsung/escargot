#ifndef __EscargotSmallValue__
#define __EscargotSmallValue__

#include "runtime/SmallValueData.h"
#include "runtime/Value.h"

namespace Escargot {

class PointerValue;


#ifdef ESCARGOT_32
COMPILE_ASSERT(sizeof(SmallValueData) == 4, "");
#else
COMPILE_ASSERT(sizeof(SmallValueData) == 8, "");
#endif

class DoubleInSmallValue : public PointerValue {
public:
    virtual Type type()
    {
        return DoubleInSmallValueType;
    }

    virtual bool isDoubleInSmallValue()
    {
        return true;
    }

    DoubleInSmallValue(double v)
        : m_value(v)
    {
    }

    double value()
    {
        return m_value;
    }

protected:
    double m_value;
};

// TODO add license comment these code.

namespace SmallValueImpl {

const int kApiAlignSize = 4;
const int kApiIntSize = sizeof(int);
const int kApiInt64Size = sizeof(int64_t);

// Tag information for HeapObject.
const int kHeapObjectTag = 0;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for Smi.
const int kSmiTag = 1;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t ptr_size>
struct SmiTagging;

template <int kSmiShiftSize>
inline int32_t IntToSmiT(int value)
{
    int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
    uintptr_t tagged_value = (static_cast<uintptr_t>(value) << smi_shift_bits) | kSmiTag;
    return (int32_t)(tagged_value);
}

// Smi constants for 32-bit systems.
template <>
struct SmiTagging<4> {
    enum {
        kSmiShiftSize = 0,
        kSmiValueSize = 31
    };
    static int SmiShiftSize()
    {
        return kSmiShiftSize;
    }
    static int SmiValueSize()
    {
        return kSmiValueSize;
    }
    inline static int SmiToInt(intptr_t value)
    {
        int shift_bits = kSmiTagSize + kSmiShiftSize;
        // Throw away top 32 bits and shift down (requires >> to be sign extending).
        return static_cast<int>(reinterpret_cast<intptr_t>(value) >> shift_bits);
    }
    inline static int32_t IntToSmi(int value)
    {
        return IntToSmiT<kSmiShiftSize>(value);
    }
    inline static bool IsValidSmi(intptr_t value)
    {
        // To be representable as an tagged small integer, the two
        // most-significant bits of 'value' must be either 00 or 11 due to
        // sign-extension. To check this we add 01 to the two
        // most-significant bits, and check if the most-significant bit is 0
        //
        // CAUTION: The original code below:
        // bool result = ((value + 0x40000000) & 0x80000000) == 0;
        // may lead to incorrect results according to the C language spec, and
        // in fact doesn't work correctly with gcc4.1.1 in some cases: The
        // compiler may produce undefined results in case of signed integer
        // overflow. The computation must be done w/ unsigned ints.
        return static_cast<uintptr_t>(value + 0x40000000U) < 0x80000000U;
    }
};


typedef SmiTagging<kApiAlignSize> PlatformSmiTagging;
const int kSmiShiftSize = PlatformSmiTagging::kSmiShiftSize;
const int kSmiValueSize = PlatformSmiTagging::kSmiValueSize;

#define HAS_SMI_TAG(value) \
    ((reinterpret_cast<intptr_t>(value) & ::Escargot::SmallValueImpl::kSmiTagMask) == ::Escargot::SmallValueImpl::kSmiTag)
#define HAS_OBJECT_TAG(value) \
    ((reinterpret_cast<intptr_t>(value) & ::Escargot::SmallValueImpl::kHeapObjectTagMask) == ::Escargot::SmallValueImpl::kHeapObjectTag)


extern SmallValueData* smallValueUndefined;
extern SmallValueData* smallValueNull;
extern SmallValueData* smallValueTrue;
extern SmallValueData* smallValueFalse;
extern SmallValueData* smallValueEmpty;
extern SmallValueData* smallValueDeleted;
}

class SmallValue {
public:
    SmallValue(const SmallValue& from)
    {
        m_data.payload = from.m_data.payload;
    }

    SmallValue(const uint32_t& from)
    {
        if (LIKELY(SmallValueImpl::PlatformSmiTagging::IsValidSmi(from))) {
            m_data.payload = SmallValueImpl::PlatformSmiTagging::IntToSmi(from);
        } else {
            fromValue(Value(from));
        }
    }

    SmallValue(const Value& from)
    {
        fromValue(from);
        ASSERT(m_data.payload);
    }

    operator Value()
    {
        if (HAS_OBJECT_TAG(m_data.payload)) {
            PointerValue* v = (PointerValue*)m_data.payload;
            if (UNLIKELY(v->isDoubleInSmallValue())) {
                return Value(v->asDoubleInSmallValue()->value());
            } else {
                if (v == (PointerValue*)SmallValueImpl::smallValueUndefined->payload) {
                    return Value();
                } else if (v == (PointerValue*)SmallValueImpl::smallValueNull->payload) {
                    return Value(Value::Null);
                } else if (v == (PointerValue*)SmallValueImpl::smallValueTrue->payload) {
                    return Value(Value::True);
                } else if (v == (PointerValue*)SmallValueImpl::smallValueFalse->payload) {
                    return Value(Value::False);
                } else if (v == (PointerValue*)SmallValueImpl::smallValueEmpty->payload) {
                    return Value(Value::EmptyValue);
                } else if (v == (PointerValue*)SmallValueImpl::smallValueDeleted->payload) {
                    return Value(Value::DeletedValue);
                }
                return Value(v);
            }
        } else {
            int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return Value(value);
        }
        return Value();
    }

    uint32_t toUint32(ExecutionState& state)
    {
        if (UNLIKELY(HAS_OBJECT_TAG(m_data.payload))) {
            return operator Escargot::Value().toUint32(state);
        } else {
            int32_t value = SmallValueImpl::PlatformSmiTagging::SmiToInt(m_data.payload);
            return (uint32_t)value;
        }
    }

protected:
    void fromValue(const Value& from)
    {
        if (from.isPointerValue()) {
            m_data.payload = (size_t)from.asPointerValue();
        } else {
            int32_t i32;
            if (from.isInt32() && SmallValueImpl::PlatformSmiTagging::IsValidSmi(i32 = from.asInt32())) {
                m_data.payload = SmallValueImpl::PlatformSmiTagging::IntToSmi(i32);
            } else if (from.isNumber()) {
                m_data.payload = reinterpret_cast<intptr_t>(new DoubleInSmallValue(from.asNumber()));
            } else if (from.isUndefined()) {
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueUndefined->payload);
            } else if (from.isTrue()) {
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueTrue->payload);
            } else if (from.isFalse()) {
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueFalse->payload);
            } else if (from.isNull()) {
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueNull->payload);
            } else if (from.isEmpty()) {
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueEmpty->payload);
            } else {
                ASSERT(from.isDeleted());
                m_data.payload = reinterpret_cast<intptr_t>(SmallValueImpl::smallValueDeleted->payload);
            }
        }
    }

    SmallValueData m_data;
};
}


#endif
