/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "runtime/Context.h"
#include "runtime/GlobalObject.h"
#include "runtime/JSON.h"
#include "runtime/RawJSONObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/VMInstance.h"

namespace Escargot {

static Value builtinJSONParse(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return JSON::parse(state, argv[0], argv[1]);
}

static Value builtinJSONStringify(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    return JSON::stringify(state, argv[0], argv[1], argv[2]);
}

// https://tc39.es/proposal-json-parse-with-source/#sec-json.rawjson
static Value builtinJSONRawJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 1. Let jsonString be ? ToString(text).
    String* jsonString = argv[0].toString(state);
    // 2. Throw a SyntaxError exception if jsonString is the empty String, or
    // if either the first or last code unit of jsonString is any of 0x0009 (CHARACTER TABULATION), 0x000A (LINE FEED), 0x000D (CARRIAGE RETURN), or 0x0020 (SPACE).
    // 3. Parse StringToCodePoints(jsonString) as a JSON text as specified in ECMA-404.
    // Throw a SyntaxError exception if it is not a valid JSON text as defined in that specification,
    // or if its outermost value is an object or array as defined in that specification.
    auto msg = "input value is not valid JSON text";
    if (jsonString->length() == 0) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, msg);
    }
    auto isWrongCodePoint = [](char32_t cp) -> bool {
        if (cp == 0x9 || cp == 0xa || cp == 0xd || cp == 0x20) {
            return true;
        }
        return false;
    };
    if (isWrongCodePoint(jsonString->codePointAt(0).codePoint) || isWrongCodePoint(jsonString->codePointAt(jsonString->length() - 1).codePoint)) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, msg);
    }

    Value jsonParseResult;
    try {
        jsonParseResult = JSON::parse(state, jsonString, Value());
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, msg);
    }
    if (jsonParseResult.isObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::SyntaxError, msg);
    }
    // 4. Let internalSlotsList be « [[IsRawJSON]] ».
    // 5. Let obj be OrdinaryObjectCreate(null, internalSlotsList).
    // 6. Perform ! CreateDataPropertyOrThrow(obj, "rawJSON", jsonString).
    // 7. Perform ! SetIntegrityLevel(obj, frozen).
    // 8. Return obj.
    return new RawJSONObject(state, jsonString);
}

// https://tc39.es/proposal-json-parse-with-source/#sec-json.israwjson
static Value builtinJSONIsRawJSON(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // 1. If Type(O) is Object and O has an [[IsRawJSON]] internal slot, return true.
    if (argv[0].isObject() && argv[0].asObject()->isRawJSONObject()) {
        return Value(true);
    }
    // 2. Return false.
    return Value(false);
}

void GlobalObject::initializeJSON(ExecutionState& state)
{
    ObjectPropertyNativeGetterSetterData* nativeData = new ObjectPropertyNativeGetterSetterData(true, false, true,
                                                                                                [](ExecutionState& state, Object* self, const Value& receiver, const EncodedValue& privateDataFromObjectPrivateArea) -> Value {
                                                                                                    ASSERT(self->isGlobalObject());
                                                                                                    return self->asGlobalObject()->json();
                                                                                                },
                                                                                                nullptr);

    defineNativeDataAccessorProperty(state, ObjectPropertyName(state.context()->staticStrings().JSON), nativeData, Value(Value::EmptyValue));
}

void GlobalObject::installJSON(ExecutionState& state)
{
    m_json = new Object(state);
    m_json->setGlobalIntrinsicObject(state);
    m_json->directDefineOwnProperty(state, ObjectPropertyName(state, Value(state.context()->vmInstance()->globalSymbols().toStringTag)),
                                    ObjectPropertyDescriptor(Value(state.context()->staticStrings().JSON.string()), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::ConfigurablePresent)));


    redefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().JSON),
                        ObjectPropertyDescriptor(m_json, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_jsonParse = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().parse, builtinJSONParse, 2, NativeFunctionInfo::Strict));
    m_json->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().parse),
                                    ObjectPropertyDescriptor(m_jsonParse,
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    m_jsonStringify = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().stringify, builtinJSONStringify, 3, NativeFunctionInfo::Strict));
    m_json->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().stringify),
                                    ObjectPropertyDescriptor(m_jsonStringify,
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto rawJSON = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().rawJSON, builtinJSONRawJSON, 1, NativeFunctionInfo::Strict));
    m_json->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().rawJSON),
                                    ObjectPropertyDescriptor(rawJSON,
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    auto isRawJSON = new NativeFunctionObject(state, NativeFunctionInfo(state.context()->staticStrings().isRawJSON, builtinJSONIsRawJSON, 1, NativeFunctionInfo::Strict));
    m_json->directDefineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().isRawJSON),
                                    ObjectPropertyDescriptor(isRawJSON,
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
}
} // namespace Escargot
