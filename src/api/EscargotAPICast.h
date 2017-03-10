#ifndef __ESCARGOT_APICAST__
#define __ESCARGOT_APICAST__

namespace Escargot {

inline VMInstance* toImpl(VMInstanceRef* v)
{
    return reinterpret_cast<VMInstance*>(v);
}

inline VMInstanceRef* toRef(VMInstance* v)
{
    return reinterpret_cast<VMInstanceRef*>(v);
}

inline Context* toImpl(ContextRef* v)
{
    return reinterpret_cast<Context*>(v);
}

inline ContextRef* toRef(Context* v)
{
    return reinterpret_cast<ContextRef*>(v);
}

inline ExecutionState* toImpl(ExecutionStateRef* v)
{
    return reinterpret_cast<ExecutionState*>(v);
}

inline ExecutionStateRef* toRef(ExecutionState* v)
{
    return reinterpret_cast<ExecutionStateRef*>(v);
}

inline String* toImpl(StringRef* v)
{
    return reinterpret_cast<String*>(v);
}

inline StringRef* toRef(String* v)
{
    return reinterpret_cast<StringRef*>(v);
}

inline Object* toImpl(ObjectRef* v)
{
    return reinterpret_cast<Object*>(v);
}

inline ObjectRef* toRef(Object* v)
{
    return reinterpret_cast<ObjectRef*>(v);
}

} // namespace Escargot
#endif
