#include "Escargot.h"
#include "shell/DefaultJobQueue.h"

namespace Escargot {

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


void installTestFunctions(ExecutionState& state)
{
    GlobalObject* globalObject = state.context()->globalObject();

    AtomicString drainJobQueue(state, "drainJobQueue");
    globalObject->defineOwnProperty(state, ObjectPropertyName(drainJobQueue),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(drainJobQueue, builtinDrainJobQueue, 0, nullptr, NativeFunctionInfo::Strict), false),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
    AtomicString addPromiseReactions(state, "addPromiseReactions");
    globalObject->defineOwnProperty(state, ObjectPropertyName(addPromiseReactions),
                                    ObjectPropertyDescriptor(new FunctionObject(state,
                                                                                NativeFunctionInfo(addPromiseReactions, builtinAddPromiseReactions, 3, nullptr, NativeFunctionInfo::Strict), false),
                                                             (ObjectPropertyDescriptor::PresentAttribute)(ObjectPropertyDescriptor::AllPresent)));
}
}
