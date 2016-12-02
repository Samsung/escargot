#include "Escargot.h"
#include "FunctionObject.h"
#include "ExecutionContext.h"
#include "Context.h"
#include "parser/ScriptParser.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ErrorObject.h"


namespace Escargot {

void FunctionObject::initFunctionObject(ExecutionState& state)
{
    if (m_isConstructor) {
        m_structure = state.context()->defaultStructureForFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(Object::createFunctionPrototypeObject(state, this)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->functionParameters().size()));
    } else {
        m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionParameters().size()));
    }
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2, false)
    , m_isConstructor(false)
    , m_codeBlock(codeBlock)
{
    initFunctionObject(state);
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, bool isConstructor)
    : FunctionObject(state, new CodeBlock(state.context(), info), isConstructor)
{
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, bool isConstructor)
    : Object(state, isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_isConstructor(isConstructor)
    , m_codeBlock(codeBlock)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
}

Value FunctionObject::getFunctionPrototypeSlowCase(ExecutionState& state)
{
    return getOwnProperty(state, state.context()->staticStrings().prototype);
}

bool FunctionObject::setFunctionPrototypeSlowCase(ExecutionState& state, const Value& v)
{
    return defineOwnProperty(state, state.context()->staticStrings().prototype, ObjectPropertyDescriptorForDefineOwnProperty(v));
}

Value FunctionObject::call(ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression)
{
    Context* ctx = state.context();

    // prepare ByteCodeBlock if needed
    if (!m_codeBlock->byteCodeBlock()) {
        Node* ast;
        if (m_codeBlock->cachedASTNode()) {
            ast = m_codeBlock->cachedASTNode();
        } else {
            ast = state.context()->scriptParser().parseFunction(m_codeBlock);
        }

        ByteCodeGenerator g;
        g.generateByteCode(ctx, m_codeBlock, ast);
    }

    // prepare env, ec
    bool isStrict = m_codeBlock->isStrict();
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* env;
    ExecutionContext* ec;
    size_t stackStorageSize = m_codeBlock->identifierOnStackCount();
    if (m_codeBlock->canAllocateEnvironmentOnStack()) {
        // no capture, very simple case
        record = new(alloca(sizeof(LexicalEnvironment))) FunctionEnvironmentRecordOnStack(state, receiver, this, isNewExpression);
        if (LIKELY(state.executionContext() != nullptr)) {
            env = new(alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());
        } else {
            env = new(alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, nullptr);
        }
        ec = new(alloca(sizeof(ExecutionContext))) ExecutionContext(ctx, env, isStrict);
    } else {
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            record = new FunctionEnvironmentRecordOnHeap(state, receiver, this, isNewExpression);
        } else {
            record = new FunctionEnvironmentRecordNotIndexed(state, receiver, this, isNewExpression);
        }
        if (LIKELY(state.executionContext() != nullptr)) {
            env = new LexicalEnvironment(record, state.executionContext()->lexicalEnvironment());
        } else {
            env = new LexicalEnvironment(record, nullptr);
        }
        ec = new ExecutionContext(ctx, env, isStrict);
    }

    ec->giveStackStorage(ALLOCA(stackStorageSize * sizeof(Value), Value, state));
    Value* stackStorage = ec->stackStorage();
    for (size_t i = 0; i < stackStorageSize; i ++) {
        stackStorage[i] = Value();
    }
    Value resultValue;
    ExecutionState newState(ctx, ec, &resultValue);

    // TODO binding function name

    // prepare parameters
    const CodeBlock::FunctionParametersInfoVector& info = m_codeBlock->parametersInfomation();
    size_t parameterCopySize = std::min(argc, info.size());

    if (UNLIKELY(m_codeBlock->needsComplexParameterCopy())) {
        for (size_t i = 0; i < parameterCopySize; i ++) {
            if (info[i].m_isHeapAllocated) {
                record->setHeapValueByIndex(info[i].m_index, argv[i]);
            } else {
                stackStorage[info[i].m_index] = argv[i];
            }
        }
    } else {
        for (size_t i = 0; i < parameterCopySize; i ++) {
            stackStorage[i] = argv[i];
        }
    }

    // run function
    ByteCodeInterpreter::interpret(newState, m_codeBlock);

    return resultValue;
}

Value FunctionObject::call(const Value& callee, ExecutionState& state, const Value& receiver, const size_t& argc, Value* argv, bool isNewExpression)
{
    if (LIKELY(callee.isFunction())) {
        return callee.asFunction()->call(state, receiver, argc, argv, isNewExpression);
    } else {
        ErrorObject::throwBuiltinError(state, ErrorObject::TypeError, errorMessage_Call_NotFunction);
        RELEASE_ASSERT_NOT_REACHED();
    }
}

}
