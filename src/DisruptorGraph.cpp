#include "impl/DisruptorGraph.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

#include "impl/Memory.hpp"
#include "impl/SharedMemory.hpp"

// Need features:
// - customization BarrierStorage = Barrier for inproc and Barrier& for shared memory
// - recalculation dependencies graph after ~IPub()/~ISub()
// - implementation SPSC

using namespace dlsm::Disruptor::Graph;

constexpr std::string_view str(Type v) {
    // C++23 std::to_underlying()
    const auto i = static_cast<std::underlying_type_t<Type>>(v);
    constexpr std::array<const char*, 3> strs{"SPSC", "SPMC", "MPMC"};
    return strs[i];  // NOLINT
}
constexpr std::string_view str(Wait v) {
    // C++23 std::to_underlying()
    const auto i = static_cast<std::underlying_type_t<Type>>(v);
    constexpr std::array<const char*, 4> strs{"Spins", "Yield", "Block", "Share"};
    return strs[i];  // NOLINT
}
constexpr std::string_view str(Stat v) {
    // C++23 std::to_underlying()
    const auto i = static_cast<std::underlying_type_t<Type>>(v);
    constexpr std::array<const char*, 4> strs{"Empty", "Init", "Ready", "Updating"};
    return strs[i];  // NOLINT
}

namespace dlsm::Disruptor::Graph {

// Slot is a Barrier with some metainformation in standard layout memory
// SHOULD be replaced with external polymorphism and C++23 std::flat_map<>
struct alignas(CacheLineSize) Slot {
    Barrier barrier_;
    dlsm::Str::Flat<16> name_;

    Slot() = default;

    Barrier& barrier() { return barrier_; }
    std::string_view name() const { return name_; }
    void name(std::string_view v) { name_ = v; }
    std::vector<const Slot*> dependencies() const {
        std::vector<const Slot*> ptrs;
        static_assert(offsetof(Slot, barrier_) == 0);
        static_assert(offsetof(Slot, barrier_.last_) == 0);

        for (auto ptr : barrier_.dependencies().items_) {
            if (ptr) {
                static_assert(std::is_same_v<decltype(ptr), Sequence::Ptr>);
                // Convert dependency const Sequence* to its aggregator const Slot*
                ptrs.emplace_back(reinterpret_cast<const Slot*>(ptr));
            }
        }
        return ptrs;
    }
};

static_assert(std::is_standard_layout_v<Slot>);
static_assert(alignof(Slot) == CacheLineSize);
}  // namespace dlsm::Disruptor::Graph

template <>
struct std::formatter<Layout> : std::formatter<std::string> {
    auto format(const Layout& v, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format(
                "items:{} slots:{} graph:{}  size:{}",
                std::format("'{}' {:2}@{}x{}({} bytes)", v.items_.type(), v.items_.size_, v.items_.align_,
                            v.items_.capacity_, v.items_.size_ * v.items_.capacity_),
                std::format("{}x{}({}x{})", v.slots_.numPub_, v.slots_.numSub_, v.slots_.maxPub_, v.slots_.maxSub_),
                std::format("{}({})", str(v.graph_.type_), str(v.graph_.wait_)), v.size()),
            ctx);
    }
};

namespace dlsm::Disruptor::Graph {

template <typename T>
T castTo(const auto& bytes) {
    return std::launder(reinterpret_cast<T>(std::data(bytes)));
}

template <typename U>
std::span<std::byte> nextTo(const U& that, std::size_t align, std::size_t size, std::size_t n) {
    void* next = const_cast<U*>(&that);  // NOLINT
    std::size_t bytes = size * n;
    std::size_t space = align + bytes;

    void* aligned = std::align(align, bytes, next, space);
    // C++23 std::start_lifetime_as_array
    return {std::launder(reinterpret_cast<std::byte*>(aligned)), bytes};
}

template <typename T, typename U>
std::span<T> nextTo(const U& that, std::size_t n = 1) {
    auto bytes = nextTo(that, alignof(T), sizeof(T), n);
    return {castTo<T*>(bytes), n};
}

struct alignas(CacheLineSize) VLA : Layout {
    std::atomic<Stat> state_{Stat::Empty};  // Kind of SeqLock

    std::span<Slot> slots() {
        const auto count = (slots_.maxPub_ > 0 ? 1 : 0) + slots_.maxSub_;
        return nextTo<Slot>(*(this + 1), count);
    }

    std::span<std::byte> wait() {
        // clang-format off
        const auto [align, size] = [&]() -> std::pair<std::size_t, std::size_t> {
            using namespace dlsm::Disruptor::Waits;
            switch (graph_.wait_) {
                case Wait::Spins: return {alignof(SpinsStrategy), sizeof(SpinsStrategy)};
                case Wait::Yield: return {alignof(YieldStrategy), sizeof(YieldStrategy)};
                case Wait::Block: return {alignof(BlockStrategy), sizeof(BlockStrategy)};
                case Wait::Share: return {alignof(ShareStrategy), sizeof(ShareStrategy)};
            }
            return {0ULL, 0ULL};
        }();
        // clang-format on
        return nextTo(*slots().end(), align, size, 1);
    }

    std::span<Sequence::Atomic> sequences() {
        const auto size = (graph_.type_ == Type::MPMC) ? items_.capacity_ : 0;
        return nextTo<Sequence::Atomic>(*wait().end(), size);
    }

    std::span<std::byte> items() { return nextTo(*sequences().end(), items_.align_, items_.size_, items_.capacity_); }

    std::size_t size() {
        auto begin = reinterpret_cast<const std::byte*>(this);
        auto end = &*items().end();
        return static_cast<std::size_t>(end - begin) + alignof(VLA);
    }

    VLA(const Layout& that) : Layout{that} {}
    VLA(const Layout& that, Stat state) : Layout{that} {
        dlsm::Memory::checkAlignment(&that);
        const auto s = slots();
        dlsm::Memory::checkAlignment(std::data(s));
        std::uninitialized_default_construct(std::begin(s), std::end(s));
        // clang-format off
        using namespace dlsm::Disruptor::Waits;
        switch(graph_.wait_) {
            case Wait::Spins: std::construct_at(castTo<SpinsStrategy*>(wait())); break;
            case Wait::Yield: std::construct_at(castTo<YieldStrategy*>(wait())); break;
            case Wait::Block: std::construct_at(castTo<BlockStrategy*>(wait())); break;
            case Wait::Share: std::construct_at(castTo<ShareStrategy*>(wait())); break;
        }
        // clang-format on
        const auto seq = sequences();
        dlsm::Memory::checkAlignment(std::data(seq));
        std::uninitialized_default_construct(std::begin(seq), std::end(seq));
        // Fill items by zeroes(no ctor & dtor)
        auto i = items();
        dlsm::Memory::checkAlignment(std::data(i), that.items_.align_);
        std::fill(std::begin(i), std::end(i), std::byte{0});
        state_.store(state);
    }
    ~VLA() {
        if (state_.load() == Stat::Empty) return;
        std::destroy(std::begin(slots()), std::end(slots()));
        // clang-format off
        using namespace dlsm::Disruptor::Waits;
        switch(graph_.wait_) {
            case Wait::Spins: std::destroy_at(castTo<SpinsStrategy*>(wait())); break;
            case Wait::Yield: std::destroy_at(castTo<YieldStrategy*>(wait())); break;
            case Wait::Block: std::destroy_at(castTo<BlockStrategy*>(wait())); break;
            case Wait::Share: std::destroy_at(castTo<ShareStrategy*>(wait())); break;
        }
        // clang-format on
        std::destroy(std::begin(sequences()), std::end(sequences()));
        static_cast<Layout>(*this) = Layout{};  // NOLINT(cppcoreguidelines-slicing)
        state_.store(Stat::Empty);
    }
};

std::size_t Layout::size() const { return VLA{*this}.size(); }

void Layout::check(const Graph& that) const {
    if ((graph_.type_ != that.type_) || (graph_.wait_ != that.wait_)) {
        throw std::runtime_error{std::format("Layout::Graph missmatch: type:{}={} wait:{}={}", ::str(graph_.type_),
                                             ::str(that.type_), ::str(graph_.wait_), ::str(that.wait_))};
    }
}
void Layout::check(const Slots& that) const {
    if ((slots_.maxPub_ != that.maxPub_) || (slots_.maxSub_ != that.maxSub_)) {
        throw std::runtime_error{std::format("Layout::Slots missmatch: maxPub:{}={} maxSub:{}={}", slots_.maxPub_,
                                             that.maxPub_, slots_.maxSub_, that.maxSub_)};
    }
}
void Layout::check(const Items& that) const {
    if ((items_.size_ != that.size_) || (items_.align_ != that.align_) || (items_.hash_ != that.hash_) ||
        (items_.type_ != that.type_)) {
        throw std::runtime_error{std::format("Layout::Items missmatch: size:{}={} align:{}={} type:{}={}", items_.size_,
                                             that.size_, items_.align_, that.align_, items_.type(), that.type())};
    }
}
void Layout::check(const Layout& that) const {
    check(that.graph_);
    check(that.slots_);
    check(that.items_);
}

struct Lock {  // Kind of SeqLock with waiting by sleep
    std::atomic<Stat>& state_;
    const Stat old_;

    Lock(std::atomic<Stat>& s, Stat expected = Stat::Ready, std::string_view wait = "500x1")
        : state_{s}, old_{expected} {
        if (state_.compare_exchange_strong(expected, Stat::Updating)) return;
        auto r = dlsm::Str::xpair(wait);
        for (std::size_t i = 0; i < r.first; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(r.second));
            expected = Stat::Ready;
            if (state_.compare_exchange_strong(expected, Stat::Updating)) return;
        }

        throw std::runtime_error{
            std::format("IGraph state is:{} after {}x{}ms retries", str(expected), r.first, r.second)};
    }
    ~Lock() { state_.store(old_); }
};

struct ExternMemoryHolder {
    std::vector<std::byte> extern_;
    std::string description() const {
        if (extern_.empty()) return {};
        return std::format("In ExternMemory: size:{} address:{}", extern_.size(),
                           reinterpret_cast<const void*>(extern_.data()));
    }
};

struct SharedMemoryHolder {
    dlsm::SharedMemory::UPtr shared_;
    std::string description() const {
        if (!shared_) return {};
        return std::format("In SharedMemory:{}{} size:{} address:{}", shared_->name(),
                           (shared_->owner() ? "(owner)" : ""), shared_->size(), shared_->address());
    }
};

struct IGraphInternal : ExternMemoryHolder, SharedMemoryHolder, public IGraph {
    using Ptr = std::shared_ptr<IGraphInternal>;
};

template <Sequencers::Concept Sequencer>
struct IGraphImpl final : IGraphInternal, public std::enable_shared_from_this<IGraphImpl<Sequencer>> {
    static constexpr std::size_t Master = 0ULL;  // producer(s) slot index

    VLA& vla_;
    std::span<Slot> slots_;
    typename Sequencer::WaitStrategy& wait_;
    Sequencer impl_;

    IGraphImpl(VLA& vla)
        : vla_{vla},
          slots_{vla_.slots()},
          wait_{*castTo<typename Sequencer::WaitStrategy*>(vla_.wait())},
          impl_{slots_[Master].barrier(), vla_.items_.capacity_, wait_, vla_.sequences()} {
        if (vla_.state_.load() == Stat::Init) {
            slots_[Master].name("Master");
            vla_.state_.store(Stat::Ready);
        }
    }

    ~IGraphImpl() override {
        auto lock = Lock{vla_.state_};
        if (numPub() == 0 && numSub() == 0) {
            std::destroy_at(&vla_);
        }
    }

    struct Pub final : IPub {
        IGraphImpl& graph_;
        IGraph::Ptr hold_;

        Pub(IGraphImpl& graph) : graph_{graph}, hold_{graph.shared_from_this()} {}
        ~Pub() override { graph_.numPub() -= 1; }

        std::size_t available() const noexcept override { return graph_.impl_.available(); }
        Sequence::Value next() const noexcept override { return graph_.impl_.next(); }
        Sequence::Value claim(std::size_t amount) override { return graph_.impl_.claim(amount); }
        Sequence::Value tryClaim(std::size_t amount) override { return graph_.impl_.tryClaim(amount); }
        void publish(Sequence::Value last) override { return graph_.impl_.publish(last); }
    };

    template <typename Impl>
    struct Sub final : ISub, Impl {
        IGraphImpl& graph_;
        IGraph::Ptr hold_;
        Sub(IGraphImpl& graph, Slot& slot)
            : Impl{slot.barrier(), graph.impl_}, graph_{graph}, hold_{graph.shared_from_this()} {}
        ~Sub() override { graph_.numSub() -= 1; }

        std::size_t available() const noexcept override { return Impl::available(); }
        Sequence::Value last() const noexcept override { return Impl::last(); };
        Sequence::Value consume(Sequence::Value next) override { return Impl::consume(next); }
        Sequence::Value consumable(Sequence::Value next) override { return Impl::consumable(next); }
        void release(Sequence::Value last) override { return Impl::release(last); }
    };

    using Consumer = Sub<typename Sequencer::Consumer>;
    using Indirect = Sub<typename Sequencer::Indirect>;

    auto maxPub() const { return vla_.slots_.maxPub_; }
    auto maxSub() const { return vla_.slots_.maxSub_; }
    auto& numPub() { return vla_.slots_.numPub_; }
    auto& numSub() { return vla_.slots_.numSub_; }

    Slot& construct(std::string_view name, bool& allocated) {
        // Find if slot already allocated
        for (auto& slot : slots_) {
            if (std::string{slot.name()} == std::string{name}) {
                // std::cout << "exists: " << name << std::endl;
                allocated = false;
                return slot;
            }
        }
        // Allocate
        if (numSub() + 1 > maxSub()) {
            throw std::runtime_error{"Max consumer limit is reached:" + std::to_string(maxSub())};
        }
        const auto index = (numSub() += 1);

        // std::cout << "allocate: " << name << " index: " << index << std::endl;

        auto& slot = slots_[index];
        slot.name(name);
        allocated = true;
        return slot;
    }

    IPub::Ptr pub(std::string_view /*name*/) override {
        auto lock = Lock{vla_.state_};
        if (numPub() + 1 > maxPub()) {
            throw std::runtime_error{"Max producer limit is reached:" + std::to_string(maxPub())};
        }
        const auto pubs = (numPub() += 1);

        if (vla_.graph_.type_ == Type::SPMC && pubs > 1) {
            numPub() -= 1;
            throw std::runtime_error{"One publisher is already created for SPMC"};
        }

        auto result = std::make_shared<Pub>(*this);
        return result;
    }
    ISub::Ptr sub(std::string_view name, SubList dependencies) override {
        auto lock = Lock{vla_.state_};
        bool newone = false;
        auto& slot = construct(name, newone);

        const auto dsize = dependencies.size();
        if (dsize == 0) {
            auto result = std::make_shared<Consumer>(*this, slot);
            if (newone && !impl_.add(result->cursor())) {
                throw std::runtime_error("Exceeds Subscribers limit");
            }
            return result;
        }

        if (dsize > Group<>::MaxItems)
            throw std::invalid_argument("Dependencies list exceeds limit:" + std::to_string(Group<>::MaxItems));

        Group<> deps;
        for (const auto& dep : dependencies) {
            Sequence::Ptr cursor = nullptr;
            for (const auto& slot : slots_)
                if (slot.name() == dep) {
                    cursor = slot.barrier_.cursor();
                    break;
                }

            if (!cursor) {  // Dependency is not found, allocate it
                bool fallocated = false;
                cursor = construct(dep, fallocated).barrier_.cursor();
            }
            deps.add(cursor);
        }

        auto result = std::make_shared<Indirect>(*this, slot);
        result->depends(deps);

        // Remove dependencies from Master
        impl_.replace(deps, nullptr);

        // Add dependency to this sub if it is new one
        if (newone && !impl_.add(result->cursor())) {
            throw std::runtime_error("Exceeds Subscribers limit");
        }
        return result;
    }

    std::vector<std::string> dependencies(std::string_view name) const override {
        auto lock = Lock{vla_.state_};

        const auto names = [](const auto& slots) {
            std::vector<std::string> result{};
            result.reserve(slots.size());
            for (const auto& slot : slots) {
                result.emplace_back(slot->name());
            }
            return result;
        };

        if (!name.empty()) {
            for (const auto& slot : slots_) {
                if (slot.name() == name) {
                    return names(slot.dependencies());
                }
            }
        }
        return names(slots_[Master].dependencies());
    }

    std::string description() const override {
        const auto capacity = static_cast<double>(this->capacity());
        const auto print = [&](const Slot& s) -> std::string {
            const auto name = s.name();
            const auto last = s.barrier_.last();
            const auto deps = s.dependencies();

            if (name.empty() && last == dlsm::Disruptor::Sequence::Initial && deps.empty()) return "Empty";

            std::string dstr = "Empty";
            if (!deps.empty()) {
                std::string list;
                for (bool first = true; const auto& d : deps) {
                    if (first)
                        first = false;
                    else
                        list += ", ";
                    list += std::format("{:6}", d->name());
                }
                dstr = std::format("{}x[{}]", deps.size(), list);
            }
            double available = static_cast<double>(dlsm::Disruptor::available(s.barrier_)) / capacity * 100.0;
            return std::format("name: {:8} last: {:2} full: {:2}% depends: {}", name, last, available, dstr);
        };

        auto lock = Lock{vla_.state_};
        std::string str;
        str += ExternMemoryHolder::description() + '\n';
        str += SharedMemoryHolder::description() + '\n';
        str += std::format("state:{} {}\nSlot{:<2} {} pubs: {}\n", ::str(lock.old_), layout(), Master,
                           print(slots_[Master]), vla_.slots_.numPub_);
        for (std::size_t i = Master + 1; i < slots_.size(); ++i) {
            str += std::format("Slot{:<2} {}\n", i, print(slots_[i]));
        }
        return str;
    }

    std::size_t capacity() const override { return impl_.capacity(); }
    const Layout& layout() const override { return vla_; }
    const std::span<std::byte> items() const override { return vla_.items(); }
};

IGraphInternal::Ptr createTypeWait(VLA& layout) {
    using namespace dlsm::Disruptor::Waits;
    using namespace dlsm::Disruptor::Sequencers;
    // clang-format off
    switch(layout.graph_.type_) {
    case Type::SPSC: throw std::invalid_argument{"SPSC is not implemented"};
    case Type::SPMC:
        switch(layout.graph_.wait_) {
        case Wait::Spins: return std::make_shared<IGraphImpl<SPMC<SpinsStrategy>>>(layout);
        case Wait::Yield: return std::make_shared<IGraphImpl<SPMC<YieldStrategy>>>(layout);
        case Wait::Block: return std::make_shared<IGraphImpl<SPMC<BlockStrategy>>>(layout);
        case Wait::Share: return std::make_shared<IGraphImpl<SPMC<ShareStrategy>>>(layout);
        }
        break;
    case Type::MPMC:
        switch(layout.graph_.wait_) {
        case Wait::Spins: return std::make_shared<IGraphImpl<MPMC<SpinsStrategy>>>(layout);
        case Wait::Yield: return std::make_shared<IGraphImpl<MPMC<YieldStrategy>>>(layout);
        case Wait::Block: return std::make_shared<IGraphImpl<MPMC<BlockStrategy>>>(layout);
        case Wait::Share: return std::make_shared<IGraphImpl<MPMC<ShareStrategy>>>(layout);
        }
        break;
    default: break;
    }
    // clang-format on
    throw std::invalid_argument{"unsupported arguments"};
}

IGraphInternal::Ptr create(const Layout& required, VLA& allocated, std::string_view r = "100x1") {
    if (required.graph_.type_ == Type::SPMC && required.slots_.maxPub_ != 1) {
        throw std::invalid_argument{"Type::SPMC supports only one producer, current limit:" +
                                    std::to_string(required.slots_.maxPub_)};
    }

    auto expected = Stat::Empty;
    if (allocated.state_.compare_exchange_strong(expected, Stat::Init)) {
        std::construct_at(&allocated, required, Stat::Init);
        // Construction in allocated memory
        return createTypeWait(allocated);
    }

    if (expected == Stat::Init) {  // Waiting construction in separate thread/process
        auto retries = dlsm::Str::xpair(r);
        for (std::size_t i = 0; i < retries.first; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retries.second));
            expected = allocated.state_.load();
            if (expected == Stat::Ready) break;
        }
    }

    // 3) attaching to memory with external ownership(check provided, no ownership)
    auto lock = Lock{allocated.state_};
    required.check(allocated);
    // Attaching to already created memory
    return Graph::createTypeWait(allocated);
}

IGraphInternal::Ptr create(const Layout& required, std::span<std::byte> space) {
    if (required.size() > space.size()) {
        throw std::invalid_argument{
            std::format("IGraph::Layout requires {} bytes, only {} provided", required.size(), space.size())};
    }
    // 2) construction in memory with external ownership (initialization, no ownership)
    VLA& layout = nextTo<VLA>(space.front()).front();
    layout.state_ = Stat::Empty;
    return Graph::create(required, layout);
}

IGraph::Ptr IGraph::create(Type type, Wait wait, Layout::Items items) {
    // 1) construction in memory with internal ownership (initialization, with ownership)
    items.capacity_ = ceilingNextPowerOfTwo(items.capacity_);
    Layout required{{type, wait}, {(type == Type::MPMC ? 4U : 1U), 4}, items};
    // Allocate storage for required layout and move its ownership to internal
    auto internal = std::vector<std::byte>(required.size());
    auto graph = Graph::create(required, internal);
    graph->extern_ = std::move(internal);
    return graph;
}

IGraph::Ptr IGraph::inproc(const Layout& required, std::span<std::byte> space) {
    return Graph::create(required, space);
}

IGraph::Ptr IGraph::shared(const Layout& required, const std::string& opts, std::string_view attaching) {
    if (required.graph_.wait_ == Wait::Block) {
        throw std::invalid_argument{"Wait::Block is not allowed for Layout in Shared Memory"};
    }
    auto smem = dlsm::SharedMemory::create(opts + ",size=" + std::to_string(required.size()));
    auto& allocated = smem->reference<VLA>();
    auto graph = Graph::create(required, allocated, attaching);
    graph->shared_ = std::move(smem);
    return graph;
}

}  // namespace dlsm::Disruptor::Graph
