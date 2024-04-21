#pragma once

#include <signal.h>

#include <array>
#include <atomic>
#include <string>

namespace dlsm::Signal {

enum Type : int {
    NONE = 0,
    BUS = SIGBUS,
    INT = SIGINT,
    TERM = SIGTERM,
    HUP = SIGHUP,
    KILL = SIGKILL,
    ABORT = SIGABRT,
    CHLD = SIGCHLD,
};

std::string str(const Type signal);
std::string str(const siginfo_t& info);
void send(const Type signal);

using Handler = void (*)(int signal, siginfo_t* info, void* ucontext);

struct Guard {
    Guard(const Type signal, const Handler callback);
    Guard(Guard&& that) noexcept;
    ~Guard() noexcept(false);

    bool restoring() const { return restore_; }

private:
    struct sigaction set(const Type signal, const Handler callback);

    struct sigaction previous_ = {};
    const Type signal_{Type::NONE};
    bool restore_{true};
};

template <Type... signals>
struct WatcherOf {
    WatcherOf() : guards_{Guard{signals, handler}...} {}
    ~WatcherOf() { instance().type.store(Type::NONE); }

    void wait() const noexcept { instance().type.wait(Type::NONE); }

    operator bool() const noexcept { return instance().type.load() != Type::NONE; }

    static Type value() { return instance().type.load(); }
    static siginfo_t info() { return instance().info.load(); }

private:
    struct Data {
        std::atomic<Type> type{Type::NONE};
        std::atomic<siginfo_t> info = {};
    };

    static Data& instance() {
        static Data instance;
        return instance;
    }

    static void handler(int signal, siginfo_t* info, void*) noexcept {
        auto& data = instance();
        data.info.store(*info);
        data.type.store(static_cast<Type>(signal));
        data.type.notify_all();
    }

    std::array<Guard, sizeof...(signals)> guards_;
};

using Termination = WatcherOf<Type::TERM, Type::INT>;

}  // namespace dlsm::Signal