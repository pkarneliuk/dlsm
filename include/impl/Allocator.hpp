#pragma once

#include <stdlib.h>    // posix_memalign
#include <sys/mman.h>  // madvise

#include <limits>
#include <system_error>

namespace dlsm {

template <typename T>
struct THPAllocator {
    using value_type = T;

    THPAllocator() = default;

    template <class U>
    constexpr THPAllocator(const THPAllocator<U>&) noexcept {}

    constexpr static std::size_t HugePageSize = 1 << 21;  // 2 MB

    static void checkAllocationSize(std::size_t n) noexcept(false) {
        if (n == 0 || n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
    }
};

template <typename T>
struct MAdviseAllocator : public THPAllocator<T> {
    using Base = THPAllocator<T>;

    [[nodiscard]] T* allocate(std::size_t n) {
        Base::checkAllocationSize(n);
        void* p = nullptr;
        if (int err = ::posix_memalign(&p, Base::HugePageSize, n * sizeof(T)); err != 0) {
            throw std::system_error(err, std::generic_category(), "posix_memalign()");
        }
        if (0 != ::madvise(p, n * sizeof(T), MADV_HUGEPAGE)) {
            throw std::system_error(errno, std::generic_category(), "madvise(MADV_HUGEPAGE)");
        }
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t n) noexcept { ::free(p); }  // NOLINT
};

template <typename T>
struct MmapAllocator : public THPAllocator<T> {
    using Base = THPAllocator<T>;

    static constexpr std::size_t roundToHugePageSize(std::size_t n) noexcept {
        return (((n - 1) / Base::HugePageSize) + 1) * Base::HugePageSize;
    }

    [[nodiscard]] T* allocate(std::size_t n) {
        Base::checkAllocationSize(n);
        void* p = ::mmap(nullptr, roundToHugePageSize(n * sizeof(T)), PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (p == MAP_FAILED) {
            throw std::system_error(errno, std::generic_category(), "mmap(MAP_HUGETLB)");
        }

        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (0 != ::munmap(p, roundToHugePageSize(n * sizeof(T)))) {
            // ignore errno value
        }
    }
};

}  // namespace dlsm
