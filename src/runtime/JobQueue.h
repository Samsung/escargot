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
    virtual size_t enqueueJob(Job* job) = 0;
};
}
#endif // ESCARGOT_ENABLE_PROMISE
#endif // __EscargotJobQueue__
