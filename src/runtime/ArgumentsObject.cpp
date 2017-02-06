#include "Escargot.h"
#include "ArgumentsObject.h"
#include "Context.h"
#include "EnvironmentRecord.h"
#include "Environment.h"

namespace Escargot {

struct ArgumentsObjectArgData : public PointerValue {
    FunctionEnvironmentRecord* m_targetRecord;
    CodeBlock* m_codeBlock;
    AtomicString m_name;

    ArgumentsObjectArgData(FunctionEnvironmentRecord* targetRecord, CodeBlock* codeBlock, AtomicString name)
        : m_targetRecord(targetRecord)
        , m_codeBlock(codeBlock)
        , m_name(name)
    {
    }

    virtual Type type()
    {
        return ExtraDataType;
    }
};

static Value ArgumentsObjectNativeGetter(ExecutionState& state, Object* self, const Value& objectInternalData)
{
    ArgumentsObjectArgData* data = (ArgumentsObjectArgData*)objectInternalData.asPointerValue();
    CodeBlock::IdentifierInfo info = data->m_codeBlock->identifierInfos()[data->m_codeBlock->findName(data->m_name)];
    ASSERT(!info.m_needToAllocateOnStack);
    return data->m_targetRecord->getHeapValueByIndex(info.m_indexForIndexedStorage);
}

static bool ArgumentsObjectNativeSetter(ExecutionState& state, Object* self, const Value& setterInputData, Value& objectInternalData)
{
    ArgumentsObjectArgData* data = (ArgumentsObjectArgData*)objectInternalData.asPointerValue();
    CodeBlock::IdentifierInfo info = data->m_codeBlock->identifierInfos()[data->m_codeBlock->findName(data->m_name)];
    ASSERT(!info.m_needToAllocateOnStack);
    data->m_targetRecord->setHeapValueByIndex(info.m_indexForIndexedStorage, setterInputData);
    return true;
}

void* ArgumentsObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(ArgumentsObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(ArgumentsObject, m_argumentPropertyInfo));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(ArgumentsObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

ArgumentsObject::ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
{
    // http://www.ecma-international.org/ecma-262/5.1/#sec-10.6
    // Let len be the number of elements in args.
    size_t len = record->argc();
    // Let obj be the result of creating a new ECMAScript object.
    // Set all the internal methods of obj as specified in 8.12.
    // Set the [[Class]] internal property of obj to "Arguments".
    // Let Object be the standard built-in Object constructor (15.2.2).
    // Set the [[Prototype]] internal property of obj to the standard built-in Object prototype object (15.2.4).
    Object* obj = this;

    // Call the [[DefineOwnProperty]] internal method on obj passing "length", the Property Descriptor {[[Value]]: len, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}, and false as arguments.
    obj->defineOwnProperty(state, state.context()->staticStrings().length, ObjectPropertyDescriptor(Value(len), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    CodeBlock* blk = record->functionObject()->codeBlock();
    bool isStrict = blk->isStrict();
    // Let map be the result of creating a new object as if by the expression new Object() where Object is the standard built-in constructor with that name
    // Let mappedNames be an empty List.
    std::vector<AtomicString> mappedNames;
    // Let indx = len - 1.
    int64_t indx = ((int64_t)len - 1);
    // Repeat while indx >= 0,

    m_argumentPropertyInfo.resizeWithUninitializedValues(len);

    while (indx >= 0) {
        // Let val be the element of args at 0-origined list position indx.
        Value val = record->argv()[indx];
        // Call the [[DefineOwnProperty]] internal method on obj passing ToString(indx), the property descriptor {[[Value]]: val, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false as arguments.
        m_argumentPropertyInfo[indx].first = val;
        m_argumentPropertyInfo[indx].second = ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent);

        // If indx is less than the number of elements in names, then
        if ((size_t)indx < blk->parametersInfomation().size()) {
            // Let name be the element of names at 0-origined list position indx.
            AtomicString name = blk->parametersInfomation()[indx].m_name;
            // If strict is false and name is not an element of mappedNames, then
            if (!isStrict && std::find(mappedNames.begin(), mappedNames.end(), name) == mappedNames.end()) {
                // Add name as an element of the list mappedNames.
                mappedNames.push_back(name);
                // Let g be the result of calling the MakeArgGetter abstract operation with arguments name and env.
                // Let p be the result of calling the MakeArgSetter abstract operation with arguments name and env.
                // Set the [[ParameterMap]] internal property of obj to map.
                // Set the [[Get]], [[GetOwnProperty]], [[DefineOwnProperty]], and [[Delete]] internal methods of obj to the definitions provided below.
                auto data = new ArgumentsObjectArgData(record, blk, name);
                // Call the [[DefineOwnProperty]] internal method of map passing ToString(indx), the Property Descriptor {[[Set]]: p, [[Get]]: g, [[Configurable]]: true}, and false as arguments.
                auto gsData = new ObjectPropertyNativeGetterSetterData(true, true, true, ArgumentsObjectNativeGetter, ArgumentsObjectNativeSetter);
                m_argumentPropertyInfo[indx].first = Value(data);
                m_argumentPropertyInfo[indx].second = ObjectStructurePropertyDescriptor::createDataButHasNativeGetterSetterDescriptor(gsData);
            }
        }
        // Let indx = indx - 1
        indx--;
    }

    // If strict is false, then
    if (!isStrict) {
        // Call the [[DefineOwnProperty]] internal method on obj passing "callee", the property descriptor {[[Value]]: func, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}, and false as arguments.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().callee), ObjectPropertyDescriptor(record->functionObject(), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

        Value caller;
        ExecutionContext* pec = ec->parent();
        while (pec) {
            if (pec->lexicalEnvironment()->record()->isDeclarativeEnvironmentRecord() && pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                caller = pec->lexicalEnvironment()->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject();
                break;
            }
            pec = pec->parent();
        }
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), ObjectPropertyDescriptor(caller, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
    } else {
        // Else, strict is true so
        // Let thrower be the [[ThrowTypeError]] function Object (13.2.3).
        FunctionObject* thrower = state.context()->globalObject()->throwTypeError();
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "callee", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().callee), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
        // Call the [[DefineOwnProperty]] internal method of obj with arguments "caller", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        obj->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    }
}

ObjectGetResult ArgumentsObject::getOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx;
    if (LIKELY(P.isUIntType())) {
        idx = P.uintValue();
    } else {
        idx = P.string(state)->tryToUseAsIndex();
    }
    if (LIKELY(idx != Value::InvalidIndexValue)) {
        if (idx < m_argumentPropertyInfo.size()) {
            Value val = m_argumentPropertyInfo[idx].first;
            if (!val.isEmpty()) {
                if (m_argumentPropertyInfo[idx].second.isNativeAccessorProperty()) {
                    return ObjectGetResult(ArgumentsObjectNativeGetter(state, this, val), true, true, true);
                } else {
                    return ObjectGetResult(val, true, true, true);
                }
            }
        }
    }
    return Object::getOwnProperty(state, P);
}

bool ArgumentsObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx;
    if (LIKELY(P.isUIntType())) {
        idx = P.uintValue();
    } else {
        idx = P.string(state)->tryToUseAsIndex();
    }
    if (LIKELY(idx != Value::InvalidIndexValue)) {
        if (idx < m_argumentPropertyInfo.size()) {
            Value val = m_argumentPropertyInfo[idx].first;
            if (!val.isEmpty()) {
                if (desc.isDataWritableEnumerableConfigurable()) {
                    if (m_argumentPropertyInfo[idx].second.isNativeAccessorProperty()) {
                        ArgumentsObjectNativeSetter(state, this, desc.value(), val);
                        return true;
                    } else {
                        m_argumentPropertyInfo[idx].first = desc.value();
                        return true;
                    }
                } else {
                    if (m_argumentPropertyInfo[idx].second.isNativeAccessorProperty() && desc.isDataDescriptor()) {
                        ArgumentsObjectNativeSetter(state, this, desc.value(), val);
                    }
                    m_argumentPropertyInfo[idx].first = Value(Value::EmptyValue);
                    ObjectPropertyDescriptor newDesc(desc);
                    newDesc.setWritable(true);
                    newDesc.setConfigurable(true);
                    newDesc.setEnumerable(true);

                    if (desc.isWritablePresent()) {
                        newDesc.setWritable(desc.isWritable());
                    }
                    if (desc.isEnumerablePresent()) {
                        newDesc.setEnumerable(desc.isEnumerable());
                    }
                    if (desc.isConfigurablePresent()) {
                        newDesc.setConfigurable(desc.isConfigurable());
                    }

                    return Object::defineOwnProperty(state, P, newDesc);
                }
            }
        }
    }
    return Object::defineOwnProperty(state, P, desc);
}

bool ArgumentsObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    uint64_t idx;
    if (LIKELY(P.isUIntType())) {
        idx = P.uintValue();
    } else {
        idx = P.string(state)->tryToUseAsIndex();
    }
    if (LIKELY(idx != Value::InvalidIndexValue)) {
        if (idx < m_argumentPropertyInfo.size()) {
            Value val = m_argumentPropertyInfo[idx].first;
            if (!val.isEmpty()) {
                m_argumentPropertyInfo[idx].first = Value(Value::EmptyValue);
                return true;
            }
        }
    }
    return Object::deleteOwnProperty(state, P);
}

void ArgumentsObject::enumeration(ExecutionState& state, bool (*callback)(ExecutionState& state, Object* self, const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc, void* data), void* data)
{
    for (size_t i = 0; i < m_argumentPropertyInfo.size(); i++) {
        Value v = m_argumentPropertyInfo[i].first;
        if (!v.isEmpty()) {
            if (!callback(state, this, ObjectPropertyName(state, Value(i)), ObjectStructurePropertyDescriptor::createDataDescriptor(ObjectStructurePropertyDescriptor::AllPresent), data)) {
                return;
            }
        }
    }
    Object::enumeration(state, callback, data);
}
}
