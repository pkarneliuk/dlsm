#include <iostream>

#include "dlsm/Public.hpp"
#include "dlsm/Version.hpp"

int main(int /*argc*/, char** argv) {
    std::cout << "App:" << argv[0] << " Version:" << dlsm::Version::string << '\n';
    return 0;
}
