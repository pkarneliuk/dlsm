
#include "dlsm/Test.h"

namespace dlsm {
int Test(int a) {
    if (a > 3) {
        return Test2(a);
    }
    return 2;
}
}  // namespace dlsm