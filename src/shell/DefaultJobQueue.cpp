#ifdef ESCARGOT_ENABLE_PROMISE

#include "Escargot.h"
#include "DefaultJobQueue.h"
#include "runtime/Job.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"

namespace Escargot {

JobQueue* JobQueue::create()
{
    return DefaultJobQueue::create();
}
}

#endif
