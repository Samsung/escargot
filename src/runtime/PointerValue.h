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

    DoubleInSmallValue* asDoubleInSmallValue()
    {
        ASSERT(isDoubleInSmallValue());
        return (DoubleInSmallValue*)this;
    }
};

}

#endif
