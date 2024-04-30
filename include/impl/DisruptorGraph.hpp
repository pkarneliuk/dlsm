#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
#include <type_traits>
#include <typeinfo>

#include "impl/Disruptor.hpp"

namespace dlsm::Disruptor::Graph {

enum class Type : std::uint8_t { SPSC, SPMC, MPMC };
enum class Wait : std::uint8_t { Spins, Yield, Block, Share };
enum class Stat : std::uint8_t { Empty = 0, Init, Ready, Updating };

struct Layout {
    struct Graph {
        Type type_{Type::SPSC};
        Wait wait_{Wait::Block};
    };

    struct Slots {
        std::size_t maxPub_{0};
        std::size_t maxSub_{0};
        std::size_t numPub_{0};
        std::size_t numSub_{0};
    };

    struct Items {
        std::size_t capacity_{0};
        std::size_t size_{0};
        std::size_t align_{1};
        std::size_t hash_{0};
        std::array<char, 32> type_{'\0'};

        std::string_view type() const { return {type_.data()}; }
        void type(std::string_view v) {
            auto len = std::min(std::size(type_) - 1, std::size(v));
            auto end = std::copy_n(std::begin(v), len, std::begin(type_));
            std::fill(end, std::end(type_), '\0');
        }

        template <typename T>
        static Items create(std::size_t n = 0, std::string_view name = {}) {
            static_assert(std::is_standard_layout_v<T>);
            auto& info = typeid(T);
            auto items = Items{n, sizeof(T), alignof(T), info.hash_code()};
            items.type(name.empty() ? std::string_view{info.name()} : name);
            return items;
        }
    };

    Graph graph_;
    Slots slots_;
    Items items_;

    Layout() = default;
    Layout(const Graph& g, const Slots& s, const Items& i) : graph_{g}, slots_{s}, items_{i} {}

    std::size_t size() const;
    // Runtime checks, will throw exceptions
    void check(const Graph& graph) const;
    void check(const Slots& slots) const;
    void check(const Items& items) const;
    void check(const Layout& that) const;
};

struct IGraph {
    virtual ~IGraph() = default;

    using Ptr = std::shared_ptr<IGraph>;

    struct IPub {
        virtual ~IPub() = default;
        using Ptr = std::shared_ptr<IPub>;
        virtual std::size_t available() const noexcept = 0;
        virtual Sequence::Value next() const noexcept = 0;
        virtual Sequence::Value claim(std::size_t amount = 1) = 0;
        virtual Sequence::Value tryClaim(std::size_t amount = 1) = 0;
        virtual void publish(Sequence::Value last) = 0;
    };

    struct ISub {
        virtual ~ISub() = default;
        using Ptr = std::shared_ptr<ISub>;
        virtual std::size_t available() const noexcept = 0;
        virtual Sequence::Value last() const noexcept = 0;
        virtual Sequence::Value consume(Sequence::Value next) = 0;
        virtual Sequence::Value consumable(Sequence::Value next) = 0;
        virtual void release(Sequence::Value last) = 0;
    };

    using SubList = std::span<const std::string_view>;

    virtual IPub::Ptr pub(std::string_view name = {}) = 0;
    virtual ISub::Ptr sub(std::string_view name = {}, SubList dependencies = {}) = 0;

    virtual std::vector<std::string> dependencies(std::string_view name = {}) const = 0;
    virtual std::string description() const = 0;
    virtual std::size_t capacity() const = 0;
    virtual const Layout& layout() const = 0;
    virtual const std::span<std::byte> items() const = 0;

    template <typename T>
    Ring<T> ring(std::string_view name = {}) const {
        auto& vla = layout();
        vla.check(Layout::Items::create<T>(0, name));
        auto bytes = items();
        // C++23 std::start_lifetime_as_array
        return Ring<T>{{std::launder(reinterpret_cast<T*>(bytes.data())), bytes.size() / vla.items_.size_}};
    }

    static Ptr create(Type type, Wait wait, Layout::Items items = {1024});
    static Ptr inproc(const Layout& required, std::span<std::byte> space);
    static Ptr shared(const Layout& required, const std::string& opts, std::string_view attaching = "100x1");
};

}  // namespace dlsm::Disruptor::Graph
