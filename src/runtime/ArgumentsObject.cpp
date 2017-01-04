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

ArgumentsObject::ArgumentsObject(ExecutionState& state, FunctionEnvironmentRecord* record, ExecutionContext* ec)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER, true)
{
    m_structure = structure()->convertToWithFastAccess(state);

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
    while (indx >= 0) {
        // Let val be the element of args at 0-origined list position indx.
        Value val = record->argv()[indx];
        // Call the [[DefineOwnProperty]] internal method on obj passing ToString(indx), the property descriptor {[[Value]]: val, [[Writable]]: true, [[Enumerable]]: true, [[Configurable]]: true}, and false as arguments.
        obj->defineOwnProperty(state, ObjectPropertyName(state, Value(indx)), ObjectPropertyDescriptor(val, ObjectPropertyDescriptor::AllPresent));

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
                auto gsData = new ObjectPropertyNativeGetterSetterData(true, true, true, ArgumentsObjectNativeGetter, ArgumentsObjectNativeSetter, true);
                obj->deleteOwnProperty(state, ObjectPropertyName(state, Value(indx)));
                obj->defineNativeGetterSetterDataProperty(state, ObjectPropertyName(state, Value(indx)), gsData, data);
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
    return Object::getOwnProperty(state, P);
}

bool ArgumentsObject::defineOwnProperty(ExecutionState& state, const ObjectPropertyName& P, const ObjectPropertyDescriptor& desc) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    return Object::defineOwnProperty(state, P, desc);
}

bool ArgumentsObject::deleteOwnProperty(ExecutionState& state, const ObjectPropertyName& P) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    return Object::deleteOwnProperty(state, P);
}

void ArgumentsObject::enumeration(ExecutionState& state, std::function<bool(const ObjectPropertyName&, const ObjectStructurePropertyDescriptor& desc)> callback) ESCARGOT_OBJECT_SUBCLASS_MUST_REDEFINE
{
    Object::enumeration(state, callback);
}
}
