#include "impl/Thread.hpp"

#include <pthread.h>

#include <bit>
#include <cstring>
#include <ctime>
#include <system_error>
#include <thread>

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

Mask::Mask(const std::vector<std::size_t>& affinity) {
    for (const auto index : affinity) set(index);
}

Mask::operator std::vector<std::size_t>() const {
    std::vector<std::size_t> affinity;
    affinity.reserve(count());
    for (std::size_t i = 0; i < size(); ++i) {
        if (test(i)) affinity.emplace_back(i);
    }
    return affinity;
}

std::string Mask::to_string() const {
    const auto msb = static_cast<std::size_t>(std::countl_zero(to_ullong()));
    std::string result;
    result.reserve(size() - msb);
    for (std::size_t i = 0; i < (size() - msb); ++i) {
        result += (*this)[i] ? std::to_string(i) : std::string{"."};
    }
    return result;
}

Mask Mask::at(std::size_t index) const {
    if (index >= count()) throw std::runtime_error("No available bits at index:" + std::to_string(index));
    Mask result;
    for (std::size_t i = 0; i < size(); ++i) {
        if (test(i) && index-- == 0) {
            result.set(i);
            break;
        }
    }
    return result;
}

Mask Mask::extract(std::size_t n) {
    if (n > count()) throw std::runtime_error("No bits for extraction:" + std::to_string(n));
    Mask result;
    for (std::size_t i = 0; i < n; ++i) {
        const auto index = static_cast<std::size_t>(std::countr_zero(to_ullong()));
        reset(index);
        result.set(index);
    }
    return result;
}

void affinity(const Mask mask, std::size_t handle) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (std::size_t i = 0; i < mask.size() && i < CPU_SETSIZE; ++i) {
        if (mask.test(i)) CPU_SET(i, &cpuset);
    }

    if (const int err = ::pthread_setaffinity_np(tid(handle), sizeof(cpuset), &cpuset)) {
        throw std::system_error(err, std::system_category(), "pthread_setaffinity_np()");
    }
}

Mask affinity(std::size_t handle) {
    cpu_set_t cpuset;
    if (const int err = ::pthread_getaffinity_np(tid(handle), sizeof(cpuset), &cpuset)) {
        throw std::system_error(err, std::system_category(), "pthread_getaffinity_np()");
    }

    Mask mask;
    for (std::size_t i = 0; i < mask.size() && i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) mask.set(i);
    }
    return mask;
}

Mask AllAvailableCPU() {
    const auto cpus = std::thread::hardware_concurrency();
    Mask result;
    result.flip();  // Inverse zeros to ones
    result <<= cpus;
    result.flip();
    return result;
}

void NanoSleep::pause() noexcept {
    const timespec timeout = {0, 1};
    ::nanosleep(&timeout, nullptr);
}

}  // namespace dlsm::Thread