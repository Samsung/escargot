/*
 * Copyright (c) 2016-present Samsung Electronics Co., Ltd
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

#include "Escargot.h"
#include "VMInstance.h"

namespace Escargot {

extern size_t g_doubleInSmallValueTag;
extern size_t g_objectRareDataTag;

VMInstance::VMInstance()
{
    std::setlocale(LC_ALL, "en_US.utf8");
    // if you don't use localtime() or timegm() or etc. you should call tzset() to use tzname[]
    tzset();

    m_timezone = nullptr;
    if (getenv("TZ")) { // Usually used by StarFish
        m_timezoneID = getenv("TZ");
    } else { // Used by escargot standalone
        m_timezoneID = "";
    }

    if (!String::emptyString) {
        String::emptyString = new (NoGC) ASCIIString("");
    }

    DoubleInSmallValue temp(0);
    g_doubleInSmallValueTag = *((size_t*)&temp);

    {
        ASCIIString str("", 0);
        g_asciiStringTag = *((size_t*)&str);
    }
    {
        Latin1String str("", 0);
        g_latin1StringTag = *((size_t*)&str);
    }
    {
        UTF16String str(u"", 0);
        g_utf16StringTag = *((size_t*)&str);
    }
    {
        RopeString str;
        g_ropeStringTag = *((size_t*)&str);
    }
    {
        StringView str;
        g_stringViewTag = *((size_t*)&str);
    }

    ObjectRareData data(nullptr);
    g_objectRareDataTag = *((size_t*)&data);
}
}
