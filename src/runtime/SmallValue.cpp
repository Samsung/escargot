#include "Escargot.h"
#include "SmallValueData.h"

namespace Escargot {

namespace SmallValueImpl {
SmallValueData* smallValueUndefined = new SmallValueData();
SmallValueData* smallValueNull = new SmallValueData();
SmallValueData* smallValueTrue = new SmallValueData();
SmallValueData* smallValueFalse = new SmallValueData();
SmallValueData* smallValueEmpty = new SmallValueData();
SmallValueData* smallValueDeleted = new SmallValueData();
}

}
