#include <unistd.h>

#include "impl/SharedMemory.hpp"
#include "unit/Unit.hpp"

TEST(SharedMemory, Construction) {
    EXPECT_THAT([] { dlsm::SharedMemory::create(""); },
                ThrowsMessage<std::invalid_argument>(
                    "SharedMemory with opts: '' error: Missing required key=value for key:name"));

    EXPECT_THAT([] { dlsm::SharedMemory::create("name=/shm,create=on"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("error: Missing required key=value for key:size")));

    EXPECT_THAT([] { dlsm::SharedMemory::create("name=/shm,create=on,size=abc"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("error: 'size' has invalid value:abc")));

    EXPECT_THAT([] { dlsm::SharedMemory::create("name=/unit/test,create=on,size=4096"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("shm_open() with: /unit/test: Invalid argument")));

    EXPECT_THAT([] { dlsm::SharedMemory::create("name=/shm,size=16,create=on,readonly=on"); },
                ThrowsMessage<std::invalid_argument>(EndsWith("create and readonly options are not allowed")));

    EXPECT_THAT(
        [] { dlsm::SharedMemory::create("name=/unit-test-1,size=4096"); },
        ThrowsMessage<std::invalid_argument>(EndsWith("shm_open() with: /unit-test-1: No such file or directory")));

    EXPECT_THAT(
        [] {
            auto m1 = dlsm::SharedMemory::create("name=/unit-test-2,create=on,size=4096");
            auto m2 = dlsm::SharedMemory::create("name=/unit-test-2,create=on,size=4096");
        },
        ThrowsMessage<std::invalid_argument>(EndsWith("shm_open() with: /unit-test-2: File exists")));

    EXPECT_NO_THROW({
        auto owner = dlsm::SharedMemory::create("name=/unit-test-3,create=on,lock=on,size=4096");
        EXPECT_EQ(owner->name(), "/unit-test-3");
        EXPECT_EQ(owner->size(), 4096);
        EXPECT_NE(owner->address(), nullptr);

        {
            auto w = reinterpret_cast<std::size_t*>(owner->address());
            for (std::size_t i = 0, size = owner->size() / sizeof(w[0]); i < size; ++i) {
                w[i] = i;
            }
        }

        auto reader = dlsm::SharedMemory::create("name=/unit-test-3,readonly=on,size=8192");
        EXPECT_EQ(reader->name(), "/unit-test-3");
        EXPECT_EQ(reader->size(), 4096);
        EXPECT_NE(reader->address(), nullptr);

        {
            auto r = reinterpret_cast<std::size_t*>(reader->address());
            for (std::size_t i = 0, size = reader->size() / sizeof(r[0]); i < size; ++i) {
                EXPECT_EQ(r[i], i) << "item at index i must be equal to i";
            }
        }
    });
}

TEST(SharedMemory, DISABLED_HugePages2MB) {
    // See: https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt
    // Requires preconfigured huge pages in system, do:
    //  echo 20 > /proc/sys/vm/nr_hugepages
    // Watch:
    //  watch -n 1 "ls -la /dev/shm && grep '' -R /sys/kernel/mm/hugepages/"
    EXPECT_NO_THROW({
        const auto size = 20 * 1024 * 1024;
        auto owner = dlsm::SharedMemory::create("name=/unit-test-4,create=on,huge2mb=on,size=" + std::to_string(size));
        EXPECT_EQ(owner->name(), "/unit-test-4");
        EXPECT_EQ(owner->size(), size);
        EXPECT_NE(owner->address(), nullptr);
    });
}
