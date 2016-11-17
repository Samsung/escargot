#ifndef __EscargotContext__
#define __EscargotContext__

#include "Context.h"

namespace Escargot {

class VMInstance;

class Context : public gc {
public:
    Context(VMInstance* instance);
protected:
    VMInstance* m_instance;
};

}

#endif
