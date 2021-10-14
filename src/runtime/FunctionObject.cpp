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

FunctionObject::FunctionSource FunctionObject::createFunctionSourceFromScriptSource(ExecutionState& state, AtomicString functionName, size_t argumentValueArrayCount, Value* argumentValueArray, Value bodyString, bool useStrict, bool isGenerator, bool isAsync, bool allowSuperCall, bool findSrcName)
{
    StringBuilder src, srcToTest;
    if (useStrict) {
        src.appendString("'use strict'; ");
    }

    if (isGenerator && isAsync) {
        src.appendString("async function* ");
        srcToTest.appendString("async function* ");
    } else if (isGenerator) {
        src.appendString("function* ");
        srcToTest.appendString("function* ");
    } else if (isAsync) {
        src.appendString("async function ");
        srcToTest.appendString("async function ");
    } else {
        src.appendString("function ");
        srcToTest.appendString("function ");
    }

    src.appendString(functionName.string());
    src.appendString("(");
    srcToTest.appendString(functionName.string());
    srcToTest.appendString("(");

    for (size_t i = 0; i < argumentValueArrayCount; i++) {
        String* p = argumentValueArray[i].toString(state);
        src.appendString(p);
        srcToTest.appendString(p);
        if (i != argumentValueArrayCount - 1) {
            src.appendString(",");
            srcToTest.appendString(",");

            for (size_t j = 0; j < p->length(); j++) {
                char16_t c = p->charAt(j);
                if (c == '}' || c == '{' || c == ')' || c == '(') {
                    ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
                }
            }
        }
    }

    GC_disable();

    try {
        srcToTest.appendString("\n) { }");
        String* cur = srcToTest.finalize(&state);
        esprima::parseProgram(state.context(), StringView(cur, 0, cur->length()), nullptr, false, false, false, SIZE_MAX, false, false, true, true);

        // reset ASTAllocator
        state.context()->astAllocator().reset();
        GC_enable();
    } catch (esprima::Error* orgError) {
        // reset ASTAllocator
        state.context()->astAllocator().reset();
        GC_enable();

        delete orgError;

        ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is a script parse error in parameter name");
    }

    String* source = bodyString.toString(state);

    auto data = source->bufferAccessData();

    for (size_t i = 0; i < data.length; i++) {
        char16_t c;
        if (data.has8BitContent) {
            c = ((LChar*)data.buffer)[i];
        } else {
            c = ((char16_t*)data.buffer)[i];
        }
        if (c == '{') {
            break;
        } else if (c == '}') {
            ErrorObject::throwBuiltinError(state, ErrorObject::SyntaxError, "there is unbalanced braces(}) in Function Constructor input");
        }
    }

    String* scriptSource;
#if defined(ENABLE_RELOADABLE_STRING)
    if (UNLIKELY(source->isReloadableString())) {
        src.appendString("\n) {\n");
        String* head = src.finalize(&state);

        bool is8Bit = head->has8BitContent() && source->has8BitContent();
        const char tail[] = "\n}";
        class ReloadableStringData : public gc {
        public:
            ReloadableStringData(String* head, ReloadableString* source)
                : m_head(head)
                , m_source(source)
                , m_dest(nullptr)
            {
            }

            String* m_head;
            ReloadableString* m_source;
            ReloadableString* m_dest;
        };

        ReloadableStringData* data = new ReloadableStringData(head, source->asReloadableString());

        scriptSource = new ReloadableString(state.context()->vmInstance(), is8Bit, head->length() + source->length() + sizeof(tail) - 1,
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
                ptr = fillDestBuffer(ptr, data->m_source, is8Bit);
                // unload original source immediately
                data->m_source->unload();

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
        src.appendString("\n) {\n");
        src.appendString(source);
        src.appendString("\n}");
        scriptSource = src.finalize(&state);
#if defined(ENABLE_RELOADABLE_STRING)
    }
#endif

    ScriptParser parser(state.context());
    String* srcName = String::emptyString;
    // find srcName through outer script
    if (findSrcName) {
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
