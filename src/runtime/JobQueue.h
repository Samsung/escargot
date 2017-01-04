#ifndef __EscargotJobQueue__
#define __EscargotJobQueue__

#ifdef ESCARGOT_ENABLE_PROMISE

#include "Job.h"

namespace Escargot {

class ExecutionState;

class JobQueue : public gc {
protected:
    JobQueue() {}
public:
    static JobQueue* create();
    virtual size_t enqueueJob(Job* job) = 0;
};
}
#endif // ESCARGOT_ENABLE_PROMISE
#endif // __EscargotJobQueue__
