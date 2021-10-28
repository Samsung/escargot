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
#include "FunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/Context.h"
#include "runtime/ReloadableString.h"
#include "interpreter/ByteCode.h"
#include "parser/ast/ProgramNode.h"
#include "parser/ScriptParser.h"
#include "parser/esprima_cpp/esprima.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"

namespace Escargot {

void FunctionObject::initStructureAndValues(ExecutionState& state, bool isConstructor, bool isGenerator, bool isAsync)
{
    if (isGenerator) {
        // Unlike function instances, the object that is the value of the GeneratorFunction’s of AsyncGeneratorFunction prototype property
        // does not have a constructor property whose value is the GeneratorFunction or the AsyncGeneratorFunction instance.
        m_structure = state.context()->defaultStructureForFunctionObject();
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
        ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2 < m_structure->propertyCount());
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionLength()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->functionName().string()));
    } else {
        if (isConstructor) {
            m_structure = state.context()->defaultStructureForFunctionObject();
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2 < m_structure->propertyCount());
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = ObjectPropertyValue::EmptyValue; // lazy init on VMInstance::functionPrototypeNativeGetter
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionLength()));
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->functionName().string()));
        } else {
            m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0 < m_structure->propertyCount());
            ASSERT(ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1 < m_structure->propertyCount());
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionLength()));
            m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
        }
    }
}

// function for derived classes. derived class MUST initlize member variable of FunctionObject.
FunctionObject::FunctionObject(ExecutionState& state, Object* proto, size_t defaultSpace)
    : Object(state, proto, defaultSpace)
    , m_codeBlock(nullptr)
{
}

FunctionObject::FunctionObject(ObjectStructure* structure, ObjectPropertyValueVector&& values, Object* proto)
    : Object(structure, std::move(values), proto)
    , m_codeBlock(nullptr)
{
}

FunctionObject::FunctionSource FunctionObject::createFunctionSourceFromScriptSource(ExecutionState& state, AtomicString functionName, size_t argCount, Value* argArray, Value bodyValue, bool useStrict, bool isGenerator, bool isAsync, bool allowSuperCall, bool isInternalSource)
{
    StringBuilder src, parameters;
    if (useStrict) {
        src.appendString("'use strict'; ");
    }

    if (isGenerator && isAsync) {
        src.appendString("async function* ");
    } else if (isGenerator) {
        src.appendString("function* ");
    } else if (isAsync) {
        src.appendString("async function ");
    } else {
        src.appendString("function ");
    }

    // function name
    src.appendString(functionName.string());

    {
        // function parameters
        parameters.appendString("(");
        for (size_t i = 0; i < argCount; i++) {
            String* p = argArray[i].toString(state);
            parameters.appendString(p);
            if (i != argCount - 1) {
                parameters.appendString(",");
            }
        }
        parameters.appendString("\n)");
    }

    String* parameterStr = parameters.finalize(&state);
    String* originBodyStr = bodyValue.toString(state);
    String* bodyStr = String::emptyString;

    {
        StringBuilder body;
        body.appendString(" {\n");
        body.appendString(originBodyStr);
        body.appendString("\n}");
        bodyStr = body.finalize(&state);

        // simple syntax check for dynamic generated function except internal source
        if (!isInternalSource) {
            GC_disable();
            try {
                esprima::simpleSyntaxCheckFunctionElements(state.context(), parameterStr, bodyStr, useStrict, isGenerator, isAsync);

                // reset ASTAllocator
                state.context()->astAllocator().reset();
                GC_enable();
            } catch (esprima::Error* syntaxError) {
                String* errorMessage = syntaxError->message;
                // reset ASTAllocator
                state.context()->astAllocator().reset();
                GC_enable();
                delete syntaxError;

                ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, errorMessage);
            }
        }
    }

    String* scriptSource;

#if defined(ENABLE_RELOADABLE_STRING)
    if (UNLIKELY(originBodyStr->isReloadableString())) {
        src.appendString(parameterStr);
        src.appendString(" {\n");

        String* headStr = src.finalize(&state);

        bool is8Bit = headStr->has8BitContent() && originBodyStr->has8BitContent();
        const char tail[] = "\n}";
        class ReloadableStringData : public gc {
        public:
            ReloadableStringData(String* head, ReloadableString* body)
                : m_head(head)
                , m_body(body)
                , m_dest(nullptr)
            {
            }

            String* m_head;
            ReloadableString* m_body;
            ReloadableString* m_dest;
        };

        ReloadableStringData* data = new ReloadableStringData(headStr, originBodyStr->asReloadableString());

        scriptSource = new ReloadableString(state.context()->vmInstance(), is8Bit, headStr->length() + originBodyStr->length() + sizeof(tail) - 1,
                                            data, [](void* callbackData) -> void* {
                ReloadableStringData* data = reinterpret_cast<ReloadableStringData*>(callbackData);
                bool is8Bit = data->m_dest->has8BitContent();

                char* dest = reinterpret_cast<char*>(malloc((data->m_dest->length() + 1) * (is8Bit ? 1 : 2)));
                char* ptr = dest;

                auto fillDestBuffer = [](char* ptr, String* src, bool is8Bit) -> char* {
                    auto bad = src->bufferAccessData();
                    if (is8Bit) {
                        for (size_t i = 0; i < bad.length; i ++) {
                            ptr[i] = bad.charAt(i);
                        }
                        ptr += bad.length;
                    } else {
                        char16_t* ptrAs16 = (char16_t*)ptr;
                        for (size_t i = 0; i < bad.length; i++) {
                            ptrAs16[i] = bad.charAt(i);
                        }
                        ptr += (bad.length * 2);
                    }
                    return ptr;
                };

                ptr = fillDestBuffer(ptr, data->m_head, is8Bit);
                ptr = fillDestBuffer(ptr, data->m_body, is8Bit);
                // unload original body source immediately
                data->m_body->unload();

                const char tail[] = "\n}";
                size_t tailLength = sizeof(tail) - 1;
                if (is8Bit) {
                    for (size_t i = 0; i < tailLength; i ++) {
                        ptr[i] = tail[i];
                    }
                    ptr += tailLength;
                } else {
                    char16_t* ptrAs16 = (char16_t*)ptr;
                    for (size_t i = 0; i < tailLength; i ++) {
                        ptrAs16[i] = tail[i];
                    }
                    ptr += (tailLength * 2);
                }

                *ptr = 0;
                if (!is8Bit) {
                    ptr++;
                    *ptr = 0;
                }

                return dest; }, [](void* memoryPtr, void* callbackData) { free(memoryPtr); });

        data->m_dest = scriptSource->asReloadableString();

    } else {
#endif
        src.appendString(parameterStr);
        src.appendString(bodyStr);
        scriptSource = src.finalize(&state);
#if defined(ENABLE_RELOADABLE_STRING)
    }
#endif

    ScriptParser parser(state.context());
    String* srcName = String::emptyString;
    // find srcName through outer script except internal source
    if (!isInternalSource) {
        auto script = state.resolveOuterScript();
        if (script) {
            srcName = script->srcName();
        }
    }

    Script* script = parser.initializeScript(scriptSource, srcName, nullptr, false, false, false, false, false, allowSuperCall, false, true, false).scriptThrowsExceptionIfParseError(state);
    InterpretedCodeBlock* cb = script->topCodeBlock()->childBlockAt(0);
    LexicalEnvironment* globalEnvironment = new LexicalEnvironment(new GlobalEnvironmentRecord(state, script->topCodeBlock(), state.context()->globalObject(), state.context()->globalDeclarativeRecord(), state.context()->globalDeclarativeStorage()), nullptr);

    FunctionObject::FunctionSource fs;
    fs.script = script;
    fs.codeBlock = cb;
    fs.outerEnvironment = globalEnvironment;

    return fs;
}
} // namespace Escargot
