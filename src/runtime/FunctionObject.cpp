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
#include "util/Util.h"

namespace Escargot {

size_t g_functionObjectTag;

void FunctionObject::initFunctionObject(ExecutionState& state)
{
    if (isConstructor()) {
        m_structure = state.context()->defaultStructureForFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(Object::createFunctionPrototypeObject(state, this)));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2] = (Value(m_codeBlock->parametersInfomation().size()));
    } else {
        m_structure = state.context()->defaultStructureForNotConstructorFunctionObject();
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 0] = (Value(m_codeBlock->functionName().string()));
        m_values[ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 1] = (Value(m_codeBlock->parametersInfomation().size()));
    }

    // If Strict is true, then
    if (m_codeBlock && m_codeBlock->isStrict() && !m_codeBlock->isNativeFunction()) {
        // Let thrower be the [[ThrowTypeError]] function Object (13.2.3).
        FunctionObject* thrower = state.context()->globalObject()->throwTypeError();
        // Call the [[DefineOwnProperty]] internal method of F with arguments "caller", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().caller), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
        // Call the [[DefineOwnProperty]] internal method of F with arguments "arguments", PropertyDescriptor {[[Get]]: thrower, [[Set]]: thrower, [[Enumerable]]: false, [[Configurable]]: false}, and false.
        defineOwnProperty(state, ObjectPropertyName(state.context()->staticStrings().arguments), ObjectPropertyDescriptor(JSGetterSetter(thrower, thrower), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::NonEnumerablePresent | ObjectPropertyDescriptor::NonConfigurablePresent)));
    }
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, ForBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2, false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(nullptr)
{
    ASSERT(!isConstructor());
    initFunctionObject(state);
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info)
    : Object(state, info.m_isConsturctor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3, false)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
{
    ASSERT(isConstructor());
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
    m_structure = state.context()->defaultStructureForBuiltinFunctionObject();
}

FunctionObject::FunctionObject(ExecutionState& state, CodeBlock* codeBlock, LexicalEnvironment* outerEnv)
    : Object(state, codeBlock->isConsturctor() ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(outerEnv)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
}

NEVER_INLINE void FunctionObject::generateBytecodeBlock(ExecutionState& state)
{
    Vector<CodeBlock*, gc_allocator<CodeBlock*>>& v = state.context()->compiledCodeBlocks();

    size_t currentCodeSizeTotal = 0;
    for (size_t i = 0; i < v.size(); i++) {
        currentCodeSizeTotal += v[i]->m_byteCodeBlock->m_code.size();
        currentCodeSizeTotal += (v[i]->m_byteCodeBlock->m_locData.size() * sizeof(std::pair<size_t, size_t>));
        currentCodeSizeTotal += v[i]->m_byteCodeBlock->m_literalData.size() * sizeof(size_t);
        currentCodeSizeTotal += v[i]->m_byteCodeBlock->m_objectStructuresInUse.size() * sizeof(size_t);
        currentCodeSizeTotal += v[i]->m_byteCodeBlock->m_getObjectCodePositions.size() * sizeof(size_t);
    }
    // ESCARGOT_LOG_INFO("codeSizeTotal %lfMB\n", (int)currentCodeSizeTotal / 1024.0 / 1024.0);

    const size_t codeSizeMax = 1024 * 1024 * 2;
    if (currentCodeSizeTotal > codeSizeMax) {
        std::vector<CodeBlock*, gc_allocator<CodeBlock*>> codeBlocksInCurrentStack;

        ExecutionContext* ec = state.executionContext();
        while (ec) {
            auto env = ec->lexicalEnvironment();
            if (env->record()->isDeclarativeEnvironmentRecord() && env->record()->asDeclarativeEnvironmentRecord()->isFunctionEnvironmentRecord()) {
                auto cblk = env->record()->asDeclarativeEnvironmentRecord()->asFunctionEnvironmentRecord()->functionObject()->codeBlock();
                if (cblk->script() && cblk->byteCodeBlock()) {
                    if (std::find(codeBlocksInCurrentStack.begin(), codeBlocksInCurrentStack.end(), cblk) == codeBlocksInCurrentStack.end()) {
                        codeBlocksInCurrentStack.push_back(cblk);
                    }
                }
            }
            ec = ec->parent();
        }

        for (size_t i = 0; i < v.size(); i++) {
            if (std::find(codeBlocksInCurrentStack.begin(), codeBlocksInCurrentStack.end(), v[i]) == codeBlocksInCurrentStack.end()) {
                v[i]->m_byteCodeBlock = nullptr;
            }
        }

        v.clear();
        v.resizeWithUninitializedValues(codeBlocksInCurrentStack.size());

        for (size_t i = 0; i < codeBlocksInCurrentStack.size(); i++) {
            v[i] = codeBlocksInCurrentStack[i];
            if (v[i]->m_byteCodeBlock) {
                v[i]->m_byteCodeBlock->m_locData.clear();
            }
        }
    }
    ASSERT(!m_codeBlock->isNativeFunction());

    auto ret = state.context()->scriptParser().parseFunction(m_codeBlock);
    Node* ast = ret.first;

    ByteCodeGenerator g;
    m_codeBlock->m_byteCodeBlock = g.generateByteCode(state.context(), m_codeBlock, ast, ret.second, false, false, false);

    v.pushBack(m_codeBlock);

    if (m_codeBlock->cachedASTNode()) {
        m_codeBlock->m_cachedASTNode = nullptr;
        delete ast;
    } else {
        delete ast;
    }
}

Value FunctionObject::call(ExecutionState& state, const Value& receiverSrc, const size_t& argc, Value* argv, bool isNewExpression)
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

    Context* ctx = state.context();
    bool isStrict = m_codeBlock->isStrict();

    if (m_codeBlock->hasCallNativeFunctionCode()) {
        const CodeBlock::FunctionParametersInfoVector& info = m_codeBlock->parametersInfomation();
        CallNativeFunction* code = (CallNativeFunction*)&m_codeBlock->byteCodeBlock()->m_code[0];
        FunctionEnvironmentRecordOnStack record(state, this, argc, argv);
        LexicalEnvironment env(&record, outerEnvironment());
        ExecutionContext ec(ctx, state.executionContext(), &env, isStrict);
        ExecutionState newState(&state, &ec);

        size_t len = info.size();
        if (argc < len) {
            Value* newArgv = (Value*)alloca(sizeof(Value) * len);
            for (size_t i = 0; i < argc; i++) {
                newArgv[i] = argv[i];
            }
            for (size_t i = argc; i < len; i++) {
                newArgv[i] = Value();
            }
            argv = newArgv;
        }

        Value receiver = receiverSrc;
        // prepare receiver
        if (UNLIKELY(!isStrict)) {
            if (receiver.isUndefinedOrNull()) {
                receiver = ctx->globalObject();
            } else {
                receiver = receiver.toObject(state);
            }
        }

        try {
            return code->m_fn(newState, receiver, argc, argv, isNewExpression);
        } catch (const Value& v) {
            ByteCodeInterpreter::processException(newState, v, &ec, SIZE_MAX);
        }
    }

    // prepare ByteCodeBlock if needed
    if (UNLIKELY(m_codeBlock->byteCodeBlock() == nullptr)) {
        generateBytecodeBlock(state);
    }

    ByteCodeBlock* blk = m_codeBlock->byteCodeBlock();

    size_t registerSize = blk->m_requiredRegisterFileSizeInValueSize;
    size_t stackStorageSize = m_codeBlock->identifierOnStackCount();
    size_t literalStorageSize = blk->m_numeralLiteralData.size();
    Value* literalStorageSrc = blk->m_numeralLiteralData.data();
    size_t parameterCopySize = std::min(argc, m_codeBlock->functionParameters().size());

    // prepare env, ec
    FunctionEnvironmentRecord* record;
    LexicalEnvironment* env;
    ExecutionContext* ec;
    if (LIKELY(m_codeBlock->canAllocateEnvironmentOnStack())) {
        // no capture, very simple case
        record = new (alloca(sizeof(FunctionEnvironmentRecordOnStack))) FunctionEnvironmentRecordOnStack(state, this, argc, argv);
        env = new (alloca(sizeof(LexicalEnvironment))) LexicalEnvironment(record, outerEnvironment());
        ec = new (alloca(sizeof(ExecutionContext))) ExecutionContext(ctx, state.executionContext(), env, isStrict);
    } else {
        if (LIKELY(m_codeBlock->canUseIndexedVariableStorage())) {
            record = new FunctionEnvironmentRecordOnHeap(state, this, argc, argv);
        } else {
            record = new FunctionEnvironmentRecordNotIndexed(state, this, argc, argv);
        }
        env = new LexicalEnvironment(record, outerEnvironment());
        ec = new ExecutionContext(ctx, state.executionContext(), env, isStrict);
    }

    Value* registerFile = ALLOCA((registerSize + stackStorageSize + literalStorageSize) * sizeof(Value), Value, state);
    Value* stackStorage = registerFile + registerSize;

    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }

    Value* literalStorage = stackStorage + stackStorageSize;
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = literalStorageSrc[i];
    }

    ExecutionState newState(ctx, &state, ec);

    // binding function name
    const CodeBlock::FunctionNameSaveInfo& info = m_codeBlock->m_functionNameSaveInfo;
    if (info.m_isAllocated) {
        if (LIKELY(info.m_isAllocatedOnStack)) {
            ASSERT(m_codeBlock->canUseIndexedVariableStorage());
            stackStorage[info.m_index] = this;
        } else {
            if (m_codeBlock->canUseIndexedVariableStorage()) {
                ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                ((FunctionEnvironmentRecordOnHeap*)record)->FunctionEnvironmentRecordOnHeap::setHeapValueByIndex(info.m_index, this);
            } else {
                record->setMutableBinding(state, m_codeBlock->functionName(), this);
            }
        }
    }

    // prepare parameters
    if (UNLIKELY(m_codeBlock->needsComplexParameterCopy())) {
        if (!m_codeBlock->canUseIndexedVariableStorage()) {
            for (size_t i = 0; i < parameterCopySize; i++) {
                record->setMutableBinding(state, m_codeBlock->functionParameters()[i], argv[i]);
            }
        } else {
            const CodeBlock::FunctionParametersInfoVector& info = m_codeBlock->parametersInfomation();
            for (size_t i = 0; i < info.size(); i++) {
                Value val(Value::ForceUninitialized);
                // NOTE: consider the special case with duplicated parameter names (**test262: S10.2.1_A3)
                if (i < argc)
                    val = argv[i];
                else if (info[i].m_index >= argc)
                    continue;
                else
                    val = Value();
                if (info[i].m_isHeapAllocated) {
                    ASSERT(record->isFunctionEnvironmentRecordOnHeap());
                    ((FunctionEnvironmentRecordOnHeap*)record)->FunctionEnvironmentRecordOnHeap::setHeapValueByIndex(info[i].m_index, val);
                } else {
                    stackStorage[info[i].m_index] = val;
                }
            }
        }
    } else {
        for (size_t i = 0; i < parameterCopySize; i++) {
            stackStorage[i] = argv[i];
        }
    }

    // prepare receiver
    if (!isStrict) {
        if (receiverSrc.isUndefinedOrNull()) {
            stackStorage[m_codeBlock->thisSymbolIndex()] = ctx->globalObject();
        } else {
            stackStorage[m_codeBlock->thisSymbolIndex()] = receiverSrc.toObject(state);
        }
    } else {
        stackStorage[m_codeBlock->thisSymbolIndex()] = receiverSrc;
    }

    if (UNLIKELY(m_codeBlock->hasArgumentsBinding())) {
        ASSERT(m_codeBlock->usesArgumentsObject());
        if (m_codeBlock->hasArgumentsBindingInParameterOrChildFD()) {
            record->asFunctionEnvironmentRecord()->m_isArgumentObjectCreated = true;
        }
    }

    // run function
    const Value returnValue = ByteCodeInterpreter::interpret(newState, blk, 0, registerFile);
    if (blk->m_shouldClearStack)
        clearStack<512>();

    return returnValue;
}
}
