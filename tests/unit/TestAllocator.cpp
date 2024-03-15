#include <vector>

#include "impl/Allocator.hpp"
#include "unit/Unit.hpp"

/* See: https://docs.kernel.org/admin-guide/mm/hugetlbpage.html
# echo 10 > /proc/sys/vm/nr_hugepages
$ grep Huge /proc/meminfo
AnonHugePages:         0 kB
ShmemHugePages:        0 kB
FileHugePages:         0 kB
HugePages_Total:      10
HugePages_Free:       10
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
Hugetlb:           20480 kB

$ cat /sys/kernel/mm/transparent_hugepage/enabled
always [madvise] never
*/

template <template <typename> class Allocator>
void CheckCommon() {
    EXPECT_NO_THROW({
        static constexpr std::size_t size = (1 << 21);  // 2 MB
        auto allocator = Allocator<char>{};
        auto v = allocator.allocate(size);
        EXPECT_NE(v, nullptr);
        v[size - 1] = v[0] = -42;  // touch the first and the last
        allocator.deallocate(v, size);
    });

    std::vector<int, Allocator<int>> items;
    EXPECT_NO_THROW({
        for (int i = 0; i < 1000000; ++i) {
            items.emplace_back(i);
        }
    });

    EXPECT_THAT([&] { (void)Allocator<int>().allocate(0); }, Throws<std::bad_array_new_length>());
}

TEST(Allocator, DISABLED_MAdviseAllocator) {
    CheckCommon<dlsm::MAdviseAllocator>();

    EXPECT_THAT([&] { (void)dlsm::MAdviseAllocator<int>().allocate(0x10000000000); },
                ThrowsMessage<std::system_error>("posix_memalign(): Cannot allocate memory"));
}

TEST(Allocator, MmapAllocator) {
    // Requires preconfigured huge pages in system, do:
    // echo 10 > /proc/sys/vm/nr_hugepages
    // CheckCommon<dlsm::MmapAllocator>();

    EXPECT_THAT([&] { (void)dlsm::MmapAllocator<int>().allocate(0x10000000000); },
                ThrowsMessage<std::system_error>("mmap(MAP_HUGETLB): Cannot allocate memory"));
}
