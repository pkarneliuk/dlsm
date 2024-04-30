#pragma once

#include <pthread.h>  // for ::pthread_condattr_setpshared() in ShareStrategy

#include <array>
#include <atomic>
#include <bit>  // for std::has_single_bit() - is power of two
#include <cassert>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <format>
#include <limits>
#include <new>  // for std::hardware_destructive_interference_size
#include <span>
#include <type_traits>

#include "impl/Thread.hpp"

// Disclamer: This implementation is not portable! The atomic operations
// below wont work expected on platforms with Weak Memory Model, like:
// ARM, PowerPC, Itanium. This implementation was tested on x86/64 CPU.
// For portability, some std::atomic_thread_fence() are necessary.
namespace dlsm::Disruptor {

constexpr std::size_t ceilingNextPowerOfTwo(const std::size_t value) {
    std::size_t result = 2;
    while (result < value) result <<= 1;
    return result;
}

constexpr bool isPowerOf2(const std::size_t value) { return std::has_single_bit(value); }

static constexpr auto CacheLineSize = 64;  // std::hardware_destructive_interference_size;

struct alignas(CacheLineSize) Sequence {
    using Value = std::int64_t;
    static constexpr Value Initial = -1;
    static constexpr Value Max = std::numeric_limits<Value>::max();

    struct Atomic : public std::atomic<Value> {
        using Base = std::atomic<Value>;
        using Base::Base;
        Atomic(const Atomic& that) : Base{that.load()} {}
    };
    static_assert(Atomic::is_always_lock_free);

    using Ptr = const Sequence::Atomic*;

    Atomic value_;  // implicit padding by alignas

    Sequence(Value v = Initial) : value_{v} {}

    bool operator==(const Sequence& that) const { return load() == that.load(); }

    inline Ptr ptr() const { return &value_; }
    operator Ptr() const { return &value_; }
    operator Ptr() { return &value_; }

    inline void store(const Value value) { value_.store(value, std::memory_order_release); }
    inline Value load() const { return value_.load(std::memory_order_acquire); }
    inline Value add(Value v) { return value_.fetch_add(v, std::memory_order_relaxed); }
    inline bool cas(Value expected, Value desired) {
        return value_.compare_exchange_weak(expected, desired, std::memory_order_relaxed, std::memory_order_relaxed);
        // return std::atomic_compare_exchange_strong(&m_fieldsValue, &expectedSequence, nextSequence);
    }
};

static_assert(sizeof(Sequence::Atomic) == 8);
static_assert(sizeof(Sequence) == CacheLineSize);
static_assert(alignof(Sequence) == CacheLineSize);

template <std::size_t N = CacheLineSize / sizeof(Sequence::Ptr)>
struct Group {
    constexpr static std::size_t MaxItems = N;
    using Pointers = std::array<Sequence::Ptr, MaxItems>;
    Pointers items_{nullptr};

    Group() = default;
    Group(const Group& that) = default;
    template <class... Seq>  // Add array of pointers to Sequnce
    Group(const Seq... seqs) : items_{seqs...} {
        static_assert(sizeof...(seqs) <= MaxItems);
    }

    std::size_t size() const {
        std::size_t count = 0UL;
        for (const auto& p : items_) {
            if (p != nullptr) ++count;
        }
        return count;
    }

    bool add(Sequence::Ptr ptr) { return replace(nullptr, ptr); }
    bool del(Sequence::Ptr ptr) { return replace(ptr, nullptr); }
    bool replace(Sequence::Ptr removable, Sequence::Ptr desired) {
        for (auto& p : items_) {
            if (p == removable) {
                p = desired;
                return true;
            }
        }
        return false;
    }
};

namespace Barriers {
// Barrier is:
//  - Sequence number, it represents last processed item index
//  - Dependencies, list of references to sequence numbers,
//    whose progress blocks current processing

template <typename BarrierType>
concept Concept = requires(BarrierType b, Sequence::Ptr r, const Group<>& g, Sequence::Ptr ptr, Sequence::Value s) {
    { b.cursor() } noexcept -> std::same_as<Sequence::Ptr>;
    { b.last() } noexcept -> std::same_as<Sequence::Value>;
    { b.release(s) } noexcept -> std::same_as<void>;
    { b.size() } noexcept -> std::same_as<std::size_t>;
    { b.contains(ptr) } noexcept -> std::same_as<bool>;
    { b.add(ptr) } noexcept -> std::same_as<bool>;
    { b.del(ptr) } noexcept -> std::same_as<bool>;
    { b.replace(r, ptr) } noexcept -> std::same_as<bool>;
    { b.replace(g, ptr) } noexcept -> std::same_as<bool>;
    { b.dependencies() } noexcept -> std::same_as<Group<>>;
    { b.set(g) } noexcept -> std::same_as<void>;
    { b.depends(g) } noexcept -> std::same_as<void>;
    { b.minimumSequence() } noexcept -> std::same_as<Sequence::Value>;
};

// PointerBarrier keeps dependencies as raw pointers.
// Barriers connections must be done before publishing data.
template <std::size_t N = CacheLineSize / sizeof(Sequence::Ptr)>
struct PointerBarrier {
    constexpr static std::size_t MaxItems = N;
    using Pointers = std::array<Sequence::Ptr, MaxItems>;

    alignas(CacheLineSize) Sequence last_{Sequence::Initial};
    alignas(CacheLineSize) Pointers pointers_{nullptr};

    PointerBarrier() = default;
    PointerBarrier(const PointerBarrier& that) = default;

    Sequence::Ptr cursor() const noexcept { return last_; }
    Sequence::Value last() const noexcept { return last_.load(); }
    void release(Sequence::Value sequence) noexcept { last_.store(sequence); }

    std::size_t size() const noexcept {
        std::size_t count = 0UL;
        for (const auto& p : pointers_) {
            if (p != nullptr) ++count;
        }
        return count;
    }

    bool contains(const Sequence::Ptr ptr) const noexcept {
        if (ptr) {
            for (const auto& p : pointers_) {
                if (p == ptr) return true;
            }
        }
        return false;
    }

    bool add(const Sequence::Ptr ptr) noexcept { return replace(nullptr, ptr); }
    bool del(const Sequence::Ptr ptr) noexcept { return replace(ptr, nullptr); }

    bool replace(const Sequence::Ptr removable, const Sequence::Ptr desired) noexcept {
        for (auto& p : pointers_) {
            if (p == removable) {
                p = desired;
                return true;
            }
        }
        return false;
    }

    bool replace(const Group<>& removable, const Sequence::Ptr desired) noexcept {
        bool replaced = false;
        for (const auto& ptr : removable.items_) {
            if (ptr) {
                if (replaced) {
                    replace(ptr, nullptr);
                } else {
                    replaced = replace(ptr, desired);
                }
            }
        }
        return replaced;
    }

    Group<> dependencies() const noexcept {
        Group<> result;
        for (std::size_t i = 0; i < pointers_.size(); ++i) {
            result.items_[i] = pointers_[i];  // NOLINT
        }

        return result;
    }

    void set(const Group<>& dependencies) noexcept {
        for (std::size_t i = 0; i < pointers_.size(); ++i) {
            pointers_[i] = dependencies.items_[i];  // NOLINT
        }
    }

    void depends(const Group<>& dependencies) noexcept {
        set(dependencies);
        last_.store(dependencies.size() ? minimumSequence() : Sequence::Initial);
    }

    Sequence::Value minimumSequence(Sequence::Value minimum = Sequence::Max) const noexcept {
        for (const auto& ptr : pointers_) {
            if (ptr) {
                Sequence::Value seq = ptr->load();
                if (seq < minimum) minimum = seq;
            }
        }
        return minimum;
    }
};

// AtomicsBarrier keeps dependencies as atomic pointers.
// Barriers connections can be changed diring data publishing.
template <std::size_t N = CacheLineSize / sizeof(Sequence::Ptr)>
struct AtomicsBarrier {
    constexpr static std::size_t MaxItems = N;
    using Atomics = std::array<std::atomic<Sequence::Ptr>, MaxItems>;
    static_assert(Atomics::value_type::is_always_lock_free);

    alignas(CacheLineSize) Sequence last_{Sequence::Initial};
    alignas(CacheLineSize) Atomics pointers_{nullptr};

    AtomicsBarrier() = default;
    AtomicsBarrier(const AtomicsBarrier& that) : last_{that.last_.load()} {
        for (std::size_t i = 0; auto& p : pointers_) {
            p = that.pointers_[i++].load();  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
    }

    Sequence::Ptr cursor() const noexcept { return last_; }
    Sequence::Value last() const noexcept { return last_.load(); }
    void release(Sequence::Value sequence) noexcept { last_.store(sequence); }

    std::size_t size() const noexcept {
        std::size_t count = 0UL;
        for (const auto& p : pointers_) {
            if (p != nullptr) ++count;
        }
        return count;
    }

    bool contains(const Sequence::Ptr ptr) const noexcept {
        if (ptr) {
            for (const auto& p : pointers_) {
                if (p == ptr) return true;
            }
        }
        return false;
    }

    bool add(const Sequence::Ptr ptr) noexcept { return replace(nullptr, ptr); }
    bool del(const Sequence::Ptr ptr) noexcept { return replace(ptr, nullptr); }

    bool replace(const Sequence::Ptr removable, const Sequence::Ptr desired) noexcept {
        for (auto& p : pointers_) {
            auto expected = removable;
            if (p.compare_exchange_strong(expected, desired)) {
                return true;
            }
        }
        return false;
    }

    bool replace(const Group<>& removable, const Sequence::Ptr desired) noexcept {
        bool replaced = false;
        for (const auto& ptr : removable.items_) {
            if (ptr) {
                if (replaced) {
                    replace(ptr, nullptr);
                } else {
                    replaced = replace(ptr, desired);
                }
            }
        }
        return replaced;
    }

    Group<> dependencies() const noexcept {
        Group<> result;
        for (std::size_t i = 0; i < pointers_.size(); ++i) {
            result.items_[i] = pointers_[i].load();  // NOLINT
        }

        return result;
    }

    void set(const Group<>& dependencies) noexcept {
        for (std::size_t i = 0; i < pointers_.size(); ++i) {
            pointers_[i] = dependencies.items_[i];  // NOLINT
        }
    }

    void depends(const Group<>& dependencies) noexcept {
        set(dependencies);
        last_.store(dependencies.size() ? minimumSequence() : Sequence::Initial);
    }

    Sequence::Value minimumSequence(Sequence::Value minimum = Sequence::Max) const noexcept {
        for (const auto& p : pointers_) {
            auto ptr = p.load(std::memory_order_acquire);
            if (ptr) {
                Sequence::Value seq = ptr->load();
                if (seq < minimum) minimum = seq;
            }
        }
        return minimum;
    }
};

// OffsetsBarrier keeps dependencies as atomic offsets relative to its last.
// It can be stored in shared memory for inter-process communications.
template <std::size_t N = CacheLineSize / sizeof(std::ptrdiff_t)>
struct OffsetsBarrier {
    constexpr static std::size_t MaxItems = N;
    using Offsets = std::array<std::atomic<std::ptrdiff_t>, MaxItems>;
    static_assert(Offsets::value_type::is_always_lock_free);

    alignas(CacheLineSize) Sequence last_{Sequence::Initial};
    alignas(CacheLineSize) Offsets offsets_{0};  // offsets relative to last_

    OffsetsBarrier() = default;
    OffsetsBarrier(const OffsetsBarrier& that) : last_{that.last_.load()} {
        for (std::size_t i = 0; auto& p : offsets_) {
            auto off = that.offsets_[i].load();  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            p = off ? offset(that.ptr(off)) : 0;
            ++i;
        }
    }

    Sequence::Ptr cursor() const noexcept { return last_; }
    Sequence::Value last() const noexcept { return last_.load(); }
    void release(Sequence::Value sequence) noexcept { last_.store(sequence); }

    std::ptrdiff_t offset(const Sequence::Ptr ptr) const { return ptr - &last_.value_; }
    Sequence::Ptr ptr(std::ptrdiff_t offset) const { return &last_.value_ + offset; }

    std::size_t size() const noexcept {
        std::size_t count = 0UL;
        for (const auto& p : offsets_) {
            if (p != 0) ++count;
        }
        return count;
    }

    bool contains(const Sequence::Ptr ptr) const noexcept {
        if (ptr) {
            const auto off = offset(ptr);
            for (const auto& p : offsets_) {
                if (p == off) return true;
            }
        }
        return false;
    }

    bool add(const Sequence::Ptr ptr) noexcept { return replace(nullptr, ptr); }
    bool del(const Sequence::Ptr ptr) noexcept { return replace(ptr, nullptr); }

    bool replace(std::ptrdiff_t removable, const std::ptrdiff_t desired) noexcept {
        for (auto& p : offsets_) {
            auto expected = removable;
            if (p.compare_exchange_strong(expected, desired)) {
                return true;
            }
        }
        return false;
    }

    bool replace(const Sequence::Ptr removable, const Sequence::Ptr desired) noexcept {
        const auto rem = removable ? offset(removable) : 0;
        const auto off = desired ? offset(desired) : 0;
        return replace(rem, off);
    }

    bool replace(const Group<>& removable, const Sequence::Ptr desired) noexcept {
        bool replaced = false;
        const auto off = desired ? offset(desired) : 0;
        for (const auto& ptr : removable.items_) {
            if (ptr) {
                if (replaced) {
                    replace(offset(ptr), 0);
                } else {
                    replaced = replace(offset(ptr), off);
                }
            }
        }
        return replaced;
    }

    Group<> dependencies() const noexcept {
        Group<> result;
        for (std::size_t i = 0; i < offsets_.size(); ++i) {
            auto offset = offsets_[i].load();                          // NOLINT
            result.items_[i] = (offset != 0) ? ptr(offset) : nullptr;  // NOLINT
        }

        return result;
    }

    void set(const Group<>& dependencies) noexcept {
        for (std::size_t i = 0; i < offsets_.size(); ++i) {
            auto ptr = dependencies.items_[i];    // NOLINT
            offsets_[i] = ptr ? offset(ptr) : 0;  // NOLINT
        }
    }

    void depends(const Group<>& dependencies) noexcept {
        set(dependencies);
        last_.store(dependencies.size() ? minimumSequence() : Sequence::Initial);
    }

    Sequence::Value minimumSequence(Sequence::Value minimum = Sequence::Max) const noexcept {
        for (const auto& offset : offsets_) {
            auto off = offset.load(std::memory_order_acquire);
            if (off != 0) {
                Sequence::Value seq = ptr(off)->load();
                if (seq < minimum) minimum = seq;
            }
        }
        return minimum;
    }
};

static_assert(Concept<PointerBarrier<>>);
static_assert(Concept<AtomicsBarrier<>>);
static_assert(Concept<OffsetsBarrier<>>);

}  // namespace Barriers

using Barrier = Barriers::OffsetsBarrier<8>;

static_assert(sizeof(Barrier) == CacheLineSize * 2);
static_assert(alignof(Barrier) == CacheLineSize);
static_assert(offsetof(Barrier, last_) == 0);

template <class... Barrier>
Group<> group(const Barrier&... dependsOn) {
    return Group<>{dependsOn.cursor()...};
}

template <Barriers::Concept Barrier>
inline std::size_t available(const Barrier& b) {
    auto min = b.minimumSequence();
    auto end = b.last();
    // runtime error: signed integer overflow: 9223372036854775807 - -1 cannot be represented in type 'Sequence::Value'
    auto result = (min >= end) ? (min - end) : (end - min);
    return static_cast<std::size_t>(result);
}

namespace Waits {

template <typename Strategy>
concept Concept = requires(Strategy s, Sequence::Value seq, const Barrier& seqs, Sequence::Ptr sptr) {
    { s.wait(seq, seqs) } -> std::same_as<Sequence::Value>;
    { s.wait(seq, sptr) } -> std::same_as<Sequence::Value>;
    { s.signalAllWhenBlocking() } -> std::same_as<void>;
};

template <Barriers::Concept Barrier>
inline bool waitingDone(Sequence::Value& result, const Sequence::Value sequence, const Barrier& seqs) {
    return (result = seqs.minimumSequence()) >= sequence;
}

inline bool waitingDone(Sequence::Value& result, const Sequence::Value sequence, Sequence::Ptr sptr) {
    return (result = sptr->load()) >= sequence;
}

struct SpinsStrategy {
    struct Spinner {
        static constexpr std::uint32_t Limit = 10U;
        static constexpr std::uint32_t Sleep = 20U;
        inline static const std::uint32_t Initial = [] {
            return std::thread::hardware_concurrency() > 1 ? 0U : Limit;
        }();
        std::uint32_t iteration_ = Initial;

        void once() {
            // Exponentially longer sequences of busy-waits calls
            if (iteration_ < Limit) {
                auto count = 2 << iteration_;
                while (count-- != 0) dlsm::Thread::pause();
            } else {
                if (iteration_ == Sleep) {
                    iteration_ = Limit - 1;
                    dlsm::Thread::NanoSleep::pause();
                } else {
                    std::this_thread::yield();
                }
            }
            ++iteration_;
        }
    };

    SpinsStrategy() = default;
    template <Barriers::Concept Barrier>
    Sequence::Value wait(const Sequence::Value sequence, const Barrier& seqs) const {
        Spinner spinner;
        Sequence::Value result = 0;
        while (!waitingDone(result, sequence, seqs)) spinner.once();
        return result;
    }

    Sequence::Value wait(const Sequence::Value sequence, Sequence::Ptr sptr) const {
        Spinner spinner;
        Sequence::Value result = 0;
        while (!waitingDone(result, sequence, sptr)) spinner.once();
        return result;
    }

    void signalAllWhenBlocking() {}
};

struct YieldStrategy {
    const std::size_t spinTries_;
    YieldStrategy(std::size_t spinTries = 10UL) : spinTries_{spinTries} {}

    static void waitOnce(std::size_t& iteration) {
        if (iteration == 0) {
            std::this_thread::yield();
        } else {
            --iteration;
        }
    }

    template <Barriers::Concept Barrier>
    Sequence::Value wait(const Sequence::Value sequence, const Barrier& seqs) {
        std::size_t iteration = spinTries_;
        Sequence::Value result = 0;
        while (!waitingDone(result, sequence, seqs)) {
            waitOnce(iteration);
        }
        return result;
    }

    Sequence::Value wait(const Sequence::Value sequence, Sequence::Ptr sptr) const {
        std::size_t iteration = spinTries_;
        Sequence::Value result = 0;
        while (!waitingDone(result, sequence, sptr)) {
            waitOnce(iteration);
        }
        return result;
    }

    void signalAllWhenBlocking() {}
};

struct BlockStrategy {
    std::mutex mutex_;
    std::condition_variable_any cv_;

    BlockStrategy() = default;

    template <Barriers::Concept Barrier>
    Sequence::Value wait(const Sequence::Value sequence, const Barrier& seqs) {
        Sequence::Value result = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() { return waitingDone(result, sequence, seqs); });
        return result;
    }

    Sequence::Value wait(const Sequence::Value sequence, Sequence::Ptr sptr) {
        Sequence::Value result = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() { return waitingDone(result, sequence, sptr); });
        return result;
    }

    void signalAllWhenBlocking() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.notify_all();
    }
};

struct ShareStrategy {
    pthread_mutex_t mutex_{};
    pthread_cond_t cv_{};

    struct Lock {
        pthread_mutex_t& m_;
        Lock(pthread_mutex_t& m) : m_{m} { pthread_mutex_lock(&m_); }
        ~Lock() { pthread_mutex_unlock(&m_); }
    };

    ShareStrategy() {
        pthread_mutexattr_t mutexattr_{};
        pthread_mutexattr_init(&mutexattr_);
        pthread_mutexattr_setpshared(&mutexattr_, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex_, &mutexattr_);
        pthread_mutexattr_destroy(&mutexattr_);

        pthread_condattr_t cvattr_{};
        pthread_condattr_init(&cvattr_);
        pthread_condattr_setpshared(&cvattr_, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&cv_, &cvattr_);
        pthread_condattr_destroy(&cvattr_);
    }
    ~ShareStrategy() {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cv_);
    }

    template <Barriers::Concept Barrier>
    Sequence::Value wait(const Sequence::Value sequence, const Barrier& seqs) {
        Sequence::Value result = 0;

        auto lock = Lock{mutex_};
        while (!waitingDone(result, sequence, seqs)) {
            if (int err = pthread_cond_wait(&cv_, &mutex_); err != 0) {
                throw std::system_error(err, std::generic_category(), "ShareStrategy::wait(seqs): ");
            }
        }

        return result;
    }

    Sequence::Value wait(const Sequence::Value sequence, Sequence::Ptr sptr) {
        Sequence::Value result = 0;

        auto lock = Lock{mutex_};
        while (!waitingDone(result, sequence, sptr)) {
            if (int err = pthread_cond_wait(&cv_, &mutex_); err != 0) {
                throw std::system_error(err, std::generic_category(), "ShareStrategy::wait(sptr): ");
            }
        }

        return result;
    }

    void signalAllWhenBlocking() {
        auto lock = Lock{mutex_};
        if (int err = pthread_cond_broadcast(&cv_); err != 0) {
            throw std::system_error(err, std::generic_category(), "ShareStrategy::signalAllWhenBlocking(): ");
        }
    }
};

static_assert(Concept<SpinsStrategy>);
static_assert(Concept<YieldStrategy>);
static_assert(Concept<BlockStrategy>);
static_assert(Concept<ShareStrategy>);

}  // namespace Waits

namespace Sequencers {

template <typename ProduceType>
concept Produce = requires(ProduceType p, std::size_t amount, Sequence::Value lo, Sequence::Value hi) {
    { p.available() } noexcept -> std::same_as<std::size_t>;
    { p.claim() } -> std::same_as<Sequence::Value>;
    { p.claim(amount) } -> std::same_as<Sequence::Value>;     // returns the 'end()' sequence value
    { p.tryClaim(amount) } -> std::same_as<Sequence::Value>;  // may return Sequence::Initial
    { p.publish(hi) } -> std::same_as<void>;
    { p.publish(lo, hi) } -> std::same_as<void>;
};

template <typename ConsumeType>
concept Consume = requires(ConsumeType c, Sequence::Value next, Sequence::Value last) {
    { c.available() } noexcept -> std::same_as<std::size_t>;
    { c.last() } -> std::same_as<Sequence::Value>;
    { c.consume(next) } -> std::same_as<Sequence::Value>;
    { c.consumable(next) } -> std::same_as<Sequence::Value>;  // max consumable or Sequence::Initial
    { c.release(last) } -> std::same_as<void>;
};

template <typename SequencerType>
concept Concept = requires(SequencerType s, std::size_t count, Sequence::Value seq) {
    typename SequencerType::WaitStrategy;
    requires Produce<SequencerType>;
    requires Consume<typename SequencerType::Consumer>;
    requires Consume<typename SequencerType::Indirect>;
    { s.last() } -> std::same_as<Sequence::Value>;  // returns last published value
    { s.next() } -> std::same_as<Sequence::Value>;  // returns next claimable value
    { s.capacity() } -> std::same_as<std::size_t>;
    { s.published(seq) } -> std::same_as<bool>;              // is published
    { s.consumable(seq) } -> std::same_as<Sequence::Value>;  // max consumable or Sequence::Initial
};

template <Waits::Concept WaitType, Barriers::Concept Type, typename Derived>
struct Producer {
    using WaitStrategy = WaitType;
    using BarrierType = std::remove_reference_t<Type>;  // Defines Barrier type
    using BarrierStorage = Type;                        // Defines how a Barrier is stored, by value or reference
    struct Consumer {
        BarrierStorage barrier_;
        Derived& producer_;

        explicit Consumer(BarrierType& b, Derived& p) : barrier_{b}, producer_{p} {
            barrier_.depends(p.cursor());  // cursor points to last claimed< not published
            barrier_.release(p.next() - 1);
        }

        inline std::size_t available() const noexcept {
            return static_cast<std::size_t>(producer_.next() - (last() + 1));
        }
        inline Group<> dependencies() const { return barrier_.dependencies(); }
        inline Sequence::Ptr cursor() const { return barrier_.cursor(); }
        inline Sequence::Value last() const { return barrier_.last(); }
        Sequence::Value consume(Sequence::Value sequence) const { return producer_.consume(sequence); }
        Sequence::Value consumable(Sequence::Value sequence) const { return producer_.consumable(sequence); }
        inline void release(Sequence::Value sequence) {
            barrier_.release(sequence);
            producer_.wait_.signalAllWhenBlocking();
        }
    };

    struct Indirect {
        BarrierStorage barrier_;
        WaitStrategy& wait_;

        Indirect(BarrierType& barrier, WaitStrategy& wait, const Group<>& deps) : barrier_{barrier}, wait_{wait} {
            depends(deps);
        }

        template <typename Node>
        explicit Indirect(Barrier& barrier, const Node& that)  // Not a copy but linking to
            : barrier_{barrier}, wait_{that.wait_} {
            depends(group(that));
        }

        inline std::size_t available() const noexcept {
            return static_cast<std::size_t>(barrier_.minimumSequence() - last());
        }
        inline void depends(const Group<>& dependencies) { barrier_.depends(dependencies); }
        inline Group<> dependencies() const { return barrier_.dependencies(); }
        inline Sequence::Ptr cursor() const { return barrier_.cursor(); }
        inline Sequence::Value last() const { return barrier_.last(); }
        inline Sequence::Value consume(Sequence::Value sequence) const { return wait_.wait(sequence, barrier_); }
        Sequence::Value consumable(Sequence::Value sequence) const {
            const auto published = barrier_.minimumSequence();
            return sequence <= published ? published : Sequence::Initial;
        }
        inline void release(Sequence::Value sequence) {
            barrier_.release(sequence);
            wait_.signalAllWhenBlocking();
        }
    };

    BarrierStorage barrier_;
    WaitStrategy& wait_;
    const std::size_t capacity_;

    Producer(BarrierType& barrier, WaitType& wait, std::size_t capacity)
        : barrier_{barrier}, wait_{wait}, capacity_{capacity} {
        if (!isPowerOf2(capacity_)) {
            throw std::invalid_argument{"Capacity must be power-of-two, value:" + std::to_string(capacity_)};
        }
    }

    inline auto add(const Sequence::Ptr ptr) noexcept { return barrier_.add(ptr); }
    inline auto del(const Sequence::Ptr ptr) noexcept { return barrier_.del(ptr); }
    inline auto replace(const Group<>& removable, const Sequence::Ptr desired) noexcept {
        return barrier_.replace(removable, desired);
    }

    void depends(const Group<>& dependencies) { barrier_.set(dependencies); }
    inline Group<> dependencies() { return barrier_.dependencies(); }
    inline Sequence::Ptr cursor() const { return barrier_.cursor(); }
    inline Sequence::Value last() const { return barrier_.last(); }
    inline std::size_t capacity() const { return capacity_; }
};

template <Waits::Concept WaitType, Barriers::Concept BarrierType = Barrier&>
struct SPMC : Producer<WaitType, BarrierType, SPMC<WaitType, BarrierType>> {
    using Base = Producer<WaitType, BarrierType, SPMC<WaitType, BarrierType>>;

    Sequence::Value next_{Sequence::Initial + 1};

    SPMC(BarrierType& barrier, std::size_t capacity, WaitType& wait, std::span<Sequence::Atomic> = {})
        : Base{barrier, wait, capacity} {}

    inline Sequence::Value next() const { return next_; }

    std::size_t available() const noexcept {
        const auto n = next();
        return Base::capacity() - static_cast<std::size_t>(n - Base::barrier_.minimumSequence(n - 1)) + 1;
    }

    template <std::size_t N = 1UL>
    Sequence::Value claim(std::size_t count = N) {
        static_assert(N > 0);
        const auto amount = static_cast<Sequence::Value>(std::min(count, Base::capacity()));
        const auto next = next_ + amount;
        const auto wrap = next - static_cast<Sequence::Value>(Base::capacity());
        Base::wait_.wait(wrap, Base::barrier_);
        next_ = next;
        return next;
    }

    template <std::size_t N = 1UL>
    Sequence::Value tryClaim(std::size_t count = N) {
        static_assert(N > 0);
        const auto amount = static_cast<Sequence::Value>(std::min(count, Base::capacity()));
        const auto next = next_ + amount;
        const auto wrap = next - static_cast<Sequence::Value>(Base::capacity());
        if (wrap > Base::barrier_.minimumSequence()) return Sequence::Initial;
        next_ = next;
        return next;
    }

    bool published(Sequence::Value sequence) const { return sequence <= Base::barrier_.last(); }

    void publish(Sequence::Value sequence) {
        Base::barrier_.release(sequence);
        Base::wait_.signalAllWhenBlocking();
    }

    void publish(Sequence::Value, Sequence::Value hi) {
        Base::barrier_.release(hi - 1);
        Base::wait_.signalAllWhenBlocking();
    }

    Sequence::Value consume(Sequence::Value sequence) const { return Base::wait_.wait(sequence, Base::cursor()); }
    Sequence::Value consumable(Sequence::Value sequence) const {
        const auto published = Base::barrier_.last();
        return sequence <= published ? published : Sequence::Initial;
    }
};

template <Waits::Concept WaitType, Barriers::Concept BarrierType = Barrier&>
struct MPMC : Producer<WaitType, BarrierType, MPMC<WaitType, BarrierType>> {
    using Base = Producer<WaitType, BarrierType, MPMC<WaitType, BarrierType>>;

    const std::size_t mask_;
    std::vector<Sequence::Atomic> internal_{};
    std::span<Sequence::Atomic> published_{};

    MPMC(BarrierType& barrier, std::size_t capacity, WaitType& wait, std::span<Sequence::Atomic> external = {})
        : Base{barrier, wait, capacity}, mask_{capacity - 1}, internal_{}, published_{external} {
        if (external.empty()) {  // Allocate its own array for published sequences
            internal_.resize(capacity);
            published_ = internal_;
        } else {
            if (external.size() != capacity) {
                throw std::invalid_argument{
                    std::format("External storage size({}) != capacity({})", external.size(), capacity)};
            }
        }

        for (auto& i : published_) i.store(Sequence::Initial);
    }

    inline Sequence::Value next() const { return Base::barrier_.last() + 1; }

    std::size_t available() const noexcept {
        return Base::capacity() - static_cast<std::size_t>(next() - Base::barrier_.minimumSequence(next() - 1)) + 1;
    }

    template <std::size_t N = 1UL>
    Sequence::Value claim(std::size_t count = N) {
        static_assert(N > 0);
        auto amount = static_cast<Sequence::Value>(std::min(count, Base::capacity()));
        const auto current = Base::barrier_.last_.add(amount) + 1;
        const auto next = current + amount;
        const auto wrap = next - static_cast<Sequence::Value>(Base::capacity());

        Sequence::Value gating = 0;
        Waits::SpinsStrategy::Spinner spinner;
        while (wrap > (gating = Base::barrier_.minimumSequence(current))) {
            spinner.once();
        }

        return next;
    }

    template <std::size_t N = 1UL>
    Sequence::Value tryClaim(std::size_t count = N) {
        static_assert(N > 0);
        const auto amount = static_cast<Sequence::Value>(std::min(count, Base::capacity()));

        Sequence::Value current;  // NOLINT(cppcoreguidelines-init-variables)
        Sequence::Value next;     // NOLINT(cppcoreguidelines-init-variables)
        do {                      // NOLINT(cppcoreguidelines-avoid-do-while)
            current = Base::barrier_.last();
            next = current + amount;
            const auto wrap = next - static_cast<Sequence::Value>(Base::capacity());
            if (wrap > Base::barrier_.minimumSequence()) return Sequence::Initial;
        } while (!Base::barrier_.last_.cas(current, next));
        return next + 1;
    }

    void setAvailable(Sequence::Value sequence) {
        auto& seq = published_[static_cast<std::size_t>(sequence) & mask_];
        assert(seq.load() == Sequence::Initial ||
               seq.load() == (sequence - static_cast<Sequence::Value>(this->capacity_)));
        seq.store(sequence);
    }

    bool published(Sequence::Value sequence) const {
        return published_[static_cast<std::size_t>(sequence) & mask_].load() == sequence;
    }

    void publish(Sequence::Value sequence) {
        setAvailable(sequence);
        this->wait_.signalAllWhenBlocking();
    }

    void publish(Sequence::Value lo, Sequence::Value hi) {
        for (auto i = lo; i < hi; ++i) {
            setAvailable(i);
        }
        Base::wait_.signalAllWhenBlocking();
    }

    Sequence::Value isAvailableNext(Sequence::Value lastKnownPublished) const {
        // Prefetch next sequences for availability
        Sequence::Value seq = lastKnownPublished + 1;
        // Prefetch up to end of current cache line
        static_assert(CacheLineSize / sizeof(Sequence::Atomic) == 8);
        Sequence::Value limit = lastKnownPublished | (4 - 1);
        for (; seq <= limit; ++seq) {
            if (!published(seq)) {
                return seq - 1;
            }
        }

        return limit;
    }

    Sequence::Value consume(Sequence::Value sequence) const {
        if (!published(sequence)) {
            Base::wait_.wait(sequence, &published_[static_cast<std::size_t>(sequence) & mask_]);
        }
        return isAvailableNext(sequence);
    }

    Sequence::Value consumable(Sequence::Value sequence) const {
        if (!published(sequence)) return Sequence::Initial;
        return isAvailableNext(sequence);
    }
};

static_assert(Concept<SPMC<Waits::BlockStrategy>>);
static_assert(Concept<MPMC<Waits::BlockStrategy>>);

}  // namespace Sequencers

template <typename IndexerType, typename Item>
concept Indexer = requires(IndexerType p, Sequence::Value sequence) {
    { p.operator[](sequence) } noexcept -> std::same_as<Item&>;
    { p.size() } noexcept -> std::same_as<std::size_t>;
    { p.data() } noexcept -> std::same_as<Item*>;
};

template <typename T>
struct Ring : public std::span<T> {
    using std::span<T>::size;
    using std::span<T>::data;
    const std::size_t mask_;

    explicit constexpr Ring(std::span<T> buffer) : std::span<T>{buffer}, mask_{size() - 1} {
        if (data() == nullptr) {
            throw std::invalid_argument{"Ring pointer is nullptr"};
        }
        if (!isPowerOf2(size())) {
            throw std::invalid_argument{"Ring size must be power-of-two, value:" + std::to_string(size())};
        }
    }
    // clang-format off
          T& operator[](std::ptrdiff_t seq)       noexcept { return data()[static_cast<std::size_t>(seq) & mask_]; }
    const T& operator[](std::ptrdiff_t seq) const noexcept { return data()[static_cast<std::size_t>(seq) & mask_]; }
    // clang-format on
};

static_assert(Indexer<Ring<int>, int>);

namespace Processor {

// Describes how consumed T should be released
enum class ConsumedType : std::uint8_t { Exit, Release, Keep };

template <typename HandlerType, typename T>
concept Handler = requires(HandlerType h, bool running, T& data, Sequence::Value sequence, std::size_t available,
                           std::exception_ptr eptr) {
    { h.onRunning(running) } noexcept -> std::same_as<void>;
    { h.onBatch(sequence, available) } -> std::same_as<void>;
    { h.onConsume(data, sequence, available) } -> std::same_as<ConsumedType>;
    { h.onTimeout(sequence) } -> std::same_as<void>;
    { h.onException(eptr, sequence) } -> std::same_as<void>;
};

template <typename T>
struct DefaultHandler {
    using Consumed = ConsumedType;
    void onRunning(bool) noexcept {}
    void onBatch(Sequence::Value, std::size_t) {}
    Consumed onConsume(T&, Sequence::Value, std::size_t) { return Consumed::Release; }
    void onTimeout(Sequence::Value) {}
    void onException(const std::exception_ptr& eptr, Sequence::Value sequence) noexcept(false) {
        try {
            if (eptr) std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            throw std::runtime_error("exception on #" + std::to_string(sequence) + " what:" + std::string{e.what()});
        } catch (...) {
            std::throw_with_nested(std::runtime_error("exception on #" + std::to_string(sequence)));
        }
    }
};

static_assert(Handler<DefaultHandler<int>, int>);

template <typename T, Sequencers::Consume Barrier, Indexer<T> Indexer, Handler<T> Handler>
struct Batch {
    Barrier& barrier_;
    Indexer& indexer_;
    Handler& handler_;
    std::atomic_flag running_;
    std::atomic_flag halting_;

    Batch([[maybe_unused]] const T& dummy, Barrier& barrier, Indexer& indexer, Handler& handler)
        : barrier_{barrier}, indexer_{indexer}, handler_{handler} {}

    void run() {
        if (running_.test_and_set()) throw std::runtime_error{"Processor is already running"};
        halting_.clear();
        handler_.onRunning(true);

        auto next = barrier_.last() + 1;
        while (!halting_.test(std::memory_order_consume)) try {
                const auto available = barrier_.consume(next);
                handler_.onBatch(next, static_cast<std::size_t>(available - next + 1));
                while (next <= available) {
                    switch (handler_.onConsume(indexer_[next], next, static_cast<std::size_t>(available - next))) {
                        case ConsumedType::Exit:
                            halting_.test_and_set();
                            [[fallthrough]];
                        case ConsumedType::Release:
                            barrier_.release(next);
                            [[fallthrough]];
                        case ConsumedType::Keep:
                            break;
                    }
                    ++next;
                }
                barrier_.release(available);
            } catch (...) {
                try {
                    handler_.onException(std::current_exception(), next);
                } catch (...) {
                    handler_.onRunning(false);
                    running_.clear();
                    running_.notify_all();
                    throw;
                }
            }

        handler_.onRunning(false);
        running_.clear();
        running_.notify_all();
    }

    bool running() const noexcept { return running_.test(); }
    void halt() {
        halting_.test_and_set();
        running_.wait(true);
    }
};

}  // namespace Processor

// Move template implementation to Disruptor.cpp and remove these extern
extern template struct Group<8>;
extern template struct Barriers::PointerBarrier<8>;
extern template struct Barriers::AtomicsBarrier<8>;
extern template struct Barriers::OffsetsBarrier<8>;

extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>>;

extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>&>;

extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>>;

extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>&>;
extern template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>&>;

}  // namespace dlsm::Disruptor
