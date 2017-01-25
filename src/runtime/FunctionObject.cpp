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

void* FunctionObject::operator new(size_t size)
{
    static bool typeInited = false;
    static GC_descr descr;
    if (!typeInited) {
        GC_word obj_bitmap[GC_BITMAP_SIZE(FunctionObject)] = { 0 };
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_structure));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_rareData));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_prototype));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_values));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_outerEnvironment));
        GC_set_bit(obj_bitmap, GC_WORD_OFFSET(FunctionObject, m_codeBlock));
        descr = GC_make_descriptor(obj_bitmap, GC_WORD_LEN(FunctionObject));
        typeInited = true;
    }
    return GC_MALLOC_EXPLICITLY_TYPED(size, descr);
}

void FunctionObject::initFunctionObject(ExecutionState& state)
{
    if (m_isConstructor) {
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
    , m_isConstructor(false)
    , m_codeBlock(codeBlock)
    , m_outerEnvironment(nullptr)
{
    initFunctionObject(state);
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, bool isConstructor)
    : Object(state, isConstructor ? (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3) : (ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 2), false)
    , m_isConstructor(isConstructor)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
}

FunctionObject::FunctionObject(ExecutionState& state, NativeFunctionInfo info, ForBuiltin)
    : Object(state, ESCARGOT_OBJECT_BUILTIN_PROPERTY_NUMBER + 3, false)
    , m_isConstructor(true)
    , m_codeBlock(new CodeBlock(state.context(), info))
    , m_outerEnvironment(nullptr)
{
    initFunctionObject(state);
    setPrototype(state, state.context()->globalObject()->functionPrototype());
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

NEVER_INLINE void FunctionObject::generateBytecodeBlock(ExecutionState& state)
{
    Vector<CodeBlock*, gc_allocator<CodeBlock*>>& v = state.context()->compiledCodeBlocks();

    size_t currentCodeSizeTotal = 0;
    for (size_t i = 0; i < v.size(); i++) {
        currentCodeSizeTotal += v[i]->m_byteCodeBlock->m_code.size();
    }
    // printf("codeSizeTotal %lfMB\n", (int)currentCodeSizeTotal / 1024.0 / 1024.0);

    const int codeSizeMax = 1024 * 1024 * 2;
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
        }
    }
    ASSERT(!m_codeBlock->isNativeFunction());

    Node* ast;
    if (m_codeBlock->cachedASTNode()) {
        ast = m_codeBlock->cachedASTNode();
    } else {
        ast = state.context()->scriptParser().parseFunction(m_codeBlock);
    }

    ByteCodeGenerator g;
    g.generateByteCode(state.context(), m_codeBlock, ast);
    v.pushBack(m_codeBlock);

    if (m_codeBlock->cachedASTNode()) {
        m_codeBlock->m_cachedASTNode = nullptr;
        delete ast;
    } else {
        delete ast;
    }
}

Value FunctionObject::call(ExecutionState& state, const Value& receiverOrg, const size_t& argc, Value* argv, bool isNewExpression)
{
    Value receiver = receiverOrg;
    Context* ctx = state.context();

    // prepare ByteCodeBlock if needed
    if (UNLIKELY(m_codeBlock->byteCodeBlock() == nullptr)) {
        generateBytecodeBlock(state);
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
        record = new FunctionEnvironmentRecordNotIndexed(state, receiver, this, argc, argv, isNewExpression);
        if (m_codeBlock->canUseIndexedVariableStorage()) {
            record = new FunctionEnvironmentRecordOnHeap(state, receiver, this, argc, argv, isNewExpression);
        } else {
            record = new FunctionEnvironmentRecordNotIndexed(state, receiver, this, argc, argv, isNewExpression);
        }
        env = new LexicalEnvironment(record, outerEnvironment());
        ec = new ExecutionContext(ctx, state.executionContext(), env, isStrict);
    }

    Value registerFile[m_codeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize];
    memset(registerFile, 0, m_codeBlock->byteCodeBlock()->m_requiredRegisterFileSizeInValueSize * sizeof(Value));
    Value* stackStorage = ALLOCA(stackStorageSize * sizeof(Value), Value, state);
    Value resultValue;
    for (size_t i = 0; i < stackStorageSize; i++) {
        stackStorage[i] = Value();
    }
    ExecutionState newState(ctx, ec, &resultValue);

    // binding function name
    if (m_codeBlock->m_functionNameIndex != SIZE_MAX) {
        const CodeBlock::FunctionNameSaveInfo& info = m_codeBlock->m_functionNameSaveInfo;
        if (LIKELY(info.m_isAllocatedOnStack)) {
            ASSERT(m_codeBlock->canUseIndexedVariableStorage());
            stackStorage[info.m_index] = this;
        } else {
            if (m_codeBlock->canUseIndexedVariableStorage()) {
                record->setHeapValueByIndex(info.m_index, this);
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
                record->setMutableBinding(state, m_codeBlock->functionParameters()[i], argv[i]);
            }
        } else {
            for (size_t i = 0; i < info.size(); i++) {
                Value val;
                // NOTE: consider the special case with duplicated parameter names (**test262: S10.2.1_A3)
                if (i < argc)
                    val = argv[i];
                else if (info[i].m_index >= argc)
                    continue;
                if (info[i].m_isHeapAllocated) {
                    record->setHeapValueByIndex(info[i].m_index, val);
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

    if (UNLIKELY(m_codeBlock->hasArgumentsBinding())) {
        ASSERT(m_codeBlock->usesArgumentsObject());
        AtomicString arguments = state.context()->staticStrings().arguments;
        for (size_t i = 0; i < m_codeBlock->functionParameters().size(); i++) {
            if (UNLIKELY(m_codeBlock->functionParameters()[i] == arguments)) {
                record->asFunctionEnvironmentRecord()->m_isArgumentObjectCreated = true;
                break;
            }
        }

        for (size_t i = 0; i < m_codeBlock->childBlocks().size(); i++) {
            CodeBlock* cb = m_codeBlock->childBlocks()[i];
            if (cb->isFunctionDeclaration() && cb->functionName() == arguments) {
                record->asFunctionEnvironmentRecord()->m_isArgumentObjectCreated = true;
                break;
            }
        }
    }

    // run function
    clearStack<512>();
    ByteCodeInterpreter::interpret(newState, m_codeBlock->byteCodeBlock(), 0, registerFile, stackStorage);

    return resultValue;
}
}
