#include <iostream>

#include "dlsm/Public.h"
#include "dlsm/Version.h"

int main(int argc, char** argv) {
    std::cout << argv[0] << " Version:" << dlsm::Version::String << std::endl;
    return 0;
}
