#include "Escargot.h"
#include "shell/DefaultJobQueue.h"

namespace Escargot {

#ifdef ESCARGOT_ENABLE_PROMISE
#ifdef ESCARGOT_ENABLE_VENDORTEST

static Value builtinDrainJobQueue(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    DefaultJobQueue* jobQueue = DefaultJobQueue::get(state.context()->jobQueue());
    while (jobQueue->hasNextJob()) {
        auto jobResult = jobQueue->nextJob()->run(state);
        if (!jobResult.error.isEmpty())
            return Value(false);
    }
    return Value(true);
}

static Value builtinAddPromiseReactions(ExecutionState& state, Value thisValue, size_t argc, Value* argv, bool isNewExpression)
{
    PromiseObject* promise = argv[0].toObject(state)->asPromiseObject();
    PromiseReaction::Capability capability = PromiseObject::newPromiseCapability(state, promise->get(state, state.context()->staticStrings().constructor).value(state, promise).toObject(state));
    promise->appendReaction(argv[1].toObject(state)->asFunctionObject(), argv[2].toObject(state)->asFunctionObject(), capability);
    return Value();
}
#endif
#endif


void installTestFunctions(ExecutionState& state)
{
#ifdef ESCARGOT_ENABLE_VENDORTEST
    GlobalObject* globalObject = state.context()->globalObject();

#ifdef ESCARGOT_ENABLE_PROMISE
    AtomicString drainJobQueue(state, "drainJobQueue");
    globalObject->defineOwnProperty(state, ObjectPropertyName(drainJobQueue),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(drainJobQueue, builtinDrainJobQueue, 0, nullptr, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString addPromiseReactions(state, "addPromiseReactions");
    globalObject->defineOwnProperty(state, ObjectPropertyName(addPromiseReactions),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(addPromiseReactions, builtinAddPromiseReactions, 3, nullptr, NativeFunctionInfo::Strict)),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
#endif // ESCARGOT_ENABLE_PROMISE

#else // ESCARGOT_ENABLE_VENDORTEST
/* Do nothong */
#endif // ESCARGOT_ENABLE_VENDORTEST
}
}
