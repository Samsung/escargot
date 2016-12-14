#ifndef __EscargotPointerValue__
#define __EscargotPointerValue__

namespace Escargot {

class String;
class Object;
class FunctionObject;
class ArrayObject;
class StringObject;
class NumberObject;
class RegExpObject;
class DateObject;
class DoubleInSmallValue;

class PointerValue : public gc {
public:
    enum Type {
        StringType,
        ObjectType,
        FunctionObjectType,
        ArrayObjectType,
        DoubleInSmallValueType,
        DummyInSmallValueType,
    };

    virtual Type type() = 0;
    virtual bool isString()
    {
        return false;
    }

    virtual bool isObject()
    {
        return false;
    }

    virtual bool isFunctionObject()
    {
        return false;
    }

    virtual bool isArrayObject()
    {
        return false;
    }

    virtual bool isStringObject()
    {
        return false;
    }

    virtual bool isNumberObject()
    {
        return false;
    }

    virtual bool isDateObject()
    {
        return false;
    }

    virtual bool isRegExpObject()
    {
        return false;
    }

    virtual bool isDoubleInSmallValue()
    {
        return false;
    }

    virtual bool isDummyInSmallValueType()
    {
        return false;
    }

    String* asString()
    {
        ASSERT(isString());
        return (String*)this;
    }

    Object* asObject()
    {
        ASSERT(isObject());
        return (Object*)this;
    }

    FunctionObject* asFunctionObject()
    {
        ASSERT(isFunctionObject());
        return (FunctionObject*)this;
    }

    StringObject* asStringObject()
    {
        ASSERT(isStringObject());
        return (StringObject*)this;
    }

    NumberObject* asNumberObject()
    {
        ASSERT(isNumberObject());
        return (NumberObject*)this;
    }

    RegExpObject* asRegExpObject()
    {
        ASSERT(isRegExpObject());
        return (RegExpObject*)this;
    }

    DateObject* asDateObject()
    {
        ASSERT(isDateObject());
        return (DateObject*)this;
    }

    DoubleInSmallValue* asDoubleInSmallValue()
    {
        ASSERT(isDoubleInSmallValue());
        return (DoubleInSmallValue*)this;
    }
};
}

#endif
