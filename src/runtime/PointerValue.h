#ifndef __EscargotPointerValue__
#define __EscargotPointerValue__

namespace Escargot {

class String;
class Object;
class FunctionObject;
class ArrayObject;
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

    DoubleInSmallValue* asDoubleInSmallValue()
    {
        ASSERT(isDoubleInSmallValue());
        return (DoubleInSmallValue*)this;
    }
};

}

#endif
