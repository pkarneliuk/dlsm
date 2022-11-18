#include <dlsm/Public.hpp>

namespace dlsm {

int Public::set(int v) { return _member = v; }
int Public::get() { return _member; }

}  // namespace dlsm