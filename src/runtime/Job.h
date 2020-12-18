/*
 * Copyright (c) 2017-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef __EscargotJob__
#define __EscargotJob__

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
    Job(JobType type, Context* relatedContext)
        : m_type(type)
        , m_relatedContext(relatedContext)
    {
    }

public:
    virtual ~Job() {}
    static Job* create()
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    virtual SandBox::SandBoxResult run() = 0;
    Context* relatedContext() const
    {
        return m_relatedContext;
    }

    JobType type() const
    {
        return m_type;
    }

private:
    JobType m_type;
    Context* m_relatedContext;
};

// https://www.ecma-international.org/ecma-262/10.0/#sec-promisereactionjob
class PromiseReactionJob : public Job {
public:
    PromiseReactionJob(Context* relatedContext, PromiseReaction reaction, Value argument)
        : Job(JobType::PromiseReactionJob, relatedContext)
        , m_reaction(reaction)
        , m_argument(argument)
    {
    }

    SandBox::SandBoxResult run();

private:
    PromiseReaction m_reaction;
    Value m_argument;
};

// https://www.ecma-international.org/ecma-262/10.0/#sec-promiseresolvethenablejob
class PromiseResolveThenableJob : public Job {
public:
    PromiseResolveThenableJob(Context* relatedContext, PromiseObject* promise, Object* thenable, Object* then)
        : Job(JobType::PromiseResolveThenableJob, relatedContext)
        , m_promise(promise)
        , m_thenable(thenable)
        , m_then(then)
    {
    }

    SandBox::SandBoxResult run();

private:
    PromiseObject* m_promise;
    Object* m_thenable;
    Object* m_then;
};


class ScriptJob : public Job {
};
} // namespace Escargot
#endif // __EscargotJob__
