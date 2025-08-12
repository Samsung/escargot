#include "Escargot.h"
#include "interpreter/ByteCode.h"
#include "interpreter/ByteCodeGenerator.h"
#include "interpreter/ByteCodeInterpreter.h"
#include "runtime/Global.h"
#include "runtime/GlobalObject.h"
#include "runtime/Context.h"
#include "runtime/Environment.h"
#include "runtime/EnvironmentRecord.h"
#include "runtime/ExtendedNativeFunctionObject.h"
#include "runtime/VMInstance.h"
#include "runtime/ShadowRealmObject.h"
#include "runtime/NativeFunctionObject.h"
#include "runtime/ScriptFunctionObject.h"
#include "parser/Script.h"
#include "parser/ScriptParser.h"

namespace Escargot {
#if defined(ESCARGOT_ENABLE_SHADOWREALM)
// https://tc39.es/proposal-shadowrealm/#sec-shadowrealm
static Value builtinShadowRealmConstructor(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // If NewTarget is undefined, throw a TypeError exception.
    if (!newTarget) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, ErrorObject::Messages::GlobalObject_ConstructorRequiresNew);
    }

    // Let O be ? OrdinaryCreateFromConstructor(NewTarget, "%ShadowRealm.prototype%", « [[ShadowRealm]] »).
    Object* proto = Object::getPrototypeFromConstructor(state, newTarget.value(), [](ExecutionState& state, Context* constructorRealm) -> Object* {
        return constructorRealm->globalObject()->shadowRealmPrototype();
    });
    ShadowRealmObject* O = new ShadowRealmObject(state, proto);

    // Let callerContext be the running execution context.
    // Perform ? InitializeHostDefinedRealm().
    // Let innerContext be the running execution context.
    Context* innerContext = new Context(state.context()->vmInstance());
    // Remove innerContext from the execution context stack and restore callerContext as the running execution context.
    // Let realmRec be the Realm of innerContext.
    // Set O.[[ShadowRealm]] to realmRec.
    O->setRealmContext(innerContext);
    // Perform ? HostInitializeShadowRealm(realmRec, innerContext, O).
    // Assert: realmRec.[[GlobalObject]] is an ordinary object.
    ASSERT(innerContext->globalObject()->isOrdinary());
    // Return O.
    return O;
}

static Value execute(ExecutionState& state, Script* script, bool isExecuteOnEvalFunction, bool inStrictMode, Context* callerContext, Context* evalContext)
{
    InterpretedCodeBlock* topCodeBlock = script->topCodeBlock();
    ByteCodeBlock* byteCodeBlock = topCodeBlock->byteCodeBlock();

    ExecutionState* newState;
    if (byteCodeBlock->needsExtendedExecutionState()) {
        newState = new (alloca(sizeof(ExtendedExecutionState))) ExtendedExecutionState(evalContext);
    } else {
        newState = new (alloca(sizeof(ExecutionState))) ExecutionState(evalContext);
    }

    ExecutionState* codeExecutionState = newState;

    EnvironmentRecord* globalRecord = new GlobalEnvironmentRecord(state, topCodeBlock, evalContext->globalObject(), evalContext->globalDeclarativeRecord(), evalContext->globalDeclarativeStorage());
    LexicalEnvironment* globalLexicalEnvironment = new LexicalEnvironment(globalRecord, nullptr);
    newState->setLexicalEnvironment(globalLexicalEnvironment, topCodeBlock->isStrict());

    EnvironmentRecord* globalVariableRecord = globalRecord;

    if (isExecuteOnEvalFunction) {
        EnvironmentRecord* newVariableRecord = new DeclarativeEnvironmentRecordNotIndexed(state, true);
        ExecutionState* newVariableState = new ExtendedExecutionState(evalContext);
        newVariableState->setLexicalEnvironment(new LexicalEnvironment(newVariableRecord, globalLexicalEnvironment), topCodeBlock->isStrict());
        newVariableState->setParent(newState);
        codeExecutionState = newVariableState;
    }

    const InterpretedCodeBlock::IdentifierInfoVector& identifierVector = topCodeBlock->identifierInfos();
    size_t identifierVectorLen = identifierVector.size();

    const auto& globalLexicalVector = topCodeBlock->blockInfo(0)->identifiers();
    size_t globalLexicalVectorLen = globalLexicalVector.size();

    {
        VirtualIdDisabler d(evalContext); // we should create binding even there is virtual ID

        for (size_t i = 0; i < globalLexicalVectorLen; i++) {
            codeExecutionState->lexicalEnvironment()->record()->createBinding(*codeExecutionState, globalLexicalVector[i].m_name, false, globalLexicalVector[i].m_isMutable, false);
        }

        for (size_t i = 0; i < identifierVectorLen; i++) {
            // https://www.ecma-international.org/ecma-262/5.1/#sec-10.5
            // Step 2. If code is eval code, then let configurableBindings be true.
            if (identifierVector[i].m_isVarDeclaration) {
                globalVariableRecord->createBinding(*codeExecutionState, identifierVector[i].m_name, isExecuteOnEvalFunction, identifierVector[i].m_isMutable, true, topCodeBlock);
            }
        }
    }

    Value thisValue(evalContext->globalObjectProxy());

    const size_t literalStorageSize = byteCodeBlock->m_numeralLiteralData.size();
    const size_t registerFileSize = byteCodeBlock->m_requiredTotalRegisterNumber;
    ASSERT(registerFileSize == byteCodeBlock->m_requiredOperandRegisterNumber + topCodeBlock->totalStackAllocatedVariableSize() + literalStorageSize);

    Value* registerFile;
    registerFile = CustomAllocator<Value>().allocate(registerFileSize);
    // we need to reset allocated memory because customAllocator read it
    memset(static_cast<void*>(registerFile), 0, sizeof(Value) * registerFileSize);
    registerFile[0] = Value();

    Value* stackStorage = registerFile + byteCodeBlock->m_requiredOperandRegisterNumber;
    stackStorage[0] = thisValue;

    Value* literalStorage = stackStorage + topCodeBlock->totalStackAllocatedVariableSize();
    Value* src = byteCodeBlock->m_numeralLiteralData.data();
    for (size_t i = 0; i < literalStorageSize; i++) {
        literalStorage[i] = src[i];
    }

    Value resultValue;
#ifdef ESCARGOT_DEBUGGER
    // set the next(first) breakpoint to be stopped in a newer script execution
    context()->setAsAlwaysStopState();
#endif
    resultValue = Interpreter::interpret(codeExecutionState, byteCodeBlock, reinterpret_cast<size_t>(byteCodeBlock->m_code.data()), registerFile);
    clearStack<512>();

    // we give up program bytecodeblock after first excution for reducing memory usage
    topCodeBlock->setByteCodeBlock(nullptr);

    return resultValue;
}

static Value performShadowRealmEval(ExecutionState& state, Value& sourceText, Context* callerRealm, Context* evalRealm)
{
    ScriptParser parser(evalRealm);
    bool strictFromOutside = false;
    Script* script = parser.initializeScript(nullptr, 0, sourceText.asString(), evalRealm->staticStrings().lazyEvalCode().string(), nullptr, false, true, false, false, strictFromOutside, false, false, false, true).scriptThrowsExceptionIfParseError(state);

    ExtendedExecutionState stateForNewGlobal(evalRealm);
    Value result;
    try {
        result = execute(stateForNewGlobal, script, true, script->topCodeBlock()->isStrict(), callerRealm, evalRealm);
    } catch (const Value& e) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "ShadowRealm.evaluate failed");
    }

    // https://tc39.es/proposal-shadowrealm/#sec-getwrappedvalue
    if (result.isObject()) {
        if (!result.isCallable()) {
            ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "The result of ShadowRealm.evaluate must be a callable function");
        }
    }
    return result;
}

static Value builtinShadowRealmEvaluate(ExecutionState& state, Value thisValue, size_t argc, Value* argv, Optional<Object*> newTarget)
{
    // Let O be the this value.
    const Value& O = thisValue;
    // Perform ? ValidateShadowRealmObject(O).
    if (!O.asObject()->isShadowRealmObject()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "this value must be a ShadowRealm object");
    }
    // If sourceText is not a String, throw a TypeError exception.
    if (!argv[0].isString()) {
        ErrorObject::throwBuiltinError(state, ErrorCode::TypeError, "sourceText must be a String");
    }
    // Let callerRealm be the current Realm Record.
    Context* callerRealm = state.context();
    // Let evalRealm be O.[[ShadowRealm]].
    Context* evalRealm = O.asObject()->asShadowRealmObject()->realmContext();
    // Return ? PerformShadowRealmEval(sourceText, callerRealm, evalRealm).
    return performShadowRealmEval(state, argv[0], callerRealm, evalRealm);
}
#endif
void GlobalObject::initializeShadowRealm(ExecutionState& state)
{
    installShadowRealm(state);
}
void GlobalObject::installShadowRealm(ExecutionState& state)
{
#if defined(ESCARGOT_ENABLE_SHADOWREALM)
    const StaticStrings* strings = &state.context()->staticStrings();

    m_shadowRealm = new NativeFunctionObject(state, NativeFunctionInfo(strings->ShadowRealm, builtinShadowRealmConstructor, 0), NativeFunctionObject::__ForBuiltinConstructor__);
    m_shadowRealm->setGlobalIntrinsicObject(state);

    m_shadowRealmPrototype = new PrototypeObject(state);
    m_shadowRealmPrototype->setGlobalIntrinsicObject(state, true);

    m_shadowRealm->setFunctionPrototype(state, m_shadowRealmPrototype);

    m_shadowRealmPrototype->directDefineOwnProperty(state, ObjectPropertyName(state.context()->vmInstance()->globalSymbols().toStringTag),
                                                    ObjectPropertyDescriptor(String::fromASCII("ShadowRealm"), ObjectPropertyDescriptor::ConfigurablePresent));

    m_shadowRealmPrototype->directDefineOwnProperty(state, ObjectPropertyName(strings->evaluate),
                                                    ObjectPropertyDescriptor(new NativeFunctionObject(state, NativeFunctionInfo(strings->evaluate, builtinShadowRealmEvaluate, 1, NativeFunctionInfo::Strict)), (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));

    directDefineOwnProperty(state, ObjectPropertyName(strings->ShadowRealm),
                            ObjectPropertyDescriptor(m_shadowRealm, (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::WritablePresent | ObjectPropertyDescriptor::ConfigurablePresent)));
#endif
}
} // namespace Escargot
