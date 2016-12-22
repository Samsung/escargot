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
    , m_outerEnvironment(nullptr)
{
    initFunctionObject(state);
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, bool isConstructor)
    : FunctionObject(state, new CodeBlock(state.context(), info), nullptr, isConstructor)
{
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin)
    : FunctionObject(state, new CodeBlock(state.context(), info), nullptr, true)
{
    m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnv, bool isConstructor)
    : Object(state, isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_isConstructor(isConstructor)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(outerEnv)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
}

Value FunctionObject::call(ExecutionState& state, const Value& receiverOrg, const size_t& argc, Value* argv, bool isNewExpression)
{
    Value receiver = receiverOrg;
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

    if (!isStrict) {
        if (receiver.isUndefinedOrNull()) {
            receiver = ctx->globalObject();
        } else {
            receiver = receiver.toObject(state);
        }
    }
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* env;
    ExecutionContext* ec;
    size_t stackStorageSize = m_codeBlock->identifierOnStackCount();
    if (m_codeBlock->canAllocateEnvironmentOnStack()) {
        // no capture, very simple case
        record = new (alloca(sizeof(FunctionEnvironmentRecordOnStack))) FunctionEnvironmentRecordOnStack(state, receiver, this, argc, argv, isNewExpression);
        env = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, outerEnvironment());
        ec = new (alloca(sizeof(ExecutionContext))) ExecutionContext(ctx, state.executionContext(), env, isStrict);
    } else {
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            record = new FunctionEnvironmentRecordOnHeap(state, receiver, this, argc, argv, isNewExpression);
        } else {
            record = new FunctionEnvironmentRecordNotIndexed(state, receiver, this, argc, argv, isNewExpression);
        }
        env = new LexicalEnvironment(record, outerEnvironment());
        ec = new ExecutionContext(ctx, state.executionContext(), env, isStrict);
    }

    Value* stackStorage = ALLOCA(stackStorageSize * sizeof(Value), Value, state);
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }
    Value resultValue;
    ExecutionState newState(ctx, ec, &resultValue);

    // binding function name
    if (m_codeBlock->m_functionNameIndex != SIZE_MAX) {
        if (LIKELY(m_codeBlock->m_functionNameSaveInfo.m_isAllocatedOnStack)) {
            ASSERT(m_codeBlock->canUseIndexedVariableStorage());
            stackStorage[m_codeBlock->m_functionNameSaveInfo.m_index] = this;
        } else {
            if (m_codeBlock->canUseIndexedVariableStorage()) {
                record->setHeapValueByIndex(m_codeBlock->m_functionNameSaveInfo.m_index, this);
            } else {
                record->setMutableBinding(state, m_codeBlock->functionName(), this);
            }
        }
    }
    // prepare parameters
    const CodeBlock::FunctionParametersInfoVector& info = m_codeBlock->parametersInfomation();
    size_t parameterCopySize = std::min(argc, m_codeBlock->functionParameters().size());

    if (UNLIKELY(m_codeBlock->needsComplexParameterCopy())) {
        if (!m_codeBlock->canUseIndexedVariableStorage()) {
            for (size_t i = 0; i < parameterCopySize; i++) {
                env->record()->setMutableBinding(state, m_codeBlock->functionParameters()[i], argv[i]);
            }

        } else {
            for (size_t i = 0; i < parameterCopySize; i++) {
                if (info[i].m_isHeapAllocated) {
                    record->setHeapValueByIndex(info[i].m_index, argv[i]);
                } else {
                    stackStorage[info[i].m_index] = argv[i];
                }
            }
        }
    } else {
        for (size_t i = 0; i < parameterCopySize; i++) {
            stackStorage[i] = argv[i];
        }
    }

    // run function
    ByteCodeInterpreter::interpret(newState, m_codeBlock, 0, stackStorage);

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
