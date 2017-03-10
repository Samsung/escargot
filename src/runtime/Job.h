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

#ifndef __EscargotJob__
#define __EscargotJob__

#ifdef ESCARGOT_ENABLE_PROMISE

#include "SandBox.h"
#include "runtime/PromiseObject.h"

namespace Escargot {

class ExecutionState;

class Job : public gc {
public:
    enum JobType {
        ScriptJob,
        PromiseReactionJob,
        PromiseResolveThenableJob,
    };

protected:
    Job(JobType type)
        : m_type(type)
    {
    }

public:
    static Job* create()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual SandBox::SandBoxResult run(ExecutionState& state) = 0;

private:
    JobType m_type;
};

class PromiseReactionJob : public Job {
public:
    PromiseReactionJob(PromiseReaction reaction, Value argument)
        : Job(JobType::PromiseReactionJob)
        , m_reaction(reaction)
        , m_argument(argument)
    {
    }

    SandBox::SandBoxResult run(ExecutionState& state);

private:
    PromiseReaction m_reaction;
    Value m_argument;
};

class PromiseResolveThenableJob : public Job {
public:
    PromiseResolveThenableJob(PromiseObject* promise, Object* thenable, FunctionObject* then)
        : Job(JobType::PromiseResolveThenableJob)
        , m_promise(promise)
        , m_thenable(thenable)
        , m_then(then)
    {
    }

    SandBox::SandBoxResult run(ExecutionState& state);

private:
    PromiseObject* m_promise;
    Object* m_thenable;
    FunctionObject* m_then;
};


class ScriptJob : public Job {
};
}
#endif // ESCARGOT_ENABLE_PROMISE
#endif // __EscargotJob__
