#include <barrier>
#include <thread>

#include "impl/Disruptor.hpp"
#include "unit/Unit.hpp"

using namespace dlsm::Disruptor;

static_assert(ceilingNextPowerOfTwo(1) == 2);
static_assert(ceilingNextPowerOfTwo(3) == 4);
static_assert(ceilingNextPowerOfTwo(7) == 8);
static_assert(ceilingNextPowerOfTwo(8) == 8);

static_assert(isPowerOf2(1) == true);
static_assert(isPowerOf2(2) == true);
static_assert(isPowerOf2(4) == true);
static_assert(isPowerOf2(0) == false);
static_assert(isPowerOf2(3) == false);
static_assert(isPowerOf2(5) == false);

namespace {
template <Sequencers::Concept S>
void SequenceClaimPublishWaitRelease(S& p) {
    EXPECT_EQ(p.capacity(), 8);
    EXPECT_EQ(p.dependencies().size(), 0);
    EXPECT_EQ(p.available(), p.capacity());

    Barrier cb;
    auto c = typename std::decay_t<decltype(p)>::Consumer(cb, p);
    EXPECT_EQ(c.dependencies().size(), 1);
    EXPECT_EQ(c.available(), 0) << "nothig is available";
    EXPECT_EQ(c.last(), p.next() - 1) << "created Consumer points to already published seq";
    p.depends(group(c));
    EXPECT_EQ(p.dependencies().size(), 1);
    // EXPECT_EQ(p.last(), Sequence::Initial) << typeid(p).name();
    EXPECT_EQ(p.next(), 0);

    {  // Claim next 1
        EXPECT_EQ(p.available(), p.capacity()) << "none was claimed";
        EXPECT_EQ(c.available(), 0) << "nothing was was claimed yet";
        EXPECT_EQ(p.claim(), 1);
        EXPECT_EQ(p.next(), 1);
        EXPECT_EQ(p.barrier_.minimumSequence(), -1);
        EXPECT_EQ(p.available(), p.capacity() - 1) << "one was claimed";
        EXPECT_EQ(c.available(), 1) << "nothing was published yet but was claimed";
        // EXPECT_EQ(p.last(), -1);
        EXPECT_EQ(p.next(), 1) << "next claimed will be 1";

        EXPECT_FALSE(p.published(0));
        p.publish(0);
        EXPECT_TRUE(p.published(0));

        // EXPECT_EQ(p.last(), 0);
        EXPECT_EQ(p.next(), 1);
        EXPECT_EQ(c.available(), 1) << "one was published and is available for consumer";

        EXPECT_EQ(p.available(), p.capacity() - 1) << "one was published but not released";
        EXPECT_EQ(c.consume(0), 0) << "consume";
        EXPECT_EQ(c.last(), Sequence::Initial);
        EXPECT_EQ(c.available(), 1) << "it was not released yet and is still available";
        c.release(0);
        EXPECT_EQ(p.available(), p.capacity()) << "one was published and released by consumer";
        EXPECT_EQ(c.available(), 0) << "nothing was published";
        EXPECT_EQ(c.last(), 0);
    }

    Barrier ib;  // Join IndirectConsumer and Claim next 4
    [[maybe_unused]] auto i = typename std::decay_t<decltype(p)>::Indirect(ib, p.wait_, group(c));
    EXPECT_EQ(i.dependencies().size(), 1);
    EXPECT_EQ(i.last(), c.last()) << "created Indirect points to last Consumer seq";
    EXPECT_EQ(i.available(), 0) << "nothig is available";
    p.depends(group(i));  // replace dependency p -> c to p -> i, so graph is like p -> c -> i
    {
        EXPECT_EQ(p.available(), p.capacity()) << "none was claimed";
        EXPECT_EQ(c.available(), 0) << "nothing was was claimed yet";
        EXPECT_EQ(p.claim(4), 5);
        EXPECT_EQ(p.available(), p.capacity() - 4) << "4 were claimed";
        // EXPECT_EQ(p.last(), 4 or 0); ???
        EXPECT_EQ(p.next(), 5);
        EXPECT_EQ(c.consumable(2), Sequence::Initial);
        EXPECT_FALSE(p.published(4));
        p.publish(1, 5);
        EXPECT_TRUE(p.published(4));
        EXPECT_FALSE(p.published(5));
        EXPECT_EQ(p.last(), 4);
        EXPECT_EQ(p.next(), 5);
        EXPECT_EQ(c.available(), 4);
        EXPECT_EQ(i.available(), 0) << "published new 4 items, but c has not processed them yet";

        EXPECT_EQ(p.available(), p.capacity() - 4) << "4 were published but not released";
        EXPECT_EQ(c.consumable(5), Sequence::Initial);
        // EXPECT_EQ(c.consumable(2), 4); MPMC returns 3 because of limit to scan next pub seqs
        EXPECT_EQ(c.consumable(4), 4);
        EXPECT_EQ(c.consume(4), 4) << "consume by c";
        EXPECT_EQ(c.last(), 0);
        EXPECT_EQ(c.available(), 4);

        EXPECT_EQ(i.consumable(1), Sequence::Initial);
        EXPECT_EQ(i.available(), 0) << "c has not released 4 items";
        c.release(4);

        EXPECT_EQ(i.available(), 4) << "c has released 4 items and they are available";
        EXPECT_EQ(i.consumable(5), Sequence::Initial);
        EXPECT_EQ(i.consumable(2), 4);
        EXPECT_EQ(i.consumable(4), 4);
        EXPECT_EQ(i.consume(4), 4) << "consume by i";
        i.release(4);
        EXPECT_EQ(i.available(), 0);

        EXPECT_EQ(p.available(), p.capacity()) << "4 were published and released by both consumers";
        EXPECT_EQ(c.available(), 0);
        EXPECT_EQ(c.last(), 4);
        EXPECT_EQ(i.available(), 0);
        EXPECT_EQ(i.last(), 4);
    }

    {  // tryClaim next 2
        EXPECT_EQ(p.available(), p.capacity()) << "none was claimed";
        EXPECT_EQ(p.next(), 5);
        EXPECT_EQ(p.last(), 4);
        EXPECT_EQ(p.tryClaim(2), 7);
        EXPECT_EQ(p.available(), p.capacity() - 2) << "2 were claimed";
        EXPECT_EQ(c.available(), 2);
        EXPECT_EQ(i.available(), 0);
        EXPECT_EQ(p.next(), 7);
        // EXPECT_EQ(p.last(), 6 MPMC or 4 SPMC);
        EXPECT_EQ(p.tryClaim(7), Sequence::Initial) << "7 no available for immidiate claming";

        EXPECT_FALSE(p.published(6));
        p.publish(5, 7);
        EXPECT_TRUE(p.published(6));

        EXPECT_EQ(p.available(), p.capacity() - 2) << "2 were claimed";
        EXPECT_EQ(p.last(), 6);
        EXPECT_EQ(p.next(), 7);
        EXPECT_EQ(p.tryClaim(7), Sequence::Initial) << "7 no available for immidiate claming";
        EXPECT_EQ(p.available(), p.capacity() - 2) << "2 were claimed";
        EXPECT_EQ(p.last(), 6);
        EXPECT_EQ(p.next(), 7);
    }
}

}  // namespace

TEST(Disruptor, Sequence) {
    EXPECT_EQ(Sequence{}.load(), Sequence::Initial);
    Sequence s{0ULL};
    EXPECT_EQ(s.load(), 0ULL);
    s.store(42);
    EXPECT_EQ(s.load(), 42ULL);
}

TEST(Disruptor, Group) {
    using Group = Group<>;

    Group g;
    EXPECT_EQ(g.size(), 0UL);
    Sequence seqs[Group::MaxItems] = {4, 3, 2, 1};     // NOLINT
    for (const auto& s : seqs) EXPECT_TRUE(g.add(s));  // NOLINT
    EXPECT_EQ(g.size(), std::size(seqs));
    Sequence additional;
    EXPECT_FALSE(g.add(additional.ptr())) << "Storage overflow, insertion failed";
    EXPECT_EQ(g.size(), std::size(seqs));

    EXPECT_TRUE(g.replace(seqs[0], nullptr));
    EXPECT_EQ(g.size(), std::size(seqs) - 1);

    EXPECT_TRUE(g.del(seqs[1].ptr()));
    EXPECT_EQ(g.size(), std::size(seqs) - 2);

    Sequence replacement{42};
    EXPECT_FALSE(g.del(replacement)) << "replacement is not in Group";
    EXPECT_FALSE(g.replace(seqs[0], replacement)) << "seqs[0] has been deleted before";
    EXPECT_FALSE(g.replace(seqs[1], replacement)) << "seqs[1] has been deleted before";
    EXPECT_EQ(g.size(), std::size(seqs) - 2);
    EXPECT_TRUE(g.replace(seqs[2], replacement)) << "seqs[2] deletion";
    EXPECT_EQ(g.size(), std::size(seqs) - 2);
}

TEST(Disruptor, Barriers) {
    const auto test = [](const auto& barrier) {
        using Barrier = std::decay_t<decltype(barrier)>;
        {
            Barrier empty;
            EXPECT_EQ(empty.size(), 0UL);
            EXPECT_EQ(empty.minimumSequence(), Sequence::Max);
            EXPECT_EQ(empty.minimumSequence(12UL), 12UL);
        }
        {
            Barrier b;
            Sequence A{5}, B{9};
            b.depends(Group{A.ptr(), B.ptr()});
            EXPECT_EQ(b.size(), 2);
            EXPECT_TRUE(b.contains(A));
            EXPECT_TRUE(b.contains(B));
            EXPECT_EQ(b.last(), 5);
            EXPECT_EQ(b.minimumSequence(), 5);
            Barrier b2 = b;
            EXPECT_TRUE(b2.contains(A));
            EXPECT_TRUE(b2.contains(B));
            EXPECT_EQ(b2.last(), 5);
            EXPECT_EQ(*b2.cursor(), 5);
            EXPECT_EQ(b2.minimumSequence(), 5);
            b2.depends(Group{});
            EXPECT_EQ(b2.size(), 0);
            EXPECT_EQ(b2.last(), Sequence::Initial);
            EXPECT_EQ(b2.minimumSequence(), Sequence::Max);
            EXPECT_EQ(b.last(), 5);
            EXPECT_EQ(*b.cursor(), 5);
            EXPECT_EQ(b.minimumSequence(), 5);
        }
        {
            Barrier b;
            Sequence seqs[Barrier::MaxItems] = {4, 3, 2, 1};   // NOLINT
            for (const auto& s : seqs) EXPECT_TRUE(b.add(s));  // NOLINT
            EXPECT_EQ(b.size(), std::size(seqs));
            Sequence additional;
            EXPECT_FALSE(b.add(additional)) << "Storage overflow, insertion failed";
            EXPECT_EQ(b.size(), std::size(seqs));

            EXPECT_FALSE(b.contains(nullptr));
            EXPECT_TRUE(b.contains(seqs[0]));
            EXPECT_TRUE(b.replace(seqs[0].ptr(), nullptr));
            EXPECT_FALSE(b.contains(seqs[0]));
            EXPECT_EQ(b.size(), std::size(seqs) - 1);

            EXPECT_TRUE(b.del(seqs[1]));
            EXPECT_EQ(b.size(), std::size(seqs) - 2);

            Sequence replacement{42};
            EXPECT_FALSE(b.del(replacement)) << "replacement is not in Group";
            EXPECT_FALSE(b.replace(seqs[0].ptr(), replacement)) << "seqs[0] has been deleted before";
            EXPECT_FALSE(b.replace(seqs[1].ptr(), replacement)) << "seqs[1] has been deleted before";
            EXPECT_EQ(b.size(), std::size(seqs) - 2);
            EXPECT_TRUE(b.replace(seqs[2].ptr(), replacement)) << "seqs[1] has been deleted before";
            EXPECT_EQ(b.size(), std::size(seqs) - 2);
            EXPECT_TRUE(b.contains(replacement));
            EXPECT_TRUE(b.contains(seqs[3]));

            Group<> removable{seqs[2].ptr(), seqs[3].ptr(), replacement.ptr()};
            Sequence newone{13};
            EXPECT_FALSE(b.contains(newone));
            EXPECT_TRUE(b.replace(removable, newone));
            EXPECT_FALSE(b.contains(seqs[2]));
            EXPECT_FALSE(b.contains(seqs[3]));
            EXPECT_FALSE(b.contains(replacement));
            EXPECT_TRUE(b.contains(newone));
            EXPECT_EQ(b.size(), std::size(seqs) - 3);
        }
        {
            Sequence A{3}, B{2};
            Barrier b;
            b.add(A);
            b.add(B);
            EXPECT_EQ(b.size(), 2);
            EXPECT_EQ(b.minimumSequence(), 2);
            EXPECT_EQ(available(b), 3);
            B.store(5);  // Replace 2 -> 5
            EXPECT_EQ(b.minimumSequence(), 3);
            EXPECT_EQ(available(b), 4);
            b.release(6);
            EXPECT_EQ(available(b), 3);
        }
    };

    const auto safe = [](const auto& barrier) {
        using Barrier = std::decay_t<decltype(barrier)>;
        {  // Check thread-safety of a Barrier
            Barrier b;
            std::barrier sync(2);
            const auto test = [&](std::size_t count) {
                std::array<Sequence, 2> seqs = {1, 2};
                EXPECT_EQ(b.size(), 0);
                sync.arrive_and_wait();
                for (std::size_t i = 0; i < count; ++i) {
                    EXPECT_LE(b.size(), seqs.size());
                    for (const auto& s : seqs) EXPECT_TRUE(b.add(s));
                    EXPECT_GE(b.size(), seqs.size());
                    for (const auto& s : seqs) EXPECT_TRUE(b.contains(s));
                    EXPECT_GE(b.size(), seqs.size());
                    for (const auto& s : seqs) EXPECT_TRUE(b.del(s));
                    EXPECT_LE(b.size(), seqs.size());
                }
                sync.arrive_and_wait();
                EXPECT_EQ(b.size(), 0);
            };

            std::jthread T1(test, 10000);
            std::jthread T2(test, 10000);
        }
    };

    using namespace dlsm::Disruptor::Barriers;
    test(PointerBarrier<>{});
    test(AtomicsBarrier<>{});
    test(OffsetsBarrier<>{});
    safe(AtomicsBarrier<>{});
    safe(OffsetsBarrier<>{});
}

TEST(Disruptor, Waits) {
    const auto test = [](auto strategy) {
        Sequence A{3}, B{2}, C{1};
        Barrier b;
        b.add(A);
        b.add(B);
        b.add(C);

        {
            std::jthread T([&] {
                // const auto to = std::chrono::microseconds{10};
                EXPECT_EQ(strategy.wait(0, A), 3);
                EXPECT_EQ(strategy.wait(0, b), 1);
                // EXPECT_EQ(strategy.waitFor(0, g, to), 1);
                // EXPECT_EQ(strategy.waitFor(5, g, to), 1);  // Timeout
            });
        }

        {
            std::jthread T([&] {
                EXPECT_EQ(strategy.wait(4, b), 5);
                EXPECT_EQ(A.load(), 7);
                EXPECT_EQ(B.load(), 6);
                EXPECT_EQ(C.load(), 5);
            });
            A.store(7);
            B.store(6);
            C.store(5);
            strategy.signalAllWhenBlocking();
        }
    };

    using namespace dlsm::Disruptor::Waits;
    test(SpinsStrategy{});
    test(YieldStrategy{});
    test(BlockStrategy{});
    test(ShareStrategy{});
}

TEST(Disruptor, SPMC) {
    Waits::BlockStrategy wait;
    Barrier barrier;

    {
        EXPECT_THAT([&] { Sequencers::SPMC(barrier, 3, wait); },
                    ThrowsMessage<std::invalid_argument>("Capacity must be power-of-two, value:3"));
    }

    Sequencers::SPMC p(barrier, 8, wait);
    SequenceClaimPublishWaitRelease(p);
}

TEST(Disruptor, MPMC) {
    Waits::BlockStrategy wait;
    Barrier barrier;

    {
        EXPECT_THAT([&] { Sequencers::MPMC(barrier, 3, wait); },
                    ThrowsMessage<std::invalid_argument>("Capacity must be power-of-two, value:3"));

        std::vector<Sequence::Atomic> external(16);
        EXPECT_THAT([&] { Sequencers::MPMC(barrier, 4, wait, external); },
                    ThrowsMessage<std::invalid_argument>("External storage size(16) != capacity(4)"));
    }

    Sequencers::MPMC p(barrier, 8, wait);
    EXPECT_EQ(p.published_.size(), p.capacity());
    constexpr auto I = Sequence::Initial;
    EXPECT_THAT(p.published_, ElementsAreArray(std::vector<Sequence::Atomic>{I, I, I, I, I, I, I, I}));
    SequenceClaimPublishWaitRelease(p);
    EXPECT_THAT(p.published_, ElementsAreArray(std::vector<Sequence::Atomic>{0, 1, 2, 3, 4, 5, 6, I}));
}

TEST(Disruptor, Ring) {
    long A3[3] = {0, 1, 2};                 // NOLINT
    EXPECT_THAT([&] { Ring<long> r(A3); },  // NOLINT
                ThrowsMessage<std::invalid_argument>("Ring size must be power-of-two, value:3"));

    EXPECT_THAT(
        [&] {
            Ring<long> r({A3, 0});  // NOLINT
        },                          // NOLINT
        ThrowsMessage<std::invalid_argument>("Ring size must be power-of-two, value:0"));

    const auto test0 = [&] { Ring<long> r({(long*)nullptr, 4}); };
    EXPECT_THAT([&] { test0(); }, ThrowsMessage<std::invalid_argument>("Ring pointer is nullptr"));

    auto A4 = std::array<long, 4>{};
    auto V8 = std::vector<long>(8, 0);

    using R8 = Ring<long>;

    R8 r8{V8};
    EXPECT_EQ(r8.size(), 8);
    r8[1 + static_cast<long>(r8.size())] = 42;

    const auto& cr8 = r8;
    EXPECT_EQ(cr8[1], 42);
    EXPECT_EQ(V8[1], 42);

    EXPECT_EQ(Ring<long>{A4}.size(), 4);
    EXPECT_EQ(Ring<long>{V8}.size(), 8);

    long buffer[8] = {1};  // NOLINT
    auto r = Ring<long>{buffer};

    EXPECT_EQ(r.size(), 8);
    EXPECT_EQ(r.size_bytes(), sizeof(buffer));
    EXPECT_FALSE(r.empty());
    EXPECT_EQ(r[0], 1);
    r[0] = 42;
    EXPECT_EQ(buffer[0], 42);
    EXPECT_EQ(buffer[0], r[static_cast<long>(r.size())]);
    EXPECT_EQ(&r[0], &r[static_cast<long>(r.size())]) << "wrap indexing in a ring storage";
}

TEST(Disruptor, Processor) {
    struct TestHandler : Processor::DefaultHandler<int> {
        std::size_t batches = 0;
        void onBatch(Sequence::Value, std::size_t s) { batches += s; }
        Consumed onConsume(int&, Sequence::Value seq, std::size_t) {
            if (seq == 0) return Consumed::Release;
            if (seq == 1) return Consumed::Keep;
            if (seq >= 3) throw std::invalid_argument{"Check OnException"};
            return Consumed::Exit;
        }
    };

    using Impl = Sequencers::SPMC<Waits::BlockStrategy, Barrier>;

    typename Impl::BarrierType pbar;
    typename Impl::WaitStrategy wait;
    Impl p(pbar, 8, wait);

    auto buffer = std::vector<int>{16, 0};
    auto ring = Ring<int>{buffer};

    typename Impl::BarrierType cbar;
    auto c = Impl::Consumer{cbar, p};
    p.add(c.cursor());

    TestHandler handler;
    Processor::Batch batch{int{}, c, ring, handler};
    p.publish(p.claim() - 1);
    p.publish(p.claim() - 1);
    p.publish(p.claim() - 1);

    EXPECT_FALSE(batch.running());
    EXPECT_EQ(handler.batches, 0);
    batch.run();
    EXPECT_EQ(handler.batches, 3);
    EXPECT_FALSE(batch.running());
    batch.halt();
    EXPECT_FALSE(batch.running());

    EXPECT_THAT(
        [&] {
            p.publish(p.claim() - 1);
            batch.run();
        },
        ThrowsMessage<std::runtime_error>("exception on #3 what:Check OnException"));
    EXPECT_FALSE(batch.running());
}
