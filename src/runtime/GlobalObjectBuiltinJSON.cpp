/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#include "Escargot.h"
#include "GlobalObject.h"
#include "Context.h"
#include "VMInstance.h"
#include "StringObject.h"
#include "ArrayObject.h"
#include "TypedArrayObject.h"
#include "BooleanObject.h"

#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag
#define RAPIDJSON_ERROR_CHARTYPE char
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/internal/dtoa.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

namespace Escargot {

template <typename Encoding>
struct JSONStringStream {
    typedef typename Encoding::Ch Ch;

    JSONStringStream(const Ch* src, size_t length)
        : src_(src)
        , head_(src)
        , tail_(src + length)
    {
    }

    Ch Peek() const
    {
        if (UNLIKELY(tail_ <= src_)) {
            return 0;
        }
        return *src_;
    }
    Ch Take()
    {
        if (UNLIKELY(tail_ <= src_)) {
            return 0;
        }
        return *src_++;
    }
    size_t Tell() const { return static_cast<size_t>(src_ - head_); }
    Ch* PutBegin()
    {
        RAPIDJSON_ASSERT(false);
        return 0;
    }
    void Put(Ch) { RAPIDJSON_ASSERT(false); }
    void Flush() { RAPIDJSON_ASSERT(false); }
    size_t PutEnd(Ch*)
    {
        RAPIDJSON_ASSERT(false);
        return 0;
    }

    const Ch* src_; //!< Current read position.
    const Ch* head_; //!< Original head of the string.
    const Ch* tail_;
};

template <typename CharType, typename JSONCharType>
static Value parseJSONWorker(ExecutionState& state, rapidjson::GenericValue<JSONCharType>& value)
{
    volatile int sp;
    size_t currentStackBase = (size_t)&sp;
#ifdef STACK_GROWS_DOWN
    if (UNLIKELY((state.stackBase() - currentStackBase) > STACK_LIMIT_FROM_BASE)) {
#else
    if (UNLIKELY((currentStackBase - state.stackBase()) > STACK_LIMIT_FROM_BASE)) {
#endif
        ErrorObject::throwBuiltinError(state, ErrorObject::RangeError, "Maximum call stack size exceeded");
    }

    if (value.IsBool()) {
        return Value(value.GetBool());
    } else if (value.IsInt()) {
        return Value(value.GetInt());
    } else if (value.IsUint()) {
        return Value(value.GetUint());
    } else if (value.IsInt64()) {
        return Value(value.GetInt64());
    } else if (value.IsUint64()) {
        return Value(value.GetUint64());
    } else if (value.IsDouble()) {
        return Value(value.GetDouble());
    } else if (value.IsNull()) {
        return Value(Value::Null);
    } else if (value.IsString()) {
        if (std::is_same<CharType, char16_t>::value) {
            const char16_t* chars = (const char16_t*)value.GetString();
            unsigned len = value.GetStringLength();
            if (isAllLatin1(chars, len)) {
                return new Char8String(chars, len);
            } else {
                return new UTF16String(chars, len);
            }
        } else {
            const char* valueAsString = (const char*)value.GetString();
            if (isAllASCII(valueAsString, strlen(valueAsString))) {
                return new Char8String(valueAsString);
            } else {
                return new UTF16String(utf8StringToUTF16String(valueAsString, strlen(valueAsString)));
            }
        }
    } else if (value.IsArray()) {
        ArrayObject* arr = new ArrayObject(state);
        size_t i = 0;
        auto iter = value.Begin();
        while (iter != value.End()) {
            arr->defineOwnProperty(state, ObjectPropertyName(state, Value(i++)), ObjectPropertyDescriptor(parseJSONWorker<CharType, JSONCharType>(state, *iter), ObjectPropertyDescriptor::AllPresent));
            iter++;
        }
        return arr;
    } else if (value.IsObject()) {
        Object* obj = new Object(state);
        auto iter = value.MemberBegin();
        while (iter != value.MemberEnd()) {
            Value propertyName = parseJSONWorker<CharType, JSONCharType>(state, iter->name);
            ASSERT(propertyName.isString());
            obj->defineOwnProperty(state, ObjectPropertyName(state, propertyName), ObjectPropertyDescriptor(parseJSONWorker<CharType, JSONCharType>(state, iter->value), ObjectPropertyDescriptor::AllPresent));
            iter++;
        }
        return obj;
    } else {
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template <typename CharType, typename JSONCharType>
Value parseJSON(ExecutionState& state, const CharType* data, size_t length)
{
    auto strings = &state.context()->staticStrings();
    rapidjson::GenericDocument<JSONCharType> jsonDocument;

    JSONStringStream<JSONCharType> stringStream(data, length);
    jsonDocument.ParseStream(stringStream);
    if (jsonDocument.HasParseError()) {
        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, strings->JSON.string(), true, strings->parse.string(), rapidjson::GetParseError_En(jsonDocument.GetParseError()));
    }

    return parseJSONWorker<CharType, JSONCharType>(state, jsonDocument);
}

String* codePointTo4digitString(int codepoint)
{
    StringBuilder ret;
    int d = 16 * 16 * 16;
    for (int i = 0; i < 4; ++i) {
        if (codepoint >= d) {
            char16_t c;
            if (codepoint / d < 10) {
                c = (codepoint / d) + '0';
            } else {
                c = (codepoint / d) - 10 + 'a';
            }
            codepoint %= d;
            ret.appendChar(c);
        } else {
            ret.appendChar('0');
        }
        d >>= 4;
    }

    return ret.finalize();
}

static Value builtinJSONParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();

    // 1, 2, 3
    String* JText = argv[0].toString(state);
    Value unfiltered;

    if (JText->has8BitContent()) {
        size_t len = JText->length();
        char16_t* char16Buf = new char16_t[len];
        std::unique_ptr<char16_t[]> buf(char16Buf);
        const LChar* srcBuf = JText->characters8();
        for (size_t i = 0; i < len; i++) {
            char16Buf[i] = srcBuf[i];
        }
        unfiltered = parseJSON<char16_t, rapidjson::UTF16<char16_t>>(state, buf.get(), JText->length());
    } else {
        unfiltered = parseJSON<char16_t, rapidjson::UTF16<char16_t>>(state, JText->characters16(), JText->length());
    }

    // 4
    Value reviver = argv[1];
    if (reviver.isObject() && reviver.isPointerValue() && reviver.asPointerValue()->isFunctionObject()) {
        Object* root = new Object(state);
        root->defineOwnProperty(state, ObjectPropertyName(state, String::emptyString), ObjectPropertyDescriptor(unfiltered, ObjectPropertyDescriptor::AllPresent));
        std::function<Value(Value, const ObjectPropertyName&)> Walk;
        Walk = [&](Value holder, const ObjectPropertyName& name) -> Value {
            Value val = holder.asPointerValue()->asObject()->get(state, name).value(state, holder);
            if (val.isObject()) {
                if (val.asObject()->isArrayObject()) {
                    ArrayObject* arrObject = val.asObject()->asArrayObject();
                    uint32_t i = 0;
                    uint32_t len = arrObject->length(state);
                    while (i < len) {
                        Value newElement = Walk(val, ObjectPropertyName(state, Value(i).toString(state)));
                        if (newElement.isUndefined()) {
                            arrObject->deleteOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)));
                        } else {
                            arrObject->defineOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)), ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                        i++;
                    }
                } else if (val.asObject()->isTypedArrayObject()) {
                    ArrayBufferView* arrObject = val.asObject()->asArrayBufferView();
                    uint32_t i = 0;
                    uint32_t len = arrObject->arraylength();
                    while (i < len) {
                        Value newElement = Walk(val, ObjectPropertyName(state, Value(i).toString(state)));
                        if (newElement.isUndefined()) {
                            arrObject->deleteOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)));
                        } else {
                            arrObject->defineOwnProperty(state, ObjectPropertyName(state, Value(i).toString(state)), ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                        i++;
                    }
                } else {
                    Object* object = val.asObject();
                    std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>> keys;
                    object->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
                        if (desc.isEnumerable()) {
                            std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>>* keys = (std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>>*)data;
                            keys->push_back(P);
                        }
                        return true;
                    },
                                        &keys);
                    for (auto key : keys) {
                        if (!object->hasOwnProperty(state, key))
                            continue;
                        Value newElement = Walk(val, key);
                        if (newElement.isUndefined()) {
                            object->deleteOwnProperty(state, key);
                        } else {
                            object->defineOwnProperty(state, key, ObjectPropertyDescriptor(newElement, ObjectPropertyDescriptor::AllPresent));
                        }
                    }
                }
            }
            Value arguments[] = { name.toPlainValue(state), val };
            return FunctionObject::call(state, reviver, holder, 2, arguments);
        };
        return Walk(root, ObjectPropertyName(state, String::emptyString));
    }

    // 5
    return unfiltered;
}

static Value builtinJSONStringify(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    auto strings = &state.context()->staticStrings();

    // 1, 2, 3
    Value value = argv[0];
    Value replacer = argv[1];
    Value space = argv[2];
    String* indent = new Char8String("");
    ValueVector stack;
    std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>> propertyList;
    bool propertyListTouched = false;

    // 4
    FunctionObject* replacerFunc = NULL;
    if (replacer.isObject()) {
        if (replacer.isFunction()) {
            replacerFunc = replacer.asFunction();
        } else if (replacer.asObject()->isArrayObject()) {
            propertyListTouched = true;
            ArrayObject* arrObject = replacer.asObject()->asArrayObject();

            std::vector<Value::ValueIndex> indexes;
            arrObject->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool { //Value key, HiddenClassPropertyInfo* propertyInfo) {
                Value::ValueIndex idx = P.toPlainValue(state).toIndex(state);
                if (idx != Value::InvalidIndexValue) {
                    std::vector<Value::ValueIndex>* indexes = (std::vector<Value::ValueIndex>*)data;
                    indexes->push_back(idx);
                }
                return true;
            },
                                   &indexes);
            std::sort(indexes.begin(), indexes.end(), std::less<Value::ValueIndex>());
            for (uint32_t i = 0; i < indexes.size(); ++i) {
                String* item = nullptr;
                Value property = arrObject->get(state, ObjectPropertyName(state, Value(indexes[i]))).value(state, arrObject);
                if (property.isString()) {
                    item = property.asString();
                } else if (property.isNumber()) {
                    item = property.toString(state);
                } else if (property.isObject() && (property.asPointerValue()->isStringObject() || property.asPointerValue()->isNumberObject())) {
                    item = property.toString(state);
                }
                if (item) {
                    bool flag = false;
                    for (size_t i = 0; i < propertyList.size(); i++) {
                        ObjectPropertyName& v = propertyList[i];
                        if (v.toPropertyName(state).equals(item)) {
                            flag = true;
                            break;
                        }
                    }
                    if (!flag)
                        propertyList.push_back(ObjectPropertyName(state, Value(item)));
                }
            }
        }
    }

    // 5
    if (space.isObject()) {
        if (space.isPointerValue() && space.asPointerValue()->isNumberObject()) {
            space = Value(space.toNumber(state));
        } else if (space.isPointerValue() && space.asPointerValue()->isStringObject()) {
            space = space.toString(state);
        }
    }

    // 6, 7, 8
    String* gap = String::emptyString;
    if (space.isNumber()) {
        int space_cnt = std::min(space.toInteger(state), 10.0);
        if (space_cnt >= 1) {
            UTF8StringData gapData;
            gapData.resizeWithUninitializedValues(space_cnt);
            for (int i = 0; i < space_cnt; i++) {
                gapData[i] = ' ';
            }
            gap = new Char8String(gapData.data(), gapData.length());
        }
    } else if (space.isString()) {
        if (space.asString()->length() <= 10) {
            gap = space.asString();
        } else {
            gap = space.asString()->substring(0, 10);
        }
    }

    std::function<Value(ObjectPropertyName key, Object * holder)> Str;
    std::function<String*(ArrayObject*)> JA;
    std::function<String*(Object*)> JO;
    std::function<String*(ObjectPropertyName value)> Quote;

    Str = [&](ObjectPropertyName key, Object* holder) -> Value {
        Value value = holder->get(state, key).value(state, holder);
        if (value.isObject()) {
            Object* valObj = value.asPointerValue()->asObject();
            Value toJson = valObj->get(state, ObjectPropertyName(state, strings->toJSON)).value(state, valObj);
            if (toJson.isPointerValue() && toJson.asPointerValue()->isFunctionObject()) {
                Value arguments[] = { key.toPlainValue(state) };
                value = FunctionObject::call(state, toJson, value, 1, arguments);
            }
        }

        if (replacerFunc != NULL) {
            Value arguments[] = { key.toPlainValue(state), value };
            value = FunctionObject::call(state, replacerFunc, holder, 2, arguments);
        }

        if (value.isObject()) {
            if (value.asObject()->isNumberObject()) {
                value = Value(value.toNumber(state));
            } else if (value.asObject()->isStringObject()) {
                value = Value(value.toString(state));
            } else if (value.asObject()->isBooleanObject()) {
                value = Value(value.asObject()->asBooleanObject()->primitiveValue());
            }
        }
        if (value.isNull()) {
            return strings->null.string();
        }
        if (value.isBoolean()) {
            return value.asBoolean() ? strings->stringTrue.string() : strings->stringFalse.string();
        }
        if (value.isString()) {
            return Value(Quote(ObjectPropertyName(state, value)));
        }
        if (value.isNumber()) {
            double d = value.toNumber(state);
            if (std::isfinite(d)) {
                return value.toString(state);
            }
            return strings->null.string();
        }
        if (value.isObject() && !value.isFunction()) {
            if (value.asObject()->isArrayObject()) {
                return JA(value.asObject()->asArrayObject());
            } else {
                return JO(value.asObject());
            }
        }

        return Value();
    };

    Quote = [&](ObjectPropertyName value) -> String* {
        String* str = value.toPropertyName(state).plainString();

        StringBuilder product;
        product.appendChar('"');
        for (size_t i = 0; i < str->length(); ++i) {
            char16_t c = str->charAt(i);

            if (c == u'\"' || c == u'\\') {
                product.appendChar('\\');
                product.appendChar(c);
            } else if (c == u'\b') {
                product.appendChar('\\');
                product.appendChar('b');
            } else if (c == u'\f') {
                product.appendChar('\\');
                product.appendChar('f');
            } else if (c == u'\n') {
                product.appendChar('\\');
                product.appendChar('n');
            } else if (c == u'\r') {
                product.appendChar('\\');
                product.appendChar('r');
            } else if (c == u'\t') {
                product.appendChar('\\');
                product.appendChar('t');
            } else if (c < u' ') {
                product.appendChar('\\');
                product.appendChar('u');
                product.appendString(codePointTo4digitString(c));
            } else {
                product.appendChar(c);
            }
        }
        product.appendChar('"');
        return product.finalize(&state);
    };

    JA = [&](ArrayObject* arrayObj) -> String* {
        // 1
        for (size_t i = 0; i < stack.size(); i++) {
            Value& v = stack[i];
            if (v == Value(arrayObj)) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->JSON.string(), false, strings->stringify.string(), errorMessage_GlobalObject_JAError);
            }
        }
        // 2
        stack.push_back(Value(arrayObj));
        // 3
        String* stepback = indent;
        // 4
        StringBuilder newIndent;
        newIndent.appendString(indent);
        newIndent.appendString(gap);
        indent = newIndent.finalize(&state);
        // 5
        std::vector<String*, gc_allocator_ignore_off_page<String*>> partial;
        // 6, 7
        uint32_t len = arrayObj->length(state);
        uint32_t index = 0;
        // 8
        while (index < len) {
            Value strP = Str(ObjectPropertyName(state, Value(index).toString(state)), arrayObj);
            if (strP.isUndefined()) {
                partial.push_back(strings->null.string());
            } else {
                partial.push_back(strP.asString());
            }
            index++;
        }
        // 9
        StringBuilder final;
        final.appendChar('[');
        if (partial.size() != 0) {
            if (gap->length() == 0) {
                for (size_t i = 0; i < partial.size(); ++i) {
                    final.appendString(partial[i]);
                    if (i < partial.size() - 1) {
                        final.appendChar(',');
                    }
                }
            } else {
                final.appendChar('\n');
                final.appendString(indent);
                StringBuilder seperatorBuilder;
                seperatorBuilder.appendChar(',');
                seperatorBuilder.appendChar('\n');
                seperatorBuilder.appendString(indent);
                String* seperator = seperatorBuilder.finalize(&state);
                for (size_t i = 0; i < partial.size(); ++i) {
                    final.appendString(partial[i]);
                    if (i < partial.size() - 1) {
                        final.appendString(seperator);
                    }
                }
                final.appendChar('\n');
                final.appendString(stepback);
            }
        }
        final.appendChar(']');

        // 11
        stack.pop_back();
        // 12
        indent = stepback;

        return final.finalize(&state);
    };

    JO = [&](Object* value) -> String* {
        // 1
        for (size_t i = 0; i < stack.size(); i++) {
            if (stack[i] == value) {
                ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, strings->JSON.string(), false, strings->stringify.string(), errorMessage_GlobalObject_JOError);
            }
        }
        // 2
        stack.push_back(Value(value));
        // 3
        String* stepback = indent;
        // 4
        StringBuilder newIndent;
        newIndent.appendString(indent);
        newIndent.appendString(gap);
        indent = newIndent.finalize(&state);
        // 5, 6
        std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>> k;
        if (propertyListTouched) {
            k = propertyList;
        } else {
            value->enumeration(state, [](ExecutionState& state, Object* self, const ObjectPropertyName& P, const ObjectStructurePropertyDescriptor& desc, void* data) -> bool {
                std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>>* k = (std::vector<ObjectPropertyName, GCUtil::gc_malloc_ignore_off_page_allocator<ObjectPropertyName>>*)data;
                if (desc.isEnumerable()) {
                    k->push_back(P);
                }
                return true;
            },
                               &k);
        }

        // 7
        std::vector<String*, gc_allocator_ignore_off_page<String*>> partial;
        // 8
        int len = k.size();
        for (int i = 0; i < len; ++i) {
            Value strP = Str(k[i], value);
            if (!strP.isUndefined()) {
                StringBuilder member;
                member.appendString(Quote(k[i]));
                member.appendChar(':');
                if (gap->length() != 0) {
                    member.appendChar(' ');
                }
                member.appendString(strP.toString(state));
                partial.push_back(member.finalize(&state));
            }
        }
        // 9
        StringBuilder final;
        final.appendChar('{');
        if (partial.size() != 0) {
            if (gap->length() == 0) {
                for (size_t i = 0; i < partial.size(); ++i) {
                    final.appendString(partial[i]);
                    if (i < partial.size() - 1) {
                        final.appendChar(',');
                    }
                }
            } else {
                final.appendChar('\n');
                final.appendString(indent);
                StringBuilder seperatorBuilder;
                seperatorBuilder.appendChar(',');
                seperatorBuilder.appendChar('\n');
                seperatorBuilder.appendString(indent);
                String* seperator = seperatorBuilder.finalize(&state);
                for (size_t i = 0; i < partial.size(); ++i) {
                    final.appendString(partial[i]);
                    if (i < partial.size() - 1) {
                        final.appendString(seperator);
                    }
                }
                final.appendChar('\n');
                final.appendString(stepback);
            }
        }
        final.appendChar('}');
        // 11
        stack.pop_back();
        // 12
        indent = stepback;

        return final.finalize(&state);
    };

    // 9
    Object* wrapper = new Object(state);
    // 10
    wrapper->defineOwnProperty(state, ObjectPropertyName(state, String::emptyString), ObjectPropertyDescriptor(value, ObjectPropertyDescriptor::AllPresent));
    return Str(ObjectPropertyName(state, Value(String::emptyString)), wrapper);
}

void GlobalObject::installJSON(ExecutionState& state)
{
    m_json = new Object(state);
    m_json->markThisObjectDontNeedStructureTransitionTable(state);
    m_json->defineOwnPropertyThrowsException(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                             ObjectPropertyDescriptor(Value(state.context()->staticStrings().JSON.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().JSON),
                      ObjectPropertyDescriptor(m_json, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_jsonParse = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().parse, builtinJSONParse, 2, nullptr, NativeFunctionInfo::Strict));
    m_json->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().parse),
                              ObjectPropertyDescriptor(m_jsonParse,
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_jsonStringify = new FunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringify, builtinJSONStringify, 3, nullptr, NativeFunctionInfo::Strict));
    m_json->defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringify),
                              ObjectPropertyDescriptor(m_jsonStringify,
                                                       (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
}
