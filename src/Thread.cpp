#include "impl/Thread.hpp"

#include <pthread.h>

#include <cstring>
#include <ctime>
#include <system_error>

namespace {
pthread_t tid(std::size_t handle = 0) { return (handle == 0) ? pthread_self() : static_cast<pthread_t>(handle); }
}  // namespace

namespace dlsm::Thread {

void name(const std::string& name, std::size_t handle) {
    if (const int err = pthread_setname_np(tid(handle), name.c_str())) {
        throw std::system_error(err, std::generic_category(), "pthread_setname_np()");
    }
}

std::string name(std::size_t handle) {
    std::string name(16, '\0');
    if (const int err = pthread_getname_np(tid(handle), std::data(name), std::size(name))) {
        throw std::system_error(err, std::generic_category(), "pthread_getname_np()");
    }
    name.resize(std::strlen(name.c_str()));
    return name;
}

void affinity(std::size_t cpuid, std::size_t handle) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (cpuid == AllCPU) {
        for (std::size_t i = 0; i < CPU_SETSIZE; ++i) {
            CPU_SET(i, &cpuset);
        }
    } else {
        CPU_SET(cpuid, &cpuset);
    }

    if (const int err = ::pthread_setaffinity_np(tid(handle), sizeof(cpuset), &cpuset)) {
        throw std::system_error(err, std::system_category(), "pthread_setaffinity_np()");
    }
}

std::size_t getaffinity(std::size_t handle) {
    cpu_set_t cpuset;
    if (const int err = ::pthread_getaffinity_np(tid(handle), sizeof(cpuset), &cpuset)) {
        throw std::system_error(err, std::system_category(), "pthread_getaffinity_np()");
    }
    for (std::size_t i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) return i;
    }
    return 0;
}

void NanoSleep::pause() noexcept {
    const timespec timeout = {0, 1};
    ::nanosleep(&timeout, nullptr);
}

}  // namespace dlsm::Thread