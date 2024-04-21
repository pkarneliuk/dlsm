#include "impl/Signal.hpp"

#include <cstring>
#include <format>
#include <system_error>
#include <type_traits>
#include <utility>

namespace dlsm::Signal {

std::string str(const Type signal) {
    const auto description = ::sigdescr_np(signal);
    // C++23 std::to_underlying()
    if (description == nullptr) return "Unknown signal " + std::to_string(std::underlying_type_t<Type>(signal));
    return description;
}

std::string str(const siginfo_t& i) { return std::format("PID:{} signal:{} addr:{}", i.si_pid, i.si_signo, i.si_addr); }

void send(const Type signal) {
    if (0 != ::raise(signal)) {
        throw std::system_error(errno, std::generic_category(), "Signal::send(" + std::to_string(signal) + ") failed");
    }
}

Guard::Guard(const Type signal, const Handler callback) : previous_{set(signal, callback)}, signal_{signal} {}
Guard::Guard(Guard&& that) noexcept : previous_{that.previous_}, signal_{that.signal_}, restore_{that.restore_} {
    that.restore_ = false;
    struct sigaction empty = {};
    that.previous_ = empty;
}
Guard::~Guard() noexcept(false) {
    if (!restore_) return;
    if (0 != ::sigaction(signal_, &previous_, nullptr)) {
        throw std::system_error(errno, std::generic_category(),
                                "sigaction() failed to restore callback for signal:" + str(signal_));
    }
}

struct sigaction Guard::set(const Type signal, const Handler callback) {
    struct sigaction action = {};
    if (0 != ::sigemptyset(&action.sa_mask)) {
        throw std::system_error(errno, std::generic_category(), "sigemptyset()");
    }

    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = callback;

    struct sigaction previous = {};
    if (0 != ::sigaction(signal, &action, &previous)) {
        throw std::system_error(errno, std::generic_category(), "sigaction() failed to set callback: " + str(signal));
    }

    return previous;
}

}  // namespace dlsm::Signal
