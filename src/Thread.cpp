#include "impl/Thread.hpp"

#include <pthread.h>

#include <cstring>
#include <ctime>
#include <system_error>

namespace dlsm::Thread {

void name(const std::string& name, std::size_t native_handle) {
    auto thread = (native_handle == 0) ? pthread_self() : static_cast<pthread_t>(native_handle);
    if (const int err = pthread_setname_np(thread, name.c_str())) {
        throw std::system_error(err, std::generic_category(), "pthread_setname_np()");
    }
}

std::string name(std::size_t native_handle) {
    auto thread = (native_handle == 0) ? pthread_self() : static_cast<pthread_t>(native_handle);
    std::string name(16, '\0');
    if (const int err = pthread_getname_np(thread, std::data(name), std::size(name))) {
        throw std::system_error(err, std::generic_category(), "pthread_getname_np()");
    }
    name.resize(std::strlen(name.c_str()));
    return name;
}

void affinity(std::size_t cpuid, std::size_t native_handle) {
    auto thread = (native_handle == 0) ? pthread_self() : static_cast<pthread_t>(native_handle);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (cpuid == AllCPU) {
        for (std::size_t i = 0; i < sizeof(cpu_set_t) * 8; ++i) {
            CPU_SET(i, &cpuset);
        }
    } else {
        CPU_SET(cpuid, &cpuset);
    }

    if (const int err = ::pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset)) {
        throw std::system_error(err, std::system_category(), "pthread_setaffinity_np()");
    }
}

void NanoSleep::pause() noexcept {
    const timespec timeout = {0, 1};
    ::nanosleep(&timeout, nullptr);
}

}  // namespace dlsm::Thread