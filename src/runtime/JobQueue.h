/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
    virtual size_t enqueueJob(ExecutionState& state, Job* job) = 0;
};

class DefaultJobQueue : public JobQueue {
private:
    DefaultJobQueue() {}
public:
    static DefaultJobQueue* create()
    {
        return new DefaultJobQueue();
    }

    size_t enqueueJob(ExecutionState& state, Job* job);
    bool hasNextJob()
    {
        return !m_jobs.empty();
    }

    Job* nextJob()
    {
        ASSERT(!m_jobs.empty());
        Job* job = m_jobs.front();
        m_jobs.pop_front();
        return job;
    }

    static DefaultJobQueue* get(JobQueue* jobQueue)
    {
        return (DefaultJobQueue*)jobQueue;
    }

private:
    std::list<Job*, gc_allocator<Job*> > m_jobs;
};
}
#endif // ESCARGOT_ENABLE_PROMISE
#endif // __EscargotJobQueue__
