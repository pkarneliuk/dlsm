#include "impl/Disruptor.hpp"

namespace dlsm::Disruptor {

template struct Group<8>;
template struct Barriers::PointerBarrier<8>;
template struct Barriers::AtomicsBarrier<8>;
template struct Barriers::OffsetsBarrier<8>;

template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>>;

template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::SPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>&>;

template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>>;

template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::SpinsStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::YieldStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::BlockStrategy, Barriers::OffsetsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::PointerBarrier<8>&>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::AtomicsBarrier<8>&>;
template struct Sequencers::MPMC<Waits::ShareStrategy, Barriers::OffsetsBarrier<8>&>;

}  // namespace dlsm::Disruptor
