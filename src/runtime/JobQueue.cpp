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

#include "Escargot.h"
#include "JobQueue.h"
#include "runtime/Job.h"
#include "runtime/Context.h"
#include "runtime/SandBox.h"
#include "runtime/VMInstance.h"

namespace Escargot {

void JobQueue::enqueueJob(Job* job)
{
    m_jobs.push_back(job);
}

void JobQueue::clearJobRelatedWithSpecificContext(Context* context)
{
    auto iter = m_jobs.begin();
    while (iter != m_jobs.end()) {
        if ((*iter)->relatedContext() == context) {
            iter = m_jobs.erase(iter);
        } else {
            iter++;
        }
    }
}
}
